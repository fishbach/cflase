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
const QByteArray LaserTTL    = QByteArray::fromHex("a6a6a6a6a6a60001");
const QByteArray LaserIdle   = QByteArray::fromHex("a5a5a5a5a5a5000102000000");
const QByteArray LaserData   = QByteArray::fromHex("a5a5a5a5a5a50001");

QByteArray toByteArray(quint16 number) { return QByteArray(reinterpret_cast<const char *>(&number), sizeof(number)); }

}

EasyLase::EasyLase() :
    device_(DeviceName)
{
}

EasyLase::~EasyLase()
{
    logFunctionTrace
    if (device_.isOpen()) disconnect();
}

bool EasyLase::check(bool condition, const QString & msg)
{
    if (condition) return true;
    if (!error_.isNull()) return false;
    if (!msg.isEmpty()) {
        error_ = msg;
        if (!device_.errorString().isEmpty()) error_ += QString(" - %1").arg(device_.errorString());
    } else {
        error_ = device_.errorString();
        if (error_.isEmpty()) error_ = "unknown";
    }
    logWarn("error: %1", error_);
    disconnect();
    if (errorCallback_) errorCallback_();
    return false;
}

void EasyLase::connect()
{
    logFunctionTrace
    if (device_.isOpen()) disconnect();
    error_ = QString();
    bool ok = device_.open(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::ExistingOnly);
    if (!ok) {
        check(false, QString("cannot connect to EasyLase device %1").arg(DeviceName));
    } else {
        logInfo("connected to EasyLase device %1", DeviceName);
    }
}

void EasyLase::disconnect()
{
    device_.close();
    logInfo("disconnected from EasyLase device %1", DeviceName);
}

void EasyLase::setTTL(quint8 hiLow)
{
    logFunctionTrace
    QByteArray data = LaserTTL;
    data += toByteArray(hiLow);
    check(device_.write(data) == data.size(), "laser ttl");
}

void EasyLase::idle()
{
    logFunctionTrace
    check(device_.write(LaserIdle) == LaserIdle.size(), "laser idle");
}

bool EasyLase::isReady()
{
    if (!check(device_.write(LaserStatus) == LaserStatus.size(), "write status request")) return false;
    char c;
    if (!check(device_.getChar(&c), "read status")) return false;
    if (c == '\x33') return true;
    check(c == '\xcc', QString("funny status code: %1").arg((quint8)c));
    return false;
}

void EasyLase::show(quint16 pps, const Points & points)
{
    logFunctionTrace
    if (!check(points.size() <= MaxPoints, QString("too many points: %1").arg(points.size()))) return;
    QByteArray data = LaserData;
    data += toByteArray(pps);
    const quint16 size = points.size() * sizeof(Point);
    data += toByteArray(size);
    data.append(reinterpret_cast<const char *>(points.data()), size);
    logTrace("sending %1 bytes : %2", data.size(), data.toHex());
    check(device_.write(data) == data.size(), "laser data");
}
