#include "laser.h"

#include <cflib/util/log.h>

using namespace cflib::util;

USE_LOG(LogCat::Etc)

Laser::Laser()
:
    ThreadVerify("Laser", Worker),
    readyTimer_(this, &Laser::checkEasyLaseReady)
{
    setThreadPrio(QThread::TimeCriticalPriority);
    easyLase_.setErrorCallback([this]() { easyLaseError(); });
    reset();
}

Laser::~Laser()
{
    easyLase_.idle();
    stopVerifyThread();
}

void Laser::reset()
{
    if (!verifyThreadCall(&Laser::reset)) return;
    readyTimer_.stop();
    hasError_ = false;
    error_ = QString();
    easyLase_.connect();
    easyLase_.idle();
}

bool Laser::hasError() const
{
    SyncedThreadCall<bool> stc(this);
    if (!stc.verify(&Laser::hasError)) return stc.retval();
    return hasError_;
}

QString Laser::errorString() const
{
    SyncedThreadCall<QString> stc(this);
    if (!stc.verify(&Laser::errorString)) return stc.retval();
    return error_;
}

void Laser::setErrorCallback(VoidFunc callback)
{
    if (!verifyThreadCall(&Laser::setErrorCallback, callback)) return;
    logFunctionTrace
    errorCallback_ = callback;
}

void Laser::waitForFinish()
{
    if (!verifySyncedThreadCall(&Laser::waitForFinish)) return;
    logFunctionTrace
}

void Laser::on()
{
    if (!verifyThreadCall(&Laser::on)) return;
    logFunctionTrace
    easyLase_.setTTL(0x03);
}

void Laser::off()
{
    if (!verifyThreadCall(&Laser::off)) return;
    logFunctionTrace
    easyLase_.setTTL(0x00);
}

void Laser::idle()
{
    if (!verifyThreadCall(&Laser::idle)) return;
    logFunctionTrace
    easyLase_.idle();
}

void Laser::show(quint16 pps, const EasyLase::Points & points)
{
    if (!verifyThreadCall(&Laser::show, pps, points)) return;
    logFunctionTrace
    easyLase_.show(pps, points);
}

void Laser::easyLaseError()
{
    hasError_ = true;
    error_ = easyLase_.errorString();
    if (errorCallback_) errorCallback_();
}

void Laser::checkEasyLaseReady()
{
    if (!easyLase_.isReady()) {
        readyTimer_.singleShot(0.002);
        return;
    }
    EasyLase::Points points = pointQueue_.dequeue();
    easyLase_.show(EasyLase::MaxSpeed, points);
    if (!pointQueue_.isEmpty()) {
        readyTimer_.singleShot(0.002);
    } else {
        logInfo("out of points");
    }
}

void Laser::test()
{
    if (!verifyThreadCall(&Laser::test)) return;
    logFunctionTrace

    EasyLase::Points points(EasyLase::MaxPoints, { .g = 35 });
    for (int i = 0 ; i < 200 ; ++i) {
        pointQueue_.enqueue(points);
    }
    logInfo("start");
    checkEasyLaseReady();
}