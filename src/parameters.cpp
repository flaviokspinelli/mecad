#include "model.h"
#include <stdexcept>

QStringList Model::expressionFields(const QString &type) {
    if(type=="box") return {"w","h","d"};
    if(type=="cylinder") return {"r","d"};
    if(type=="sphere" || type=="fillet") return {"r"};
    if(type=="extrude") return {"d"};
    return {};
}
void Model::setParameters(const QMap<QString,QString> &definitions) {
    parameters::resolve(definitions); // Validate all definitions before changing the document.
    auto document=json();QJsonObject object;
    for(auto it=definitions.begin();it!=definitions.end();++it) object[it.key()]=it.value();
    document["version"]=3;document["namedParameters"]=object;
    commit(document);
}
void Model::setExpression(const QString &id,const QString &field,const QString &expression) {
    const auto &feature=get(id);
    if(!expressionFields(feature.type).contains(field))
        throw std::runtime_error("Este campo ainda não aceita expressões.");
    auto p=feature.p;auto bindings=p["expressions"].toObject();
    if(expression.trimmed().isEmpty()) bindings.remove(field);
    else bindings[field]=expression;
    if(bindings.isEmpty()) p.remove("expressions");else p["expressions"]=bindings;
    auto document=json();auto features=document["features"].toArray();
    for(int i=0;i<features.size();++i) {
        auto entry=features[i].toObject();
        if(entry["id"]==id) {entry["parameters"]=p;features[i]=entry;break;}
    }
    document["features"]=features;document["version"]=3;commit(document);
}
