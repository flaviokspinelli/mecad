#pragma once
#include <QMap>
#include <QString>

namespace parameters {
// Canonical units: mm and radians. Dimensions remain explicit during arithmetic.
struct Quantity {
    double value = 0;
    int length = 0;
    int angle = 0;
};
// Pure evaluation: no scripts, IO, process execution or mutable global symbols.
Quantity evaluate(const QString &expression, const QMap<QString, Quantity> &symbols = {});
QMap<QString, Quantity> resolve(const QMap<QString, QString> &definitions);
}
