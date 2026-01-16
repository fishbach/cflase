/*
 * easylase.c  - kernel module for JMLaser EasyLase USB,
 * a USB based DAC for laser (show) scanning applications
 *
 * (C) 2025        Christian Fischbach <me@cfish.de>
 * (C) 2005        Robin Adams <radams@linux-laser.org>
 * (C) 2004        Robin Cutshaw <robin@hamlabs.com>
 * (C) 2002        Kuba Ober (kuba@mareimbrium.org)
 * (C) 1999 - 2001 Greg Kroah-Hartman (greg@kroah.com)
 *                 Bill Ryder (bryder@sgi.com)
 *
 * based on ft245.c and ftdi-sio.c
 *
 * see http://www.jmlaser.com for Hardware Details
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#define EASYLASE_DBG_ERROR		0x01
#define EASYLASE_DBG_INIT		0x02
#define EASYLASE_DBG_PROBE		0x04
#define EASYLASE_DBG_OPENCLOSE	0x08
#define EASYLASE_DBG_POLL		0x10
#define EASYLASE_DBG_WRITE		0x20
#define EASYLASE_DBG_CONTROL	0x40
#define EASYLASE_DBG_READ		0x80
#define EASYLASE_DBG_ALL		0xff

#define  EASYLASE_SET_LATENCY_TIMER_REQUEST 9
#define  EASYLASE_SET_LATENCY_TIMER_REQUEST_TYPE 0x40

static int debug = 0x07;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level");

#undef dbg
#define dbg(level, format, arg...) do { if (debug&level) pr_debug("easylase: " format "\n" , ## arg); } while (0)

#define DRIVER_VERSION "v2.0.0"
#define DRIVER_AUTHOR "Robin Adams, <radams@linux-laser.org>"
#define DRIVER_DESC "kernel module for EasyLase USB"

#define	MAX_EASYLASES		16
#define	USB_EASYLASE_MINOR_BASE	192

#define	READ_BUFFER_SIZE	0x10000		// 64k
#define	MAX_REQ_PACKET_SIZE	0x10000		// 64k
#define	RETRY_TIMEOUT		(HZ)
#define	MAX_WRITE_RETRY		5

#define EASYLASE_FLAGS_DEV_OPEN		0x01
#define EASYLASE_FLAGS_RX_BUSY		0x02
#define EASYLASE_FLAGS_DEV_ERROR		0x08
#define EASYLASE_FLAGS_PENDING_CLEANUP	0x10
#define EASYLASE_FLAGS_MASK			0x1f

#define FTDI_VID		0x0403
#define EASYLASE_DID		0xdbd0

static struct usb_device_id easylase_table [] = {
	{ USB_DEVICE(FTDI_VID, EASYLASE_DID) },	// EasyLase USB
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, easylase_table);

struct easylase_state {
	struct usb_device		*dev;		/* USB device handle */
	int				inEP;		/* read endpoint */
	int				outEP;		/* write endpoint */
	const struct usb_device_id	*info;		/* device info */
	int				subminor;	/* which minor dev #? */
	struct mutex			mtx;		/* locks this struct */

	char				*rbufp;		/* read buffer for I/O */
	int				rbufhead;
	int				rbufcnt;

	char				*callbackbuf;	/* read buffer for callback */
	int				callbackbufsize;

	char				*wbufp;		/* write buffer for I/O */
	int				wbufsize;

	struct urb			*rx_urb;
	int				flags;
	int				roverruns;
	int				roverruncnt;
	int				usboverruns;

	wait_queue_head_t		wait;		/* for timed waits */
	struct usb_host_endpoint	*endpoint;
	struct usb_interface		*interface;
};

static DEFINE_MUTEX(state_table_mutex);

static int		easylase_release(struct inode *, struct file *);
static void		easylase_disconnect(struct usb_interface *);
static int		easylase_probe(struct usb_interface *, const struct usb_device_id *);
static void 		easylase_read_bulk_callback(struct urb *);
static ssize_t		easylase_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t		easylase_write(struct file *, const char __user *, size_t, loff_t *);
static int		easylase_open(struct inode *, struct file *);
static __poll_t		easylase_poll(struct file *, struct poll_table_struct *);
static void		easylase_dump_buf(unsigned char *, unsigned char *, int);
static void		easylase_cleanup(struct easylase_state *);

static const struct file_operations usb_easylase_fops = {
	.owner		= THIS_MODULE,
	.read		= easylase_read,
	.write		= easylase_write,
	.open		= easylase_open,
	.release	= easylase_release,
	.poll		= easylase_poll,
	.llseek		= noop_llseek,
};

static struct usb_class_driver easylase_class = {
	.name		= "easylase%d",
	.fops		= &usb_easylase_fops,
	.minor_base	= USB_EASYLASE_MINOR_BASE,
};

static struct usb_driver easylase_driver = {
	.name		= "easylase",
	.id_table	= easylase_table,
	.probe		= easylase_probe,
	.disconnect	= easylase_disconnect,
};

static int easylase_set_latency_timer(struct usb_device *udev, int v)
{
	char buf[1];
	int rv = 0;

	rv = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			     EASYLASE_SET_LATENCY_TIMER_REQUEST,
			     EASYLASE_SET_LATENCY_TIMER_REQUEST_TYPE,
			     v, 0,
			     buf, 0, 5000);

	if (rv < 0) {
		dbg(EASYLASE_DBG_ERROR, "Unable to write latency timer: %i", rv);
		return -EIO;
	}

	return rv;
}

static int easylase_probe(struct usb_interface *interface, const struct usb_device_id *easylase_info)
{
	struct usb_device *dev = interface_to_usbdev(interface);
	int ret;
	struct usb_host_endpoint *endpoint;
	struct usb_interface_descriptor *interfaced;
	int direction, ep;
	char mfg[30], product[30], serialnumber[30];
	struct easylase_state *easylase = NULL;

	if (dev->descriptor.bNumConfigurations != 1 ||
	    dev->config[0].desc.bNumInterfaces != 1) {
		dbg(EASYLASE_DBG_ERROR, "Bogus easylase config info");
		return -ENODEV;
	}

	interfaced = &interface->cur_altsetting->desc;

	if ((interfaced->bInterfaceClass != USB_CLASS_PER_INTERFACE &&
	     interfaced->bInterfaceClass != USB_CLASS_VENDOR_SPEC) ||
	    interfaced->bNumEndpoints != 2) {
		dbg(EASYLASE_DBG_ERROR, "Bogus easylase interface info");
		dbg(EASYLASE_DBG_ERROR, "biclass %x biend %x",
		    interfaced->bInterfaceClass,
		    interfaced->bNumEndpoints);
		return -ENODEV;
	}

	memset(mfg, 0, 30);
	memset(product, 0, 30);
	memset(serialnumber, 0, 30);
	usb_string(dev, dev->descriptor.iManufacturer, mfg, 29);
	usb_string(dev, dev->descriptor.iProduct, product, 29);
	usb_string(dev, dev->descriptor.iSerialNumber, serialnumber, 29);
	dbg(EASYLASE_DBG_PROBE, "easylase_probe mfg/product/sn %s/%s/%s",
	    mfg, product, serialnumber);

	easylase = kzalloc(sizeof(*easylase), GFP_KERNEL);
	if (!easylase) {
		dev_err(&interface->dev, "no memory!\n");
		return -ENOMEM;
	}

	mutex_init(&easylase->mtx);
	easylase->info = easylase_info;
	easylase->interface = interface;

	init_waitqueue_head(&easylase->wait);

	easylase->endpoint = endpoint = interface->cur_altsetting->endpoint;
	easylase->outEP = easylase->inEP = -1;

	ep = endpoint[0].desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	direction = endpoint[0].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK;
	if (direction == USB_DIR_IN) {
		easylase->inEP = ep;
		easylase->callbackbufsize = endpoint[0].desc.wMaxPacketSize;
	} else {
		easylase->wbufsize = endpoint[0].desc.wMaxPacketSize;
		easylase->outEP = ep;
	}

	ep = endpoint[1].desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	direction = endpoint[1].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK;
	if (direction == USB_DIR_IN) {
		easylase->inEP = ep;
		easylase->callbackbufsize = endpoint[0].desc.wMaxPacketSize;
	} else {
		easylase->wbufsize = endpoint[0].desc.wMaxPacketSize;
		easylase->outEP = ep;
	}

	dbg(EASYLASE_DBG_PROBE, "max packet size read %d write %d",
	    easylase->callbackbufsize, easylase->wbufsize);

	easylase->rbufp = kmalloc(READ_BUFFER_SIZE, GFP_KERNEL);
	if (!easylase->rbufp) {
		dev_err(&interface->dev, "no memory for read buffer!\n");
		ret = -ENOMEM;
		goto error;
	}

	easylase->rbufhead = 0;
	easylase->rbufcnt = 0;
	easylase->callbackbufsize = 4096;

	easylase->callbackbuf = kmalloc(easylase->callbackbufsize, GFP_KERNEL);
	if (!easylase->callbackbuf) {
		dev_err(&interface->dev, "no memory for callback buffer!\n");
		ret = -ENOMEM;
		goto error;
	}

	easylase->wbufp = kmalloc(READ_BUFFER_SIZE, GFP_KERNEL);
	if (!easylase->wbufp) {
		dev_err(&interface->dev, "no memory for write buffer!\n");
		ret = -ENOMEM;
		goto error;
	}

	if (easylase->outEP == -1 || easylase->inEP == -1 ||
	    endpoint[0].desc.bmAttributes != USB_ENDPOINT_XFER_BULK ||
	    endpoint[1].desc.bmAttributes != USB_ENDPOINT_XFER_BULK) {
		dbg(EASYLASE_DBG_ERROR, "Bogus endpoints");
		ret = -ENODEV;
		goto error;
	}

	easylase->dev = dev;

	easylase->rx_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!easylase->rx_urb) {
		dbg(EASYLASE_DBG_ERROR, "usb_alloc_urb failed");
		ret = -ENOMEM;
		goto error;
	}

	usb_set_intfdata(interface, easylase);
	ret = usb_register_dev(interface, &easylase_class);

	if (ret) {
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	easylase->subminor = interface->minor - USB_EASYLASE_MINOR_BASE;

	dev_info(&interface->dev, "USB EASYLASE #%d connected, major/minor %d/%d\n",
		 easylase->subminor, USB_MAJOR, USB_EASYLASE_MINOR_BASE + easylase->subminor);

	easylase_set_latency_timer(dev, 2);
	return 0;

error:
	kfree(easylase->wbufp);
	kfree(easylase->callbackbuf);
	kfree(easylase->rbufp);
	kfree(easylase);
	return ret;
}

static int easylase_open(struct inode *inode, struct file *file)
{
	struct easylase_state *easylase;
	int subminor;
	int value = 0;
	int res;
	int result;
	struct usb_interface *interface;

	dbg(EASYLASE_DBG_OPENCLOSE, "easylase_open");

	mutex_lock(&state_table_mutex);

	subminor = iminor(inode);
	interface = usb_find_interface(&easylase_driver, subminor);

	if (!interface) {
		mutex_unlock(&state_table_mutex);
		return -ENODEV;
	}

	easylase = usb_get_intfdata(interface);
	if (!easylase) {
		mutex_unlock(&state_table_mutex);
		return -ENODEV;
	}

	mutex_lock(&easylase->mtx);
	mutex_unlock(&state_table_mutex);

	if (easylase->flags & EASYLASE_FLAGS_DEV_OPEN) {
		value = -EBUSY;
		goto done;
	}

	dbg(EASYLASE_DBG_OPENCLOSE, "open #%d", subminor);

	file->private_data = easylase;

	easylase->rbufhead = 0;
	easylase->rbufcnt = 0;
	easylase->flags = EASYLASE_FLAGS_DEV_OPEN;
	easylase->roverruns = 0;
	easylase->roverruncnt = 0;
	easylase->usboverruns = 0;

	usb_fill_bulk_urb(easylase->rx_urb, easylase->dev,
			  usb_rcvbulkpipe(easylase->dev, easylase->inEP),
			  easylase->callbackbuf, easylase->callbackbufsize,
			  easylase_read_bulk_callback, easylase);

	res = usb_submit_urb(easylase->rx_urb, GFP_KERNEL);
	if (res)
		dev_warn(&easylase->interface->dev, "failed submit rx_urb %d\n", res);

	result = usb_control_msg(easylase->dev, usb_sndctrlpipe(easylase->dev, 0),
				 0, 0x40, 0, 0, NULL, 0, HZ * 5);

	if (result == -ETIMEDOUT)
		dbg(EASYLASE_DBG_ERROR, "easylase_open control timeout");
	else if (result < 0)
		dbg(EASYLASE_DBG_ERROR, "easylase_open control failed");
	else
		dbg(EASYLASE_DBG_OPENCLOSE, "easylase_open control ok (%d)", result);

done:
	mutex_unlock(&easylase->mtx);
	return value;
}

static ssize_t easylase_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	struct easylase_state *easylase;
	int retval = 0;

	if (len > MAX_REQ_PACKET_SIZE) {
		dbg(EASYLASE_DBG_ERROR, "easylase_read %zu too large", len);
		return -EINVAL;
	}

	easylase = file->private_data;
	mutex_lock(&easylase->mtx);

	if (!easylase->dev) {
		dbg(EASYLASE_DBG_ERROR, "easylase_read %zu no dev!", len);
		retval = -ENODEV;
		goto done;
	}

	if (easylase->flags & EASYLASE_FLAGS_DEV_ERROR) {
		dbg(EASYLASE_DBG_ERROR, "easylase_read device error");
		retval = -EIO;
		goto done;
	}

	dbg(EASYLASE_DBG_READ, "easylase_read ask %zu avail %d head %d flags 0x%x",
	    len, easylase->rbufcnt, easylase->rbufhead, easylase->flags);

	if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN)) {
		dbg(EASYLASE_DBG_ERROR, "easylase_read not open! (flags 0x%x) 1",
		    easylase->flags);
		retval = -ENODEV;
		goto done;
	}

	if ((easylase->flags & EASYLASE_FLAGS_RX_BUSY) || (easylase->rbufcnt == 0)) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto done;
		}

		while ((easylase->flags & EASYLASE_FLAGS_RX_BUSY) || (easylase->rbufcnt == 0)) {
			mutex_unlock(&easylase->mtx);
			if (wait_event_interruptible(easylase->wait,
			    !(easylase->flags & EASYLASE_FLAGS_RX_BUSY) ||
			    easylase->rbufcnt > 0 ||
			    !(easylase->flags & EASYLASE_FLAGS_DEV_OPEN))) {
				return -EINTR;
			}
			mutex_lock(&easylase->mtx);

			if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN))
				break;
		}
	}

	if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN)) {
		dbg(EASYLASE_DBG_ERROR, "easylase_read not open! (flags 0x%x) 2",
		    easylase->flags);
		retval = -ENODEV;
		goto done;
	}

	if (!easylase->dev) {
		dbg(EASYLASE_DBG_ERROR, "easylase_read no dev!");
		retval = -ENODEV;
		goto done;
	}

	if (easylase->rbufcnt < len)
		len = easylase->rbufcnt;

	if (copy_to_user(buf, &easylase->rbufp[easylase->rbufhead], len)) {
		retval = -EFAULT;
		goto done;
	} else {
		dbg(EASYLASE_DBG_READ, "easylase_read returned %zu", len);
		retval = len;
		easylase->rbufcnt -= len;
		if (easylase->rbufcnt == 0)
			easylase->rbufhead = 0;
		else
			easylase->rbufhead += len;
	}

done:
	mutex_unlock(&easylase->mtx);
	return retval;
}

static ssize_t easylase_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	struct easylase_state *easylase;
	ssize_t bytes_written = 0;

	if (len > MAX_REQ_PACKET_SIZE) {
		dbg(EASYLASE_DBG_ERROR, "easylase_write %zu too large", len);
		return -EINVAL;
	}

	easylase = file->private_data;
	mutex_lock(&easylase->mtx);

	if (!easylase->dev) {
		mutex_unlock(&easylase->mtx);
		dbg(EASYLASE_DBG_ERROR, "easylase_write %zu no dev!", len);
		return -ENODEV;
	}

	if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN)) {
		dbg(EASYLASE_DBG_ERROR, "easylase_write %zu not open!", len);
		mutex_unlock(&easylase->mtx);
		return -ENODEV;
	}

	if (easylase->flags & EASYLASE_FLAGS_DEV_ERROR) {
		dbg(EASYLASE_DBG_ERROR, "easylase_write device error");
		mutex_unlock(&easylase->mtx);
		return -EIO;
	}

	dbg(EASYLASE_DBG_WRITE, "easylase_write len %zu flags 0x%x", len, easylase->flags);

	while (len > 0) {
		char *obuf = easylase->wbufp;
		int maxretry = MAX_WRITE_RETRY;
		unsigned long copy_size, thistime;

		thistime = copy_size = len;
		if (copy_from_user(obuf, buf, copy_size)) {
			bytes_written = -EFAULT;
			break;
		}

		easylase_dump_buf("write ", obuf, copy_size);

		while (thistime) {
			int result;
			int count;

			if (signal_pending(current)) {
				if (!bytes_written)
					bytes_written = -EINTR;
				goto done;
			}

			result = usb_bulk_msg(easylase->dev,
					      usb_sndbulkpipe(easylase->dev, easylase->outEP),
					      obuf, thistime, &count, HZ*10);

			if (count != thistime)
				dbg(EASYLASE_DBG_WRITE, "write USB count %d thistime %lu",
				    count, thistime);
			if (result)
				dbg(EASYLASE_DBG_WRITE, "write USB err - %d", result);

			if (count) {
				obuf += count;
				thistime -= count;
				maxretry = MAX_WRITE_RETRY;
				continue;
			} else if (!result) {
				break;
			}

			if (result == -ETIMEDOUT) {
				if (!maxretry--) {
					if (!bytes_written)
						bytes_written = -ETIME;
					goto done;
				}
				mutex_unlock(&easylase->mtx);
				wait_event_interruptible_timeout(easylase->wait, 0, RETRY_TIMEOUT);
				mutex_lock(&easylase->mtx);
				continue;
			}

			if (!bytes_written)
				bytes_written = -EIO;
			goto done;
		}
		bytes_written += copy_size;
		len -= copy_size;
		buf += copy_size;
	}

done:
	mutex_unlock(&easylase->mtx);
	dbg(EASYLASE_DBG_WRITE, "wrote %zd", bytes_written);
	return bytes_written;
}

static void easylase_read_bulk_callback(struct urb *urb)
{
	struct easylase_state *easylase = urb->context;
	int res, rbuftail, callbackndx, chunksize;
	int count = urb->actual_length;

	if (!easylase) {
		dbg(EASYLASE_DBG_ERROR, "easylase_read_callback easylase NULL!");
		return;
	}

	if (urb->status != 0)
		dbg(EASYLASE_DBG_READ, "easylase_read_bulk_callback count %d flags %d status %d over %d overcnt %d usbover %d",
		    count, easylase->flags, urb->status,
		    easylase->roverruns, easylase->roverruncnt, easylase->usboverruns);

	if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN)) {
		dbg(EASYLASE_DBG_READ, "easylase_read_bulk_callback dev not open!");
		return;
	}

	if (easylase->flags & EASYLASE_FLAGS_RX_BUSY) {
		dbg(EASYLASE_DBG_ERROR, "easylase RX busy");
		goto reload;
	}

	easylase->flags |= EASYLASE_FLAGS_RX_BUSY;

	switch (urb->status) {
	case -EOVERFLOW:
		easylase->usboverruns++;
		dbg(EASYLASE_DBG_ERROR, "read data overrun");
		break;

	case 0:
		if (count > 2) {
			dbg(EASYLASE_DBG_CONTROL,
			    "read count %d head/cnt %d/%d (%d+%d) %02x %02x",
			    count, easylase->rbufhead, easylase->rbufcnt, count/64, count%64,
			    (unsigned char)easylase->callbackbuf[2],
			    (unsigned char)easylase->callbackbuf[3]);
			if (count < 32)
				easylase_dump_buf("read ", easylase->callbackbuf,
						  count < 16 ? count : 16);
		}

		callbackndx = 0;
		while (callbackndx < count) {
			chunksize = count - callbackndx;
			if (chunksize > 64)
				chunksize = 64;
			if (chunksize < 2) {
				dbg(EASYLASE_DBG_ERROR, "easylase_read_bulk_callback short %d",
				    chunksize);
				easylase_dump_buf("read ", &easylase->callbackbuf[callbackndx],
						  chunksize);
				break;
			}

			if ((easylase->callbackbuf[callbackndx+0] != 0x31) ||
			    ((easylase->callbackbuf[callbackndx+1] != 0x60) &&
			     (easylase->callbackbuf[callbackndx+1] != 0x00))) {
				dbg(EASYLASE_DBG_ERROR,
				    "2 byte header error!  May be out of sync.");
				dbg(EASYLASE_DBG_ERROR, "easylase_read_bulk_callback count %d chunksize %d flags %d status %d over %d overcnt %d usbover %d",
				    count, chunksize, easylase->flags,
				    urb->status, easylase->roverruns,
				    easylase->roverruncnt,
				    easylase->usboverruns);
				easylase_dump_buf("readerr ",
						  &easylase->callbackbuf[callbackndx],
						  chunksize < 16 ? chunksize : 16);
				break;
			}

			if (chunksize == 2)
				break;

			callbackndx += 2;
			chunksize -= 2;

			rbuftail = easylase->rbufhead + easylase->rbufcnt;
			if (rbuftail + chunksize > READ_BUFFER_SIZE) {
				easylase->roverruns++;
				easylase->roverruncnt += chunksize;
				dbg(EASYLASE_DBG_ERROR,
				    "easylase_read_bulk_callback dropped %d", chunksize);
			} else {
				memcpy(&easylase->rbufp[rbuftail],
				       &easylase->callbackbuf[callbackndx],
				       chunksize);
				easylase->rbufcnt += chunksize;
				dbg(EASYLASE_DBG_CONTROL,
				    "easylase_read_bulk_callback added %d", chunksize);
				easylase_dump_buf("read chunk ", &easylase->callbackbuf[callbackndx],
						  chunksize < 16 ? chunksize : 16);
				wake_up_interruptible(&easylase->wait);
			}
			callbackndx += chunksize;
		}
		goto reload;

	case -ETIMEDOUT:
		dbg(EASYLASE_DBG_ERROR, "read no response");
		break;

	case -ENOENT:
		dbg(EASYLASE_DBG_ERROR, "read urb killed");
		break;

	case -ECONNRESET:
		dbg(EASYLASE_DBG_ERROR, "read connection reset");
		break;

	case -EILSEQ:
		dbg(EASYLASE_DBG_ERROR, "CRC error");
		break;

	case -EPROTO:
		dbg(EASYLASE_DBG_ERROR, "internal error");
		break;

	default:
		dbg(EASYLASE_DBG_ERROR, "easylase RX status %d", urb->status);
		break;
	}

	easylase->flags &= ~EASYLASE_FLAGS_RX_BUSY;
	easylase->flags |= EASYLASE_FLAGS_DEV_ERROR;
	wake_up_interruptible(&easylase->wait);
	return;

reload:
	usb_fill_bulk_urb(easylase->rx_urb, easylase->dev,
			  usb_rcvbulkpipe(easylase->dev, easylase->inEP),
			  easylase->callbackbuf, easylase->callbackbufsize,
			  easylase_read_bulk_callback, easylase);
	easylase->rx_urb->status = 0;

	res = usb_submit_urb(easylase->rx_urb, GFP_ATOMIC);
	if (res)
		dev_warn(&easylase->interface->dev, "failed submit rx_urb %d\n", res);

	easylase->flags &= ~EASYLASE_FLAGS_RX_BUSY;
	wake_up_interruptible(&easylase->wait);
}

static __poll_t easylase_poll(struct file *file, struct poll_table_struct *wait)
{
	struct easylase_state *easylase;
	__poll_t rp, wp;
	static __poll_t lastrp = 0, lastwp = 0;

	easylase = file->private_data;
	if (!easylase || !easylase->dev) {
		dbg(EASYLASE_DBG_ERROR, "easylase_poll NULL easylase!");
		return EPOLLERR;
	}

	if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN)) {
		dbg(EASYLASE_DBG_ERROR, "easylase_poll device not open");
		return EPOLLERR | EPOLLHUP;
	}

	if (easylase->flags & EASYLASE_FLAGS_DEV_ERROR) {
		dbg(EASYLASE_DBG_ERROR, "easylase_poll device error");
		return EPOLLERR | EPOLLHUP;
	}

	poll_wait(file, &easylase->wait, wait);

	if (!easylase || !easylase->dev) {
		dbg(EASYLASE_DBG_ERROR, "easylase_poll NULL easylase! (after wait)");
		return EPOLLERR;
	}

	if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN)) {
		dbg(EASYLASE_DBG_ERROR, "easylase_poll device not open (after wait)");
		return EPOLLERR | EPOLLHUP;
	}

	if (easylase->flags & EASYLASE_FLAGS_DEV_ERROR) {
		dbg(EASYLASE_DBG_ERROR, "easylase_poll device error");
		return EPOLLERR | EPOLLHUP;
	}

	rp = easylase->rbufcnt == 0 ? 0 : EPOLLIN | EPOLLRDNORM;
	wp = EPOLLOUT | EPOLLWRNORM;

	if ((lastrp != rp) || (lastwp != wp)) {
		dbg(EASYLASE_DBG_POLL, "easylase_poll rd 0x%02x wr 0x%02x", rp, wp);
		lastrp = rp;
		lastwp = wp;
	}

	return rp | wp;
}

static int easylase_release(struct inode *inode, struct file *file)
{
	struct easylase_state *easylase;

	dbg(EASYLASE_DBG_OPENCLOSE, "easylase_release");

	if (!file) {
		dbg(EASYLASE_DBG_ERROR, "Release with no file structure!");
		return 0;
	}

	easylase = file->private_data;
	if (!easylase) {
		dbg(EASYLASE_DBG_ERROR, "Release with no device structure!");
		return 0;
	}

	if (easylase->flags & EASYLASE_FLAGS_PENDING_CLEANUP)
		dbg(EASYLASE_DBG_ERROR, "Release with device pending cleanup!");
	else if (!(easylase->flags & EASYLASE_FLAGS_DEV_OPEN)) {
		dbg(EASYLASE_DBG_ERROR, "Release with device not open (flags)!");
		return 0;
	}

	if ((easylase->flags & ~EASYLASE_FLAGS_MASK) != 0) {
		dbg(EASYLASE_DBG_ERROR, "Release corrupt flags! 0x%x", easylase->flags);
		return 0;
	}

	dbg(EASYLASE_DBG_OPENCLOSE, "easylase_release flags 0x%x close #%d over %d overcnt %d usbover %d",
	    easylase->flags, easylase->subminor, easylase->roverruns,
	    easylase->roverruncnt, easylase->usboverruns);

	mutex_lock(&state_table_mutex);
	mutex_lock(&easylase->mtx);

	if (easylase->flags & EASYLASE_FLAGS_PENDING_CLEANUP) {
		easylase_cleanup(easylase);
	} else {
		easylase->flags = 0;
		usb_kill_urb(easylase->rx_urb);
		mutex_unlock(&easylase->mtx);
	}

	mutex_unlock(&state_table_mutex);

	return 0;
}

static void easylase_disconnect(struct usb_interface *interface)
{
	struct easylase_state *easylase;
	int subminor;

	mutex_lock(&state_table_mutex);
	easylase = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	if (!easylase) {
		dbg(EASYLASE_DBG_ERROR, "easylase_disconnect NULL easylase!");
		mutex_unlock(&state_table_mutex);
		return;
	}

	subminor = easylase->subminor;

	dbg(EASYLASE_DBG_OPENCLOSE, "easylase_disconnect");
	dbg(EASYLASE_DBG_OPENCLOSE, "easylase_disconnect flags 0x%x", easylase->flags);

	mutex_lock(&easylase->mtx);
	usb_deregister_dev(interface, &easylase_class);

	if (easylase->flags & EASYLASE_FLAGS_DEV_OPEN) {
		dbg(EASYLASE_DBG_ERROR, "easylase_disconnect Disconnect before release!");
		usb_kill_urb(easylase->rx_urb);
		easylase->flags = EASYLASE_FLAGS_PENDING_CLEANUP;
		mutex_unlock(&easylase->mtx);
	} else {
		easylase->flags = 0;
		easylase_cleanup(easylase);
	}

	dev_info(&interface->dev, "USB EASYLASE #%d disconnected\n", subminor);
	mutex_unlock(&state_table_mutex);
}

static void easylase_cleanup(struct easylase_state *easylase)
{
	usb_free_urb(easylase->rx_urb);
	wake_up_interruptible(&easylase->wait);
	kfree(easylase->rbufp);
	kfree(easylase->callbackbuf);
	kfree(easylase->wbufp);
	mutex_unlock(&easylase->mtx);
	kfree(easylase);
}

static void easylase_dump_buf(unsigned char *str, unsigned char *buf, int len)
{
	int i, j, linendx;
	unsigned char linebuf[81], c;

	if (!buf)
		return;

	for (i = 0; i < len; i += 16) {
		linendx = 0;
		for (j = 0; (j < 16) && (i+j < len); j++) {
			c = buf[i+j];
			if (((c>>4) & 0x0f) > 9)
				linebuf[linendx++] = ((c>>4) & 0x0f)-10 + 'a';
			else
				linebuf[linendx++] = ((c>>4) & 0x0f) + '0';
			if ((c & 0x0f) > 9)
				linebuf[linendx++] = (c & 0x0f)-10 + 'a';
			else
				linebuf[linendx++] = (c & 0x0f) + '0';
			linebuf[linendx++] = ' ';
		}
		for (; j < 16; j++) {
			linebuf[linendx++] = ' ';
			linebuf[linendx++] = ' ';
			linebuf[linendx++] = ' ';
		}
		linebuf[linendx++] = ' ';
		linebuf[linendx++] = '|';
		for (j = 0; (j < 16) && (i+j < len); j++) {
			c = buf[i+j];
			if ((c >= '!') && (c <= '~'))
				linebuf[linendx++] = c;
			else
				linebuf[linendx++] = '.';
		}
		for (; j < 16; j++)
			linebuf[linendx++] = ' ';
		linebuf[linendx++] = '|';
		linebuf[linendx++] = '\0';
		dbg((EASYLASE_DBG_WRITE|EASYLASE_DBG_CONTROL|EASYLASE_DBG_READ), "%s%s", str, linebuf);
	}
}

static int __init usb_easylase_init(void)
{
	int result;

	dbg(EASYLASE_DBG_INIT, "EASYLASE driver init (debug=0x%02x)", debug);

	result = usb_register(&easylase_driver);
	if (result < 0) {
		pr_err("easylase: usb_register failed. Error number %d\n", result);
		return result;
	}

	pr_info("%s:%s\n", DRIVER_VERSION, DRIVER_DESC);
	return 0;
}

static void __exit usb_easylase_cleanup(void)
{
	dbg(EASYLASE_DBG_INIT, "EASYLASE driver exit");
	usb_deregister(&easylase_driver);
}

module_init(usb_easylase_init);
module_exit(usb_easylase_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");