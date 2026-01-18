#include "easylase.h"

#include <cflib/util/log.h>

#include <bit>

USE_LOG(LogCat::Etc)

namespace {

static_assert(sizeof(Easylase::Point) == 8);
static_assert(std::bit_cast<std::array<quint8, 8>>(Easylase::Point{.x = 0x1234, .y = 0x5678})[0] == 0x34);
static_assert(std::bit_cast<std::array<quint8, 8>>(Easylase::Point{.x = 0x1234, .y = 0x5678})[1] == 0x12);
static_assert(std::bit_cast<std::array<quint8, 8>>(Easylase::Point{.x = 0x1234, .y = 0x5678})[2] == 0x78);
static_assert(std::bit_cast<std::array<quint8, 8>>(Easylase::Point{.x = 0x1234, .y = 0x5678})[3] == 0x56);

const QString    DeviceName = "/dev/easylase0";
const QByteArray LaserOn    = QByteArray::fromHex("a6a6a6a6a6a600010300");
const QByteArray LaserOff   = QByteArray::fromHex("a6a6a6a6a6a600010000");
const QByteArray LaserIdle  = QByteArray::fromHex("a5a5a5a5a5a5000102000000");
const QByteArray LaserData  = QByteArray::fromHex("a5a5a5a5a5a50001");

}

Easylase::Easylase() :
    device_(DeviceName)
{
}

bool Easylase::connect()
{
    logFunctionTrace
    bool ok = device_.open(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::ExistingOnly);
    if (!ok) {
        logWarn("cannot connect to Easylase device %1 : %2", DeviceName, device_.errorString());
    } else {
        logInfo("connected to Easylase at %1", DeviceName);
    }
    return ok;
}

QString Easylase::error() const
{
    return device_.errorString();
}

bool Easylase::on()
{
    logFunctionTrace
    qint64 c = device_.write(LaserOn);
    return c == LaserOn.size();
}

bool Easylase::off()
{
    logFunctionTrace
    qint64 c = device_.write(LaserOff);
    return c == LaserOff.size();
}

bool Easylase::idle()
{
    logFunctionTrace
    qint64 c = device_.write(LaserIdle);
    return c == LaserIdle.size();
}

bool Easylase::show(quint16 pps, const Points & points)
{
    logFunctionTrace
    // QByteArray data = LaserData + QByteArray::fromHex(
    //     "f401" "a00f");
    // for (int i = 0 ; i < 250 ; ++i)
    //     data += QByteArray::fromHex("0008000800300000");
    // for (int i = 0 ; i < 250 ; ++i)
    //     data += QByteArray::fromHex("a008a00800300000");
    QByteArray data = LaserData;
    qint64 c = device_.write(data);
    return c == data.size();
}
