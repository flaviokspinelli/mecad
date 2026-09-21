#include "model.h"
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <QUuid>
#include <QLineF>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace {
void requireSketch(bool ok, const QString &message) {
    if (!ok) throw std::runtime_error(message.toStdString());
}
QJsonObject parameters(const Feature &feature, const sketch::System &system) {
    auto p = feature.p;
    p["closed"] = p["profile"] == "rectangle" || p["closed"].toBool();
    p["profile"] = "polyline";
    p["constraintSystem"] = system.json();
    for (const auto *key : {"x","y","w","h"}) p.remove(key);
    return p;
}
double cross(QPointF a, QPointF b, QPointF c) {
    const auto u=b-a, v=c-a; return u.x()*v.y()-u.y()*v.x();
}
bool onSegment(QPointF a,QPointF b,QPointF c) {
    return std::abs(cross(a,b,c)) < 1e-9 && c.x() >= std::min(a.x(),b.x())-1e-9 &&
        c.x() <= std::max(a.x(),b.x())+1e-9 && c.y() >= std::min(a.y(),b.y())-1e-9 && c.y() <= std::max(a.y(),b.y())+1e-9;
}
bool intersects(QPointF a,QPointF b,QPointF c,QPointF d) {
    const double x=cross(a,b,c), y=cross(a,b,d), z=cross(c,d,a), w=cross(c,d,b);
    return ((x>0 && y<0 || x<0 && y>0) && (z>0 && w<0 || z<0 && w>0)) ||
        onSegment(a,b,c) || onSegment(a,b,d) || onSegment(c,d,a) || onSegment(c,d,b);
}
}
sketch::System Model::sketchSystem(const QString &id) const {
    const auto &f=get(id);
    requireSketch(f.type=="sketch", "Selecione elementos de um sketch.");
    if (f.p.contains("constraintSystem")) return sketch::System::fromJson(f.p["constraintSystem"].toObject());
    const auto p=f.p;
    const bool rectangle=p["profile"]=="rectangle";
    requireSketch(rectangle || p["profile"]=="polyline", "Restrições disponíveis para linhas e polígonos; curvas ainda não suportadas.");
    QVector<QPointF> points;
    if (rectangle) {
        const double x=p["x"].toDouble(), y=p["y"].toDouble(), w=p["w"].toDouble(40), h=p["h"].toDouble(30);
        points={{x,y},{x+w,y},{x+w,y+h},{x,y+h}};
    } else for (auto v:p["points"].toArray()) {
        const auto a=v.toArray(); points.append({a[0].toDouble(),a[1].toDouble()});
    }
    sketch::System system;
    for (int i=0;i<points.size();++i) system.points.append({QString("p%1").arg(i),points[i]});
    const int count=(rectangle || p["closed"].toBool()) ? points.size() : points.size()-1;
    for (int i=0;i<count;++i) system.lines.append({QString("e%1").arg(i),system.points[i].id,system.points[(i+1)%points.size()].id});
    if (rectangle) for(int i=0;i<4;++i)
        system.constraints.append({QString("rectangle%1").arg(i),i%2 ? sketch::Relation::Vertical : sketch::Relation::Horizontal,system.lines[i].id,{},{}});
    system.validate(); return system;
}
void Model::resolveSketch(Feature &f) {
    auto system=sketch::System::fromJson(f.p["constraintSystem"].toObject()).solved();
    const int n=system.points.size(), count=f.p["closed"].toBool() ? n : n-1;
    requireSketch(n>=2 && (!f.p["closed"].toBool() || n>=3) && system.lines.size()==count,
                  "O sketch restrito exige um contorno simples conectado.");
    QJsonArray points;
    for (int i=0;i<n;++i) {
        auto a=system.points[i].position;
        for(int j=0;j<i;++j) requireSketch(QLineF(a,system.points[j].position).length()>1e-5,
            "A restrição colapsa pontos do contorno. Nenhuma alteração foi aplicada.");
        points.append(QJsonArray{a.x(),a.y()});
    }
    for (int i=0;i<count;++i) {
        requireSketch(!system.lines[i].construction && system.lines[i].start==system.points[i].id &&
                      system.lines[i].end==system.points[(i+1)%n].id, "Conectividade de sketch incompatível.");
        for (int j=i+1;j<count;++j) {
            if(j==i+1 || (f.p["closed"].toBool() && i==0 && j==count-1)) continue;
            requireSketch(!intersects(system.points[i].position,system.points[(i+1)%n].position,
                                     system.points[j].position,system.points[(j+1)%n].position),
                          "A restrição cria um contorno auto-intersectante.");
        }
    }
    for(int i=0;i<(f.p["closed"].toBool() ? n : n-2);++i) {
        const auto a=system.points[i].position,b=system.points[(i+1)%n].position,c=system.points[(i+2)%n].position;
        const auto u=b-a,v=c-b;
        requireSketch(std::abs(cross(a,b,c))>1e-9 || u.x()*v.x()+u.y()*v.y()>=0,
                      "A restrição sobrepõe segmentos consecutivos.");
    }
    if(f.p["closed"].toBool()) {
        double twiceArea=0;
        const auto origin=system.points[0].position;
        for(int i=1;i<n-1;++i) twiceArea+=cross(origin,system.points[i].position,system.points[i+1].position);
        requireSketch(std::abs(twiceArea)>1e-10,"A restrição produz um perfil fechado sem área.");
    }
    f.p["constraintSystem"]=system.json(); f.p["points"]=points;
}
QString Model::sketchEntityId(const QString &id,const QString &kind,int index) const {
    const auto &f=get(id); const auto system=sketchSystem(id);
    auto locate=[&](const TopoDS_Vertex &vertex) {
        auto q=BRep_Tool::Pnt(vertex);
        for (const auto &p:system.points) {
            auto world=planePoint(f.p["plane"].toString("XY"),p.position.x(),p.position.y(),f.p["offset"].toDouble());
            if ((world-QVector3D(q.X(),q.Y(),q.Z())).length()<1e-4) return p.id;
        }
        throw std::runtime_error("A seleção do sketch mudou; selecione novamente.");
    };
    requireSketch(kind=="edge" || kind=="vertex", "Selecione uma linha ou um vértice do sketch.");
    TopTools_IndexedMapOfShape topology;
    TopExp::MapShapes(f.shape,kind=="edge" ? TopAbs_EDGE : TopAbs_VERTEX,topology);
    requireSketch(index>=0 && index<topology.Extent(), "Índice de seleção inválido.");
    if(kind=="vertex") return locate(TopoDS::Vertex(topology(index+1)));
    TopoDS_Vertex a,b; TopExp::Vertices(TopoDS::Edge(topology(index+1)),a,b);
    auto first=locate(a), second=locate(b);
    for (const auto &line:system.lines)
        if ((line.start==first && line.end==second) || (line.start==second && line.end==first)) return line.id;
    throw std::runtime_error("Aresta sem entidade de sketch correspondente.");
}
QString Model::constrainSketch(const QString &id,sketch::Relation relation,const QString &first,const QString &second,QPointF value) {
    auto system=sketchSystem(id);
    auto constraint=QUuid::createUuid().toString(QUuid::WithoutBraces);
    system.constraints.append({constraint,relation,first,second,value});
    const auto &f=get(id); edit(id,parameters(f,system),f.name); return constraint;
}
void Model::removeSketchConstraint(const QString &id,const QString &constraint) {
    auto system=sketchSystem(id);
    auto old=system.constraints.size();
    system.constraints.erase(std::remove_if(system.constraints.begin(),system.constraints.end(),
        [&](const auto &c){return c.id==constraint;}),system.constraints.end());
    requireSketch(old!=system.constraints.size(), "Restrição inexistente.");
    const auto &f=get(id); edit(id,parameters(f,system),f.name);
}
