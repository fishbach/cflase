#pragma once

#include <cflib/net/rmiservice.h>

namespace services {

class MainService :
    public cflib::net::RMIService<int>
{
    SERIALIZE_CLASS
public:
    MainService();
    ~MainService();
rmi:
    int test() { return 42; }

cfsignals:
    rsig<void (const QList<int> & events), void()> newEvents;
};

}
