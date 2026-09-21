#include "viewport.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <limits>
#include <QPainterPath>
#include <QMap>
#include <QSet>

QVector<Viewport::SelectionTarget> Viewport::pickArea(QRectF area, bool crossing) const {
    QVector<SelectionTarget> result;
    QPainterPath region;
    region.addRect(area);
    auto matches = [&](const QVector<QVector3D> &points, bool closed) {
        if (points.empty())
            return false;
        bool inside = true;
        QPainterPath path;
        path.moveTo(project(points.front()));
        for (const auto &point : points) {
            auto pixel = project(point);
            inside = inside && area.contains(pixel);
            path.lineTo(pixel);
        }
        if (closed)
            path.closeSubpath();
        return inside || (crossing && (path.intersects(region) || path.contains(area.center())));
    };
    for (const auto &feature : model->features) {
        if (feature.inactive || !feature.visible || feature.type == "remove" || model->consumed(feature.id))
            continue;
        if (selectionFilter == "face") {
            if (model->isMesh(feature.id)) continue;
            QMap<int, QVector<QVector3D>> faces;
            QSet<int> crossed;
            for (const auto &triangle : mesh) if (model->features[triangle.feature].id == feature.id) {
                QVector<QVector3D> points{triangle.a, triangle.b, triangle.c};
                faces[triangle.face] += points;
                if (matches(points, true)) crossed.insert(triangle.face);
            }
            for (auto it = faces.cbegin(); it != faces.cend(); ++it)
                if (crossing ? crossed.contains(it.key()) : matches(it.value(), false))
                    result.append({feature.id, "face", it.key(), {}});
            continue;
        }
        if (selectionFilter == "vertex") {
            if (model->isMesh(feature.id))
                continue;
            TopTools_IndexedMapOfShape vertices;
            TopExp::MapShapes(feature.shape, TopAbs_VERTEX, vertices);
            for (int i = 1; i <= vertices.Extent(); ++i) {
                auto p = BRep_Tool::Pnt(TopoDS::Vertex(vertices(i)));
                QVector3D point(p.X(), p.Y(), p.Z());
                if (area.contains(project(point)))
                    result.append({feature.id, "vertex", i - 1, {point}});
            }
            continue;
        }
        QVector<QVector3D> all;
        bool hit = false;
        if (!model->isMesh(feature.id)) {
            TopTools_IndexedMapOfShape edges;
            TopExp::MapShapes(feature.shape, TopAbs_EDGE, edges);
            for (int i = 1; i <= edges.Extent(); ++i) {
                auto edge = TopoDS::Edge(edges(i));
                if (BRep_Tool::Degenerated(edge))
                    continue;
                BRepAdaptor_Curve curve(edge);
                if (!std::isfinite(curve.FirstParameter()) || !std::isfinite(curve.LastParameter()))
                    continue;
                QVector<QVector3D> points;
                int steps = curve.GetType() == GeomAbs_Line ? 1 : 128;
                for (int j = 0; j <= steps; ++j) {
                    auto p = curve.Value(curve.FirstParameter() + (curve.LastParameter() - curve.FirstParameter()) * j / steps);
                    points.append({float(p.X()), float(p.Y()), float(p.Z())});
                }
                bool match = matches(points, false);
                if (selectionFilter == "edge" && match)
                    result.append({feature.id, "edge", i - 1, points});
                hit = hit || match;
                all += points;
            }
        }
        if (selectionFilter == "edge")
            continue;
        for (const auto &triangle : mesh) {
            if (model->features[triangle.feature].id != feature.id)
                continue;
            QVector<QVector3D> points{triangle.a, triangle.b, triangle.c};
            hit = hit || matches(points, true);
            all += points;
        }
        if ((!crossing && matches(all, false)) || (crossing && hit))
            result.append({feature.id, "object", -1, {}});
    }
    return result;
}

Viewport::SelectionTarget Viewport::pickDetail(QPointF pixel, bool objectOnly) const {
    const auto projection = matrix();
    auto depth = [&](QVector3D point) { return projection.map(point).z(); };
    auto surface = [&](QPointF position) {
        SelectionTarget result;
        float closest = std::numeric_limits<float>::max();
        QVector3D origin, direction;
        ray(position, origin, direction);
        for (const auto &triangle : mesh) {
            auto e1 = triangle.b - triangle.a, e2 = triangle.c - triangle.a;
            auto h = QVector3D::crossProduct(direction, e2);
            float determinant = QVector3D::dotProduct(e1, h);
            if (std::abs(determinant) < 1e-10)
                continue;
            auto s = origin - triangle.a;
            float u = QVector3D::dotProduct(s, h) / determinant;
            auto q = QVector3D::crossProduct(s, e1);
            float v = QVector3D::dotProduct(direction, q) / determinant;
            float distance = QVector3D::dotProduct(e2, q) / determinant;
            if (u < -1e-6 || v < -1e-6 || u + v > 1.000001 || distance < 0 || distance >= closest)
                continue;
            closest = distance;
            result = {model->features[triangle.feature].id, "object", triangle.face, {origin + direction * distance}};
        }
        return result;
    };
    auto object = surface(pixel);
    float objectDepth = object.feature.isEmpty() ? 2.f : depth(object.geometry[0]);
    auto visible = [&](QVector3D point) {
        auto front = surface(project(point));
        return front.feature.isEmpty() || depth(point) <= depth(front.geometry[0]) + 1e-5f;
    };
    SelectionTarget bestVertex, bestEdge;
    double vertexDistance = 9, edgeDistance = 7;
    float vertexDepth = 2, edgeDepth = 2;
    const bool wantVertex = !objectOnly && (selectionFilter == "auto" || selectionFilter == "vertex");
    const bool wantEdge =
        objectOnly || selectionFilter == "object" || selectionFilter == "auto" || selectionFilter == "edge";
    for (const auto &feature : model->features) {
        bool sketch = feature.type == "sketch";
        bool eligible = !feature.inactive && feature.type != "remove" && feature.visible &&
                        (!model->consumed(feature.id) || (sketch && sketchMode && feature.id == selected));
        if (!eligible || (!sketch && model->isMesh(feature.id)))
            continue;
        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(feature.shape, TopAbs_EDGE, edges);
        if (wantVertex) {
            TopTools_IndexedMapOfShape points;
            TopExp::MapShapes(feature.shape, TopAbs_VERTEX, points);
            for (int i = 1; i <= points.Extent(); ++i) {
                auto p = BRep_Tool::Pnt(TopoDS::Vertex(points(i)));
                QVector3D point(p.X(), p.Y(), p.Z());
                double distance = QLineF(pixel, project(point)).length();
                float z = depth(point);
                if (distance <= vertexDistance && (distance < vertexDistance - .1 || z < vertexDepth) &&
                    visible(point)) {
                    vertexDistance = distance;
                    vertexDepth = z;
                    bestVertex = {feature.id, "vertex", i - 1, {point}};
                }
            }
        }
        for (int i = 1; wantEdge && i <= edges.Extent(); ++i) {
            auto edge = TopoDS::Edge(edges(i));
            if (BRep_Tool::Degenerated(edge))
                continue;
            BRepAdaptor_Curve curve(edge);
            if (!std::isfinite(curve.FirstParameter()) || !std::isfinite(curve.LastParameter()))
                continue;
            const int steps = curve.GetType() == GeomAbs_Line ? 1 : 128;
            QVector<QVector3D> points;
            for (int j = 0; j <= steps; ++j) {
                auto p = curve.Value(curve.FirstParameter() +
                                     (curve.LastParameter() - curve.FirstParameter()) * j / steps);
                points.append({float(p.X()), float(p.Y()), float(p.Z())});
            }
            for (int j = 1; j < points.size(); ++j) {
                auto a = project(points[j - 1]), b = project(points[j]), segment = b - a;
                double squaredLength = QPointF::dotProduct(segment, segment);
                if (squaredLength < 1e-10)
                    continue;
                double t = std::clamp(QPointF::dotProduct(pixel - a, segment) / squaredLength, 0., 1.);
                double distance = QLineF(pixel, a + segment * t).length();
                auto point = points[j - 1] * (1 - t) + points[j] * t;
                float z = depth(point);
                if (distance <= edgeDistance && (distance < edgeDistance - .1 || z < edgeDepth) &&
                    visible(point)) {
                    edgeDistance = distance;
                    edgeDepth = z;
                    bestEdge = {feature.id, "edge", i - 1, points};
                }
            }
        }
        if (!sketch)
            continue;
        if (!objectOnly && selectionFilter == "face")
            continue;
        // Profile interiors compete by depth with solids, never by creation order.
        const auto &p = feature.p;
        QPolygonF polygon;
        const auto planeName = p["plane"].toString("XY");
        auto add = [&](double u, double v) {
            polygon << project(Model::planePoint(planeName, u, v, p["offset"].toDouble()));
        };
        double x = p["x"].toDouble(), y = p["y"].toDouble();
        if (p["profile"] == "rectangle") {
            double w = p["w"].toDouble(), h = p["h"].toDouble();
            add(x, y);
            add(x + w, y);
            add(x + w, y + h);
            add(x, y + h);
        } else if (p["profile"] == "circle") {
            for (int j = 0; j < 128; ++j)
                add(x + p["r"].toDouble() * std::cos(j * 2 * M_PI / 128),
                    y + p["r"].toDouble() * std::sin(j * 2 * M_PI / 128));
        } else if (p["profile"] == "polyline" && p["closed"].toBool()) {
            for (auto vertex : p["points"].toArray()) {
                auto pair = vertex.toArray();
                add(pair[0].toDouble(), pair[1].toDouble());
            }
        }
        if (polygon.size() < 3 || !polygon.containsPoint(pixel, Qt::OddEvenFill))
            continue;
        QVector3D origin, direction;
        ray(pixel, origin, direction);
        auto normal = Model::planeNormal(planeName);
        double denominator = QVector3D::dotProduct(direction, normal);
        if (std::abs(denominator) < 1e-8)
            continue;
        auto planeOrigin = Model::planePoint(planeName, 0, 0, p["offset"].toDouble());
        auto point = origin + direction * (QVector3D::dotProduct(planeOrigin - origin, normal) / denominator);
        float z = depth(point);
        if (z <= objectDepth + 1e-6f) {
            objectDepth = z;
            object = {feature.id, "object", -1, {}};
        }
    }
    if (!bestVertex.feature.isEmpty())
        return bestVertex;
    if (!bestEdge.feature.isEmpty()) {
        if (objectOnly || selectionFilter == "object")
            return {bestEdge.feature, "object", -1, {}};
        return bestEdge;
    }
    if (!objectOnly && (selectionFilter == "auto" || selectionFilter == "face") && object.index >= 0 &&
        !model->isMesh(object.feature)) {
        object.kind = "face";
        return object;
    }
    if (objectOnly || selectionFilter == "auto" || selectionFilter == "object")
        return object;
    return {};
}
