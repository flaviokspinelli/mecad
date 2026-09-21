#pragma once
#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QStringList>
#include <QVector>

namespace sketch {
struct Point { QString id; QPointF position; };
struct Line { QString id, start, end; bool construction = false; };
enum class Relation { Coincident, Horizontal, Vertical, Fixed, DistanceX, DistanceY };
struct Constraint {
    QString id;
    Relation relation;
    QString first, second;
    QPointF value;
};
struct Solution {
    bool consistent = false;
    int degreesOfFreedom = -1;
    QMap<QString, QPointF> positions;
    // Rank of the solution-space projection onto each point (not additive).
    QMap<QString, int> pointDegreesOfFreedom;
    QStringList conflictCandidates; // Contributors, not a minimal conflict set.
    QStringList redundantConstraints;
    double maximumResidual = 0;
};
// Sketch domain embedded in native .mcad v2 for constrained single-chain profiles.
class System {
public:
    QVector<Point> points;
    QVector<Line> lines;
    QVector<Constraint> constraints;
    void validate() const;
    QJsonObject json() const;
    static System fromJson(const QJsonObject &document);
    Solution solve() const; // Pure preview; neither success nor failure mutates input.
    System solved() const; // Returns a new document, throws on inconsistency.
};
}
