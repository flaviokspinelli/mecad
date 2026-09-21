#include "recovery.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

namespace {
void requireRecovery(bool ok, const QString &message) {
    if (!ok) throw std::runtime_error(message.toStdString());
}
}

RecoveryStore::RecoveryStore(QString root) : directory(QDir(root).absolutePath()),
    id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    requireRecovery(QDir().mkpath(directory), "Não foi possível criar a pasta de recuperação.");
    owner = std::make_unique<QLockFile>(pathFor(id)+".lock");
    owner->setStaleLockTime(0); // A live owner never expires merely due to age.
    requireRecovery(owner->tryLock(0), "Não foi possível reservar a recuperação desta sessão.");
}

QString RecoveryStore::pathFor(const QString &session) const {
    requireRecovery(!QUuid(session).isNull() && QUuid(session).toString(QUuid::WithoutBraces) == session,
                    "Identificador de recuperação inválido.");
    return QDir(directory).filePath(session+".json");
}

QJsonObject RecoveryStore::read(const QString &session) const {
    const auto path = pathFor(session);
    requireRecovery(!QFileInfo(path).isSymLink(), "Atalhos não são arquivos de recuperação.");
    QFile file(path);
    requireRecovery(file.open(QIODevice::ReadOnly), file.errorString());
    requireRecovery(file.size() < 210000000, "Arquivo de recuperação muito grande.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    requireRecovery(error.error == QJsonParseError::NoError && document.isObject(), "Recuperação inválida ou incompleta.");
    const auto root = document.object();
    requireRecovery(root["format"] == "MecaCADRecovery" && root["version"].toDouble() == 1 &&
                        root["session"] == session && root["document"].isObject() &&
                        root["originalPath"].isString() && root["savedAt"].isString(),
                    "Formato de recuperação não suportado.");
    requireRecovery(QDateTime::fromString(root["savedAt"].toString(), Qt::ISODateWithMs).isValid(),
                    "Data de recuperação inválida.");
    return root;
}

void RecoveryStore::write(const Model &model) {
    if (!model.dirty) return;
    const QJsonObject root{{"format","MecaCADRecovery"},{"version",1},{"session",id},
        {"originalPath",model.filePath},{"savedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"document",model.json()}};
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    requireRecovery(bytes.size() < 210000000, "Projeto muito grande para recuperação automática.");
    QSaveFile file(pathFor(id));
    requireRecovery(file.open(QIODevice::WriteOnly), file.errorString());
    requireRecovery(file.write(bytes) == bytes.size(), file.errorString());
    requireRecovery(file.commit(), file.errorString());
}

void RecoveryStore::clear() {
    const auto path = pathFor(id);
    if (QFile::exists(path))
        requireRecovery(QFile::remove(path), "Não foi possível remover a recuperação desta sessão.");
}

QList<RecoveryEntry> RecoveryStore::available() const {
    QList<RecoveryEntry> entries;
    for (const auto &file : QDir(directory).entryInfoList({"*.json"}, QDir::Files | QDir::NoSymLinks)) {
        const auto session = file.completeBaseName();
        if (session == id) continue;
        try {
            QLockFile claim(pathFor(session)+".lock");
            claim.setStaleLockTime(0);
            if (!claim.tryLock(0)) continue;
            const auto root = read(session);
            entries.append({session,root["originalPath"].toString(),
                            QDateTime::fromString(root["savedAt"].toString(), Qt::ISODateWithMs)});
        } catch (const std::exception &) {
            // Preserve unreadable/unsupported records for manual diagnosis.
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.savedAt > b.savedAt; });
    return entries;
}

QString RecoveryStore::recover(const QString &session, Model &destination) {
    requireRecovery(!destination.dirty, "Salve ou descarte o documento atual antes de recuperar outro.");
    requireRecovery(session != id, "Esta é a recuperação da sessão atual.");
    QLockFile claim(pathFor(session)+".lock");
    claim.setStaleLockTime(0);
    requireRecovery(claim.tryLock(0), "A recuperação está em uso em outra sessão.");
    const auto root = read(session);
    Model recovered;
    recovered.loadJson(root["document"].toObject());
    // Recovered work is an unsaved copy. Saving cannot silently overwrite the original.
    recovered.filePath = root["originalPath"].toString();
    recovered.markUnsaved();
    write(recovered); // A new durable copy must exist before retiring the old journal.
    requireRecovery(QFile::remove(pathFor(session)), "A cópia foi preservada, mas a recuperação antiga não pôde ser removida.");
    recovered.filePath.clear();
    destination = std::move(recovered);
    return root["originalPath"].toString();
}
