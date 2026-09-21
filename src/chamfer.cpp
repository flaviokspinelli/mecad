#include "model.h"
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>

namespace {
void requireChamfer(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
double distance(const QJsonObject &p,const char *key) {
    const auto q=p[key];double n=q.toDouble();
    requireChamfer(q.isDouble() && std::isfinite(n) && n>1e-5 && n<=1e6,"Distância de chanfro inválida.");
    return n;
}
}
TopoDS_Shape Model::chamferShape(const TopoDS_Shape &source,const QJsonObject &p) {
    TopTools_IndexedMapOfShape edges;TopExp::MapShapes(source,TopAbs_EDGE,edges);
    requireChamfer(p["edges"].isArray() && !p["edges"].toArray().empty(),"Selecione ao menos uma aresta para chanfrar.");
    if(p.contains("sourceEdgeCount"))
        requireChamfer(p["sourceEdgeCount"].isDouble() && p["sourceEdgeCount"].toDouble()==edges.Extent(),
                       "A topologia do corpo mudou. Selecione novamente as arestas do chanfro.");
    std::set<int> indices;
    for(auto v:p["edges"].toArray()) {
        int i=v.toInt(-1);
        requireChamfer(v.isDouble() && v.toDouble()==i && i>=0 && i<edges.Extent(),"Aresta de chanfro inválida.");
        indices.insert(i);
    }
    const auto mode=p["mode"].toString("equal");
    requireChamfer(mode=="equal" || mode=="two" || mode=="angle","Modo de chanfro inválido.");
    const auto side=p["side"].toString("first");
    requireChamfer(side=="first" || side=="second","Lado do chanfro inválido.");
    const double d=distance(p,"d"),d2=mode=="two"?distance(p,"d2"):d;
    double angle=p["angle"].toDouble();
    if(mode=="angle")requireChamfer(p["angle"].isDouble() && std::isfinite(angle) && angle>0 && angle<90,
                                   "O ângulo deve ser maior que 0 e menor que 90 graus.");
    TopTools_IndexedDataMapOfShapeListOfShape adjacency;
    TopExp::MapShapesAndAncestors(source,TopAbs_EDGE,TopAbs_FACE,adjacency);
    BRepFilletAPI_MakeChamfer operation(source);
    for(int i:indices) {
        const auto edge=TopoDS::Edge(edges(i+1));
        requireChamfer(adjacency.Contains(edge),"Aresta sem face de suporte.");
        TopTools_IndexedMapOfShape faces;
        for(TopTools_ListIteratorOfListOfShape it(adjacency.FindFromKey(edge));it.More();it.Next()) faces.Add(it.Value());
        requireChamfer(faces.Extent()==2,"O chanfro exige uma aresta entre duas faces distintas.");
        const auto face=TopoDS::Face(faces(side=="first"?1:2));
        if(mode=="equal")operation.Add(d,edge);
        else if(mode=="two")operation.Add(d,d2,edge,face);
        else operation.AddDA(d,angle*std::numbers::pi/180,edge,face);
    }
    operation.Build();
    requireChamfer(operation.IsDone(),"Não foi possível construir o chanfro. Reduza as distâncias ou altere a seleção.");
    return operation.Shape();
}
