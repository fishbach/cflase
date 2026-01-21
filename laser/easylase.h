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
    using VoidFunc = std::function<void ()>;

public:
    EasyLase();
    ~EasyLase();

    // If any error occurs, device will be disconnected automatically.
    // connect() needs to be called again after an error.
    bool hasError() const { return !error_.isNull(); }
    QString errorString() const { return error_; }

    // Callback is only called, when a call of one of the public members raises an error.
    // This class has no threading.
    void setErrorCallback(VoidFunc callback) { errorCallback_ = callback; }

    void connect();
    void disconnect();

    // no need to check isReady here
    // Attention:
    // Bits to Pin assignment is different to the description in the EasyLase USB manual.
    // bit 7 (MSB) -> Pin 3
    // ...
    // bit 0 (LSB) -> Pin 10
    void setTTL(quint8 hiLow);

    // For range of pps and points see constants above.
    // We have double buffering and every call to show(...) sends a new frame.
    // After two frames isRead() will go false until the first frame is completed.
    // idle() can be called at any time and clears both buffers and stops output.
    // If you call show() when isReady() returns false, funny behaviour can be experienced.
    void idle();
    bool isReady();
    void show(quint16 pps, const Points & points);
    void show(const Point & point) { return show(MinSpeed, Points(1, point)); }

private:
    bool check(bool condition, const QString & msg);

private:
    QFile device_;
    QString error_;
    VoidFunc errorCallback_;
};
