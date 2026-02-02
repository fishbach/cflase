#include "laserservice.h"

#include <cflib/util/log.h>

USE_LOG(LogCat::Http)

namespace services {

LaserService::LaserService() :
    RMIService(serializeTypeInfo().typeName)
{
    laser_.setErrorCallback([this](const QString & msg) {
        logDebug("signaling error: %1", msg);
        error(msg);
    });
    laser_.setActiveCallback([this](bool onOff) {
        logDebug("signaling active: %1", onOff);
        active(onOff);
    });
    laser_.setFinishedCallback([this]() {
        logDebug("signaling finished");
        finished();
    });
    laser_.reset();
}

LaserService::~LaserService()
{
    stopVerifyThread();
}

bool LaserService::on()
{
    laser_.on();
    laser_.waitForFinish();
    return !laser_.hasError();
}

bool LaserService::off()
{
    laser_.off();
    laser_.waitForFinish();
    return !laser_.hasError();
}

bool LaserService::idle()
{
    laser_.idle();
    laser_.waitForFinish();
    return !laser_.hasError();
}

void LaserService::show(const dao::LaserPoints & points, bool repeat, quint16 pps)
{
    laser_.show(points, repeat, pps);
}

void LaserService::identity()
{
    laser_.identity();
}

void LaserService::move(double dx, double dy)
{
    laser_.move(dx, dy);
}

void LaserService::scale(double sx, double sy)
{
    laser_.scale(sx, sy);
}

void LaserService::rotate(double radiant)
{
    laser_.rotate(radiant);
}

}
