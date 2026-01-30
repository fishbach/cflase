#pragma once

#include <laser/laser.h>
#include <cflib/net/rmiservice.h>

namespace services {

class LaserService :
    public cflib::net::RMIService<int>
{
    SERIALIZE_CLASS
public:
    LaserService();
    ~LaserService();

rmi:
    bool on();
    bool off();

    bool idle();
    bool show(const dao::LaserPoints & points, bool repeat, quint16 pps);

cfsignals:
    rsig<void (const QString & error), void ()> error;
    rsig<void (bool active), void ()> active;
    rsig<void (), void ()> finished;

private:
    Laser laser_;
};

}
