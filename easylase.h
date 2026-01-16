#pragma once

#include <QtCore>

class Easylase
{
public:
    const quint32 MinSpeed  = 500;
    const quint32 MaxSpeed  = 65000;
    const quint32 SpeedStep = 500;

public:
    Easylase();

    bool connect();
    QString error() const;

    bool on();
    bool off();
    bool idle();
    bool beam();

private:
    QFile device_;
};
