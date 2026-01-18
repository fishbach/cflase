#include "easylase.h"

#include <cflib/util/log.h>

#include <bit>

USE_LOG(LogCat::Etc)

namespace {

static_assert(sizeof(EasyLase::Point) == 8);
static_assert(std::bit_cast<std::array<quint8, 8>>(EasyLase::Point{.x = 0x1234, .y = 0x5678, .r = 0x9a, .g = 0xbc})[0] == 0x34);
static_assert(std::bit_cast<std::array<quint8, 8>>(EasyLase::Point{.x = 0x1234, .y = 0x5678, .r = 0x9a, .g = 0xbc})[1] == 0x12);
static_assert(std::bit_cast<std::array<quint8, 8>>(EasyLase::Point{.x = 0x1234, .y = 0x5678, .r = 0x9a, .g = 0xbc})[2] == 0x78);
static_assert(std::bit_cast<std::array<quint8, 8>>(EasyLase::Point{.x = 0x1234, .y = 0x5678, .r = 0x9a, .g = 0xbc})[3] == 0x56);
static_assert(std::bit_cast<std::array<quint8, 8>>(EasyLase::Point{.x = 0x1234, .y = 0x5678, .r = 0x9a, .g = 0xbc})[4] == 0x9a);
static_assert(std::bit_cast<std::array<quint8, 8>>(EasyLase::Point{.x = 0x1234, .y = 0x5678, .r = 0x9a, .g = 0xbc})[5] == 0xbc);

const QString    DeviceName = "/dev/easylase0";
const QByteArray LaserStatus = QByteArray::fromHex("a9a9a9a9a9a9");
const QByteArray LaserOn     = QByteArray::fromHex("a6a6a6a6a6a600010300");
const QByteArray LaserOff    = QByteArray::fromHex("a6a6a6a6a6a600010000");
const QByteArray LaserReset  = QByteArray::fromHex("a5a5a5a5a5a5000102000000");
const QByteArray LaserData   = QByteArray::fromHex("a5a5a5a5a5a50001");

QByteArray toByteArray(quint16 number) { return QByteArray(reinterpret_cast<const char *>(&number), sizeof(number)); }

}

EasyLase::EasyLase() :
    device_(DeviceName)
{
}

bool EasyLase::connect()
{
    logFunctionTrace
    bool ok = device_.open(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::ExistingOnly);
    if (!ok) {
        logWarn("cannot connect to EasyLase device %1 : %2", DeviceName, device_.errorString());
    } else {
        logInfo("connected to EasyLase at %1", DeviceName);
    }
    return ok;
}

QString EasyLase::error() const
{
    return device_.errorString();
}

bool EasyLase::isReady()
{
    if (device_.write(LaserStatus) != LaserStatus.size()) {
        logWarn("cannot write status command: ", device_.errorString());
        return false;
    }
    char c;
    if (!device_.getChar(&c)) {
        logWarn("cannot read status: ", device_.errorString());
        return false;
    }
    return c == '\x33'; // 0xcc -> busy
}

bool EasyLase::on()
{
    logFunctionTrace
    return device_.write(LaserOn) == LaserOn.size();
}

bool EasyLase::off()
{
    logFunctionTrace
    return device_.write(LaserOff) == LaserOff.size();
}

bool EasyLase::reset()
{
    logFunctionTrace
    return device_.write(LaserReset) == LaserReset.size();
}

bool EasyLase::show(quint16 pps, const Points & points)
{
    logFunctionTrace
    if (points.empty()) return reset();
    if (points.size() > MaxPoints) return false;
    QByteArray data = LaserData;
    data += toByteArray(pps);
    const quint16 size = points.size() * sizeof(Point);
    data += toByteArray(size);
    data.append(reinterpret_cast<const char *>(points.data()), size);
    logTrace("sending (hex): %1", data.toHex());
    qint64 c = device_.write(data);
    return c == data.size();
}
