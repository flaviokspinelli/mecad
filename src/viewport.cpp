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
                std::abs(pitch) > 89 ? QVector3D(0, 1, 0) : QVector3D(0, 0, 1));
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
        span = std::max(20., std::sqrt((X - x) * (X - x) + (Y - y) * (Y - y) + (Z - z) * (Z - z)) * 1.5);
    }
    update();
}
void Viewport::view(QString name) {
    if (sketchMode)
        name = plane == "XY" ? "top" : plane == "XZ" ? "front" : "right";
    if (name == "top") {
        yaw = 0;
        pitch = 90;
    } else if (name == "front") {
        yaw = -90;
        pitch = 0;
    } else if (name == "right") {
        yaw = 0;
        pitch = 0;
    } else {
        yaw = -55;
        pitch = 32;
    }
    update();
}
void Viewport::setTool(QString name) {
    tool = name;
    draft.clear();
    setCursor(tool.isEmpty() ? Qt::ArrowCursor : Qt::CrossCursor);
    setFocus();
    update();
}
void Viewport::paintGL() {
    QColor bg = light ? QColor("#e8edf2") : QColor("#202936");
    glClearColor(bg.redF(), bg.greenF(), bg.blueF(), 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (ready && !vertices.empty()) {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
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
            shader.setUniformValue("color", chosen ? QVector3D(.2, .74, 1) : QVector3D(.68, .77, .86));
            glDrawArrays(GL_TRIANGLES, start * 3, (end - start) * 3);
            start = end;
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
    if (mesh.empty() || sketchMode) {
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
    if (mesh.empty() || sketchMode) {
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
    p.drawText(24, 30, sketchMode ? "SKETCH  /  " + plane + "  /  mm" : "DESIGN  /  SÓLIDOS  /  mm");
    if (mesh.empty() && model->features.empty() && !sketchMode) {
        p.setPen(light ? QColor("#576a7e") : QColor("#90a2b7"));
        p.setFont(QFont("Helvetica Neue", 20));
        p.drawText(rect().adjusted(0, 0, 0, -32), Qt::AlignCenter, "Sua próxima peça começa aqui");
        p.setFont(QFont("Helvetica Neue", 12));
        p.drawText(rect().adjusted(0, 42, 0, 0), Qt::AlignCenter,
                   "Crie um sketch ou abra o exemplo de suporte");
    }
    // Compact orientation control with directly selectable views.
    int cx = width() - 94;
    QRect face(cx, 48, 66, 48);
    p.setPen(QPen(QColor("#75879b"), 1));
    p.setBrush(light ? QColor("#fafcff") : QColor("#344154"));
    p.drawRoundedRect(face, 5, 5);
    p.setPen(light ? QColor("#344154") : QColor("#dce7f4"));
    p.setFont(QFont("Helvetica Neue", 10, QFont::DemiBold));
    p.drawText(face, Qt::AlignCenter, "TOP");
    p.drawText(QRect(cx - 13, 100, 55, 24), Qt::AlignCenter, "FRONT");
    p.drawText(QRect(cx + 43, 100, 45, 24), Qt::AlignCenter, "RIGHT");
    p.drawText(QRect(cx, 129, 65, 24), Qt::AlignCenter, "ISO");
    p.setPen(light ? QColor("#506479") : QColor("#869bb1"));
    p.setFont(QFont("Helvetica Neue", 10));
    p.drawText(24, height() - 22,
               sketchMode ? "Clique para desenhar  ·  Enter conclui linha  ·  Esc cancela  ·  Grade: 1 mm"
                          : "Botão central: pan  ·  Shift + central: órbita  ·  Roda: zoom  ·  F: enquadrar");
}
QString Viewport::pick(QPointF pixel) const {
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
void Viewport::mousePressEvent(QMouseEvent *e) {
    setFocus();
    last = pressed = e->position();
}
void Viewport::mouseMoveEvent(QMouseEvent *e) {
    auto delta = e->position() - last;
    last = e->position();
    cursor = planeAt(e->position());
    const bool middle = e->buttons().testFlag(Qt::MiddleButton);
    const bool alternative = e->buttons().testFlag(Qt::LeftButton) && e->modifiers().testFlag(Qt::AltModifier);
    if (middle || alternative) {
        const bool orbit = !sketchMode && (middle ? e->modifiers().testFlag(Qt::ShiftModifier)
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
    if (QLineF(pressed, e->position()).length() > 4 || e->button() != Qt::LeftButton)
        return;
    int cx = width() - 94;
    auto s = e->position();
    if (s.x() > cx - 15 && s.x() < cx + 90 && s.y() > 45 && s.y() < 155) {
        if (s.y() < 98)
            view("top");
        else if (s.y() < 125)
            view(s.x() < cx + 43 ? "front" : "right");
        else
            view("iso");
        if (sketchMode)
            view(plane == "XY" ? "top" : plane == "XZ" ? "front" : "right");
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
    if (!e->pixelDelta().isNull()) {
        auto delta=e->pixelDelta();
        if(e->modifiers().testFlag(Qt::ShiftModifier) && !sketchMode) {
            yaw-=delta.x()*.4;
            pitch=std::clamp(pitch+float(delta.y())*.4f,-89.f,89.f);
        } else {
            auto inv=matrix().inverted();
            center+=inv.map(QVector3D(-2.*delta.x()/width(),2.*delta.y()/height(),0))-inv.map(QVector3D());
        }
    } else span = std::clamp(span * float(std::exp(-e->angleDelta().y() * .001)), 1.f, 1e6f);
    update();
}
bool Viewport::event(QEvent *event) {
    if(event->type()==QEvent::NativeGesture) {
        auto *gesture=static_cast<QNativeGestureEvent*>(event);
        if(gesture->gestureType()==Qt::ZoomNativeGesture) {
            span=std::clamp(span*float(std::exp(-gesture->value())),1.f,1e6f);
            update();event->accept();return true;
        }
    }
    return QOpenGLWidget::event(event);
}
void Viewport::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        draft.clear();
        setTool({});
    } else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (tool == "polyline")
            finishPolyline(e->modifiers().testFlag(Qt::ShiftModifier));
    } else if (e->key() == Qt::Key_F)
        fit();
    else
        QOpenGLWidget::keyPressEvent(e);
    update();
}
