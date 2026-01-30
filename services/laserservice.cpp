#include "laserservice.h"

#include <cflib/util/log.h>

USE_LOG(LogCat::Http)

namespace services {

LaserService::LaserService() :
    RMIService(serializeTypeInfo().typeName)
{
    laser_.setErrorCallback([](const QString & error) {
        QTextStream(stderr) << "error: " << error << Qt::endl;
    });
    laser_.setActiveCallback([](bool active) {
        QTextStream(stdout) << "laser: " << (active ? "on" : "off") << Qt::endl;
    });
    laser_.setFinishedCallback([this]() {
        logTrace("signaling finished");
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
    return !laser_.hasError();
}

bool LaserService::off()
{
    laser_.off();
    return !laser_.hasError();
}

bool LaserService::idle()
{
    laser_.idle();
    return !laser_.hasError();
}

bool LaserService::show(const dao::LaserPoints & points, bool repeat, quint16 pps)
{
    laser_.show(points, repeat, pps);
    return !laser_.hasError();
}

}
