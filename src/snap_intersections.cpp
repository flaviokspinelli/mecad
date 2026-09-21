#include "snap_intersections.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace snapping {
namespace {
constexpr double tolerance=1e-8; // millimeters, not pixels or relative radius.
bool finite(QPointF p){return std::isfinite(p.x()) && std::isfinite(p.y());}
bool valid(const Circle &c){return finite(c.center) && std::isfinite(c.radius) && c.radius>0;}
void append(QVector<QPointF> &points,QPointF p){
    if(!finite(p))return;
    for(const auto &existing:points)if(QLineF(existing,p).length()<=tolerance)return;
    points.append(p);
}
}
QVector<QPointF> intersections(const QLineF &segment,const Circle &circle){
    QVector<QPointF> points;
    if(!finite(segment.p1()) || !finite(segment.p2()) || !valid(circle))return points;
    const double length=segment.length();if(!std::isfinite(length) || length<=tolerance)return points;
    const QPointF unit=(segment.p2()-segment.p1())/length, delta=circle.center-segment.p1();
    const long double along=static_cast<long double>(delta.x())*unit.x()+static_cast<long double>(delta.y())*unit.y();
    const long double perpendicular=static_cast<long double>(delta.x())*unit.y()-static_cast<long double>(delta.y())*unit.x();
    if(std::abs(perpendicular)>circle.radius+tolerance)return points;
    const long double radius=circle.radius;
    const double half=std::abs(std::abs(perpendicular)-radius)<=tolerance?0:
        std::sqrt(std::max(0.L,radius*radius-perpendicular*perpendicular));
    for(double offset:{-half,half}){
        const double position=double(along)+offset;
        if(position>=-tolerance && position<=length+tolerance)
            append(points,segment.p1()+unit*std::clamp(position,0.,length));
    }
    return points;
}
QVector<QPointF> intersections(const Circle &first,const Circle &second){
    QVector<QPointF> points;if(!valid(first) || !valid(second))return points;
    const QPointF delta=second.center-first.center;const double distance=std::hypot(delta.x(),delta.y());
    if(!std::isfinite(distance) || distance<=tolerance || distance>first.radius+second.radius+tolerance ||
       distance<std::abs(first.radius-second.radius)-tolerance)return points;
    const long double a=first.radius,b=second.radius,d=distance;
    const double along=double((a*a-b*b+d*d)/(2*d));
    const bool tangent=std::abs(d-(a+b))<=tolerance || std::abs(d-std::abs(a-b))<=tolerance;
    const double height=tangent?0:std::sqrt(std::max(0.L,a*a-static_cast<long double>(along)*along));
    const QPointF unit=delta/distance,base=first.center+unit*along,perpendicular(-unit.y(),unit.x());
    append(points,base+perpendicular*height);append(points,base-perpendicular*height);return points;
}
bool onArc(const Circle &circle,QPointF start,QPointF middle,QPointF end,QPointF point){
    if(!valid(circle) || !finite(start) || !finite(middle) || !finite(end) || !finite(point))return false;
    if(std::abs(QLineF(circle.center,point).length()-circle.radius)>tolerance)return false;
    auto angle=[&](QPointF p){return std::atan2(p.y()-circle.center.y(),p.x()-circle.center.x());};
    auto positive=[](double a){a=std::fmod(a,2*std::numbers::pi);return a<0?a+2*std::numbers::pi:a;};
    const double origin=angle(start),sweep=positive(angle(end)-origin),through=positive(angle(middle)-origin);
    const double slack=tolerance/std::max(circle.radius,tolerance),candidate=positive(angle(point)-origin);
    if(QLineF(point,start).length()<=tolerance || QLineF(point,end).length()<=tolerance)return true;
    return through<=sweep ? candidate<=sweep+slack : candidate>=sweep-slack;
}
}
