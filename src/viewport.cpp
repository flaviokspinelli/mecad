#include "viewport.h"
#include "snap_intersections.h"
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
#include <gp_Circ.hxx>
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
    gridBuffer.destroy();
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
QPointF Viewport::planeAt(QPointF pixel, bool grid) const {
    QVector3D o, d;
    ray(pixel, o, d);
    auto n = Model::planeNormal(plane);
    float denominator = QVector3D::dotProduct(d, n);
    if (std::abs(denominator) < 1e-6)
        return {};
    auto hit = o + d * (QVector3D::dotProduct(Model::planePoint(plane, 0, 0, planeOffset)-o, n) / denominator);
    auto coordinates = Model::planeCoordinates(plane, hit);
    double u = coordinates.x(), v = coordinates.y();
    if (snap && grid) {
        u = std::round(u);
        v = std::round(v);
    }
    return {u, v};
}
QPointF Viewport::sketchPoint(QPointF pixel) {
    const auto previous = magnetPoint;
    const bool held = !magnetLabel.isEmpty();
    magnetLabel.clear();
    magnetGuides.clear();
    auto raw = planeAt(pixel, false), result = planeAt(pixel);
    if (!smartSnap)
        return result;
    QVector<QPair<QPointF, QString>> targets = {{{0, 0}, "Origem"}};
    QVector<QLineF> segments;
    struct CircularTarget {
        snapping::Circle shape;bool full=true;QPointF start,middle,end;
        bool accepts(QPointF point)const{return full || snapping::onArc(shape,start,middle,end,point);}
    };
    QVector<CircularTarget> circles;
    auto screen = [&](QPointF p) { return project(Model::planePoint(plane, p.x(), p.y(), planeOffset)); };
    auto local = [&](const gp_Pnt &p) {
        return Model::planeCoordinates(plane, {float(p.X()),float(p.Y()),float(p.Z())});
    };
    for (const auto &f : model->features) {
        const bool sketchOnPlane = f.type == "sketch" &&
            (f.id == selected || (f.visible && !model->consumed(f.id))) &&
            f.p["plane"].toString("XY") == plane &&
            std::abs(f.p["offset"].toDouble() - planeOffset) <= 1e-5;
        const bool cadReference = f.type != "sketch" && f.visible && !f.inactive && !model->consumed(f.id);
        if (!sketchOnPlane && !cadReference)
            continue;
        for (TopExp_Explorer it(f.shape, TopAbs_EDGE); it.More(); it.Next()) {
            BRepAdaptor_Curve edge(TopoDS::Edge(it.Current()));
            double first = edge.FirstParameter(), last = edge.LastParameter();
            if (edge.GetType() == GeomAbs_Circle && std::abs(last - first - 2 * M_PI) < 1e-7) {
                circles.append(CircularTarget{{local(edge.Circle().Location()),edge.Circle().Radius()}});
                targets.append({local(edge.Circle().Location()), "Centro"});
                for (int quadrant = 0; quadrant < 4; ++quadrant)
                    targets.append({local(edge.Value(first + (last - first) * quadrant / 4)), "Quadrante"});
                continue;
            }
            auto a = local(edge.Value(first)), b = local(edge.Value(last));
            targets.append({a, "Extremidade"});
            targets.append({b, "Extremidade"});
            targets.append({local(edge.Value((first + last) / 2)), "Ponto médio"});
            if (edge.GetType() == GeomAbs_Line)
                segments.append(QLineF(a, b));
            if (edge.GetType() == GeomAbs_Circle) {
                targets.append({local(edge.Circle().Location()), "Centro"});
                circles.append(CircularTarget{{local(edge.Circle().Location()),edge.Circle().Radius()},false,a,
                    local(edge.Value((first+last)/2)),b});
            }
        }
    }
    for (auto point : draft)
        targets.append({point, "Extremidade"});
    // Only intersect circles close enough to the cursor to be eligible for snap.
    const double reach=1.5*std::max(QLineF(raw,planeAt(pixel+QPointF(15,0),false)).length(),
                                    QLineF(raw,planeAt(pixel+QPointF(0,15),false)).length());
    circles.erase(std::remove_if(circles.begin(),circles.end(),[&](const auto &circle){
        return std::abs(QLineF(raw,circle.shape.center).length()-circle.shape.radius)>reach;
    }),circles.end());
    for (int i = 0; i < segments.size(); ++i) {
        if (!QRectF(screen(segments[i].p1()), screen(segments[i].p2()))
                 .normalized()
                 .adjusted(-15, -15, 15, 15)
                 .contains(pixel))
            continue;
        for (int j = i + 1; j < segments.size(); ++j) {
            QPointF intersection;
            if (segments[i].intersects(segments[j], &intersection) == QLineF::BoundedIntersection)
                targets.append({intersection, "Interseção"});
        }
        for(const auto &circle:circles)
            for(const auto &intersection:snapping::intersections(segments[i],circle.shape))
                if(circle.accepts(intersection))targets.append({intersection,"Interseção"});
    }
    for(int i=0;i<circles.size();++i)for(int j=i+1;j<circles.size();++j)
        for(const auto &intersection:snapping::intersections(circles[i].shape,circles[j].shape))
            if(circles[i].accepts(intersection) && circles[j].accepts(intersection))targets.append({intersection,"Interseção"});
    double best = 1e10;
    for (const auto &target : targets) {
        double distance = QLineF(pixel, screen(target.first)).length();
        double radius = held && QLineF(previous, target.first).length() < 1e-6 ? 15 : 10;
        if (distance <= radius && distance < best) {
            best = distance;
            result = target.first;
            magnetLabel = target.second;
        }
    }
    if (!magnetLabel.isEmpty()) {
        magnetPoint = result;
        return result;
    }
    double bestU = 7, bestV = 7;
    QPointF uAnchor, vAnchor;
    bool alignU = false, alignV = false;
    for (const auto &target : targets) {
        double du = QLineF(pixel, screen({target.first.x(), raw.y()})).length();
        double dv = QLineF(pixel, screen({raw.x(), target.first.y()})).length();
        if (du < bestU) {
            bestU = du;
            result.setX(target.first.x());
            uAnchor = target.first;
            alignU = true;
        }
        if (dv < bestV) {
            bestV = dv;
            result.setY(target.first.y());
            vAnchor = target.first;
            alignV = true;
        }
    }
    if (alignU) {
        magnetLabel = "Vertical";
        magnetGuides.append(QLineF(uAnchor, result));
    }
    if (alignV) {
        magnetLabel = alignU ? "Horizontal + vertical" : "Horizontal";
        magnetGuides.append(QLineF(vAnchor, result));
    }
    magnetPoint = result;
    return result;
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
    gridBuffer.create();
    vao.create();
    if (!ready && onHint)
        onHint("Falha ao iniciar a visualização OpenGL: " + shader.log());
}
void Viewport::refresh() {
    constraintDiagnostics.clear();
    for(const auto &feature:model->features)
        if(!feature.inactive && feature.type=="sketch" && feature.p.contains("constraintSystem"))
            constraintDiagnostics.insert(feature.id,model->sketchSystem(feature.id).solve());
    selectedDetails.clear();
    selectedDetail = {};
    hoveredDetail = {};
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
            if (!f.inactive && f.type == "sketch" && f.visible)
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
    areaCandidate = areaDragging = false;
    selectedDetails.clear();
    selectedDetail = {};
    hoveredDetail = {};
    draggingRotation = false;
    draggingMoveFree = draggingHandle = false;
    magnetLabel.clear();
    magnetGuides.clear();
    sketchPressCandidate = sketchDragging = false;
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

    // Draw orientation aids behind the solids, never over their faces.
    paintGrid();
    glClear(GL_DEPTH_BUFFER_BIT);

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
            while (end < int(mesh.size()) && mesh[end].feature == mesh[start].feature && mesh[end].face == mesh[start].face)
                ++end;
            bool chosen = objectSelected(model->features[mesh[start].feature].id);
            auto matchesFace = [&](const SelectionTarget &target) {
                return target.kind == "face" && target.feature == model->features[mesh[start].feature].id && target.index == mesh[start].face;
            };
            chosen = chosen || matchesFace(selectedDetail);
            for (const auto &target : selectedDetails) chosen = chosen || matchesFace(target);
            bool hovered = hoveredDetail.feature == model->features[mesh[start].feature].id &&
                           hoveredDetail.kind == "object";
            hovered = hovered || matchesFace(hoveredDetail);
            shader.setUniformValue("color", chosen    ? QVector3D(.42, .77, .94)
                                            : hovered ? QVector3D(.97, .80, .50)
                                                      : QVector3D(.83, .86, .89));
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
void Viewport::paintGrid() {
    if (!ready || choosingPlane)
        return;
    double step = span > 500 ? 100 : (span > 150 ? 10 : (span > 40 ? 5 : 1));
    QPointF local = planeAt({width() / 2., height() / 2.});
    double half = span * 1.5;
    QVector<QVector3D> lines;
    auto point = [&](double u, double v) {
        lines.append(Model::planePoint(plane, u, v, planeOffset));
        lines.append(QVector3D(0, 0, 1));
    };
    for (double i = std::floor((local.x() - half) / step) * step; i < local.x() + half; i += step) {
        point(i, local.y() - half);
        point(i, local.y() + half);
    }
    for (double i = std::floor((local.y() - half) / step) * step; i < local.y() + half; i += step) {
        point(local.x() - half, i);
        point(local.x() + half, i);
    }
    int gridCount = lines.size() / 2;
    point(-50000, 0);
    point(50000, 0);
    point(0, -50000);
    point(0, 50000);
    shader.bind();
    vao.bind();
    gridBuffer.bind();
    gridBuffer.allocate(lines.constData(), lines.size() * sizeof(QVector3D));
    shader.enableAttributeArray("position");
    shader.setAttributeBuffer("position", GL_FLOAT, 0, 3, 2 * sizeof(QVector3D));
    shader.enableAttributeArray("normal");
    shader.setAttributeBuffer("normal", GL_FLOAT, sizeof(QVector3D), 3, 2 * sizeof(QVector3D));
    shader.setUniformValue("mvp", matrix());
    shader.setUniformValue("color", light ? QVector3D(.70, .75, .80) : QVector3D(.30, .39, .46));
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES, 0, gridCount);
    shader.setUniformValue("color", QVector3D(.72, .39, .44));
    glDrawArrays(GL_LINES, gridCount, 2);
    shader.setUniformValue("color", QVector3D(.37, .67, .55));
    glDrawArrays(GL_LINES, gridCount + 2, 2);
    gridBuffer.release();
    vao.release();
    shader.release();
}
void Viewport::paintOverlay(QPainter &p) {
    p.setRenderHint(QPainter::Antialiasing);
    if (areaDragging) {
        bool crossing = areaEnd.x() < pressed.x();
        QColor color = crossing ? QColor("#62c99e") : QColor("#65ceff");
        p.setPen(QPen(color, 1, crossing ? Qt::DashLine : Qt::SolidLine));
        color.setAlpha(35);
        p.setBrush(color);
        p.drawRect(QRectF(pressed, areaEnd).normalized());
        p.setBrush(Qt::NoBrush);
    }
    for (auto &f : model->features)
        if (!f.inactive && f.type == "sketch" && (f.id == selected || (f.visible && !model->consumed(f.id)))) {
            bool chosen = objectSelected(f.id);
            bool hovered = hoveredDetail.feature == f.id && hoveredDetail.kind == "object";
            p.setPen(QPen(chosen    ? QColor("#65ceff")
                          : hovered ? QColor("#ffd080")
                                    : QColor("#aec3d3"),
                          chosen ? 2.5 : 1.7));
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
    auto drawDetail = [&](const SelectionTarget &target, QColor color) {
        if (target.geometry.empty() || (target.kind != "edge" && target.kind != "vertex"))
            return;
        p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
        if (target.kind == "vertex") {
            p.setBrush(color);
            p.drawEllipse(project(target.geometry[0]), 5, 5);
        } else {
            p.setBrush(Qt::NoBrush);
            QPolygonF line;
            for (auto point : target.geometry)
                line << project(point);
            p.drawPolyline(line);
        }
    };
    drawDetail(hoveredDetail, QColor("#ffd080"));
    if (selectedDetails.empty())
        drawDetail(selectedDetail, QColor("#65ceff"));
    else
        for (const auto &target : selectedDetails)
            drawDetail(target, QColor("#65ceff"));
    if (selectedDetails.size() > 1) {
        p.setPen(QColor("#cceaff"));
        p.drawText(290, 45, QString("%1 itens selecionados").arg(selectedDetails.size()));
    } else if (hasSubselection()) {
        p.setPen(QColor("#cceaff"));
        const auto &detail=selectedDetails.empty()?selectedDetail:selectedDetails.front();
        p.drawText(290, 45,
                   detail.index<0 ? QString("Geometria realçada") :
                   (detail.kind == "face" ? QString("Face %1 selecionada") : detail.kind == "vertex" ? QString("Vértice %1 selecionado")
                                                    : QString("Aresta %1 selecionada"))
                       .arg(detail.index + 1));
    }
    dimensions.clear();
    if (sketchMode && !selected.isEmpty()) {
        const auto &f = model->get(selected);
        const auto &params = f.p;
        if (!f.inactive && f.type == "sketch") {
            auto point = [&](double u, double v) {
                return project(
                    Model::planePoint(params["plane"].toString("XY"), u, v, params["offset"].toDouble()));
            };
            auto dimension = [&](QPointF a, QPointF b, QPointF offset, QString label, QString key,
                                 double multiplier = 1) {
                p.setPen(QPen(QColor("#e6bd7e"), 1));
                p.drawLine(a, a + offset);
                p.drawLine(b, b + offset);
                p.drawLine(a + offset, b + offset);
                for (auto end : {a + offset, b + offset})
                    p.drawLine(end + QPointF(-3, -3), end + QPointF(3, 3));
                auto mid = (a + b) / 2 + offset;
                QRectF text(mid.x() - 45, mid.y() - 12, 90, 24);
                dimensions.append({key, text, multiplier});
                p.fillRect(text, light ? QColor("#e8edf2") : QColor("#202936"));
                if (text.contains(planeHover)) {
                    p.setPen(QPen(QColor("#7dd3fc"), 1));
                    p.drawRect(text);
                }
                p.drawText(text, Qt::AlignCenter, label);
            };
            double x = params["x"].toDouble(), y = params["y"].toDouble();
            QString kind = params["profile"].toString();
            if (kind == "rectangle") {
                double w = params["w"].toDouble(), h = params["h"].toDouble();
                dimension(point(x, y), point(x + w, y), {0, 26}, QString::number(w, 'f', 2), "w");
                dimension(point(x + w, y), point(x + w, y + h), {40, 0}, QString::number(h, 'f', 2), "h");
            } else if (kind == "circle") {
                double r = params["r"].toDouble();
                dimension(point(x - r, y), point(x + r, y), {0, 0}, "Ø " + QString::number(r * 2, 'f', 2),
                          "r", 2);
            } else if(params.contains("constraintSystem")) {
                const auto system=sketch::System::fromJson(params["constraintSystem"].toObject());
                if(constraintDiagnostics.contains(f.id)) {
                    const auto &diagnostic=constraintDiagnostics[f.id];
                    for(const auto &entry:system.points) {
                        const bool fixed=diagnostic.pointDegreesOfFreedom.value(entry.id,2)==0;
                        p.setPen(QPen(QColor("#1d2e3d"),1));
                        p.setBrush(fixed?QColor("#e7d7a2"):QColor("#51c9ed"));
                        p.drawEllipse(point(entry.position.x(),entry.position.y()),4,4);
                    }
                    p.setPen(QColor("#dce6ed"));
                    p.drawText(290,65,QString("%1 · %2 graus de liberdade · pontos: dourado = preso, azul = móvel")
                        .arg(diagnostic.degreesOfFreedom==0?"Totalmente restrito":"Sub-restrito").arg(diagnostic.degreesOfFreedom));
                }
                QMap<QString,QPointF> points;for(const auto &entry:system.points)points[entry.id]=entry.position;
                int horizontal=0,vertical=0;
                for(const auto &constraint:system.constraints) {
                    const auto a=points.value(constraint.first),b=points.value(constraint.second);
                    const auto key="constraint:"+constraint.id;
                    if(constraint.relation==sketch::Relation::DistanceX)
                        dimension(point(a.x(),b.y()),point(b.x(),b.y()),{0,26.+28.*horizontal++},
                            "X "+QString::number(constraint.value.x(),'f',2),key);
                    else if(constraint.relation==sketch::Relation::DistanceY)
                        dimension(point(b.x(),a.y()),point(b.x(),b.y()),{40.+48.*vertical++,0},
                            "Y "+QString::number(constraint.value.y(),'f',2),key);
                }
            }
        }
    }
    auto pixel = [&](QPointF v) { return project(Model::planePoint(plane, v.x(), v.y(), planeOffset)); };
    if (!draft.empty()) {
        p.setPen(QPen(QColor("#62dbbf"), 2, Qt::DashLine));
        auto a = pixel(draft.front()), b = pixel(cursor);
        if (tool == "rectangle") {
            auto origin = draft.front();
            p.drawPolygon(QPolygonF{a, pixel({cursor.x(), origin.y()}), b, pixel({origin.x(), cursor.y()})});
        } else if (tool == "polygon") {
            QPolygonF outline;
            for (auto point : regularPolygon(draft.front(), cursor))
                outline << pixel(point);
            p.drawPolygon(outline);
            p.drawLine(a, b);
            p.drawText(b + QPointF(14, 18), QString("%1 lados · ↑/↓ altera").arg(polygonSides));
        } else if (tool == "circle") {
            double r = QLineF(draft.front(), cursor).length();
            QPolygonF circle;
            for (int i = 0; i < 64; ++i) {
                double angle = 2 * M_PI * i / 64;
                circle << pixel(draft.front() + QPointF(r * std::cos(angle), r * std::sin(angle)));
            }
            p.drawPolygon(circle);
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
        p.drawText(290, 24, "SKETCH  /  " + (plane.startsWith("FACE:") ? QString("FACE") : plane) + "  /  mm");
    if (mesh.empty() && model->features.empty() && !sketchMode && !choosingPlane) {
        p.setPen(light ? QColor("#576a7e") : QColor("#90a2b7"));
        p.setFont(QFont("Helvetica Neue", 12));
        p.drawText(QRect(290, 18, width() - 420, 28), Qt::AlignCenter, "Create Sketch para começar");
    }
    if (smartSnap && sketchMode && !tool.isEmpty() && !magnetLabel.isEmpty()) {
        p.save();
        auto snapPixel = [&](QPointF q) {
            return project(Model::planePoint(plane, q.x(), q.y(), planeOffset));
        };
        auto marker = snapPixel(magnetPoint);
        p.setPen(QPen(QColor("#8bdfb1"), 1, Qt::DashLine));
        for (const auto &guide : magnetGuides)
            p.drawLine(snapPixel(guide.p1()), snapPixel(guide.p2()));
        p.setPen(QPen(QColor("#a7f3c4"), 1.7));
        p.setBrush(QColor("#243d36"));
        p.drawRect(QRectF(marker - QPointF(4, 4), QSizeF(8, 8)));
        p.setFont(QFont("Helvetica Neue", 10));
        QRectF label(marker + QPointF(14, 12),
                     QSizeF(p.fontMetrics().horizontalAdvance(magnetLabel) + 14, 23));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#223b33"));
        p.drawRoundedRect(label, 4, 4);
        p.setPen(QColor("#b8f4d0"));
        p.drawText(label, Qt::AlignCenter, magnetLabel);
        p.restore();
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
    const bool homeHovered = QRect(width() - 84, 100, 50, 20).contains(planeHover.toPoint());
    p.save();
    p.translate(width() - 59, 110);
    if (homeHovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(104, 179, 221, 35));
        p.drawRoundedRect(QRectF(-13, -11, 26, 22), 4, 4);
    }
    p.setPen(QPen(homeHovered ? QColor("#c3eaff") : QColor("#a5b8c9"), 1.5, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath home;
    home.moveTo(-9, -1);
    home.lineTo(0, -8);
    home.lineTo(9, -1);
    home.moveTo(-6, -2);
    home.lineTo(-6, 7);
    home.lineTo(-2, 7);
    home.lineTo(-2, 2);
    home.lineTo(2, 2);
    home.lineTo(2, 7);
    home.lineTo(6, 7);
    home.lineTo(6, -2);
    p.drawPath(home);
    p.restore();
    p.setPen(QPen(QColor("#5bc087"), 1.5));
    p.drawLine(cubePoint({-1, -1, -1}), cubePoint({-1, 1.6, -1}));
    p.setPen(QPen(QColor("#61a7e3"), 1.5));
    p.drawLine(cubePoint({-1, -1, -1}), cubePoint({-1, -1, 1.6}));
    if (choosingPlane) {
        QString hovered;
        for (auto region = planeRegions.crbegin(); region != planeRegions.crend(); ++region)
            if (region->second.containsPoint(planeHover, Qt::OddEvenFill)) {
                hovered = region->first;
                break;
            }
        planeRegions.clear();
        int i = 0;
        for (auto planeName : {"XY", "YZ", "XZ"}) {
            QPolygonF polygon;
            // Three adjacent faces sharing the origin; all still lie on the
            // real zero-offset planes. No plane passes through another face.
            // Positive octant: XZ borders the front (Y=0) of the XY floor.
            // Draw XZ last and keep translucent faces, like the origin selector.
            const double u0 = 0, u1 = 40, v0 = 0, v1 = 40;
            for (auto q : {QPointF(u0, v0), QPointF(u1, v0), QPointF(u1, v1), QPointF(u0, v1)})
                polygon << project(Model::planePoint(planeName, q.x(), q.y()));
            QColor c = i == 0   ? QColor(102, 172, 214, 30)
                       : i == 2 ? QColor(211, 172, 105, 30)
                                : QColor(98, 184, 156, 30);
            if (hovered == planeName)
                c.setAlpha(100);
            p.setBrush(c);
            p.setPen(QPen(hovered == planeName ? QColor("#b8e6ff") : QColor("#8199ab"),
                          hovered == planeName ? 2 : 1));
            p.drawPolygon(polygon);
            planeRegions.append({planeName, polygon});
            ++i;
        }
        const QPointF labelPoints[] = {project(Model::planePoint("XY", 28, 28)),
                                       project(Model::planePoint("YZ", 28, 34)),
                                       project(Model::planePoint("XZ", 22, 16))};
        p.setFont(QFont("Helvetica Neue", 10, QFont::Medium));
        for (int index = 0; index < 3; ++index) {
            auto region = planeRegions[index];
            QRectF badge(labelPoints[index] - QPointF(21, 12), QSizeF(42, 24));
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
    if (moveHandleActive && rotationMode) {
        auto base = project(handleOrigin + moveDistances);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#72cffa"), 3));
        p.drawEllipse(base, 85, 85);
        p.setBrush(QColor("#72cffa"));
        p.drawEllipse(base + QPointF(85, 0), 6, 6);
        p.drawText(base + QPointF(-75, -100),
                   QString("Girar %1 · %2°").arg(rotationAxis).arg(rotationAngle, 0, 'f', 1));
    }
    if (moveHandleActive && !rotationMode) {
        const QColor colors[] = {QColor("#ee8886"), QColor("#88da9a"), QColor("#72cffa")};
        auto base = project(handleOrigin + moveDistances);
        QVector3D rayOrigin, rayDirection; ray(base, rayOrigin, rayDirection);
        for (int normal = 0; normal < 3; ++normal) {
            if (std::abs(rayDirection[normal]) <= .05) continue;
            const auto polygon = movePlanePolygon(normal);
            QColor fill = colors[normal];
            fill.setAlpha((draggingMoveFree ? movePlaneNormal : movePlaneAt(planeHover)) == normal ? 180 : 60);
            p.setPen(QPen(colors[normal], 1)); p.setBrush(fill);
            p.drawPolygon(polygon);
        }
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
        p.setPen(QPen(QColor("#bceaff"), 2));
        p.setBrush(draggingMoveFree ? QColor("#68ccef") : QColor("#314e63"));
        p.drawRoundedRect(QRectF(base - QPointF(7, 7), QSizeF(14, 14)), 3, 3);
        if (draggingMoveFree && movePlaneNormal >= 0)
            p.drawText(base + QPointF(10, 20), QStringList{"YZ", "XZ", "XY"}[movePlaneNormal]);
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
    return pickDetail(pixel, true).feature;
}
void Viewport::submit(QJsonObject p) {
    magnetLabel.clear();
    magnetGuides.clear();
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
QPolygonF Viewport::regularPolygon(QPointF center, QPointF vertex) const {
    QPolygonF points;
    const int sides = std::clamp(polygonSides, 3, 64);
    const double radius = QLineF(center, vertex).length();
    const double angle = std::atan2(vertex.y() - center.y(), vertex.x() - center.x());
    for (int i = 0; i < sides; ++i) {
        double theta = angle + 2 * M_PI * i / sides;
        points << center + QPointF(radius * std::cos(theta), radius * std::sin(theta));
    }
    return points;
}
int Viewport::movePlaneAt(QPointF pixel) const {
    if (QLineF(pixel, project(handleOrigin + moveDistances)).length() < 12) return -1;
    QVector3D origin, direction; ray(pixel, origin, direction);
    for (int normal = 0; normal < 3; ++normal)
        if (std::abs(direction[normal]) > .05 &&
            movePlanePolygon(normal).containsPoint(pixel, Qt::OddEvenFill)) return normal;
    return -1;
}
QPolygonF Viewport::movePlanePolygon(int normalAxis) const {
    const int a = (normalAxis + 1) % 3, b = (normalAxis + 2) % 3;
    QPolygonF polygon;
    for (const QPointF corner : {QPointF(.18,.18), QPointF(.38,.18), QPointF(.38,.38), QPointF(.18,.38)}) {
        auto point = handleOrigin + moveDistances;
        point[a] += moveHandleLength * corner.x(); point[b] += moveHandleLength * corner.y();
        polygon.append(project(point));
    }
    return polygon;
}
QVector3D Viewport::moveHandleTip(int axis) const {
    QVector3D direction;
    direction[axis] = moveHandleLength;
    return handleOrigin + moveDistances + direction;
}
void Viewport::closeDimensionEditor() {
    auto *editor = dimensionEditor.data();
    dimensionEditor.clear();
    if (editor) {
        editor->hide();
        editor->deleteLater();
    }
    update();
}
void Viewport::editDimension(const DimensionTarget &target) {
    if (selected.isEmpty() || !onDimensionEdit)
        return;
    closeDimensionEditor();
    setTool({});
    dimensionFeature = selected;
    dimensionKey = target.key;
    dimensionMultiplier = target.multiplier;
    auto *editor = new QLineEdit(this);
    dimensionEditor = editor;
    editor->setObjectName("inlineDimension");
    if(target.key.startsWith("constraint:")) {
        const auto &parameters=model->get(selected).p;
        QString expression=parameters.value("expressions").toObject().value(target.key).toString();
        if(expression.isEmpty())for(const auto &constraint:sketch::System::fromJson(parameters["constraintSystem"].toObject()).constraints)
            if(target.key=="constraint:"+constraint.id)
                expression=QString::number(constraint.relation==sketch::Relation::DistanceX?constraint.value.x():constraint.value.y(),'g',12)+" mm";
        editor->setText(expression);
    } else {
        const auto expression=model->get(selected).p.value("expressions").toObject().value(target.key).toString();
        editor->setText(expression.isEmpty()?QString::number(model->get(selected).p[target.key].toDouble()*target.multiplier,'g',12):
            target.multiplier==1?expression:QString("(%1) * %2").arg(expression).arg(target.multiplier));
    }
    editor->setProperty("originalDimensionText",editor->text());
    editor->setAlignment(Qt::AlignCenter);
    editor->setToolTip("Número em mm, unidade ou fórmula do projeto · Enter confirma · Esc cancela");
    editor->setStyleSheet("QLineEdit { background:#263b4b; color:#f4ce89; border:1px solid #72cffa; "
                          "border-radius:3px; padding:3px; }");
    auto center = target.rect.center();
    editor->setGeometry(std::clamp(int(center.x()) - 58, 0, std::max(0, width() - 116)),
                        std::clamp(int(center.y()) - 16, 0, std::max(0, height() - 32)), 116, 32);
    editor->installEventFilter(this);
    editor->show();
    editor->raise();
    editor->setFocus();
    editor->selectAll();
}
bool Viewport::eventFilter(QObject *object, QEvent *event) {
    if (object == dimensionEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                closeDimensionEditor();
                setFocus();
                return true;
            }
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                if(dimensionEditor->text()==dimensionEditor->property("originalDimensionText").toString()) {
                    closeDimensionEditor();setFocus();return true;
                }
                bool ok = false;
                double value =
                    dimensionEditor->text().trimmed().replace(',', '.').toDouble(&ok) / dimensionMultiplier;
                QString error;
                const bool bound=model->get(dimensionFeature).p.value("expressions").toObject().contains(dimensionKey);
                if(onDimensionExpression && (dimensionKey.startsWith("constraint:") || bound || !ok)) {
                    QString formula=dimensionEditor->text().trimmed();
                    if(formula.isEmpty())error="Digite uma medida ou fórmula.";
                    else {
                        if(ok)formula+=" mm";
                        if(dimensionMultiplier!=1)formula=QString("(%1) / %2").arg(formula).arg(dimensionMultiplier);
                        error=onDimensionExpression(dimensionFeature,dimensionKey,formula);
                    }
                }
                else if (!ok || !std::isfinite(value) || value <= 1e-5 || value > 1e6)
                    error = "Digite uma medida positiva válida em mm.";
                else if (onDimensionEdit)
                    error = onDimensionEdit(dimensionFeature, dimensionKey, value);
                if (error.isEmpty()) {
                    closeDimensionEditor();
                    setFocus();
                } else if (dimensionEditor) {
                    dimensionEditor->setToolTip(error);
                    dimensionEditor->setStyleSheet("QLineEdit { background:#442c32; color:#ffe0bd; "
                                                   "border:1px solid #ee8c82; padding:3px; }");
                    dimensionEditor->selectAll();
                }
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut) {
            closeDimensionEditor();
            return false;
        }
    }
    return QOpenGLWidget::eventFilter(object, event);
}
void Viewport::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && onEditSketch) {
        auto id = pick(e->position());
        if (!id.isEmpty() && model->get(id).type == "sketch") {
            setTool({});
            onEditSketch(id);
            return;
        }
    }
    QOpenGLWidget::mouseDoubleClickEvent(e);
}
void Viewport::mousePressEvent(QMouseEvent *e) {
    setFocus();
    dimensionPressed = false;
    if (e->button() == Qt::LeftButton && onDimensionEdit) {
        for (const auto &target : dimensions)
            if (target.rect.contains(e->position())) {
                pendingDimension = target;
                dimensionPressed = true;
                last = pressed = e->position();
                sketchPressCandidate = sketchDragging = cubePressed = cubeDragging = draggingHandle = false;
                return;
            }
    }
    cubePressed = e->button() == Qt::LeftButton && !cubeDirectionAt(e->position()).isNull();
    cubeDragging = false;
    if (cubePressed) {
        magnetLabel.clear();
        magnetGuides.clear();
        cameraAnimation.stop();
        cubeStartYaw = yaw;
        cubeStartPitch = pitch;
        last = pressed = e->position();
        sketchPressCandidate = false;
        draggingHandle = false;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (sketchDragging)
        draft.clear();
    sketchDragging = false;
    sketchPressCandidate =
        sketchMode && draft.empty() &&
        (tool == "rectangle" || tool == "circle" || tool == "polyline" || tool == "polygon") &&
        e->button() == Qt::LeftButton && !e->modifiers().testFlag(Qt::AltModifier) &&
        navigationMode.isEmpty() && !handleActive && !moveHandleActive &&
        cubeDirectionAt(e->position()).isNull() &&
        !QRect(width() - 84, 100, 50, 20).contains(e->position().toPoint());
    last = pressed = e->position();
    areaCandidate = e->button() == Qt::LeftButton && tool.isEmpty() && !choosingPlane &&
                    !onAcceptCommand && !handleActive && !moveHandleActive && navigationMode.isEmpty() &&
                    !e->modifiers().testFlag(Qt::AltModifier) && pickDetail(pressed).feature.isEmpty();
    areaDragging = false;
    areaEnd = pressed;
    if (sketchPressCandidate)
        sketchPressPoint = sketchPoint(pressed);
    draggingHandle = handleActive && e->button() == Qt::LeftButton &&
                     QLineF(e->position(), project(handleOrigin + handleAxis * handleDistance)).length() < 18;
    draggingMoveFree = false;
    if (moveHandleActive && rotationMode && e->button() == Qt::LeftButton &&
        !e->modifiers().testFlag(Qt::AltModifier) && navigationMode.isEmpty()) {
        rotationCenter = project(handleOrigin + moveDistances);
        double radius = QLineF(e->position(), rotationCenter).length();
        if (std::abs(radius - 85) < 12) {
            draggingRotation = true;
            rotationMouseAngle =
                std::atan2(e->position().y() - rotationCenter.y(), e->position().x() - rotationCenter.x());
            setCursor(Qt::ClosedHandCursor);
        }
        return;
    }
    if (moveHandleActive && e->button() == Qt::LeftButton && navigationMode.isEmpty() &&
        !e->modifiers().testFlag(Qt::AltModifier)) {
        movePlaneNormal = movePlaneAt(e->position());
        bool overCenter = QLineF(e->position(), project(handleOrigin + moveDistances)).length() < 12;
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
        if (movePlaneNormal >= 0 || overCenter || (!draggingHandle && !selected.isEmpty() && objectSelected(pick(e->position())))) {
            draggingHandle = false;
            draggingMoveFree = true;
            moveStartDistances = moveDistances;
            moveDragInverse = matrix().inverted();
            setCursor(Qt::ClosedHandCursor);
        }
    }
}
void Viewport::mouseMoveEvent(QMouseEvent *e) {
    if (areaCandidate && e->buttons().testFlag(Qt::LeftButton)) {
        areaEnd = e->position();
        areaDragging = QLineF(pressed, areaEnd).length() > 4;
        hoveredDetail = {};
        update();
        return;
    }
    if (e->buttons() == Qt::NoButton && tool.isEmpty() && !choosingPlane && (!onAcceptCommand || commandSelectSubelements) &&
        cubeDirectionAt(e->position()).isNull()) {
        hoveredDetail = pickDetail(e->position());
        update();
    } else
        hoveredDetail = {};
    if (draggingRotation) {
        double angle =
            std::atan2(e->position().y() - rotationCenter.y(), e->position().x() - rotationCenter.x());
        rotationAngle -= std::remainder(angle - rotationMouseAngle, 2 * M_PI) * 180 / M_PI;
        rotationMouseAngle = angle;
        if (onRotateAngle)
            onRotateAngle(rotationAngle);
        update();
        return;
    }
    planeHover = e->position();
    if (dimensionPressed) {
        update();
        return;
    }
    for (const auto &target : dimensions) {
        if (target.rect.contains(planeHover) && e->buttons() == Qt::NoButton) {
            setCursor(Qt::IBeamCursor);
            setToolTip("Clique para editar a medida em mm");
            update();
            return;
        }
    }
    if (cubePressed) {
        auto movement = e->position() - pressed;
        if (cubeDragging || QLineF(pressed, e->position()).length() > 4) {
            cubeDragging = true;
            yaw = cubeStartYaw - movement.x() * .5;
            float elevation = cubeStartPitch + movement.y() * .5;
            if (std::abs(cubeStartPitch) > 89)
                elevation = std::copysign(90.f, cubeStartPitch) -
                            std::copysign(float(std::abs(movement.y()) * .5), cubeStartPitch);
            pitch = std::clamp(elevation, -89.f, 89.f);
            update();
        }
        last = e->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (moveHandleActive && rotationMode && e->buttons() == Qt::NoButton &&
        std::abs(QLineF(planeHover, project(handleOrigin + moveDistances)).length() - 85) < 12) {
        setCursor(Qt::OpenHandCursor);
        setToolTip("Arraste o anel para girar a peça no eixo " + rotationAxis);
        return;
    }
    if (moveHandleActive && !rotationMode && e->buttons() == Qt::NoButton && movePlaneAt(planeHover) >= 0) {
        setCursor(Qt::OpenHandCursor);
        setToolTip("Arraste para mover no plano " + QStringList{"YZ", "XZ", "XY"}[movePlaneAt(planeHover)]);
        update();
        return;
    }
    setToolTip(QRect(width() - 84, 100, 50, 20).contains(planeHover.toPoint()) ? "Vista inicial"
               : !cubeDirectionAt(planeHover).isNull()
                   ? "Arraste para orbitar; clique para escolher uma vista"
                   : QString());
    bool overCube = !cubeDirectionAt(planeHover).isNull() ||
                    QRect(width() - 84, 100, 50, 20).contains(planeHover.toPoint());
    setCursor(!cubeDirectionAt(planeHover).isNull() ? Qt::OpenHandCursor
              : overCube                            ? Qt::PointingHandCursor
              : tool.isEmpty()                      ? Qt::ArrowCursor
                                                    : Qt::CrossCursor);
    auto delta = e->position() - last;
    last = e->position();
    const bool drawing = !overCube && sketchMode && !tool.isEmpty() && navigationMode.isEmpty() &&
                         !e->buttons().testFlag(Qt::MiddleButton) &&
                         !e->modifiers().testFlag(Qt::AltModifier);
    cursor = drawing ? sketchPoint(e->position()) : planeAt(e->position());
    if (!drawing) {
        magnetLabel.clear();
        magnetGuides.clear();
    }
    if (draggingMoveFree) {
        auto movement = e->position() - pressed;
        auto deltaWorld =
            moveDragInverse.map(QVector3D(2 * movement.x() / width(), -2 * movement.y() / height(), 0)) -
            moveDragInverse.map(QVector3D());
        if (movePlaneNormal >= 0) {
            const auto direction = (moveDragInverse.map(QVector3D(0,0,1)) -
                                    moveDragInverse.map(QVector3D(0,0,-1))).normalized();
            if (std::abs(direction[movePlaneNormal]) <= .05) return;
            deltaWorld -= direction * (deltaWorld[movePlaneNormal] / direction[movePlaneNormal]);
            deltaWorld[movePlaneNormal] = 0;
        }
        moveDistances = moveStartDistances + deltaWorld;
        if (snap)
            for (int axis = 0; axis < 3; ++axis)
                if (axis != movePlaneNormal)
                    moveDistances[axis] = std::round(moveDistances[axis] * 10) / 10;
        if (onMoveTranslation)
            onMoveTranslation(moveDistances);
        setCursor(Qt::ClosedHandCursor);
        update();
        return;
    }
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
    if (sketchPressCandidate && (middle || alternative || toolbarNavigation)) {
        if (sketchDragging)
            draft.clear();
        sketchPressCandidate = sketchDragging = false;
    }
    if (sketchPressCandidate && !sketchDragging && QLineF(pressed, e->position()).length() > 4) {
        draft.append(sketchPressPoint);
        sketchDragging = true;
    }
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
    if (areaCandidate && e->button() == Qt::LeftButton) {
        areaCandidate = false;
        bool dragged = QLineF(pressed, e->position()).length() > 4;
        areaDragging = false;
        if (dragged) {
            auto targets = pickArea(QRectF(pressed, e->position()).normalized(), e->position().x() < pressed.x());
            if (e->modifiers().testFlag(Qt::ShiftModifier)) {
                for (const auto &previous : selectedDetails) {
                    bool exists = std::any_of(targets.begin(), targets.end(), [&](const SelectionTarget &item) {
                        return item.feature == previous.feature && item.kind == previous.kind && item.index == previous.index;
                    });
                    if (!exists)
                        targets.append(previous);
                }
            }
            auto target = targets.empty() ? SelectionTarget{} : targets.back();
            selected = target.feature;
            if (onSelect)
                onSelect(selected);
            selectedDetails = targets;
            selectedDetail = target;
            update();
            return;
        }
    }
    if (draggingRotation) {
        draggingRotation = false;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (draggingMoveFree) {
        draggingMoveFree = false;
        setCursor(Qt::OpenHandCursor);
        update();
        return;
    }
    if (dimensionPressed) {
        dimensionPressed = false;
        if (QLineF(pressed, e->position()).length() <= 4)
            editDimension(pendingDimension);
        return;
    }
    if (cubePressed || cubeDragging) {
        bool wasDragged = cubeDragging;
        cubePressed = cubeDragging = false;
        setCursor(cubeDirectionAt(e->position()).isNull() ? Qt::ArrowCursor : Qt::OpenHandCursor);
        if (wasDragged) {
            update();
            return;
        }
    }
    bool sketchDrag = sketchPressCandidate && e->button() == Qt::LeftButton &&
                      !e->modifiers().testFlag(Qt::AltModifier) &&
                      QLineF(pressed, e->position()).length() > 4;
    if (sketchDrag && !sketchDragging)
        draft.append(sketchPressPoint);
    if (!sketchDrag && sketchDragging)
        draft.clear();
    sketchPressCandidate = sketchDragging = false;
    if (draggingHandle) {
        draggingHandle = false;
        return;
    }
    if ((!sketchDrag && QLineF(pressed, e->position()).length() > 4) || e->button() != Qt::LeftButton)
        return;
    auto s = e->position();
    auto direction = cubeDirectionAt(s);
    if (!sketchDrag && !direction.isNull()) {
        viewDirection(direction);
        return;
    }
    if (!sketchDrag && QRect(width() - 84, 100, 50, 20).contains(s.toPoint())) {
        view("iso", true);
        return;
    }
    if (choosingPlane) {
        QString name;
        sketchSupport = {};
        double offset = 0;
        auto target = pickDetail(s, true);
        if (!target.feature.isEmpty() && target.index >= 0) {
            try {
                if (model->isMesh(target.feature)) {
                    if (onHint) onHint("STL não possui faces CAD. Selecione um plano de origem.");
                    return;
                }
                name = Model::facePlane(model->get(target.feature).shape, target.index);
                sketchSupport = target;
            } catch (const std::exception &error) {
                if (onHint) onHint(QString::fromUtf8(error.what()));
                return;
            } catch (const Standard_Failure &) {
                if (onHint) onHint("Selecione uma face plana CAD.");
                return;
            }
        }
        if (name.isEmpty())
            for (auto region = planeRegions.crbegin(); region != planeRegions.crend(); ++region)
                if (region->second.containsPoint(s, Qt::OddEvenFill)) {
                    name = region->first;
                    break;
                }
        if (!name.isEmpty() && onPlaneChosen) {
            choosingPlane = false;
            onPlaneChosen(name, offset);
        }
        return;
    }
    if (sketchMode && tool == "trim") {
        auto trimTarget = pickDetail(s, true);
        if (!trimTarget.feature.isEmpty() && trimTarget.kind == "edge" && onTrimSketch)
            onTrimSketch(trimTarget);
        return;
    }
    if (sketchMode && !tool.isEmpty()) {
        cursor = sketchPoint(s);
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
            if (std::abs(b.x() - a.x()) < 1e-5 || std::abs(b.y() - a.y()) < 1e-5) {
                draft.resize(1);
                update();
                return;
            }
            submit({{"profile", "rectangle"},
                    {"x", std::min(a.x(), b.x())},
                    {"y", std::min(a.y(), b.y())},
                    {"w", std::abs(b.x() - a.x())},
                    {"h", std::abs(b.y() - a.y())}});
        } else if (draft.size() == 2 && tool == "polygon") {
            QJsonArray points;
            for (auto point : regularPolygon(draft[0], draft[1]))
                points.append(QJsonArray{point.x(), point.y()});
            submit({{"profile", "polyline"}, {"points", points}, {"closed", true}});
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
    auto target = pickDetail(s, bool(onAcceptCommand) && !commandSelectSubelements);
    auto targets = selectedDetails;
    if (targets.empty() && !selectedDetail.feature.isEmpty())
        targets.append(selectedDetail);
    if ((!onAcceptCommand || commandSelectSubelements) && e->modifiers().testFlag(Qt::ShiftModifier)) {
        if (target.feature.isEmpty())
            return;
        auto found = std::find_if(targets.begin(), targets.end(), [&](const SelectionTarget &item) {
            return item.feature == target.feature && item.kind == target.kind && item.index == target.index;
        });
        if (found == targets.end())
            targets.append(target);
        else
            targets.erase(found);
    } else {
        targets.clear();
        if (!target.feature.isEmpty())
            targets.append(target);
    }
    if (!onAcceptCommand || commandSelectSubelements)
        target = targets.empty() ? SelectionTarget{} : targets.back();
    selected = target.feature;
    if(commandSelectSubelements) {
        selectedDetail=target;selectedDetails=targets;
    }
    if (onSelect)
        onSelect(selected);
    if (!onAcceptCommand) {
        selectedDetail = target;
        selectedDetails = targets;
    }
    update();
}
void Viewport::leaveEvent(QEvent *event) {
    hoveredDetail = {};
    update();
    QOpenGLWidget::leaveEvent(event);
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
    if (sketchMode && tool == "polygon" && (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)) {
        polygonSides = std::clamp(polygonSides + (e->key() == Qt::Key_Up ? 1 : -1), 3, 64);
        update();
        return;
    }
    if (e->key() == Qt::Key_Escape) {
        draggingRotation = false;
        draggingMoveFree = draggingHandle = false;
        if (cubePressed) {
            cubePressed = false;
            cubeDragging = true;
        }
        cameraAnimation.stop();
        draft.clear();
        choosingPlane = false;
        if (onCancelCommand)
            onCancelCommand();
        else if (tool.isEmpty()) {
            selected.clear();
            if (onSelect)
                onSelect({});
        }
        setTool({});
    } else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        draggingRotation = false;
        draggingMoveFree = draggingHandle = false;
        if (onAcceptCommand) {
            onAcceptCommand();
            return;
        }
        if (tool == "polyline")
            finishPolyline(e->modifiers().testFlag(Qt::ShiftModifier));
    } else
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
