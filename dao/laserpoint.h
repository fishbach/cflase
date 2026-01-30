#pragma once

#include <cflib/serialize/serialize.h>

namespace dao {

class LaserPoint
{
    SERIALIZE_CLASS
public serialized:
    double x = 0.0;  // -1.0 ... 1.0
    double y = 0.0;  // -1.0 ... 1.0
    quint8 r = 0;
    quint8 g = 0;
    quint8 b = 0;
};

using LaserPoints = QVector<LaserPoint>;

}
