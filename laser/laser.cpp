#include "laser.h"

#include <cflib/util/log.h>

using namespace cflib::util;

USE_LOG(LogCat::Etc)

Laser::Laser()
:
    ThreadVerify("Laser", Worker)
{
    easyLase_.setErrorCallback([this]() { easyLaseError(); });
    reset();
}

Laser::~Laser()
{
    idle();
    stopVerifyThread();
}

bool Laser::hasError() const
{
    SyncedThreadCall<bool> stc(this);
    if (!stc.verify(&Laser::hasError)) return stc.retval();
    return !error_.isNull();
}

QString Laser::errorString() const
{
    SyncedThreadCall<QString> stc(this);
    if (!stc.verify(&Laser::errorString)) return stc.retval();
    return error_;
}

void Laser::reset()
{
    if (!verifyThreadCall(&Laser::reset)) return;
    error_ = QString();
    easyLase_.connect();
    idle();
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

void Laser::easyLaseError()
{
    error_ = easyLase_.errorString();
}
