#pragma once

#include <laser/easylase.h>

#include <cflib/util/threadverify.h>

class Laser : private cflib::util::ThreadVerify
{
public:
    Laser();
    ~Laser();

    bool hasError() const;
    QString errorString() const;
    void reset();

    void on();
    void off();

    void idle();

private:
    void easyLaseError();

private:
    EasyLase easyLase_;
    QString error_;
};
