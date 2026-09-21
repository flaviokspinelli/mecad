#include "viewport.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <QtMath>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <limits>

class CanvasOverlay : public QWidget {
  public:
    Viewport *canvas;
    explicit CanvasOverlay(Viewport *parent) : QWidget(parent), canvas(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        canvas->paintOverlay(painter);
    }
};

Viewport::Viewport(Model *m, QWidget *parent)
    : QOpenGLWidget(parent), model(m), buffer(QOpenGLBuffer::VertexBuffer) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(420, 360);
    setObjectName("viewport");
    overlay = new CanvasOverlay(this);
    overlay->show();
}
void Viewport::resizeEvent(QResizeEvent *event) {
    QOpenGLWidget::resizeEvent(event);
    overlay->setGeometry(rect());
}
Viewport::~Viewport() {
    makeCurrent();
    buffer.destroy();
    vao.destroy();
    doneCurrent();
}
QMatrix4x4 Viewport::matrix() const {
    float a = yaw * M_PI / 180, b = pitch * M_PI / 180;
    QVector3D direction(std::cos(b) * std::cos(a), std::cos(b) * std::sin(a), std::sin(b));
    QMatrix4x4 view;
    view.lookAt(center + direction * span * 4, center,
                QVector3D(-std::sin(b) * std::cos(a), -std::sin(b) * std::sin(a), std::cos(b)));
    float aspect = float(width()) / std::max(1, height());
    QMatrix4x4 projection;
    projection.ortho(-span * aspect / 2, span * aspect / 2, -span / 2, span / 2, span * .01f, span * 20);
    return projection * view;
}
QPointF Viewport::project(QVector3D p) const {
    auto v = matrix() * QVector4D(p, 1);
    return {(v.x() / v.w() + 1) * width() / 2, (1 - v.y() / v.w()) * height() / 2};
}
void Viewport::ray(QPointF p, QVector3D &o, QVector3D &d) const {
    auto inv = matrix().inverted();
    QVector4D a = inv * QVector4D(2 * p.x() / width() - 1, 1 - 2 * p.y() / height(), -1, 1),
              b = inv * QVector4D(2 * p.x() / width() - 1, 1 - 2 * p.y() / height(), 1, 1);
    o = a.toVector3DAffine();
    d = (b.toVector3DAffine() - o).normalized();
}
QPointF Viewport::planeAt(QPointF pixel) const {
    QVector3D o, d;
    ray(pixel, o, d);
    auto n = Model::planeNormal(plane);
    float denominator = QVector3D::dotProduct(d, n);
    if (std::abs(denominator) < 1e-6)
        return {};
    auto hit = o + d * ((planeOffset - QVector3D::dotProduct(o, n)) / denominator);
    double u = plane == "YZ" ? hit.y() : hit.x(), v = plane == "XY" ? hit.y() : hit.z();
    if (snap) {
        u = std::round(u);
        v = std::round(v);
    }
    return {u, v};
}
void Viewport::initializeGL() {
    initializeOpenGLFunctions();
    ready =
        shader.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                       "#version 150\nin vec3 position;in vec3 normal;uniform mat4 mvp;out "
                                       "vec3 N;void main(){N=normal;gl_Position=mvp*vec4(position,1.0);}") &&
        shader.addShaderFromSourceCode(
            QOpenGLShader::Fragment,
            "#version 150\nin vec3 N;uniform vec3 color;out vec4 frag;void main(){vec3 n=normalize(N);float "
            "light=0.42+0.38*abs(dot(n,normalize(vec3(0.3,-0.5,1))))+0.18*abs(dot(n,normalize(vec3(-1,0.2,0."
            "4))));frag=vec4(color*light,1);}") &&
        shader.link();
    buffer.create();
    vao.create();
    if (!ready && onHint)
        onHint("Falha ao iniciar a visualização OpenGL: " + shader.log());
}
void Viewport::refresh() {
    mesh = model->triangles();
    vertices.clear();
    for (auto &t : mesh) {
        auto n = QVector3D::crossProduct(t.b - t.a, t.c - t.a).normalized();
        for (auto p : {t.a, t.b, t.c}) {
            vertices.push_back(p);
            vertices.push_back(n);
        }
    }
    for (auto i : model->bodies())
        for (TopExp_Explorer it(model->features[i].shape, TopAbs_EDGE); it.More(); it.Next()) {
            BRepAdaptor_Curve c(TopoDS::Edge(it.Current()));
            int steps = c.GetType() == GeomAbs_Line ? 1 : 64;
            QVector3D previous;
            for (int j = 0; j <= steps; ++j) {
                auto q = c.Value(c.FirstParameter() + (c.LastParameter() - c.FirstParameter()) * j / steps);
                QVector3D point(q.X(), q.Y(), q.Z());
                if (j)
                    for (auto v : {previous, point}) {
                        vertices.push_back(v);
                        vertices.push_back({0, 0, 1});
                    }
                previous = point;
            }
        }
    upload = true;
    update();
}
void Viewport::fit() {
    Bnd_Box box;
    for (int i : model->bodies())
        BRepBndLib::Add(model->features[i].shape, box);
    if (box.IsVoid())
        for (auto &f : model->features)
            if (f.type == "sketch" && f.visible)
                BRepBndLib::Add(f.shape, box);
    if (box.IsVoid()) {
        center = {20, 15, 0};
        span = 130;
    } else {
        double x, y, z, X, Y, Z;
        box.Get(x, y, z, X, Y, Z);
        center = QVector3D((x + X) / 2, (y + Y) / 2, (z + Z) / 2);
        span = std::max(20., std::sqrt((X - x) * (X - x) + (Y - y) * (Y - y) + (Z - z) * (Z - z)) * 1.1);
    }
    update();
}
QVector3D Viewport::cameraDirection() const {
    float a = qDegreesToRadians(yaw), b = qDegreesToRadians(pitch);
    return {std::cos(b) * std::cos(a), std::cos(b) * std::sin(a), std::sin(b)};
}
bool Viewport::isViewAnimating() const {
    return cameraAnimation.state() == QAbstractAnimation::Running;
}
QVector3D Viewport::cubeDirectionAt(QPointF position) const {
    for (auto it = cubeTargets.crbegin(); it != cubeTargets.crend(); ++it)
        if (it->region.containsPoint(position, Qt::OddEvenFill))
            return it->direction;
    return {};
}
void Viewport::view(QString name, bool animated) {
    if (sketchMode)
        name = plane == "XY" ? "top" : plane == "XZ" ? "front" : "right";
    QVector3D direction;
    if (name == "top")
        direction = {0, 0, 1};
    else if (name == "bottom")
        direction = {0, 0, -1};
    else if (name == "front")
        direction = {0, -1, 0};
    else if (name == "back")
        direction = {0, 1, 0};
    else if (name == "right")
        direction = {1, 0, 0};
    else if (name == "left")
        direction = {-1, 0, 0};
    else
        direction = {std::cos(qDegreesToRadians(32.f)) * std::cos(qDegreesToRadians(-55.f)),
                     std::cos(qDegreesToRadians(32.f)) * std::sin(qDegreesToRadians(-55.f)),
                     std::sin(qDegreesToRadians(32.f))};
    viewDirection(direction, animated);
}
void Viewport::viewDirection(QVector3D direction, bool animated) {
    cameraAnimation.stop();
    if (sketchMode)
        direction = Model::planeNormal(plane);
    if (direction.isNull())
        return;
    direction.normalize();
    float targetPitch = qRadiansToDegrees(std::asin(std::clamp(direction.z(), -1.f, 1.f)));
    float targetYaw = std::abs(direction.z()) > .9999
                          ? (direction.z() > 0 ? -90.f : 90.f)
                          : qRadiansToDegrees(std::atan2(direction.y(), direction.x()));
    float startYaw = yaw, startPitch = pitch;
    targetYaw = startYaw + std::remainder(targetYaw - startYaw, 360.f);
    cameraAnimation.disconnect(this);
    if (!animated) {
        yaw = targetYaw;
        pitch = targetPitch;
        update();
        return;
    }
    cameraAnimation.setDuration(300);
    cameraAnimation.setEasingCurve(QEasingCurve::InOutCubic);
    cameraAnimation.setStartValue(0.);
    cameraAnimation.setEndValue(1.);
    connect(&cameraAnimation, &QVariantAnimation::valueChanged, this, [=, this](const QVariant &value) {
        float t = value.toFloat();
        yaw = startYaw + (targetYaw - startYaw) * t;
        pitch = startPitch + (targetPitch - startPitch) * t;
        update();
    });
    cameraAnimation.start();
}
void Viewport::setTool(QString name) {
    tool = name;
    navigationMode.clear();
    draft.clear();
    setCursor(tool.isEmpty() ? Qt::ArrowCursor : Qt::CrossCursor);
    setFocus();
    update();
}
void Viewport::paintGL() {
    QColor bg = light ? QColor("#e8edf2") : QColor("#465465");
    glClearColor(bg.redF(), bg.greenF(), bg.blueF(), 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    int pw = width() * devicePixelRatioF(), ph = height() * devicePixelRatioF();
    for (int i = 0; i < 48; ++i) {
        float t = float(i) / 47;
        QColor bottom = light ? QColor("#e8edf2") : QColor("#485666"),
               top = light ? QColor("#d8e0e7") : QColor("#344150");
        glScissor(0, i * ph / 48, pw, (i + 1) * ph / 48 - i * ph / 48);
        glClearColor(bottom.redF() * (1 - t) + top.redF() * t, bottom.greenF() * (1 - t) + top.greenF() * t,
                     bottom.blueF() * (1 - t) + top.blueF() * t, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);

    if (ready && !vertices.empty()) {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1, 1);
        shader.bind();
        vao.bind();
        buffer.bind();
        if (upload) {
            buffer.allocate(vertices.data(), int(vertices.size() * sizeof(QVector3D)));
            upload = false;
        }
        shader.enableAttributeArray("position");
        shader.setAttributeBuffer("position", GL_FLOAT, 0, 3, 2 * sizeof(QVector3D));
        shader.enableAttributeArray("normal");
        shader.setAttributeBuffer("normal", GL_FLOAT, sizeof(QVector3D), 3, 2 * sizeof(QVector3D));
        shader.setUniformValue("mvp", matrix());
        int start = 0;
        while (start < int(mesh.size())) {
            int end = start + 1;
            while (end < int(mesh.size()) && mesh[end].feature == mesh[start].feature)
                ++end;
            bool chosen = model->features[mesh[start].feature].id == selected;
            shader.setUniformValue("color", chosen ? QVector3D(.42, .77, .94) : QVector3D(.83, .86, .89));
            glDrawArrays(GL_TRIANGLES, start * 3, (end - start) * 3);
            start = end;
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
        if (showEdges && vertices.size() / 2 > mesh.size() * 3) {
            shader.setUniformValue("color", QVector3D(.16, .21, .26));
            glDrawArrays(GL_LINES, int(mesh.size() * 3), int(vertices.size() / 2 - mesh.size() * 3));
        }
        buffer.release();
        vao.release();
        shader.release();
        glDisable(GL_DEPTH_TEST);
    }
    overlay->update();
}
void Viewport::paintOverlay(QPainter &p) {
    p.setRenderHint(QPainter::Antialiasing);
    // Ground grid is an orientation aid, kept subtle when solids are present.
    if ((mesh.empty() || sketchMode) && !choosingPlane) {
        double step = span > 500 ? 100 : (span > 150 ? 10 : (span > 40 ? 5 : 1));
        QPointF local = planeAt({width() / 2., height() / 2.});
        double half = span * 1.5;
        p.setPen(QPen(light ? QColor(150, 165, 182, 85) : QColor(113, 140, 169, 45), 1));
        for (double i = std::floor((local.x() - half) / step) * step; i < local.x() + half; i += step)
            p.drawLine(project(Model::planePoint(plane, i, local.y() - half, planeOffset)),
                       project(Model::planePoint(plane, i, local.y() + half, planeOffset)));
        for (double i = std::floor((local.y() - half) / step) * step; i < local.y() + half; i += step)
            p.drawLine(project(Model::planePoint(plane, local.x() - half, i, planeOffset)),
                       project(Model::planePoint(plane, local.x() + half, i, planeOffset)));
    }
    if ((mesh.empty() || sketchMode) && !choosingPlane) {
        p.setPen(QPen(QColor("#b76370"), 1));
        p.drawLine(project(Model::planePoint(plane, -50000, 0, planeOffset)),
                   project(Model::planePoint(plane, 50000, 0, planeOffset)));
        p.setPen(QPen(QColor("#5eaa8c"), 1));
        p.drawLine(project(Model::planePoint(plane, 0, -50000, planeOffset)),
                   project(Model::planePoint(plane, 0, 50000, planeOffset)));
    }
    for (auto &f : model->features)
        if (f.type == "sketch" && (f.id == selected || (f.visible && !model->consumed(f.id)))) {
            p.setPen(
                QPen(f.id == selected ? QColor("#65ceff") : QColor("#e8b66d"), f.id == selected ? 2.5 : 1.7));
            for (TopExp_Explorer it(f.shape, TopAbs_EDGE); it.More(); it.Next()) {
                BRepAdaptor_Curve c(TopoDS::Edge(it.Current()));
                QPainterPath path;
                int steps = c.GetType() == GeomAbs_Line ? 1 : 96;
                for (int i = 0; i <= steps; ++i) {
                    auto q =
                        c.Value(c.FirstParameter() + (c.LastParameter() - c.FirstParameter()) * i / steps);
                    auto s = project(QVector3D(q.X(), q.Y(), q.Z()));
                    if (!i)
                        path.moveTo(s);
                    else
                        path.lineTo(s);
                }
                p.drawPath(path);
            }
        }
    if (sketchMode && !selected.isEmpty()) {
        const auto &f = model->get(selected);
        const auto &params = f.p;
        if (f.type == "sketch") {
            auto point = [&](double u, double v) {
                return project(
                    Model::planePoint(params["plane"].toString("XY"), u, v, params["offset"].toDouble()));
            };
            auto dimension = [&](QPointF a, QPointF b, QPointF offset, QString label) {
                p.setPen(QPen(QColor("#e6bd7e"), 1));
                p.drawLine(a, a + offset);
                p.drawLine(b, b + offset);
                p.drawLine(a + offset, b + offset);
                for (auto end : {a + offset, b + offset})
                    p.drawLine(end + QPointF(-3, -3), end + QPointF(3, 3));
                auto mid = (a + b) / 2 + offset;
                QRectF text(mid.x() - 45, mid.y() - 12, 90, 24);
                p.fillRect(text, light ? QColor("#e8edf2") : QColor("#202936"));
                p.drawText(text, Qt::AlignCenter, label);
            };
            double x = params["x"].toDouble(), y = params["y"].toDouble();
            QString kind = params["profile"].toString();
            if (kind == "rectangle") {
                double w = params["w"].toDouble(), h = params["h"].toDouble();
                dimension(point(x, y), point(x + w, y), {0, 26}, QString::number(w, 'f', 2));
                dimension(point(x + w, y), point(x + w, y + h), {40, 0}, QString::number(h, 'f', 2));
            } else if (kind == "circle") {
                double r = params["r"].toDouble();
                dimension(point(x - r, y), point(x + r, y), {0, 0}, "Ø " + QString::number(r * 2, 'f', 2));
            }
        }
    }
    auto pixel = [&](QPointF v) { return project(Model::planePoint(plane, v.x(), v.y(), planeOffset)); };
    if (!draft.empty()) {
        p.setPen(QPen(QColor("#62dbbf"), 2, Qt::DashLine));
        auto a = pixel(draft.front()), b = pixel(cursor);
        if (tool == "rectangle")
            p.drawRect(QRectF(a, b).normalized());
        else if (tool == "circle") {
            double r = QLineF(a, b).length();
            p.drawEllipse(a, r, r);
        } else {
            for (int i = 1; i < draft.size(); ++i)
                p.drawLine(pixel(draft[i - 1]), pixel(draft[i]));
            p.drawLine(pixel(draft.last()), b);
        }
        p.setBrush(QColor("#62dbbf"));
        for (auto point : draft)
            p.drawEllipse(pixel(point), 3, 3);
        p.setPen(QColor("#c7eee5"));
        p.drawText(b + QPointF(14, -14),
                   QString("%1, %2 mm").arg(cursor.x(), 0, 'f', 1).arg(cursor.y(), 0, 'f', 1));
    }
    p.setPen(light ? QColor("#475569") : QColor("#a4b3c5"));
    p.setFont(QFont("Helvetica Neue", 11));
    if (sketchMode)
        p.drawText(290, 24, "SKETCH  /  " + plane + "  /  mm");
    if (mesh.empty() && model->features.empty() && !sketchMode && !choosingPlane) {
        p.setPen(light ? QColor("#576a7e") : QColor("#90a2b7"));
        p.setFont(QFont("Helvetica Neue", 12));
        p.drawText(QRect(290, 18, width() - 420, 28), Qt::AlignCenter, "Create Sketch para começar");
    }
    cubeFaces.clear();
    cubeTargets.clear();
    // The cube follows the camera. Each visible face is a real view target.
    float ca = yaw * M_PI / 180, cb = pitch * M_PI / 180;
    QVector3D eye(std::cos(cb) * std::cos(ca), std::cos(cb) * std::sin(ca), std::sin(cb));
    QMatrix4x4 cubeView;
    cubeView.lookAt(eye * 4, QVector3D(),
                    QVector3D(-std::sin(cb) * std::cos(ca), -std::sin(cb) * std::sin(ca), std::cos(cb)));
    QPointF anchor(width() - 60, 58);
    auto cubePoint = [&](QVector3D v) {
        auto q = cubeView.map(v);
        return anchor + QPointF(q.x() * 27, -q.y() * 27);
    };
    struct Face {
        QString name;
        QVector3D n;
        QVector<QVector3D> corners;
        QColor color;
    };
    QVector<Face> faces = {
        {"top", {0, 0, 1}, {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}, QColor("#a7b4c2")},
        {"bottom", {0, 0, -1}, {{-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, -1, -1}}, QColor("#8b9baa")},
        {"front", {0, -1, 0}, {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}}, QColor("#91a1b2")},
        {"back", {0, 1, 0}, {{1, 1, -1}, {-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}}, QColor("#91a1b2")},
        {"right", {1, 0, 0}, {{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}}, QColor("#b6c0cb")},
        {"left", {-1, 0, 0}, {{-1, 1, -1}, {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}}, QColor("#9cabbc")}};
    p.setFont(QFont("Helvetica Neue", 8));
    for (auto &face : faces)
        if (QVector3D::dotProduct(eye, face.n) > .01) {
            QPolygonF polygon;
            for (auto v : face.corners)
                polygon << cubePoint(v);
            cubeFaces.append({face.name, polygon});
            cubeTargets.append({face.n, polygon});
            p.setPen(QPen(QColor("#687c90"), 1));
            p.setBrush(face.color);
            p.drawPolygon(polygon);
            auto mid = polygon.boundingRect().center();
            p.setPen(QColor("#405265"));
            p.drawText(QRectF(mid.x() - 25, mid.y() - 8, 50, 16), Qt::AlignCenter, face.name.toUpper());
        }
    // Faces are lowest priority, edges next, corners last (larger click targets).
    QVector<QVector3D> corners;
    for (const auto &face : faces)
        if (QVector3D::dotProduct(eye, face.n) > .01) {
            for (int i = 0; i < 4; ++i) {
                auto a = face.corners[i], b = face.corners[(i + 1) % 4];
                if (!corners.contains(a))
                    corners.append(a);
                auto start = cubePoint(a), end = cubePoint(b), d = end - start;
                double length = std::hypot(d.x(), d.y());
                if (length < 1)
                    continue;
                QPointF n(-d.y() / length * 5, d.x() / length * 5);
                cubeTargets.append({(a + b).normalized(), QPolygonF{start + n, end + n, end - n, start - n}});
            }
        }
    for (auto corner : corners) {
        auto point = cubePoint(corner);
        cubeTargets.append({corner.normalized(), QPolygonF(QRectF(point - QPointF(7, 7), QSizeF(14, 14)))});
    }
    for (auto it = cubeTargets.crbegin(); it != cubeTargets.crend(); ++it)
        if (it->region.containsPoint(planeHover, Qt::OddEvenFill)) {
            p.setPen(QPen(QColor("#c3eaff"), 1));
            p.setBrush(QColor(70, 166, 224, 130));
            p.drawPolygon(it->region);
            break;
        }
    p.setPen(QColor("#96abbe"));
    p.drawText(QRect(width() - 84, 102, 50, 16), Qt::AlignCenter, "HOME");
    p.setPen(QPen(QColor("#5bc087"), 1.5));
    p.drawLine(cubePoint({-1, -1, -1}), cubePoint({-1, 1.6, -1}));
    p.setPen(QPen(QColor("#61a7e3"), 1.5));
    p.drawLine(cubePoint({-1, -1, -1}), cubePoint({-1, -1, 1.6}));
    if (choosingPlane) {
        QString hovered;
        for (const auto &region : planeRegions)
            if (region.second.containsPoint(planeHover, Qt::OddEvenFill)) {
                hovered = region.first;
                break;
            }
        planeRegions.clear();
        QRectF bounds;
        int i = 0;
        for (auto planeName : {"XY", "XZ", "YZ"}) {
            QPolygonF polygon;
            for (auto q : {QPointF(-15, -15), QPointF(25, -15), QPointF(25, 25), QPointF(-15, 25)})
                polygon << project(Model::planePoint(planeName, q.x(), q.y()));
            QColor c = i == 0   ? QColor(102, 172, 214, 30)
                       : i == 1 ? QColor(211, 172, 105, 30)
                                : QColor(98, 184, 156, 30);
            if (hovered == planeName)
                c.setAlpha(100);
            p.setBrush(c);
            p.setPen(QPen(hovered == planeName ? QColor("#b8e6ff") : QColor("#8199ab"),
                          hovered == planeName ? 2 : 1));
            p.drawPolygon(polygon);
            planeRegions.append({planeName, polygon});
            bounds = bounds.united(polygon.boundingRect());
            ++i;
        }
        const QPointF labelPoints[] = {QPointF(bounds.center().x(), bounds.bottom() + 24),
                                       QPointF(bounds.left() - 32, bounds.center().y()),
                                       QPointF(bounds.right() + 32, bounds.center().y())};
        p.setFont(QFont("Helvetica Neue", 10, QFont::Medium));
        for (int index = 0; index < 3; ++index) {
            auto region = planeRegions[index];
            QRectF badge(labelPoints[index] - QPointF(21, 12), QSizeF(42, 24));
            p.setPen(QPen(QColor("#71899d"), 1));
            p.drawLine(region.second.boundingRect().center(), labelPoints[index]);
            p.setBrush(hovered == region.first ? QColor("#345b74") : QColor("#283746"));
            p.setPen(QPen(hovered == region.first ? QColor("#91cee9") : QColor("#71899d"), 1));
            p.drawRoundedRect(badge, 4, 4);
            p.setPen(QColor("#e0e9f2"));
            p.drawText(badge, Qt::AlignCenter, region.first);
            planeRegions.append({region.first, QPolygonF(badge)});
        }
        p.setFont(QFont("Helvetica Neue", 11));
        p.setPen(QColor("#e0e9f2"));
        p.drawText(QRect(290, 20, width() - 420, 30), Qt::AlignCenter,
                   "Escolha um plano para o sketch   ·   Esc para cancelar");
    }
    if (moveHandleActive) {
        const QColor colors[] = {QColor("#ee8886"), QColor("#88da9a"), QColor("#72cffa")};
        auto base = project(handleOrigin + moveDistances);
        for (int axis = 0; axis < 3; ++axis) {
            auto tip = project(moveHandleTip(axis));
            auto d = tip - base;
            double length = std::hypot(d.x(), d.y());
            p.setPen(QPen(colors[axis], 3));
            p.drawLine(base, tip);
            p.setBrush(colors[axis]);
            if (length > 1) {
                d /= length;
                QPointF n(-d.y(), d.x());
                p.drawPolygon(QPolygonF{tip, tip - d * 12 + n * 5, tip - d * 12 - n * 5});
            }
            p.drawEllipse(tip, 5, 5);
            p.drawText(tip + QPointF(9, -9), QString("XYZ")[axis]);
        }
    }
    if (handleActive) {
        auto base = project(handleOrigin), tip = project(handleOrigin + handleAxis * handleDistance);
        p.setPen(QPen(QColor("#69d4fc"), 3));
        p.drawLine(base, tip);
        auto direction = tip - base;
        if (QLineF(base, tip).length() < 1)
            direction = project(handleOrigin + handleAxis * 10) - base;
        double len = std::hypot(direction.x(), direction.y());
        if (len > 1) {
            direction /= len;
            QPointF n(-direction.y(), direction.x());
            p.setBrush(QColor("#69d4fc"));
            p.drawPolygon(QPolygonF{tip, tip - direction * 13 + n * 6, tip - direction * 13 - n * 6});
        }
        p.setPen(QPen(QColor("#d8f3ff"), 1));
        p.setBrush(QColor("#3c667c"));
        p.drawEllipse(tip, 6, 6);
        p.setPen(QColor("#edf8fc"));
        p.drawText(tip + QPointF(12, -12), QString::number(handleDistance, 'f', 2) + " mm");
    }
}
QString Viewport::pick(QPointF pixel) const {
    for (auto &feature : model->features)
        if (feature.type == "sketch" && feature.visible && !model->consumed(feature.id)) {
            const auto &p = feature.p;
            QString plane = p["plane"].toString("XY"), kind = p["profile"].toString();
            QPolygonF region;
            auto add = [&](double u, double v) {
                region << project(Model::planePoint(plane, u, v, p["offset"].toDouble()));
            };
            double x = p["x"].toDouble(), y = p["y"].toDouble();
            if (kind == "rectangle") {
                double w = p["w"].toDouble(), h = p["h"].toDouble();
                add(x, y);
                add(x + w, y);
                add(x + w, y + h);
                add(x, y + h);
            } else if (kind == "circle") {
                for (int i = 0; i < 64; ++i) {
                    double a = i * 2 * M_PI / 64, r = p["r"].toDouble();
                    add(x + r * std::cos(a), y + r * std::sin(a));
                }
            } else if (kind == "polyline" && p["closed"].toBool()) {
                for (auto vertex : p["points"].toArray()) {
                    auto a = vertex.toArray();
                    add(a[0].toDouble(), a[1].toDouble());
                }
            }
            if (region.size() > 2 && region.containsPoint(pixel, Qt::OddEvenFill))
                return feature.id;
        }
    for (auto &f : model->features)
        if (f.type == "sketch" && f.visible && (!model->consumed(f.id) || f.id == selected)) {
            for (TopExp_Explorer it(f.shape, TopAbs_EDGE); it.More(); it.Next()) {
                BRepAdaptor_Curve c(TopoDS::Edge(it.Current()));
                int steps = c.GetType() == GeomAbs_Line ? 1 : 64;
                QPointF previous;
                for (int i = 0; i <= steps; ++i) {
                    auto p =
                        c.Value(c.FirstParameter() + (c.LastParameter() - c.FirstParameter()) * i / steps);
                    auto current = project(QVector3D(p.X(), p.Y(), p.Z()));
                    if (i) {
                        auto segment = current - previous;
                        double length = QPointF::dotProduct(segment, segment);
                        if (length > 0) {
                            double t =
                                std::clamp(QPointF::dotProduct(pixel - previous, segment) / length, 0., 1.);
                            if (QLineF(pixel, previous + segment * t).length() < 8)
                                return f.id;
                        }
                    }
                    previous = current;
                }
            }
        }
    QVector3D o, d;
    ray(pixel, o, d);
    float closest = std::numeric_limits<float>::max();
    QString result;
    for (auto &t : mesh) {
        auto e1 = t.b - t.a, e2 = t.c - t.a, h = QVector3D::crossProduct(d, e2);
        float det = QVector3D::dotProduct(e1, h);
        if (std::abs(det) < 1e-8)
            continue;
        float inv = 1 / det;
        auto s = o - t.a;
        float u = inv * QVector3D::dotProduct(s, h);
        if (u < 0 || u > 1)
            continue;
        auto q = QVector3D::crossProduct(s, e1);
        float v = inv * QVector3D::dotProduct(d, q);
        if (v < 0 || u + v > 1)
            continue;
        float dist = inv * QVector3D::dotProduct(e2, q);
        if (dist > 0 && dist < closest) {
            closest = dist;
            result = model->features[t.feature].id;
        }
    }
    return result;
}
void Viewport::submit(QJsonObject p) {
    p["plane"] = plane;
    p["offset"] = planeOffset;
    draft.clear();
    if (onProfile)
        onProfile(p);
    update();
}
void Viewport::finishPolyline(bool close) {
    if (draft.size() < 2)
        return;
    QJsonArray a;
    for (auto p : draft)
        a.append(QJsonArray{p.x(), p.y()});
    submit({{"profile", "polyline"}, {"points", a}, {"closed", close}});
}
QVector3D Viewport::moveHandleTip(int axis) const {
    QVector3D direction;
    direction[axis] = moveHandleLength;
    return handleOrigin + moveDistances + direction;
}
void Viewport::mousePressEvent(QMouseEvent *e) {
    setFocus();
    last = pressed = e->position();
    draggingHandle = handleActive && e->button() == Qt::LeftButton &&
                     QLineF(e->position(), project(handleOrigin + handleAxis * handleDistance)).length() < 18;
    if (moveHandleActive && e->button() == Qt::LeftButton) {
        double nearest = 18;
        for (int axis = 0; axis < 3; ++axis) {
            double distance = QLineF(e->position(), project(moveHandleTip(axis))).length();
            if (distance < nearest) {
                nearest = distance;
                moveAxis = axis;
                draggingHandle = true;
                handleAxis = QVector3D();
                handleAxis[axis] = 1;
                handleDistance = moveDistances[axis];
            }
        }
    }
}
void Viewport::mouseMoveEvent(QMouseEvent *e) {
    planeHover = e->position();
    bool overCube = !cubeDirectionAt(planeHover).isNull() ||
                    QRect(width() - 84, 100, 50, 20).contains(planeHover.toPoint());
    setCursor(overCube ? Qt::PointingHandCursor : tool.isEmpty() ? Qt::ArrowCursor : Qt::CrossCursor);
    auto delta = e->position() - last;
    last = e->position();
    cursor = planeAt(e->position());
    if (draggingHandle) {
        auto a = project(handleOrigin), b = project(handleOrigin + handleAxis);
        auto dir = b - a;
        double length = QPointF::dotProduct(dir, dir);
        if (length > 1e-6) {
            handleDistance += QPointF::dotProduct(delta, dir) / length;
            if (snap)
                handleDistance = std::round(handleDistance * 10) / 10;
            if (moveHandleActive) {
                moveDistances[moveAxis] = handleDistance;
                if (onMoveDistance)
                    onMoveDistance(moveAxis, handleDistance);
            } else if (onHandleDistance)
                onHandleDistance(handleDistance);
        }
        update();
        return;
    }
    const bool middle = e->buttons().testFlag(Qt::MiddleButton);
    const bool alternative =
        e->buttons().testFlag(Qt::LeftButton) && e->modifiers().testFlag(Qt::AltModifier);
    const bool toolbarNavigation = e->buttons().testFlag(Qt::LeftButton) && !navigationMode.isEmpty();
    if (middle || alternative || toolbarNavigation) {
        cameraAnimation.stop();
        const bool orbit = !sketchMode && (toolbarNavigation ? navigationMode == "orbit"
                                           : middle          ? e->modifiers().testFlag(Qt::ShiftModifier)
                                                             : !e->modifiers().testFlag(Qt::ShiftModifier));
        if (!orbit) {
            auto inv = matrix().inverted();
            auto a = inv.map(QVector3D(0, 0, 0)),
                 b = inv.map(QVector3D(-2 * delta.x() / width(), 2 * delta.y() / height(), 0));
            center += b - a;
        } else {
            yaw -= delta.x() * .5;
            pitch = std::clamp(pitch + float(delta.y()) * .5f, -89.f, 89.f);
        }
    }
    update();
}
void Viewport::mouseReleaseEvent(QMouseEvent *e) {
    if (draggingHandle) {
        draggingHandle = false;
        return;
    }
    if (QLineF(pressed, e->position()).length() > 4 || e->button() != Qt::LeftButton)
        return;
    auto s = e->position();
    auto direction = cubeDirectionAt(s);
    if (!direction.isNull()) {
        viewDirection(direction);
        return;
    }
    if (QRect(width() - 84, 100, 50, 20).contains(s.toPoint())) {
        view("iso", true);
        return;
    }
    if (choosingPlane) {
        QString name;
        double offset = 0;
        QVector3D origin, direction;
        ray(s, origin, direction);
        float nearest = std::numeric_limits<float>::max();
        for (auto &t : mesh) {
            auto e1 = t.b - t.a, e2 = t.c - t.a, h = QVector3D::crossProduct(direction, e2);
            float det = QVector3D::dotProduct(e1, h);
            if (std::abs(det) < 1e-8)
                continue;
            auto so = origin - t.a;
            float u = QVector3D::dotProduct(so, h) / det;
            if (u < 0 || u > 1)
                continue;
            auto q = QVector3D::crossProduct(so, e1);
            float v = QVector3D::dotProduct(direction, q) / det, dist = QVector3D::dotProduct(e2, q) / det;
            if (v < 0 || u + v > 1 || dist < 0 || dist >= nearest)
                continue;
            auto n = QVector3D::crossProduct(e1, e2).normalized();
            auto hit = origin + direction * dist;
            if (std::abs(n.z()) > .999) {
                name = "XY";
                offset = hit.z();
            } else if (std::abs(n.y()) > .999) {
                name = "XZ";
                offset = -hit.y();
            } else if (std::abs(n.x()) > .999) {
                name = "YZ";
                offset = hit.x();
            } else
                continue;
            nearest = dist;
        }
        if (name.isEmpty())
            for (auto &region : planeRegions)
                if (region.second.containsPoint(s, Qt::OddEvenFill)) {
                    name = region.first;
                    break;
                }
        if (!name.isEmpty() && onPlaneChosen) {
            choosingPlane = false;
            onPlaneChosen(name, offset);
        }
        return;
    }
    if (sketchMode && !tool.isEmpty()) {
        cursor = planeAt(s);
        if (tool == "polyline" && draft.size() >= 3 &&
            QLineF(project(Model::planePoint(plane, draft.front().x(), draft.front().y(), planeOffset)), s)
                    .length() < 12) {
            finishPolyline(true);
            return;
        }
        if (!draft.empty() && QLineF(draft.last(), cursor).length() < 1e-5)
            return;
        draft.push_back(cursor);
        if (draft.size() == 2 && tool == "rectangle") {
            auto a = draft[0], b = draft[1];
            submit({{"profile", "rectangle"},
                    {"x", std::min(a.x(), b.x())},
                    {"y", std::min(a.y(), b.y())},
                    {"w", std::abs(b.x() - a.x())},
                    {"h", std::abs(b.y() - a.y())}});
        } else if (draft.size() == 2 && tool == "circle") {
            auto a = draft[0];
            submit({{"profile", "circle"}, {"x", a.x()}, {"y", a.y()}, {"r", QLineF(a, draft[1]).length()}});
        } else if (draft.size() == 3 && tool == "arc") {
            auto a = draft[0], b = draft[1], c = draft[2];
            submit({{"profile", "arc"},
                    {"x1", a.x()},
                    {"y1", a.y()},
                    {"xm", b.x()},
                    {"ym", b.y()},
                    {"x2", c.x()},
                    {"y2", c.y()}});
        }
        update();
        return;
    }
    selected = pick(s);
    if (onSelect)
        onSelect(selected);
    update();
}
void Viewport::wheelEvent(QWheelEvent *e) {
    cameraAnimation.stop();
    if (!e->pixelDelta().isNull()) {
        auto delta = e->pixelDelta();
        if (e->modifiers().testFlag(Qt::ShiftModifier) && !sketchMode) {
            yaw -= delta.x() * .4;
            pitch = std::clamp(pitch + float(delta.y()) * .4f, -89.f, 89.f);
        } else {
            auto inv = matrix().inverted();
            center += inv.map(QVector3D(-2. * delta.x() / width(), 2. * delta.y() / height(), 0)) -
                      inv.map(QVector3D());
        }
    } else
        span = std::clamp(span * float(std::exp(-e->angleDelta().y() * .001)), 1.f, 1e6f);
    update();
}
bool Viewport::event(QEvent *event) {
    if (event->type() == QEvent::NativeGesture) {
        auto *gesture = static_cast<QNativeGestureEvent *>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            cameraAnimation.stop();
            span = std::clamp(span * float(std::exp(-gesture->value())), 1.f, 1e6f);
            update();
            event->accept();
            return true;
        }
    }
    return QOpenGLWidget::event(event);
}
void Viewport::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        cameraAnimation.stop();
        draft.clear();
        choosingPlane = false;
        if (onCancelCommand)
            onCancelCommand();
        setTool({});
    } else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (onAcceptCommand) {
            onAcceptCommand();
            return;
        }
        if (tool == "polyline")
            finishPolyline(e->modifiers().testFlag(Qt::ShiftModifier));
    } else if (e->key() == Qt::Key_F)
        fit();
    else
        QOpenGLWidget::keyPressEvent(e);
    update();
}

void Viewport::zoomBy(float factor) {
    span = std::clamp(span * factor, 1.f, 1e6f);
    update();
}
void Viewport::setModel(Model *m) {
    model = m;
    refresh();
}
