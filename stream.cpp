#include "stream.h"

#include <cflib/util/log.h>

USE_LOG(LogCat::Compute)

Stream::Stream()
:
    ThreadVerify("Stream", Worker),
    fifo_(1)
{
}

Stream::~Stream()
{
    stopVerifyThread();
}

Laser::Points Stream::getFirst()
{
    SyncedThreadCall<Laser::Points> stc(this);
    if (!stc.verify(&Stream::getFirst)) return stc.retval();
    logFunctionTrace

    Laser::Points rv = calcNext();
    rv.append(calcNext());
    fifo_.put(calcNext());
    return rv;
}

Laser::Points Stream::getNext()
{
    logFunctionTrace
    Laser::Points rv = fifo_.take();
    triggerCalc();
    if (rv.isEmpty()) logWarn("buffer underrun");
    return rv;
}

void Stream::triggerCalc()
{
    if (!verifyThreadCall(&Stream::triggerCalc)) return;
    logFunctionTrace
    if (!fifo_.put(calcNext())) logWarn("buffer overflow");
}

Laser::Points Stream::calcNext()
{
    constexpr double Pi = std::numbers::pi_v<double>;

    int pc = Laser::OptimalPointCount;
    Laser::Points points;
    for (int i = 0 ; i < pc ; ++i) {
        points << Laser::Point{
            .x = std::cos(2 * Pi * i / pc) / 4,
            .y = std::sin(2 * Pi * i / pc) / 4,
            .g = 45
        };
    }
    return points;
}
