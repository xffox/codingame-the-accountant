#ifndef GEOM_H
#define GEOM_H

#include <cmath>
#include <ostream>

namespace geom
{
    struct Point
    {
        int x;
        int y;
    };
    inline bool operator==(const Point &left, const Point &right)
    {
        return left.x == right.x && left.y == right.y;
    }
    inline bool operator!=(const Point &left, const Point &right)
    {
        return !(left == right);
    }
    inline bool operator<(const Point &left, const Point &right)
    {
        return
            (left.x < right.x ||
             (left.x == right.x &&
              left.y < right.y));
    }

    struct Vect
    {
        double x;
        double y;

        Vect &operator+=(const Vect &that)
        {
            x += that.x;
            y += that.y;
            return *this;
        }
    };
    inline Vect operator+(const Vect &left, const Vect &right)
    {
        Vect res(left);
        // TODO: will it be moved?
        return res += right;
    }

    inline double dist(const Point &left, const Point &right)
    {
        const double dx = left.x - right.x;
        const double dy = left.y - right.y;
        return sqrt(dx*dx + dy*dy);
    }

    inline Vect norm(const Vect &v)
    {
        const auto d = sqrt(v.x*v.x + v.y*v.y);
        if(d != 0.0)
        {
            return Vect{v.x/d, v.y/d};
        }
        return v;
    }

    constexpr Vect orthogonal(const Vect &v)
    {
        return Vect{v.y, -v.x};
    }

    inline Vect normDirection(const Point &begin, const Point &end)
    {
        const double dx = end.x - begin.x;
        const double dy = end.y - begin.y;
        return norm(Vect{dx, dy});
    }

    constexpr Vect mult(const Vect &v, double m)
    {
        return Vect{v.x*m, v.y*m};
    }

    constexpr Point add(const Point &p, const Vect &v)
    {
        return Point{
            static_cast<int>(p.x + v.x),
                static_cast<int>(p.y + v.y)
        };
    }

    inline std::ostream &operator<<(std::ostream &stream, const geom::Point &p)
    {
        return stream<<'('<<p.x<<','<<p.y<<')';
    }
}

#endif
