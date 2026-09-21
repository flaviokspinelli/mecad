#pragma once
#include "model.h"
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>
#include <functional>

class Viewport : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
  public:
    explicit Viewport(Model *model, QWidget *parent = nullptr);
    ~Viewport();
    QString selected, plane = "XY", tool;
    bool sketchMode = false, snap = true, light = false;
    double planeOffset = 0;
    std::function<void(QString)> onSelect;
    std::function<void(QJsonObject)> onProfile;
    std::function<void(QString)> onHint;
    void refresh();
    void fit();
    void view(QString name);
    void setTool(QString name);
    QPointF project(QVector3D p) const;
    QPointF planeAt(QPointF pixel) const;
    void finishPolyline(bool close);
    void paintOverlay(QPainter &p);

  protected:
    void initializeGL() override;
    void resizeEvent(QResizeEvent *) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    bool event(QEvent *) override;

  private:
    Model *model;
    QWidget *overlay;
    std::vector<Triangle> mesh;
    std::vector<QVector3D> vertices;
    QOpenGLShaderProgram shader;
    QOpenGLBuffer buffer;
    QOpenGLVertexArrayObject vao;
    bool upload = true, ready = false;
    QVector3D center{20, 15, 0};
    float yaw = 45, pitch = 35, span = 150;
    QPointF last, pressed, cursor;
    QVector<QPointF> draft;
    QMatrix4x4 matrix() const;
    void ray(QPointF p, QVector3D &origin, QVector3D &direction) const;
    QString pick(QPointF p) const;
    void submit(QJsonObject p);
};
