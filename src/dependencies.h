#pragma once
#include <QJsonArray>
#include <QMap>
#include <QStringList>

// Document-level graph, independent of the geometry kernel and the interface.
class DependencyGraph {
public:
    explicit DependencyGraph(const QJsonArray &features);
    QStringList order() const;
    QStringList dependencies(const QString &id) const;
    QStringList dependents(const QString &id, bool recursive = true) const;
    void requireHistoryOrder() const;
private:
    QStringList history;
    QMap<QString, QStringList> inputs, outputs;
    QStringList sorted;
    void requireId(const QString &id) const;
};
