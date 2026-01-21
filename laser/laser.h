#pragma once

#include <laser/easylase.h>

#include <cflib/util/threadverify.h>

class Laser : private cflib::util::ThreadVerify
{
public:
    using VoidFunc = std::function<void ()>;

public:
    Laser();
    ~Laser();

    void reset();

    bool hasError() const;
    QString errorString() const;

    // attention: this callback is called from an internal thread
    void setErrorCallback(VoidFunc callback);

    // All commands are executed asynchronously.
    // This call blocks until queue is empty.
    void waitForFinish();

    void on();
    void off();

    void idle();

    void show(quint16 pps, const EasyLase::Points & points);
    void show(const EasyLase::Point & point) { return show(EasyLase::MinSpeed, EasyLase::Points(1, point)); }

    void test();

private:
    void easyLaseError();

private:
    EasyLase easyLase_;
    bool hasError_ = false;
    QString error_;
    VoidFunc errorCallback_;
};
