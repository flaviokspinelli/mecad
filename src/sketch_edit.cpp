#include "model.h"
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <QSet>
#include <QPointF>
#include <stdexcept>

void Model::editSketchElements(const QString &id, const QVector<int> &edgeIndices,
                               const QVector<int> &vertexIndices, QVector3D delta, bool erase) {
    const auto &feature = get(id);
    auto parameters = feature.p;
    const auto kind = parameters["profile"].toString();
    if (feature.type != "sketch" || (kind != "rectangle" && kind != "polyline"))
        throw std::runtime_error("Edição de subelementos disponível em retângulos, polígonos e linhas de sketch. Curvas e arestas de sólidos ainda não são editáveis.");
    QVector<QPointF> points;
    bool closed = kind == "rectangle" || parameters["closed"].toBool();
    if (kind == "rectangle") {
        double x = parameters["x"].toDouble(), y = parameters["y"].toDouble();
        double w = parameters["w"].toDouble(), h = parameters["h"].toDouble();
        points = {{x,y}, {x+w,y}, {x+w,y+h}, {x,y+h}};
    } else {
        for (auto value : parameters["points"].toArray()) {
            auto p = value.toArray();
            points.append({p[0].toDouble(), p[1].toDouble()});
        }
    }
    auto plane = parameters.value("plane").toString("XY");
    auto locate = [&](const TopoDS_Vertex &vertex) {
        auto p = BRep_Tool::Pnt(vertex);
        QVector3D world(p.X(), p.Y(), p.Z());
        for (int i = 0; i < points.size(); ++i)
            if ((planePoint(plane, points[i].x(), points[i].y(), parameters.value("offset").toDouble()) - world).length() < 1e-4)
                return i;
        throw std::runtime_error("A seleção mudou. Selecione os elementos novamente.");
    };
    TopTools_IndexedMapOfShape edges, vertices;
    TopExp::MapShapes(feature.shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(feature.shape, TopAbs_VERTEX, vertices);
    QSet<int> affected, removedEdges, removedVertices;
    int count = closed ? points.size() : points.size()-1;
    for (int index : vertexIndices) {
        if (index < 0 || index >= vertices.Extent())
            throw std::runtime_error("Vértice inválido.");
        int point = locate(TopoDS::Vertex(vertices(index+1)));
        affected.insert(point);
        removedVertices.insert(point);
    }
    for (int index : edgeIndices) {
        if (index < 0 || index >= edges.Extent())
            throw std::runtime_error("Aresta inválida.");
        TopoDS_Vertex a,b;
        TopExp::Vertices(TopoDS::Edge(edges(index+1)), a,b);
        int first = locate(a), last = locate(b);
        affected.insert(first);
        affected.insert(last);
        for (int i = 0; i < count; ++i) {
            int next = (i+1)%points.size();
            if ((i == first && next == last) || (i == last && next == first))
                removedEdges.insert(i);
        }
    }
    if (affected.empty())
        return;
    if (erase && parameters.contains("constraintSystem"))
        throw std::runtime_error("Exclusão de subelementos de sketches paramétricos ainda não disponível. O contorno e seus vínculos foram preservados.");
    Model work = *this;
    auto serialize = [&](const QVector<QPointF> &chain, bool loop) {
        QJsonObject p = parameters;
        p["profile"] = "polyline";
        p["closed"] = loop;
        QJsonArray values;
        for (auto point : chain)
            values.append(QJsonArray{point.x(), point.y()});
        p["points"] = values;
        for (auto key : {"x", "y", "w", "h"})
            p.remove(key);
        return p;
    };
    if (!erase) {
        QPointF local = planeCoordinates(plane, planePoint(plane, 0, 0) + delta);
        if (local.manhattanLength() < 1e-8)
            return;
        for (int i : affected)
            points[i] += local;
        work.edit(id, serialize(points, closed), feature.name);
    } else {
        for (int i = 0; i < count; ++i)
            if (removedVertices.contains(i) || removedVertices.contains((i+1)%points.size()))
                removedEdges.insert(i);
        if (removedEdges.size() == count) {
            work.remove(id);
        } else {
            QVector<QVector<QPointF>> chains;
            QVector<QPointF> chain;
            int start = closed ? (*removedEdges.begin()+1)%count : 0;
            for (int step = 0; step < count; ++step) {
                int i = (start+step)%count;
                if (removedEdges.contains(i)) {
                    if (!chain.empty()) chains.append(chain);
                    chain.clear();
                } else {
                    if (chain.empty()) chain.append(points[i]);
                    chain.append(points[(i+1)%points.size()]);
                }
            }
            if (!chain.empty()) chains.append(chain);
            work.edit(id, serialize(chains.front(), false), feature.name);
            for (int i = 1; i < chains.size(); ++i)
                work.add("sketch", serialize(chains[i], false), feature.name + " · trecho");
        }
    }
    commit(work.json());
}
