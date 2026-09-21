#pragma once
#include "model.h"
#include <QLineEdit>
#include <QLineF>
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>
#include <QPointer>
#include <QVariantAnimation>
#include <functional>

class Viewport : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
  public:
    explicit Viewport(Model *model, QWidget *parent = nullptr);
    ~Viewport();
    QString selected, plane = "XY", tool;
    struct SelectionTarget {
        QString feature, kind;
        int index = -1;
        QVector<QVector3D> geometry;
    };
    SelectionTarget selectedDetail, hoveredDetail;
    SelectionTarget sketchSupport;
    QVector<SelectionTarget> selectedDetails;
    QString selectionFilter = "auto";
    bool hasSubselection() const {
        for (const auto &target : selectedDetails)
            if (target.kind == "edge" || target.kind == "vertex" || target.kind == "face")
                return true;
        return selectedDetail.kind == "edge" || selectedDetail.kind == "vertex" || selectedDetail.kind == "face";
    }
    bool objectSelected(const QString &id) const {
        for (const auto &target : selectedDetails)
            if (target.feature == id && target.kind == "object")
                return true;
        return selectedDetails.empty() && selected == id && !hasSubselection();
    }
    bool sketchMode = false, snap = true, light = false;
    bool showEdges = true;
    bool smartSnap = true;
    QString navigationMode;
    bool choosingPlane = false, handleActive = false;
    QVector3D handleOrigin, handleAxis;
    double handleDistance = 10;
    bool moveHandleActive = false;
    bool rotationMode = false;
    double rotationAngle = 0;
    QString rotationAxis = "Z";
    std::function<void(double)> onRotateAngle;
    int moveAxis = 0;
    double moveHandleLength = 25;
    QVector3D moveDistances;
    std::function<void(int, double)> onMoveDistance;
    std::function<void(QVector3D)> onMoveTranslation;
    QVector3D moveHandleTip(int axis) const;
    std::function<void(QString, double)> onPlaneChosen;
    std::function<void(double)> onHandleDistance;
    std::function<void()> onCancelCommand;
    std::function<void()> onAcceptCommand;
    bool commandSelectSubelements = false;
    void setModel(Model *m);
    double planeOffset = 0;
    int polygonSides = 6;
    std::function<void(QString)> onSelect;
    std::function<void(QString)> onEditSketch;
    std::function<QString(QString, QString, double)> onDimensionEdit;
    std::function<void(QJsonObject)> onProfile;
    std::function<void(QString)> onHint;
    void refresh();
    void fit();
    void view(QString name, bool animated = false);
    void viewDirection(QVector3D direction, bool animated = true);
    QVector3D cameraDirection() const;
    QVector3D cubeDirectionAt(QPointF position) const;
    bool isViewAnimating() const;
    void setTool(QString name);
    QPointF project(QVector3D p) const;
    QPointF planeAt(QPointF pixel, bool grid = true) const;
    void finishPolyline(bool close);
    QPolygonF regularPolygon(QPointF center, QPointF vertex) const;
    void paintOverlay(QPainter &p);
    void zoomBy(float factor);

  protected:
    void initializeGL() override;
    void resizeEvent(QResizeEvent *) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    bool event(QEvent *) override;

  private:
    friend class UiTests;
    Model *model;
    void paintGrid();
    SelectionTarget pickDetail(QPointF pixel, bool objectOnly = false) const;
    QVector<SelectionTarget> pickArea(QRectF area, bool crossing) const;
    bool areaCandidate = false, areaDragging = false;
    QPointF areaEnd;
    QWidget *overlay;
    std::vector<Triangle> mesh;
    std::vector<QVector3D> vertices;
    QOpenGLShaderProgram shader;
    QOpenGLBuffer buffer;
    QOpenGLBuffer gridBuffer;
    QOpenGLVertexArrayObject vao;
    bool upload = true, ready = false;
    QVector3D center{20, 15, 0};
    float yaw = 45, pitch = 35, span = 150;
    QPointF last, pressed, cursor;
    QVector<QPointF> draft;
    QVector<QPair<QString, QPolygonF>> cubeFaces;
    struct CubeTarget {
        QVector3D direction;
        QPolygonF region;
    };
    QVector<CubeTarget> cubeTargets;
    QVariantAnimation cameraAnimation;
    QVector<QPair<QString, QPolygonF>> planeRegions;
    QPointF planeHover{-1, -1};
    bool draggingHandle = false;
    bool draggingRotation = false;
    QPointF rotationCenter;
    double rotationMouseAngle = 0;
    bool draggingMoveFree = false;
    QVector3D moveStartDistances;
    QMatrix4x4 moveDragInverse;
    bool cubePressed = false, cubeDragging = false;
    float cubeStartYaw = 0, cubeStartPitch = 0;
    bool sketchPressCandidate = false, sketchDragging = false;
    QPointF sketchPressPoint, magnetPoint;
    QString magnetLabel;
    struct DimensionTarget {
        QString key;
        QRectF rect;
        double multiplier;
    };
    QVector<DimensionTarget> dimensions;
    DimensionTarget pendingDimension;
    bool dimensionPressed = false;
    QPointer<QLineEdit> dimensionEditor;
    QString dimensionFeature, dimensionKey;
    double dimensionMultiplier = 1;
    void editDimension(const DimensionTarget &target);
    void closeDimensionEditor();
    QVector<QLineF> magnetGuides;
    QPointF sketchPoint(QPointF pixel);
    QMatrix4x4 matrix() const;
    void ray(QPointF p, QVector3D &origin, QVector3D &direction) const;
    QString pick(QPointF p) const;
    void submit(QJsonObject p);
};
