#pragma once

#include <laser/easylase.h>

#include <cflib/util/evtimer.h>
#include <cflib/util/threadverify.h>

class Laser : private cflib::util::ThreadVerify
{
public:
    static constexpr quint16 MaxSpeed  = 59899;
    static constexpr quint16 OptimalPointCount  = EasyLase::MaxPoints;

    struct Point
    {
        double x = 0.0;  // -1.0 ... 1.0
        double y = 0.0;  // -1.0 ... 1.0
        quint8 r = 0;
        quint8 g = 0;
        quint8 b = 0;
    };

    using Points = QVector<Point>;
    using VoidFunc = std::function<void ()>;
    using BoolFunc = std::function<void (bool)>;
    using StringFunc = std::function<void (const QString &)>;

public:
    Laser();
    ~Laser();

    void reset();

    bool hasError() const;
    QString errorString() const;

    // attention: this callback is called from an internal thread
    void setErrorCallback(StringFunc callback);

    // called when Laser changes on/off state
    void setActiveCallback(BoolFunc callback);

    // this is called between 137ms and 274ms before last no-repeat show ends.
    void setFinishedCallback(VoidFunc callback);

    // All commands are executed asynchronously.
    // This call blocks until queue is empty.
    void waitForFinish();

    void on();
    void off();

    // If there was something active with repeat, it is replaced by new points,
    // otherwise new points will be appended.
    void idle();
    void show(const Points & points, bool repeat = false, quint16 pps = MaxSpeed);
    void show(const Point & point) { return show(Points(1, point), true); }

private:
    void easyLaseError();
    void checkEasyLaseReady();

private:
    EasyLase                easyLase_;

    bool                    hasError_ = false;
    QString                 error_;
    StringFunc              errorCallback_;
    BoolFunc                activeCallback_;

    bool                    isActive_ = false;
    cflib::util::EVTimer    readyTimer_;
    QList<EasyLase::Points> pointQueue_;
    bool                    isRepeating_ = false;
    int                     repeatPos_ = 0;
    VoidFunc                finishedCallback_;
};
