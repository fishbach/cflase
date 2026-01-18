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

    // no need to check isReady here
    bool on();
    bool off();

    // For range of pps and points see constants above.
    // We have double buffering and every call to show(...) sends a new frame.
    // After two frames isRead() will go false until the first frame is completed.
    // reset() can be called at any time and clears both buffers and stops output.
    // If you call show() when isReady() returns false, funny behaviour can be experienced.
    bool isReady();
    bool reset();
    bool show(quint16 pps, const Points & points);
    bool show(const Point & point) { return show(MaxSpeed, Points(1, point)); }

private:
    QFile device_;
};
