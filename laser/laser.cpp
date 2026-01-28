#include "laser.h"

#include <cflib/util/log.h>

using namespace cflib::util;

USE_LOG(LogCat::Etc)

namespace {

inline quint16 convertAxis(double v) { return qMax(0, qMin(4095, qRound((v + 1.0) * 2047.5))); }

inline EasyLase::Point convertPoint(const Laser::Point & p)
{
    return EasyLase::Point{
        .x = convertAxis(p.x),
        .y = convertAxis(p.y),
        .r = p.r,
        .g = p.g,
        .b = p.b
    };
}

}

Laser::Laser()
:
    ThreadVerify("Laser", Worker),
    readyTimer_(this, &Laser::checkEasyLaseReady)
{
    setThreadPrio(QThread::TimeCriticalPriority);
    easyLase_.setErrorCallback([this]() { easyLaseError(); });
}

Laser::~Laser()
{
    idle();
    stopVerifyThread();
}

void Laser::reset()
{
    if (!verifyThreadCall(&Laser::reset)) return;
    hasError_ = false;
    error_    = QString();
    easyLase_.connect();
    idle();
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

void Laser::setErrorCallback(StringFunc callback)
{
    if (!verifyThreadCall(&Laser::setErrorCallback, callback)) return;
    logFunctionTrace
    errorCallback_ = callback;
}

void Laser::setActiveCallback(BoolFunc callback)
{
    if (!verifyThreadCall(&Laser::setActiveCallback, callback)) return;
    logFunctionTrace
    activeCallback_ = callback;
}

void Laser::setFinishedCallback(VoidFunc callback)
{
    if (!verifyThreadCall(&Laser::setFinishedCallback, callback)) return;
    logFunctionTrace
    finishedCallback_ = callback;
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

    bool doCallActiveCallback = false;
    if (isActive_) {
        logDebug("going idle");
        if (activeCallback_) doCallActiveCallback = true;
    }

    isActive_ = false;
    readyTimer_.stop();
    pointQueue_.clear();
    easyLase_.idle();
    if (doCallActiveCallback) activeCallback_(false);
}

void Laser::show(const Points & points, bool repeat, quint16 pps)
{
    if (!verifyThreadCall(&Laser::show, points, repeat, pps)) return;
    logFunctionTrace

    // empty input
    if (points.isEmpty() || pps == 0) {
        idle();
        return;
    }

    int replication = qMax(1, qRound((double)MaxSpeed / (double)pps));
    logDebug("showing %1 points %2 repeat and %3 pps (replication: %4)",
        points.size(), repeat ? "with" : "without", pps, replication);

    if (activeCallback_ && !isActive_) activeCallback_(true);

    EasyLase::Points pointBlock;

    // manage smooth continuation
    if (isActive_) {
        if (isRepeating_ || repeat) {
            pointQueue_.clear();
            easyLase_.idle();
        } else {
            pointQueue_.removeLast();   // Placeholder
            if (!pointQueue_.isEmpty() && pointQueue_.last().size() < EasyLase::MaxPoints) {
                pointBlock = pointQueue_.takeLast();
            }
        }
    }

    isActive_ = true;
    readyTimer_.stop();
    isRepeating_ = repeat;
    repeatPos_ = 0;

    pointBlock.reserve(EasyLase::MaxPoints);
    for (const Point & p : points) {
        EasyLase::Point ep = convertPoint(p);
        for (int i = 0 ; i < replication ; ++i) {
            pointBlock << ep;
            if (pointBlock.size() == EasyLase::MaxPoints) {
                pointQueue_ << pointBlock;
                pointBlock.resize(0);
            }
        }
    }
    if (!pointBlock.isEmpty()) pointQueue_ << pointBlock;

    if (repeat) {
        if (pointQueue_.size() == 1) {
            // EasyLase does the repetition.
            easyLase_.show(EasyLase::MaxSpeed, pointQueue_.takeFirst());
            return;
        }
    } else {
        // Placeholder to finish last block before going idle.
        pointQueue_ << EasyLase::Points(1, {});
    }
    checkEasyLaseReady();
}

void Laser::easyLaseError()
{
    readyTimer_.stop();
    pointQueue_.clear();
    hasError_ = true;
    error_ = easyLase_.errorString();
    if (errorCallback_) errorCallback_(error_);
}

void Laser::checkEasyLaseReady()
{
    if (!easyLase_.isReady()) {
        readyTimer_.singleShot(0.002);
        return;
    }
    if (isRepeating_) {
        easyLase_.show(EasyLase::MaxSpeed, pointQueue_[repeatPos_++]);
        if (repeatPos_ == pointQueue_.size()) repeatPos_ = 0;
        readyTimer_.singleShot(0.002);

        // check that next show has enough points
        EasyLase::Points & current = pointQueue_[repeatPos_];
        if (current.size() < EasyLase::MaxPoints) {
            int nextId = repeatPos_ + 1;
            if (nextId == pointQueue_.size()) nextId = 0;
            EasyLase::Points & next = pointQueue_[nextId];
            int missing = EasyLase::MaxPoints - current.size();
            current.append(next.mid(0, missing));
            next.remove(0, missing);
        }
    } else {
        if (pointQueue_.isEmpty()) {
            logDebug("out of points");
            idle();
        } else {
            easyLase_.show(EasyLase::MaxSpeed, pointQueue_.takeFirst());
            readyTimer_.singleShot(0.002);
            if (finishedCallback_) {
                if (pointQueue_.isEmpty())
                    logDebug("too late for finished callback");
                else if (pointQueue_.first().size() < EasyLase::MaxPoints)
                    finishedCallback_();
            }
        }
    }
}
