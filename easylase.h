#pragma once

#include <QtCore>

class EasyLase
{
public:
    static constexpr quint16 MinSpeed  = 500;
    static constexpr quint16 MaxSpeed  = 65000;
    static constexpr quint16 SpeedStep = 500;
    static constexpr quint16 MaxPoints = 16384;

    struct Point
    {
        quint16 x = 2047;  // Value 0 - 4095 X-Coordinate
        quint16 y = 2047;  // Value 0 - 4095 Y-coordinate
        quint8  r =    0;  // Value 0 -  255 Red
        quint8  g =    0;  // Value 0 -  255 Green
        quint8  b =    0;  // Value 0 -  255 Blue
        quint8  i =    0;  // Value 0 -  255 Intensity
    } __attribute__((packed));

    using Points = QVector<Point>;

public:
    EasyLase();

    bool connect();
    QString error() const;

    bool isReady();
    bool on();
    bool off();
    bool idle();
    bool show(const Point & point) { return show(MaxSpeed, Points(1, point)); }
    bool show(quint16 pps, const Points & points);

private:
    QFile device_;
};
