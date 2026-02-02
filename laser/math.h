#pragma once

#include <array>
#include <cmath>

namespace math {

constexpr double Pi = std::numbers::pi_v<double>;

class Vec2
{
public:
    double x = 0.0;
    double y = 0.0;

    constexpr double  operator[](int i) const { return (&x)[i]; }
    constexpr double& operator[](int i)       { return (&x)[i]; }
};

class Vec3
{
public:
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr double  operator[](int i) const    { return (&x)[i]; }
    constexpr double& operator[](int i)          { return (&x)[i]; }
    constexpr double dot(const Vec2 & rhs) const { return x * rhs.x + y * rhs.y + z;         }
    constexpr double dot(const Vec3 & rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }
};

class Matrix3x3
{
public:
    constexpr Matrix3x3() = default;
    constexpr Matrix3x3(const Vec3 & r0, const Vec3 & r1, const Vec3 & r2) : rows_{r0, r1, r2} {}

    constexpr const Vec3& operator[](int r) const { return rows_[r]; }
    constexpr       Vec3& operator[](int r)       { return rows_[r]; }

    constexpr Matrix3x3 transposed() const
    {
        return {
            { rows_[0][0], rows_[1][0], rows_[2][0] },
            { rows_[0][1], rows_[1][1], rows_[2][1] },
            { rows_[0][2], rows_[1][2], rows_[2][2] }
        };
    }

    constexpr Vec2 operator*(const Vec2 & v) const
    {
        return {
            rows_[0].dot(v),
            rows_[1].dot(v)
        };
    }

    constexpr Vec3 operator*(const Vec3 & v) const
    {
        return {
            rows_[0].dot(v),
            rows_[1].dot(v),
            rows_[2].dot(v)
        };
    }

    constexpr Matrix3x3 operator*(const Matrix3x3 & rhs) const
    {
        const Matrix3x3 rhsT = rhs.transposed();
        return {
            { rows_[0].dot(rhsT[0]), rows_[0].dot(rhsT[1]), rows_[0].dot(rhsT[2]) },
            { rows_[1].dot(rhsT[0]), rows_[1].dot(rhsT[1]), rows_[1].dot(rhsT[2]) },
            { rows_[2].dot(rhsT[0]), rows_[2].dot(rhsT[1]), rows_[2].dot(rhsT[2]) }
        };
    }

    static constexpr Matrix3x3 identity()
    {
        return Matrix3x3{
            { 1.0, 0.0, 0.0 },
            { 0.0, 1.0, 0.0 },
            { 0.0, 0.0, 1.0 }
        };
    }

    static constexpr Matrix3x3 makeTranslation(double dx, double dy)
    {
        return {
            { 1.0, 0.0,  dx },
            { 0.0, 1.0,  dy },
            { 0.0, 0.0, 1.0 }
        };
    }

    static constexpr Matrix3x3 makeScale(double sx, double sy)
    {
        return {
            {  sx, 0.0, 0.0 },
            { 0.0,  sy, 0.0 },
            { 0.0, 0.0, 1.0 }
        };
    }

    static constexpr Matrix3x3 makeRotation(double radiant)
    {
        const double s = std::sin(radiant);
        const double c = std::cos(radiant);
        return {
            {   c,  -s, 0.0 },
            {   s,   c, 0.0 },
            { 0.0, 0.0, 1.0 }
        };
    }

private:
    std::array<Vec3, 3> rows_;
};

}
