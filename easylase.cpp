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
const QByteArray LaserOn    = QByteArray::fromHex("a6a6a6a6a6a600010300");
const QByteArray LaserOff   = QByteArray::fromHex("a6a6a6a6a6a600010000");
const QByteArray LaserIdle  = QByteArray::fromHex("a5a5a5a5a5a5000102000000");
const QByteArray LaserData  = QByteArray::fromHex("a5a5a5a5a5a50001");

QByteArray toByteArray(quint16 number) { return QByteArray::fromRawData(reinterpret_cast<const char *>(&number), sizeof(number)); }

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
    return false;
}

bool EasyLase::on()
{
    logFunctionTrace
    qint64 c = device_.write(LaserOn);
    return c == LaserOn.size();
}

bool EasyLase::off()
{
    logFunctionTrace
    qint64 c = device_.write(LaserOff);
    return c == LaserOff.size();
}

bool EasyLase::idle()
{
    logFunctionTrace
    qint64 c = device_.write(LaserIdle);
    return c == LaserIdle.size();
}

bool EasyLase::show(quint16 pps, const Points & points)
{
    logFunctionTrace
    if (points.empty()) return idle();
    if (points.size() > 8.191) return false;
    QByteArray data = LaserData;
    data += toByteArray(pps);
    data += toByteArray(points.size());
    data.append(reinterpret_cast<const char *>(points.data()), points.size() * sizeof(Point));
    qint64 c = device_.write(data);
    return c == data.size();
}
