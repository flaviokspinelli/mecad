#include "dependencies.h"
#include <QJsonObject>
#include <QSet>
#include <functional>
#include <stdexcept>

namespace {
void check(bool ok, const QString &message) {
    if (!ok) throw std::runtime_error(message.toStdString());
}
}
DependencyGraph::DependencyGraph(const QJsonArray &features) {
    for (const auto &value : features) {
        check(value.isObject(), "Operação inválida no grafo de dependências.");
        const auto feature = value.toObject();
        const auto id = feature["id"].toString();
        check(!id.isEmpty() && !inputs.contains(id), "Identificador vazio ou duplicado no histórico.");
        check(feature["parameters"].isObject(), "Parâmetros inválidos na operação " + id);
        history.append(id);
        inputs.insert(id, {});
        outputs.insert(id, {});
    }
    for (const auto &value : features) {
        const auto feature = value.toObject();
        const auto id = feature["id"].toString();
        const auto parameters = feature["parameters"].toObject();
        for (const auto *key : {"source", "target", "tool", "support"}) {
            const auto reference = parameters[key];
            check(reference.isUndefined() || reference.isNull() || reference.isString(),
                  QString("Referência inválida em %1: %2.").arg(id, key));
            const auto input = reference.toString();
            if (input.isEmpty()) continue;
            check(inputs.contains(input), QString("A operação %1 depende de uma etapa ausente: %2 (%3).")
                  .arg(id, input, key));
            if (!inputs[id].contains(input)) {
                inputs[id].append(input);
                outputs[input].append(id);
            }
        }
    }
    QMap<QString, int> states;
    QStringList path;
    std::function<void(const QString &)> visit = [&](const QString &id) {
        if (states.value(id) == 2) return;
        if (states.value(id) == 1) {
            auto cycle = path.mid(path.indexOf(id));
            cycle.append(id);
            check(false, "Dependência circular: " + cycle.join(" → "));
        }
        states[id] = 1;
        path.append(id);
        for (const auto &input : inputs[id]) visit(input);
        path.removeLast();
        states[id] = 2;
        sorted.append(id);
    };
    for (const auto &id : history) visit(id);
}
void DependencyGraph::requireId(const QString &id) const {
    check(inputs.contains(id), "Operação inexistente no grafo: " + id);
}
QStringList DependencyGraph::order() const { return sorted; }
QStringList DependencyGraph::dependencies(const QString &id) const {
    requireId(id);
    return inputs[id];
}
QStringList DependencyGraph::dependents(const QString &id, bool recursive) const {
    requireId(id);
    QSet<QString> found;
    QStringList pending = outputs[id];
    while (!pending.empty()) {
        const auto next = pending.takeLast();
        if (found.contains(next)) continue;
        found.insert(next);
        if (recursive) pending.append(outputs[next]);
    }
    QStringList result;
    for (const auto &candidate : sorted)
        if (found.contains(candidate)) result.append(candidate);
    return result;
}
void DependencyGraph::requireHistoryOrder() const {
    QSet<QString> previous;
    for (const auto &id : history) {
        for (const auto &input : inputs[id])
            check(previous.contains(input), QString("A operação %1 depende de uma etapa futura: %2.").arg(id, input));
        previous.insert(id);
    }
}
