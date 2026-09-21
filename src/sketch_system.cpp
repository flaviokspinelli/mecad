#include "sketch_system.h"
#include <QJsonArray>
#include <QSet>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace sketch {
namespace {
constexpr double tolerance = 1e-8; // Absolute coordinate residual, in millimeters.
constexpr double rankTolerance = 1e-10;
void check(bool value, const QString &message) {
    if (!value) throw std::runtime_error(message.toStdString());
}
bool finite(QPointF p) {
    return std::isfinite(p.x()) && std::isfinite(p.y()) && std::abs(p.x()) <= 1e6 && std::abs(p.y()) <= 1e6;
}
const QStringList names{"coincident","horizontal","vertical","fixed","distanceX","distanceY"};
QString name(Relation relation) {
    const auto i = int(relation);
    check(i >= 0 && i < names.size(), "Restrição desconhecida.");
    return names[i];
}
void keys(const QJsonObject &object, const QSet<QString> &allowed) {
    for (auto it = object.begin(); it != object.end(); ++it)
        check(allowed.contains(it.key()), "Campo de sketch não suportado: " + it.key());
}
QPointF position(const QJsonValue &value) {
    check(value.isArray(), "Coordenadas de sketch inválidas.");
    const auto a = value.toArray();
    check(a.size() == 2 && a[0].isDouble() && a[1].isDouble(), "Coordenadas de sketch inválidas.");
    QPointF result(a[0].toDouble(),a[1].toDouble());
    check(finite(result), "Coordenadas fora do intervalo permitido.");
    return result;
}
QJsonArray coordinates(QPointF value) { return {value.x(),value.y()}; }
QString string(const QJsonObject &o, const char *key) {
    check(o[key].isString(), QString("Campo inválido: %1.").arg(key));
    return o[key].toString();
}
double dot(const QVector<double> &a, const QVector<double> &b) {
    long double value = 0;
    for (int i = 0; i < a.size(); ++i) value += static_cast<long double>(a[i])*b[i];
    return double(value);
}
struct Row { QVector<double> a; double b; QString id; };
}
void System::validate() const {
    check(points.size() <= 128 && lines.size() <= 256 && constraints.size() <= 256,
          "Limite do solver linear: 128 pontos, 256 linhas e 256 restrições.");
    QSet<QString> ids, pointIds, lineIds;
    auto unique = [&](const QString &id) {
        check(!id.isEmpty() && id.size() <= 128 && !ids.contains(id), "ID de sketch vazio, longo ou duplicado: " + id);
        ids.insert(id);
    };
    for (const auto &p : points) {
        unique(p.id); pointIds.insert(p.id);
        check(finite(p.position), "Ponto não finito ou fora do intervalo: " + p.id);
    }
    for (const auto &line : lines) {
        unique(line.id); lineIds.insert(line.id);
        check(pointIds.contains(line.start) && pointIds.contains(line.end) && line.start != line.end,
              "Extremidades inválidas na linha: " + line.id);
    }
    for (const auto &c : constraints) {
        unique(c.id); name(c.relation);
        check(finite(c.value), "Valor de restrição inválido: " + c.id);
        const bool line = c.relation == Relation::Horizontal || c.relation == Relation::Vertical;
        check(line ? lineIds.contains(c.first) : pointIds.contains(c.first), "Referência inválida em " + c.id);
        const bool pair = c.relation == Relation::Coincident || c.relation == Relation::DistanceX || c.relation == Relation::DistanceY;
        check(pair ? pointIds.contains(c.second) : c.second.isEmpty(), "Segunda referência inválida em " + c.id);
        if (c.relation == Relation::DistanceX) check(c.value.y() == 0, "DistanceX aceita somente a componente X.");
        else if (c.relation == Relation::DistanceY) check(c.value.x() == 0, "DistanceY aceita somente a componente Y.");
        else if (c.relation != Relation::Fixed) check(c.value.isNull(), "Esta restrição não aceita valor numérico.");
    }
}
QJsonObject System::json() const {
    validate();
    QJsonArray p,l,c;
    for (const auto &point : points) p.append(QJsonObject{{"id",point.id},{"position",coordinates(point.position)}});
    for (const auto &line : lines) l.append(QJsonObject{{"id",line.id},{"start",line.start},{"end",line.end},{"construction",line.construction}});
    for (const auto &constraint : constraints)
        c.append(QJsonObject{{"id",constraint.id},{"type",name(constraint.relation)},{"first",constraint.first},
            {"second",constraint.second},{"value",coordinates(constraint.value)}});
    return {{"format","MecaCADSketch"},{"version",1},{"points",p},{"lines",l},{"constraints",c}};
}
System System::fromJson(const QJsonObject &document) {
    keys(document,{"format","version","points","lines","constraints"});
    check(document["format"] == "MecaCADSketch" && document["version"].isDouble() && document["version"].toDouble() == 1,
          "Formato de sketch não suportado.");
    for (const auto *key : {"points","lines","constraints"}) check(document[key].isArray(), "Lista de sketch inválida.");
    check(document["points"].toArray().size() <= 128 && document["lines"].toArray().size() <= 256 &&
          document["constraints"].toArray().size() <= 256, "Sketch excede os limites do solver linear.");
    System result;
    for (auto value : document["points"].toArray()) {
        check(value.isObject(), "Ponto inválido."); auto o = value.toObject(); keys(o,{"id","position"});
        result.points.append({string(o,"id"),position(o["position"])});
    }
    for (auto value : document["lines"].toArray()) {
        check(value.isObject(), "Linha inválida."); auto o = value.toObject(); keys(o,{"id","start","end","construction"});
        check(o["construction"].isBool(), "Indicador de construção inválido.");
        result.lines.append({string(o,"id"),string(o,"start"),string(o,"end"),o["construction"].toBool()});
    }
    for (auto value : document["constraints"].toArray()) {
        check(value.isObject(), "Restrição inválida."); auto o = value.toObject(); keys(o,{"id","type","first","second","value"});
        const auto type = names.indexOf(string(o,"type")); check(type >= 0, "Restrição não suportada.");
        result.constraints.append({string(o,"id"),Relation(type),string(o,"first"),string(o,"second"),position(o["value"])});
    }
    result.validate(); return result;
}
Solution System::solve() const {
    validate();
    const int n = points.size()*2;
    QMap<QString,int> indices;
    QMap<QString,Line> lineById;
    QVector<double> initial(n);
    for (int i = 0; i < points.size(); ++i) {
        indices[points[i].id] = i*2;
        initial[i*2] = points[i].position.x(); initial[i*2+1] = points[i].position.y();
    }
    for (const auto &l : lines) lineById.insert(l.id,l);
    QVector<Row> rows;
    auto equation = [&](QString a, QString b, int axis, double value, QString id) {
        Row row{QVector<double>(n,0),value,id};
        row.a[indices[a]+axis] += 1;
        if (!b.isEmpty()) row.a[indices[b]+axis] -= 1;
        rows.append(row);
    };
    for (const auto &c : constraints) {
        switch (c.relation) {
        case Relation::Coincident:
            equation(c.first,c.second,0,0,c.id); equation(c.first,c.second,1,0,c.id); break;
        case Relation::Horizontal: case Relation::Vertical: {
            auto l = lineById[c.first];
            equation(l.end,l.start,c.relation == Relation::Horizontal ? 1 : 0,0,c.id); break;
        }
        case Relation::Fixed:
            equation(c.first,{},0,c.value.x(),c.id); equation(c.first,{},1,c.value.y(),c.id); break;
        case Relation::DistanceX: equation(c.second,c.first,0,c.value.x(),c.id); break;
        case Relation::DistanceY: equation(c.second,c.first,1,c.value.y(),c.id); break;
        }
    }
    QVector<Row> basis;
    QVector<QSet<QString>> contributors;
    QMap<QString,int> independentRows;
    Solution result;
    for (const auto &original : rows) {
        auto row = original;
        QSet<QString> involved{row.id};
        // Reorthogonalized modified Gram-Schmidt: solve A*x=b by projection
        // onto independent normalized rows, not by figure-specific corrections.
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < basis.size(); ++i) {
                const auto coefficient = dot(row.a,basis[i].a);
                if (std::abs(coefficient) > rankTolerance) involved.unite(contributors[i]);
                for (int j = 0; j < n; ++j) row.a[j] -= coefficient*basis[i].a[j];
                row.b -= coefficient*basis[i].b;
            }
        }
        const auto norm = std::sqrt(dot(row.a,row.a));
        if (norm < rankTolerance) {
            if (std::abs(row.b) > tolerance) {
                result.maximumResidual = std::abs(row.b);
                for (const auto &c : constraints) if (involved.contains(c.id)) result.conflictCandidates.append(c.id);
                return result; // Never expose partial positions on conflict.
            }
            continue;
        }
        for (auto &v : row.a) v /= norm;
        row.b /= norm;
        basis.append(row); contributors.append(involved); ++independentRows[row.id];
    }
    auto solution = initial;
    bool alreadySolved = true;
    for (const auto &row : rows)
        if (std::abs(dot(row.a,initial)-row.b)>tolerance) alreadySolved=false;
    if (!alreadySolved)
        for (const auto &row : basis) {
            const auto correction = row.b-dot(row.a,initial);
            for (int i = 0; i < n; ++i) solution[i] += correction*row.a[i];
        }
    for (const auto &row : rows)
        result.maximumResidual = std::max(result.maximumResidual,std::abs(dot(row.a,solution)-row.b));
    check(std::isfinite(result.maximumResidual) && result.maximumResidual <= tolerance,
          "O solver não atingiu a tolerância de 1e-8 mm; geometria não aplicada.");
    for (int i = 0; i < points.size(); ++i) {
        QPointF p(solution[i*2],solution[i*2+1]);
        check(finite(p), "Solução fora do intervalo permitido; geometria não aplicada.");
        result.positions.insert(points[i].id,p);
    }
    for (const auto &c : constraints) if (independentRows.value(c.id) == 0) result.redundantConstraints.append(c.id);
    result.consistent = true; result.degreesOfFreedom = n-basis.size();
    return result;
}
System System::solved() const {
    const auto result = solve();
    check(result.consistent, "Restrições conflitantes (candidatas): " + result.conflictCandidates.join(", "));
    System copy = *this;
    for (auto &point : copy.points) point.position = result.positions[point.id];
    return copy;
}
}
