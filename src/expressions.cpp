#include "expressions.h"
#include <QSet>
#include <cmath>
#include <functional>
#include <numbers>
#include <stdexcept>

namespace parameters {
namespace {
void require(bool ok, const QString &message) {
    if (!ok) throw std::runtime_error(message.toStdString());
}
const QMap<QString, Quantity> units = {
    {"mm", {1,1,0}}, {"cm", {10,1,0}}, {"m", {1000,1,0}},
    {"in", {25.4,1,0}}, {"deg", {std::numbers::pi/180,0,1}},
    {"rad", {1,0,1}}, {"pi", {std::numbers::pi,0,0}}
};
bool letter(QChar c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool digit(QChar c) { return c >= '0' && c <= '9'; }
bool validName(const QString &s) {
    if (s.isEmpty() || s.size()>64 || !letter(s[0]) || units.contains(s)) return false;
    for (auto c:s) if (!letter(c) && !digit(c)) return false;
    return true;
}
Quantity checked(Quantity q) {
    require(std::isfinite(q.value), "Resultado numérico não finito.");
    require(std::abs(q.length)<=16 && std::abs(q.angle)<=16, "Dimensão da expressão excede o limite.");
    return q;
}
class Parser {
    const QString &text;
    std::function<Quantity(const QString&)> lookup;
    int pos=0, depth=0;
    void spaces() { while(pos<text.size() && text[pos].isSpace()) ++pos; }
    bool take(QChar c) { spaces(); if(pos<text.size() && text[pos]==c) {++pos;return true;} return false; }
    QString name() {
        spaces(); int start=pos;
        if(pos<text.size() && letter(text[pos])) {
            ++pos; while(pos<text.size() && (letter(text[pos]) || digit(text[pos]))) ++pos;
        }
        return text.mid(start,pos-start);
    }
    Quantity sum() {
        auto a=product();
        for(;;) {
            bool plus=take('+'); if(!plus && !take('-')) return a;
            auto b=product();
            require(a.length==b.length && a.angle==b.angle,"Não é possível somar grandezas de unidades incompatíveis.");
            a.value += plus ? b.value : -b.value; a=checked(a);
        }
    }
    Quantity product() {
        auto a=atom();
        for(;;) {
            bool multiply=take('*'); if(!multiply && !take('/')) return a;
            auto b=atom();
            require(multiply || b.value!=0,"Divisão por zero.");
            a=checked({multiply ? a.value*b.value : a.value/b.value,
                       a.length+(multiply ? b.length : -b.length), a.angle+(multiply ? b.angle : -b.angle)});
        }
    }
    Quantity atom() {
        require(++depth<=64,"Expressão excede o limite de aninhamento.");
        struct Guard { int &n; ~Guard(){--n;} } guard{depth};
        if(take('+')) return atom();
        if(take('-')) {auto q=atom();q.value=-q.value;return q;}
        if(take('(')) {auto q=sum();require(take(')'),"Parêntese não fechado.");return q;}
        spaces();
        if(pos<text.size() && (digit(text[pos]) || text[pos]=='.' || text[pos]==',')) {
            int start=pos, digits=0;
            while(pos<text.size() && digit(text[pos])) {++pos;++digits;}
            if(pos<text.size() && (text[pos]=='.' || text[pos]==',')) {
                ++pos;while(pos<text.size() && digit(text[pos])) {++pos;++digits;}
            }
            require(digits>0,"Número inválido.");
            if(pos<text.size() && (text[pos]=='e' || text[pos]=='E')) {
                ++pos;if(pos<text.size() && (text[pos]=='+' || text[pos]=='-')) ++pos;
                int exponentStart=pos;while(pos<text.size() && digit(text[pos])) ++pos;
                require(pos>exponentStart,"Expoente inválido.");
            }
            bool ok=false; double value=text.mid(start,pos-start).replace(',','.').toDouble(&ok);
            require(ok && std::isfinite(value),"Número fora do intervalo.");
            int end=pos;auto suffix=name();
            if(!suffix.isEmpty() && units.contains(suffix) && suffix!="pi") {
                auto q=units[suffix];q.value*=value;return checked(q);
            }
            pos=end;return {value,0,0};
        }
        auto id=name();require(!id.isEmpty(),QString("Expressão inválida na posição %1.").arg(pos+1));
        return checked(units.contains(id) ? units[id] : lookup(id));
    }
public:
    Parser(const QString &s,std::function<Quantity(const QString&)> get):text(s),lookup(std::move(get)) {
        require(!s.trimmed().isEmpty() && s.size()<=4096,"Expressão vazia ou longa demais.");
    }
    Quantity run() {auto q=sum();spaces();require(pos==text.size(),QString("Símbolo inesperado na posição %1.").arg(pos+1));return q;}
};
}
Quantity evaluate(const QString &expression,const QMap<QString,Quantity> &symbols) {
    return Parser(expression,[&](const QString &id) {
        require(symbols.contains(id),"Parâmetro desconhecido: "+id);return symbols[id];
    }).run();
}
QMap<QString,Quantity> resolve(const QMap<QString,QString> &definitions) {
    require(definitions.size()<=256,"Máximo de 256 parâmetros por tabela.");
    for(auto it=definitions.begin();it!=definitions.end();++it)
        require(validName(it.key()),"Nome de parâmetro inválido ou reservado: "+it.key());
    QMap<QString,Quantity> values; QSet<QString> visiting;
    std::function<Quantity(const QString&)> visit=[&](const QString &id) -> Quantity {
        if(values.contains(id)) return values[id];
        require(definitions.contains(id),"Parâmetro desconhecido: "+id);
        require(!visiting.contains(id),"Dependência circular no parâmetro: "+id);
        visiting.insert(id);
        auto q=Parser(definitions[id],visit).run();
        visiting.remove(id);values.insert(id,q);return q;
    };
    for(auto it=definitions.begin();it!=definitions.end();++it) visit(it.key());
    return values;
}
}
