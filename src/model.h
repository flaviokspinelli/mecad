#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>
#include <QPointF>
#include <TopoDS_Shape.hxx>
#include "dependencies.h"
#include <vector>

struct Feature {
    QString id, name, type;
    QJsonObject p;
    bool visible = true;
    TopoDS_Shape shape;
    QJsonObject json() const;
};
struct Triangle {
    QVector3D a, b, c;
    int feature;
    int face = -1;
};
class Model {
  public:
    Model();
    std::vector<Feature> features;
    QString filePath;
    bool dirty = false;
    void markUnsaved();
    QString add(QString type, QJsonObject p, QString name = {});
    void edit(const QString &id, QJsonObject p, const QString &name);
    void remove(const QString &id);
    void deleteBody(const QString &id);
    void commit(const QJsonObject &document);
    void editSketchElements(const QString &id, const QVector<int> &edges, const QVector<int> &vertices,
                            QVector3D delta, bool erase);
    void toggle(const QString &id);
    void rebuild();
    DependencyGraph dependencyGraph() const;
    void moveFeature(const QString &id, int destination);
    Feature &get(const QString &id);
    const Feature &get(const QString &id) const;
    bool consumed(const QString &id) const;
    bool isMesh(const QString &id) const;
    std::vector<int> bodies(bool visibleOnly = true) const;
    std::vector<Triangle> triangles() const;
    QJsonObject json() const;
    void loadJson(const QJsonObject &root);
    void save(const QString &path);
    void load(const QString &path);
    void exportStep(const QString &path, const QString &id = {}) const;
    void exportStl(const QString &path, const QString &id = {}) const;
    void exportDxf(const QString &path, const QString &id) const;
    QString importStep(const QString &path);
    QString importStl(const QString &path);
    bool undo();
    bool redo();
    void clear();
    static double volume(const TopoDS_Shape &s);
    static QVector3D planePoint(const QString &plane, double u, double v, double offset = 0);
    static QVector3D planeNormal(const QString &plane);
    static QPointF planeCoordinates(const QString &plane, QVector3D point);
    static QString facePlane(const TopoDS_Shape &shape, int index);

  private:
    QJsonObject savedDocument;
    std::vector<QJsonObject> past, future;
    void checkpoint(const QJsonObject &before);
    void restore(const QJsonObject &root);
    void rebuildGeometry();
    TopoDS_Shape exportShape(const QString &id) const;
};
