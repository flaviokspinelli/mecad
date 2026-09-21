#pragma once
#include <QLineF>
#include <QVector>

namespace snapping {
struct Circle { QPointF center; double radius; };
// Finite segment intersections. Tangencies yield one point; coincident circles
// and degenerate/invalid inputs yield none (no unique snapping target).
QVector<QPointF> intersections(const QLineF &segment, const Circle &circle);
QVector<QPointF> intersections(const Circle &first, const Circle &second);
bool onArc(const Circle &circle,QPointF start,QPointF middle,QPointF end,QPointF point);
}
