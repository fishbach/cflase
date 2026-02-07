#include "laser.h"

#include <cflib/util/log.h>

using namespace cflib::util;
using namespace math;

USE_LOG(LogCat::Etc)

namespace {

constexpr quint16 BlockSize = Laser::OptimalPointsPerBlock / 2;

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
    hasError_  = false;
    error_     = QString();
    identity();
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
    easyLase_.idle();
    QThread::msleep(100);
    easyLase_.setTTL(0x03);
}

void Laser::off()
{
    if (!verifyThreadCall(&Laser::off)) return;
    logFunctionTrace
    easyLase_.idle();
    QThread::msleep(100);
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

void Laser::show(const Points & points, bool repeat)
{
    if (!verifyThreadCall(&Laser::show, points, repeat)) return;
    logFunctionTrace

    // empty input
    if (points.isEmpty()) {
        idle();
        return;
    }

    logDebug("showing %1 points %2 repeat", points.size(), repeat ? "with" : "without");

    if (activeCallback_ && !isActive_) activeCallback_(true);

    EasyLase::Points pointBlock;

    // manage smooth continuation
    if (isActive_) {
        if (isRepeating_ || repeat) {
            pointQueue_.clear();
        } else if (!pointQueue_.isEmpty()) {
            pointQueue_.removeLast();   // Placeholder
            if (!pointQueue_.isEmpty() && pointQueue_.last().size() < BlockSize) {
                pointBlock = pointQueue_.takeLast();
            }
        }
    }

    pointBlock.reserve(BlockSize);
    for (const Point & p : points) {
        pointBlock << convertPoint(p);
        if (pointBlock.size() == BlockSize) {
            pointQueue_ << pointBlock;
            pointBlock.resize(0);
        }
    }
    if (!pointBlock.isEmpty()) pointQueue_ << pointBlock;

    isActive_ = true;
    readyTimer_.stop();
    isRepeating_ = repeat;
    repeatPos_ = 0;
    finishedCallQueueSize_ = -1;

    if (repeat) {
        if (pointQueue_.size() == 1) {
            // EasyLase does the repetition.
            easyLase_.show(PointsPerSecond, pointQueue_.takeFirst());
            return;
        }
    } else {
        // Placeholder to finish last block before going idle.
        pointQueue_ << EasyLase::Points(1, {});
        if (finishedCallback_) finishedCallQueueSize_ = (points.size() + BlockSize - 1) / BlockSize / 2 + 1;
        logTrace("finished call queue size : %1 / %2", finishedCallQueueSize_, pointQueue_.size());
    }
    checkEasyLaseReady();
}

void Laser::identity()
{
    if (!verifyThreadCall(&Laser::identity)) return;
    logFunctionTrace
    transform_ = Matrix3x3::makeScale(0.95, 0.95);
}

void Laser::move(double dx, double dy)
{
    if (!verifyThreadCall(&Laser::move, dx, dy)) return;
    logFunctionTrace
    transform_ = Matrix3x3::makeTranslation(dx, dy) * transform_;
}

void Laser::scale(double sx, double sy)
{
    if (!verifyThreadCall(&Laser::scale, sx, sy)) return;
    logFunctionTrace
    transform_ = Matrix3x3::makeScale(sx, sy) * transform_;
}

void Laser::rotate(double radiant)
{
    if (!verifyThreadCall(&Laser::rotate, radiant)) return;
    logFunctionTrace
    transform_ = Matrix3x3::makeRotation(radiant) * transform_;
}

void Laser::easyLaseError()
{
    logFunctionTrace
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
        easyLase_.show(PointsPerSecond, pointQueue_[repeatPos_++]);
        if (repeatPos_ == pointQueue_.size()) repeatPos_ = 0;
        readyTimer_.singleShot(0.002);

        // check that next show has enough points
        EasyLase::Points & current = pointQueue_[repeatPos_];
        if (current.size() < BlockSize) {
            int nextId = repeatPos_ + 1;
            if (nextId == pointQueue_.size()) nextId = 0;
            EasyLase::Points & next = pointQueue_[nextId];
            int missing = BlockSize - current.size();
            current.append(next.mid(0, missing));
            next.remove(0, missing);
        }
    } else {
        if (pointQueue_.isEmpty()) {
            logDebug("out of points");
            idle();
        } else {
            easyLase_.show(PointsPerSecond, pointQueue_.takeFirst());
            readyTimer_.singleShot(0.002);
            if (pointQueue_.size() == finishedCallQueueSize_) finishedCallback_();
        }
    }
}

namespace {

inline quint16 convertAxis(double v) { return qMax(0, qMin(4095, qRound((v + 1.0) * 2047.5))); }

}

inline EasyLase::Point Laser::convertPoint(const Point & p)
{
    const Vec2 v = transform_ * Vec2{ p.x, p.y };
    return EasyLase::Point{
        .x = convertAxis(-v.x),
        .y = convertAxis(v.y),
        .r = p.r,
        .g = p.g,
        .b = p.b
    };
}
