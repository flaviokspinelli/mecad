#include "model.h"
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <Poly_Triangulation.hxx>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUuid>
#include <RWStl.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <StlAPI_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <cmath>
#include <gp_Circ.hxx>
#include <sstream>
#include <set>
#include <stdexcept>

namespace {
void require(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
double value(const QJsonObject &p, const char *k, double fallback = 0) {
    auto v = p.value(k);
    require(v.isUndefined() || v.isDouble(), QString("Parâmetro inválido: %1").arg(k));
    double n = v.toDouble(fallback);
    require(std::isfinite(n) && std::abs(n) <= 1e6, "Valor fora do intervalo permitido.");
    return n;
}
double positive(const QJsonObject &p, const char *k, double fallback = 1) {
    double n = value(p, k, fallback);
    require(n > 1e-5, "Dimensões devem ser positivas.");
    return n;
}
void validateDocument(const QJsonObject &root) {
    require(root["format"] == "MecaCAD" && root["version"].isDouble() &&
                (root["version"].toDouble() == 1 || root["version"].toDouble() == 2 || root["version"].toDouble() == 3) && root["units"] == "mm",
            "Formato, versão ou unidade de projeto não suportado.");
    const QSet<QString> rootKeys = {"format", "version", "units", "features", "namedParameters"};
    if(root.contains("namedParameters")) {
        require(root["version"].toInt()==3 && root["namedParameters"].isObject(), "Parâmetros nomeados exigem documento v3.");
        const auto definitions=root["namedParameters"].toObject();
        for(auto it=definitions.begin();it!=definitions.end();++it)
            require(it.value().isString(),"A fórmula do parâmetro deve ser texto.");
    }
    for (auto it = root.begin(); it != root.end(); ++it)
        require(rootKeys.contains(it.key()), "O projeto contém dados não suportados: " + it.key());
    require(root["features"].isArray() && root["features"].toArray().size() <= 2000,
            "Lista de operações inválida.");
    const QSet<QString> types = {"box", "cylinder", "sphere", "sketch", "extrude", "revolve", "boolean",
                                "hole", "transform", "copy", "remove", "fillet", "chamfer", "mesh", "import"};
    const QSet<QString> featureKeys = {"id", "name", "type", "parameters", "visible"};
    QSet<QString> previous;
    for (const auto &value : root["features"].toArray()) {
        require(value.isObject(), "Operação inválida: esperado um objeto.");
        const auto f = value.toObject();
        for (auto it = f.begin(); it != f.end(); ++it)
            require(featureKeys.contains(it.key()), "A operação contém dados não suportados: " + it.key());
        const auto id = f["id"].toString();
        const auto type = f["type"].toString();
        require(!id.isEmpty() && !previous.contains(id), "Identificador de operação vazio ou duplicado.");
        require(types.contains(type), "Tipo de operação não suportado: " + type);
        require(f["name"].isUndefined() || f["name"].isString(), "Nome de operação inválido.");
        require(f["visible"].isUndefined() || f["visible"].isBool(), "Visibilidade de operação inválida.");
        require(f["parameters"].isObject(), "Parâmetros de operação inválidos.");
        const auto p = f["parameters"].toObject();
        if (p.contains("constraintSystem"))
            require(root["version"].toInt() >= 2 && type == "sketch" && p["constraintSystem"].isObject() &&
                    p["profile"] == "polyline", "Restrições de sketch exigem documento v2 e perfil poligonal.");
        if(p.contains("expressions")) {
            require(root["version"].toInt()==3 && p["expressions"].isObject(),"Expressões exigem documento v3.");
            const auto expressions=p["expressions"].toObject();
            for(auto it=expressions.begin();it!=expressions.end();++it)
                require(Model::expressionFields(type,p).contains(it.key()) && it.value().isString(),
                        "Campo de expressão inválido: "+it.key());
        }
        auto enumeration = [&](const char *key, const QSet<QString> &allowed) {
            const auto entry = p[key];
            require(entry.isUndefined() || (entry.isString() && allowed.contains(entry.toString())),
                    QString("Valor inválido em %1: %2.").arg(id, key));
        };
        if (type == "extrude" || type == "revolve") enumeration("mode", {"join", "cut"});
        if (type == "boolean") enumeration("mode", {"join", "cut", "common"});
        if(type=="chamfer") {enumeration("mode",{"equal","two","angle"});enumeration("side",{"first","second"});}
        if (type == "transform" || type == "copy") enumeration("axis", {"X", "Y", "Z"});
        if (type == "sketch") {
            enumeration("profile", {"rectangle", "circle", "polyline", "arc"});
            require(p["closed"].isUndefined() || p["closed"].isNull() || p["closed"].isBool(),
                    "O fechamento do perfil deve ser verdadeiro ou falso.");
        }
        if (type == "sketch" || type == "hole") {
            const auto plane = p["plane"];
            require(plane.isUndefined() || (plane.isString() && (plane == "XY" || plane == "XZ" || plane == "YZ" ||
                         plane.toString().startsWith("FACE:"))), "Plano de desenho inválido.");
        }
        auto requiredReference = [&](const char *key) {
            require(!p[key].toString().isEmpty(), QString("A operação %1 exige a referência %2.").arg(id, key));
        };
        if (type == "extrude" || type == "revolve" || type == "transform" || type == "copy" ||
            type == "remove" || type == "fillet" || type == "chamfer") requiredReference("source");
        if (type == "hole" || type == "boolean") requiredReference("target");
        if (type == "boolean") requiredReference("tool");
        if (type == "sketch" && !p["support"].toString().isEmpty()) {
            auto index = p["supportFace"], count = p["supportFaceCount"];
            require(index.isDouble() && count.isDouble() && index.toDouble() == index.toInt(-1) &&
                        count.toDouble() == count.toInt(-1) && index.toInt() >= 0 && count.toInt() > index.toInt(),
                    "Referência de face do sketch inválida.");
        }
        previous.insert(id);
    }
    DependencyGraph(root["features"].toArray()).requireHistoryOrder();
}
gp_Pnt planePointExact(const QString &plane, double u, double v, double offset = 0) {
    require(std::isfinite(u) && std::isfinite(v) && std::isfinite(offset) &&
                std::abs(u) <= 1e6 && std::abs(v) <= 1e6, "Ponto fora do intervalo permitido.");
    if (plane.startsWith("FACE:")) {
        auto a = QJsonDocument::fromJson(plane.mid(5).toUtf8()).array();
        require(a.size() == 9, "Plano de face inválido.");
        for (auto number : a)
            require(number.isDouble() && std::isfinite(number.toDouble()), "Plano de face inválido.");
        gp_Pnt origin(a[0].toDouble(), a[1].toDouble(), a[2].toDouble());
        gp_Vec x(a[3].toDouble(), a[4].toDouble(), a[5].toDouble());
        gp_Vec y(a[6].toDouble(), a[7].toDouble(), a[8].toDouble());
        require(std::abs(x.Magnitude()-1)<1e-5 && std::abs(y.Magnitude()-1)<1e-5 && std::abs(x.Dot(y))<1e-5,
                "Eixos de face inválidos.");
        return origin.Translated(x*u + y*v + x.Crossed(y)*offset);
    }
    require(plane == "XY" || plane == "XZ" || plane == "YZ", "Plano inválido.");
    if (plane == "XZ")
        return {u, -offset, v};
    if (plane == "YZ")
        return {offset, u, v};
    return {u, v, offset};
}
TopoDS_Shape profile(const QJsonObject &p) {
    QString plane = p["plane"].toString("XY"), kind = p["profile"].toString("rectangle");
    double x = value(p, "x"), y = value(p, "y"), z = value(p, "offset");
    auto point = [&](double u, double v) { return planePointExact(plane, u, v, z); };
    if (kind == "circle") {
        auto n = Model::planeNormal(plane);
        return BRepBuilderAPI_MakeWire(
                   BRepBuilderAPI_MakeEdge(
                       gp_Circ(gp_Ax2(point(x, y), gp_Dir(n.x(), n.y(), n.z())), positive(p, "r", 10))))
            .Wire();
    }
    if (kind == "arc") {
        auto a = point(value(p, "x1"), value(p, "y1"));
        auto b = point(value(p, "xm"), value(p, "ym"));
        auto c = point(value(p, "x2"), value(p, "y2"));
        GC_MakeArcOfCircle arc(a, b, c);
        require(arc.IsDone(), "Os três pontos não definem um arco válido.");
        return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(arc.Value())).Wire();
    }
    BRepBuilderAPI_MakePolygon polygon;
    if (kind == "rectangle") {
        double w = positive(p, "w", 40), h = positive(p, "h", 30);
        polygon.Add(point(x, y));
        polygon.Add(point(x + w, y));
        polygon.Add(point(x + w, y + h));
        polygon.Add(point(x, y + h));
        polygon.Close();
    } else if (kind == "polyline") {
        auto points = p["points"].toArray();
        require(points.size() >= 2, "Adicione pelo menos dois pontos.");
        require(points.size() <= 10000, "Sketch muito grande.");
        for (auto v : points) {
            auto a = v.toArray();
            require(a.size() == 2 && a[0].isDouble() && a[1].isDouble(), "Ponto inválido.");
            polygon.Add(point(a[0].toDouble(), a[1].toDouble()));
        }
        if (p["closed"].toBool()) {
            require(points.size() >= 3, "Um perfil fechado exige três pontos.");
            polygon.Close();
        }
    } else
        throw std::runtime_error("Tipo de perfil não suportado.");
    require(polygon.IsDone(), "Perfil inválido: confira os pontos.");
    return polygon.Wire();
}
} // namespace
QJsonObject Feature::json() const {
    return {{"id", id}, {"name", name}, {"type", type}, {"parameters", p}, {"visible", visible}};
}
Model::Model() : savedDocument(json()) {}
void Model::markUnsaved() {
    savedDocument = {};
    dirty = true;
}
QVector3D Model::planePoint(const QString &plane, double u, double v, double offset) {
    if (plane.startsWith("FACE:")) {
        auto point = planePointExact(plane, u, v, offset);
        return {float(point.X()), float(point.Y()), float(point.Z())};
    }
    if (plane == "XZ")
        return QVector3D(u, -offset, v);
    if (plane == "YZ")
        return QVector3D(offset, u, v);
    return QVector3D(u, v, offset);
}
QVector3D Model::planeNormal(const QString &plane) {
    if (plane.startsWith("FACE:")) {
        auto origin = planePointExact(plane, 0, 0);
        gp_Vec n(origin, planePointExact(plane, 0, 0, 1));
        return QVector3D(n.X(), n.Y(), n.Z()).normalized();
    }
    if (plane == "XZ")
        return {0, -1, 0};
    if (plane == "YZ")
        return {1, 0, 0};
    return {0, 0, 1};
}
QPointF Model::planeCoordinates(const QString &plane, QVector3D point) {
    auto origin = planePoint(plane, 0, 0);
    auto x = (planePoint(plane, 1, 0)-origin).normalized();
    auto y = (planePoint(plane, 0, 1)-origin).normalized();
    return {QVector3D::dotProduct(point-origin, x), QVector3D::dotProduct(point-origin, y)};
}
QString Model::facePlane(const TopoDS_Shape &shape, int index) {
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    require(index >= 0 && index < faces.Extent(), "Selecione uma face.");
    auto face = TopoDS::Face(faces(index+1));
    BRepAdaptor_Surface surface(face);
    require(surface.GetType() == GeomAbs_Plane, "O sketch exige uma face plana. Faces curvas não definem um plano de desenho.");
    auto plane = surface.Plane();
    gp_Vec x(plane.XAxis().Direction()), normal(plane.Axis().Direction());
    if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();
    auto y = normal.Crossed(x);
    auto o = plane.Location();
    QJsonArray frame{o.X(),o.Y(),o.Z(),x.X(),x.Y(),x.Z(),y.X(),y.Y(),y.Z()};
    return "FACE:" + QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact));
}
Feature &Model::get(const QString &id) {
    for (auto &f : features)
        if (f.id == id)
            return f;
    throw std::runtime_error("Referência a uma operação inexistente.");
}
const Feature &Model::get(const QString &id) const {
    for (auto &f : features)
        if (f.id == id)
            return f;
    throw std::runtime_error("Referência a uma operação inexistente.");
}
QJsonObject Model::json() const {
    QJsonArray a;
    int version = 1;
    for (auto &f : features)
    {
        a.append(f.json());
        if (f.p.contains("constraintSystem")) version = std::max(version,2);
        if (f.p.contains("expressions")) version = 3;
    }
    QJsonObject root{{"format", "MecaCAD"}, {"version", version}, {"units", "mm"}, {"features", a}};
    if(!namedParameters.isEmpty()) {
        QJsonObject definitions;
        for(auto it=namedParameters.begin();it!=namedParameters.end();++it) definitions[it.key()]=it.value();
        root["version"]=3;root["namedParameters"]=definitions;
    }
    return root;
}
void Model::restore(const QJsonObject &root) {
    validateDocument(root);
    Model candidate;
    const auto definitions=root["namedParameters"].toObject();
    for(auto it=definitions.begin();it!=definitions.end();++it) candidate.namedParameters[it.key()]=it.value().toString();
    for (auto v : root["features"].toArray()) {
        auto o = v.toObject();
        Feature f{o["id"].toString(),         o["name"].toString(),      o["type"].toString(),
                  o["parameters"].toObject(), o["visible"].toBool(true), {}};
        require(!f.id.isEmpty(), "Operação sem identificador.");
        candidate.features.push_back(f);
    }
    candidate.rebuildGeometry();
    features = std::move(candidate.features);
    namedParameters=std::move(candidate.namedParameters);
}
void Model::checkpoint(const QJsonObject &before) {
    if (before == json())
        return;
    past.push_back(before);
    if (past.size() > 100)
        past.erase(past.begin());
    future.clear();
    dirty = json() != savedDocument;
}
void Model::loadJson(const QJsonObject &root) {
    restore(root);
    past.clear();
    future.clear();
    savedDocument = json();
    dirty = false;
}
void Model::commit(const QJsonObject &document) {
    auto before = json();
    if (before == document)
        return;
    restore(document);
    checkpoint(before);
}
QString Model::add(QString type, QJsonObject p, QString name) {
    auto before = json();
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (name.isEmpty())
        name = type + QString(" %1").arg(features.size() + 1);
    Model candidate;
    candidate.features = features;
    candidate.namedParameters = namedParameters;
    candidate.features.push_back({id, name, type, p, true, {}});
    validateDocument(candidate.json());
    candidate.rebuildGeometry();
    features = std::move(candidate.features);
    checkpoint(before);
    return id;
}
void Model::edit(const QString &id, QJsonObject p, const QString &name) {
    auto before = json();
    const auto &old = get(id).p;
    if(old.contains("expressions") && !p.contains("expressions")) p["expressions"]=old["expressions"];
    const auto bindings=old["expressions"].toObject();
    for(auto it=bindings.begin();it!=bindings.end();++it)
        require(p["expressions"].toObject()[it.key()]!=it.value() || p[it.key()]==old[it.key()],
                "Esta medida é controlada por fórmula. Edite ou remova a expressão primeiro.");
    if (old.contains("constraintSystem")) {
        require(p.contains("constraintSystem") && p["profile"] == "polyline" && p["closed"] == old["closed"],
                "Não remova as restrições nem altere a conectividade pela edição genérica.");
        if (p["constraintSystem"] == old["constraintSystem"] && p["points"] != old["points"]) {
            auto system = sketch::System::fromJson(p["constraintSystem"].toObject());
            const auto points = p["points"].toArray();
            require(points.size() == system.points.size(), "Edição deve preservar os IDs e a quantidade de pontos restritos.");
            for (int i=0;i<points.size();++i) {
                const auto point = points[i].toArray();
                require(point.size()==2 && point[0].isDouble() && point[1].isDouble(), "Coordenadas inválidas.");
                system.points[i].position = {point[0].toDouble(),point[1].toDouble()};
            }
            const auto solution=system.solve();
            require(solution.consistent,"A edição entra em conflito com as restrições do sketch.");
            if(solution.degreesOfFreedom==0) {
                // A fully constrained sketch cannot be moved by a point gesture.
                // Keep exact stored values so numerical projection creates no undo step.
                p["constraintSystem"]=old["constraintSystem"];
                p["points"]=old["points"];
            } else p["constraintSystem"] = system.json();
        }
    }
    Model candidate;
    candidate.features = features;
    candidate.namedParameters = namedParameters;
    candidate.get(id).p = p;
    candidate.get(id).name = name;
    validateDocument(candidate.json());
    candidate.rebuildGeometry();
    features = std::move(candidate.features);
    checkpoint(before);
}
void Model::toggle(const QString &id) {
    auto before = json();
    get(id).visible = !get(id).visible;
    checkpoint(before);
}
void Model::remove(const QString &id) {
    get(id); // Missing targets must not create an empty undo step.
    QStringList names;
    for (const auto &dependent : dependencyGraph().dependents(id)) names.append(get(dependent).name);
    require(names.empty(), "Há operações dependentes: " + names.join(", ") + ". Repare ou exclua essas etapas primeiro.");
    auto before = json();
    std::erase_if(features, [&](auto &f) { return f.id == id; });
    checkpoint(before);
}
bool Model::undo() {
    if (past.empty())
        return false;
    auto current = json();
    restore(past.back());
    past.pop_back();
    future.push_back(current);
    dirty = json() != savedDocument;
    return true;
}
void Model::deleteBody(const QString &id) {
    const auto &feature = get(id);
    require(feature.type != "sketch" && feature.type != "remove" && !consumed(id),
            "Selecione a peça final no desenho ou em Bodies para apagar.");
    add("remove", {{"source", id}}, "Apagar " + feature.name);
}
bool Model::redo() {
    if (future.empty())
        return false;
    auto current = json();
    restore(future.back());
    future.pop_back();
    past.push_back(current);
    dirty = json() != savedDocument;
    return true;
}
void Model::clear() {
    features.clear();
    namedParameters.clear();
    past.clear();
    future.clear();
    filePath.clear();
    savedDocument = json();
    dirty = false;
}
void Model::rebuild() {
    validateDocument(json());
    Model candidate;
    candidate.features = features;
    candidate.namedParameters = namedParameters;
    candidate.rebuildGeometry();
    features = std::move(candidate.features);
}
DependencyGraph Model::dependencyGraph() const {
    return DependencyGraph(json()["features"].toArray());
}
void Model::moveFeature(const QString &id, int destination) {
    get(id);
    require(destination >= 0 && destination < int(features.size()), "Posição de histórico inválida.");
    auto document = json();
    auto list = document["features"].toArray();
    int origin = 0;
    while (list[origin].toObject()["id"].toString() != id) ++origin;
    if (origin == destination) return;
    const auto feature = list.takeAt(origin);
    list.insert(destination, feature);
    document["features"] = list;
    commit(document); // Validation rejects moving inputs after their consumers.
}
void Model::rebuildGeometry() {
    const auto values=parameters::resolve(namedParameters);
    for (auto &f : features)
        f.shape.Nullify();
    for (const auto &id : dependencyGraph().order()) {
        auto &f = get(id);
        try {
            const auto expressions=f.p.value("expressions").toObject();
            for(auto it=expressions.begin();it!=expressions.end();++it) {
                const auto quantity=parameters::evaluate(it.value().toString(),values);
                const bool angular=it.key()=="angle";
                require(quantity.length==(angular?0:1) && quantity.angle==(angular?1:0),
                        "A expressão de "+it.key()+(angular?" deve resultar em ângulo (use deg ou rad).":" deve resultar em comprimento (use mm, cm, m ou in)."));
                f.p[it.key()]=angular?quantity.value*180/M_PI:quantity.value;
            }
            if (f.type == "sketch" && f.p.contains("constraintSystem")) resolveSketch(f);
            if (f.type == "sketch" && !f.p.value("support").toString().isEmpty()) {
                const auto &support = get(f.p["support"].toString());
                require(!support.shape.IsNull() && !isMesh(support.id), "O plano do sketch depende de um corpo CAD anterior válido.");
                TopTools_IndexedMapOfShape faces;
                TopExp::MapShapes(support.shape, TopAbs_FACE, faces);
                require(faces.Extent() == f.p["supportFaceCount"].toInt(),
                        "A topologia do suporte mudou. Não foi possível preservar a referência da face do sketch.");
                f.p["plane"] = facePlane(support.shape, f.p["supportFace"].toInt(-1));
            }
            const auto &p = f.p;
            auto source = [&](const char *key) {
                require(!isMesh(p[key].toString()) || f.type == "transform" || f.type == "copy" ||
                            f.type == "remove",
                        "Esta operação requer um sólido CAD. Conversão de malha STL ainda não disponível.");
                auto s = get(p[key].toString()).shape;
                require(!s.IsNull(), "A operação depende de uma etapa futura ou inválida.");
                return s;
            };
            gp_Pnt origin(value(p, "x"), value(p, "y"), value(p, "z"));
            if (f.type == "box")
                f.shape = BRepPrimAPI_MakeBox(origin, positive(p, "w", 40), positive(p, "h", 30),
                                              positive(p, "d", 10))
                              .Shape();
            else if (f.type == "cylinder")
                f.shape = BRepPrimAPI_MakeCylinder(gp_Ax2(origin, gp::DZ()), positive(p, "r", 10),
                                                   positive(p, "d", 20))
                              .Shape();
            else if (f.type == "sphere")
                f.shape = BRepPrimAPI_MakeSphere(origin, positive(p, "r", 10)).Shape();
            else if (f.type == "sketch")
                f.shape = profile(p);
            else if (f.type == "extrude" || f.type == "revolve") {
                const auto &sketch = get(p["source"].toString());
                require(sketch.type == "sketch", "Selecione um sketch.");
                require(sketch.p["profile"] != "arc" &&
                            (sketch.p["profile"] != "polyline" || sketch.p["closed"].toBool()),
                        "A extrusão exige um perfil fechado.");
                BRepBuilderAPI_MakeFace face(TopoDS::Wire(source("source")), true);
                require(face.IsDone(), "O perfil não forma uma face plana.");
                require(BRepCheck_Analyzer(face.Face()).IsValid(), "Perfil inválido ou auto-intersectante.");
                if (f.type == "extrude") {
                    double d = value(p, "d", 10);
                    require(std::abs(d) > 1e-5, "A extrusão não pode ter distância zero.");
                    auto n = planeNormal(sketch.p["plane"].toString("XY"));
                    f.shape =
                        BRepPrimAPI_MakePrism(face.Face(), gp_Vec(n.x() * d, n.y() * d, n.z() * d)).Shape();
                } else {
                    double angle = positive(p, "angle", 360);
                    require(angle <= 360, "Ângulo máximo: 360°.");
                    auto a = planePoint(sketch.p["plane"].toString("XY"), 0, 1) -
                             planePoint(sketch.p["plane"].toString("XY"), 0, 0);
                    f.shape = BRepPrimAPI_MakeRevol(
                                  face.Face(),
                                  gp_Ax1(planePointExact(sketch.p["plane"].toString("XY"), value(p, "axis"),
                                                         0, value(sketch.p, "offset")),
                                         gp_Dir(a.x(), a.y(), a.z())),
                                  angle * M_PI / 180)
                                  .Shape();
                }
                require(p["mode"] != "cut" || !p["target"].toString().isEmpty(),
                        "Selecione uma peça de destino para o corte.");
                if (!p["target"].toString().isEmpty()) {
                    auto target = source("target");
                    if (p["mode"] == "cut") {
                        BRepAlgoAPI_Cut cut(target, f.shape);
                        require(cut.IsDone(), "Não foi possível calcular o corte.");
                        f.shape = cut.Shape();
                        require(!f.shape.IsNull() && volume(target) - volume(f.shape) > 1e-7,
                                "A extrusão não atravessa a peça. Arraste a seta para dentro dela ou inverta o sinal da distância.");
                    } else
                        f.shape = BRepAlgoAPI_Fuse(target, f.shape).Shape();
                }
            } else if (f.type == "boolean") {
                auto a = source("target"), b = source("tool");
                if (p["mode"] == "cut")
                    f.shape = BRepAlgoAPI_Cut(a, b).Shape();
                else if (p["mode"] == "common")
                    f.shape = BRepAlgoAPI_Common(a, b).Shape();
                else
                    f.shape = BRepAlgoAPI_Fuse(a, b).Shape();
            } else if (f.type == "hole") {
                QString plane = p["plane"].toString("XY");
                auto n = planeNormal(plane);
                auto cutter =
                    BRepPrimAPI_MakeCylinder(
                        gp_Ax2(planePointExact(plane, value(p, "u"), value(p, "v"), value(p, "offset")),
                               gp_Dir(n.x(), n.y(), n.z())),
                        positive(p, "r", 3), positive(p, "d", 100))
                        .Shape();
                auto target = source("target");
                f.shape = BRepAlgoAPI_Cut(target, cutter).Shape();
                require(volume(target) - volume(f.shape) > 1e-7,
                        "O furo não intersecta o corpo. Confira centro, plano e profundidade.");
            } else if (f.type == "transform" || f.type == "copy") {
                double angle = value(p, "angle");
                QString axis = p["axis"].toString("Z");
                gp_Dir dir = axis == "X" ? gp::DX() : (axis == "Y" ? gp::DY() : gp::DZ());
                gp_Trsf rotation;
                rotation.SetRotation(gp_Ax1(gp_Pnt(value(p, "px"), value(p, "py"), value(p, "pz")), dir),
                                     angle * M_PI / 180);
                gp_Trsf translation;
                translation.SetTranslation(gp_Vec(value(p, "x"), value(p, "y"), value(p, "z")));
                f.shape = BRepBuilderAPI_Transform(source("source"), translation * rotation,
                                                   !isMesh(p["source"].toString()))
                              .Shape();
            } else if (f.type == "remove") {
                f.shape = source("source");
            } else if (f.type == "fillet") {
                auto s = source("source");
                require(!isMesh(p["source"].toString()), "Filete exige um sólido CAD, não uma malha STL.");
                BRepFilletAPI_MakeFillet fillet(s);
                TopTools_IndexedMapOfShape edges;
                TopExp::MapShapes(s, TopAbs_EDGE, edges);
                if (p.contains("edges")) {
                    require(p["edges"].isArray() && !p["edges"].toArray().empty(), "Selecione ao menos uma aresta.");
                    std::set<int> indices;
                    for (auto entry : p["edges"].toArray()) {
                        int index = entry.toInt(-1);
                        require(entry.isDouble() && entry.toDouble() == index && index >= 0 && index < edges.Extent(),
                                "Aresta de filete inválida. Selecione novamente as arestas.");
                        indices.insert(index);
                    }
                    for (int index : indices)
                        fillet.Add(positive(p, "r", 1), TopoDS::Edge(edges(index+1)));
                } else {
                    for (int i=1; i<=edges.Extent(); ++i)
                        fillet.Add(positive(p, "r", 1), TopoDS::Edge(edges(i)));
                }
                fillet.Build();
                require(fillet.IsDone(), "Não foi possível aplicar este raio às arestas. Reduza o raio ou altere a seleção.");
                f.shape = fillet.Shape();
            } else if(f.type=="chamfer") {
                f.shape=chamferShape(source("source"),p);
            } else if (f.type == "mesh") {
                auto encoded = p["stl"].toString().toLatin1();
                require(!encoded.isEmpty() && encoded.size() <= 70000000,
                        "STL vazio ou muito grande (limite 50 MB).");
                auto data = QByteArray::fromBase64(encoded);
                QTemporaryFile file;
                require(file.open() && file.write(data) == data.size() && file.flush(),
                        "Não foi possível preparar o STL.");
                auto triangulation = RWStl::ReadFile(file.fileName().toUtf8().constData());
                require(!triangulation.IsNull() && triangulation->NbTriangles() > 0,
                        "STL inválido ou sem triângulos.");
                require(triangulation->NbTriangles() <= 1000000,
                        "STL excede o limite de 1 milhão de triângulos.");
                for (int i = 1; i <= triangulation->NbNodes(); ++i) {
                    auto point = triangulation->Node(i);
                    require(std::isfinite(point.X()) && std::isfinite(point.Y()) && std::isfinite(point.Z()),
                            "STL contém coordenadas inválidas.");
                }
                TopoDS_Face face;
                BRep_Builder builder;
                builder.MakeFace(face, triangulation);
                f.shape = face;
            } else if (f.type == "import") {
                auto encoded = p["brep"].toString().toLatin1();
                require(encoded.size() < 150000000, "Modelo importado muito grande.");
                std::istringstream stream(QByteArray::fromBase64(encoded).toStdString());
                BRep_Builder builder;
                BRepTools::Read(f.shape, stream, builder);
            } else
                throw std::runtime_error("Operação desconhecida.");
            require(!f.shape.IsNull() && (isMesh(f.id) || BRepCheck_Analyzer(f.shape).IsValid()),
                    "A operação produziu geometria inválida.");
            if (f.type != "sketch" && !isMesh(f.id))
                require(TopExp_Explorer(f.shape, TopAbs_SOLID).More(),
                        "A operação não produziu um sólido. Confira posições e interseções.");
        } catch (const Standard_Failure &e) {
            throw std::runtime_error((f.name + ": " + QString::fromUtf8(e.GetMessageString())).toStdString());
        } catch (const std::exception &e) {
            throw std::runtime_error((f.name + " [" + f.id + "]: " + QString::fromUtf8(e.what())).toStdString());
        }
    }
}
bool Model::consumed(const QString &id) const {
    for (auto &f : features) {
        if (f.p["target"] == id || f.p["tool"] == id)
            return true;
        if (f.p["source"] == id && f.type != "copy")
            return true;
    }
    return false;
}
bool Model::isMesh(const QString &id) const {
    QString current = id;
    for (size_t depth = 0; depth <= features.size(); ++depth) {
        const auto &feature = get(current);
        if (feature.type == "mesh")
            return true;
        if (feature.type != "transform" && feature.type != "copy" && feature.type != "remove")
            return false;
        current = feature.p["source"].toString();
    }
    throw std::runtime_error("Dependência circular no modelo.");
}
std::vector<int> Model::bodies(bool visibleOnly) const {
    std::vector<int> result;
    for (int i = 0; i < int(features.size()); ++i) {
        const auto &f = features[i];
        if (f.type != "sketch" && f.type != "remove" && !consumed(f.id) && (!visibleOnly || f.visible))
            result.push_back(i);
    }
    return result;
}
double Model::volume(const TopoDS_Shape &s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}
std::vector<Triangle> Model::triangles() const {
    std::vector<Triangle> result;
    for (int i : bodies()) {
        const auto &shape = features[i].shape;
        if (!isMesh(features[i].id))
            BRepMesh_IncrementalMesh mesh(shape, 0.15, false, 0.35, true);
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(shape, TopAbs_FACE, faces);
        for (int faceIndex = 1; faceIndex <= faces.Extent(); ++faceIndex) {
            auto face = TopoDS::Face(faces(faceIndex));
            TopLoc_Location location;
            auto tri = BRep_Tool::Triangulation(face, location);
            if (tri.IsNull())
                continue;
            auto point = [&](int j) {
                auto p = tri->Node(j).Transformed(location.Transformation());
                return QVector3D(p.X(), p.Y(), p.Z());
            };
            for (int j = 1; j <= tri->NbTriangles(); ++j) {
                int a, b, c;
                tri->Triangle(j).Get(a, b, c);
                if (face.Orientation() == TopAbs_REVERSED)
                    std::swap(b, c);
                result.push_back({point(a), point(b), point(c), i, faceIndex-1});
            }
        }
    }
    return result;
}
void Model::save(const QString &path) {
    QSaveFile f(path);
    require(f.open(QIODevice::WriteOnly), f.errorString());
    auto bytes = QJsonDocument(json()).toJson();
    require(f.write(bytes) == bytes.size(), f.errorString());
    require(f.commit(), f.errorString());
    filePath = path;
    savedDocument = json();
    dirty = false;
}
void Model::load(const QString &path) {
    QFile f(path);
    require(f.open(QIODevice::ReadOnly), f.errorString());
    require(f.size() < 200000000, "Arquivo muito grande.");
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(f.readAll(), &error);
    require(error.error == QJsonParseError::NoError, error.errorString());
    require(doc.isObject(), "Projeto inválido: esperado um objeto JSON.");
    loadJson(doc.object());
    filePath = path;
}
TopoDS_Shape Model::exportShape(const QString &id) const {
    if (!id.isEmpty()) {
        const auto &f = get(id);
        require(f.type != "sketch" && f.type != "remove",
                "Selecione um corpo, não uma operação de remoção, para exportar.");
        return f.shape;
    }
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    auto ids = bodies();
    require(!ids.empty(), "Não há sólidos visíveis para exportar.");
    for (auto i : ids)
        builder.Add(compound, features[i].shape);
    return compound;
}
void Model::exportStep(const QString &path, const QString &id) const {
    if (!id.isEmpty())
        require(!isMesh(id), "STL é uma malha, não um sólido CAD. Conversão para STEP ainda não disponível.");
    else
        for (int i : bodies())
            require(!isMesh(features[i].id),
                    "Selecione um sólido CAD para exportar STEP; há malhas STL visíveis.");
    STEPControl_Writer w;
    require(w.Transfer(exportShape(id), STEPControl_AsIs) == IFSelect_RetDone,
            "Falha ao converter o sólido em STEP.");
    require(w.Write(path.toUtf8().constData()) == IFSelect_RetDone, "Não foi possível gravar o STEP.");
}
void Model::exportStl(const QString &path, const QString &id) const {
    auto shape = exportShape(id);
    BRepMesh_IncrementalMesh mesh(shape, 0.05, false, 0.2, true);
    StlAPI_Writer w;
    w.ASCIIMode() = false;
    require(w.Write(shape, path.toUtf8().constData()), "Não foi possível gravar o STL.");
}
QString Model::importStep(const QString &path) {
    STEPControl_Reader reader;
    require(reader.ReadFile(path.toUtf8().constData()) == IFSelect_RetDone, "Não foi possível ler o STEP.");
    require(reader.TransferRoots() > 0, "O STEP não contém geometria transferível.");
    auto shape = reader.OneShape();
    require(!shape.IsNull(), "STEP vazio.");
    std::ostringstream stream;
    BRepTools::Write(shape, stream);
    return add("import", {{"brep", QString::fromLatin1(QByteArray::fromStdString(stream.str()).toBase64())}},
               QFileInfo(path).completeBaseName());
}
QString Model::importStl(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), file.errorString());
    require(file.size() > 0 && file.size() <= 50000000, "STL vazio ou muito grande (limite 50 MB).");
    return add("mesh", {{"stl", QString::fromLatin1(file.readAll().toBase64())}},
               QFileInfo(path).completeBaseName() + " (STL)");
}
void Model::exportDxf(const QString &path, const QString &id) const {
    const auto &sk = get(id);
    require(sk.type == "sketch", "Selecione um sketch na árvore para exportar DXF.");
    const auto &p = sk.p;
    QSaveFile f(path);
    require(f.open(QIODevice::WriteOnly | QIODevice::Text), f.errorString());
    QTextStream s(&f);
    s.setLocale(QLocale::c());
    s.setRealNumberPrecision(15);
    s << "0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$"
         "INSUNITS\n70\n4\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n";
    auto line = [&](double x1, double y1, double x2, double y2) {
        s << "0\nLINE\n8\nSketch\n10\n"
          << x1 << "\n20\n"
          << y1 << "\n30\n0\n11\n"
          << x2 << "\n21\n"
          << y2 << "\n31\n0\n";
    };
    QString kind = p["profile"].toString();
    double x = value(p, "x"), y = value(p, "y");
    if (kind == "circle")
        s << "0\nCIRCLE\n8\nSketch\n10\n"
          << x << "\n20\n"
          << y << "\n30\n0\n40\n"
          << positive(p, "r") << "\n";
    else if (kind == "rectangle") {
        double w = positive(p, "w"), h = positive(p, "h");
        line(x, y, x + w, y);
        line(x + w, y, x + w, y + h);
        line(x + w, y + h, x, y + h);
        line(x, y + h, x, y);
    } else if (kind == "arc") {
        GC_MakeArcOfCircle arc(gp_Pnt(value(p, "x1"), value(p, "y1"), 0),
                               gp_Pnt(value(p, "xm"), value(p, "ym"), 0),
                               gp_Pnt(value(p, "x2"), value(p, "y2"), 0));
        // Exact circular arc, including clockwise sketches, exported in local XY.
        auto curve = arc.Value();
        gp_Pnt a = curve->Value(curve->FirstParameter()),
               b = curve->Value((curve->FirstParameter() + curve->LastParameter()) / 2),
               c = curve->Value(curve->LastParameter());
        double den = 2 * (a.X() * (b.Y() - c.Y()) + b.X() * (c.Y() - a.Y()) + c.X() * (a.Y() - b.Y()));
        double aa = a.X() * a.X() + a.Y() * a.Y(), bb = b.X() * b.X() + b.Y() * b.Y(),
               cc = c.X() * c.X() + c.Y() * c.Y();
        double cx = (aa * (b.Y() - c.Y()) + bb * (c.Y() - a.Y()) + cc * (a.Y() - b.Y())) / den,
               cy = (aa * (c.X() - b.X()) + bb * (a.X() - c.X()) + cc * (b.X() - a.X())) / den;
        double start = std::atan2(a.Y() - cy, a.X() - cx) * 180 / M_PI,
               end = std::atan2(c.Y() - cy, c.X() - cx) * 180 / M_PI;
        if (den < 0)
            std::swap(start, end);
        if (start < 0)
            start += 360;
        if (end < 0)
            end += 360;
        s << "0\nARC\n8\nSketch\n10\n"
          << cx << "\n20\n"
          << cy << "\n30\n0\n40\n"
          << std::hypot(a.X() - cx, a.Y() - cy) << "\n50\n"
          << start << "\n51\n"
          << end << "\n";
    } else {
        auto points = p["points"].toArray();
        int count = points.size();
        for (int i = 0; i < count - 1; ++i) {
            auto a = points[i].toArray(), b = points[i + 1].toArray();
            line(a[0].toDouble(), a[1].toDouble(), b[0].toDouble(), b[1].toDouble());
        }
        if (p["closed"].toBool()) {
            auto a = points.last().toArray(), b = points.first().toArray();
            line(a[0].toDouble(), a[1].toDouble(), b[0].toDouble(), b[1].toDouble());
        }
    }
    s << "0\nENDSEC\n0\nEOF\n";
    s.flush();
    require(s.status() == QTextStream::Ok && f.commit(), "Falha ao gravar DXF.");
}
