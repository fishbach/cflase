obj-m += easylase.o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build

PWD := $(shell pwd)

ccflags-y := -DDEBUG

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f Module.symvers modules.order

install: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

uninstall:
	rm -f /lib/modules/$(KVERSION)/extra/easylase.ko
	depmod -a

load:
	sudo insmod easylase.ko

unload:
	sudo rmmod easylase

reload: unload load

info:
	modinfo easylase.ko

log:
	dmesg | grep -i easylase | tail -20
