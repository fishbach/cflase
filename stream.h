#pragma once

#include <laser/laser.h>

#include <cflib/util/threadfifo.h>
#include <cflib/util/threadverify.h>

class Stream : private cflib::util::ThreadVerify
{
public:
    Stream();
    ~Stream();

    Laser::Points getFirst();
    Laser::Points getNext();

private:
    void triggerCalc();
    Laser::Points calcNext();

private:
    cflib::util::ThreadFifo<Laser::Points> fifo_;
};
