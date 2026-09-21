#pragma once
#include "model.h"
#include <QDateTime>
#include <QLockFile>
#include <memory>

struct RecoveryEntry {
    QString id, originalPath;
    QDateTime savedAt;
};
struct RecoveryScan {
    QList<RecoveryEntry> entries;
    QStringList warnings;
};

// One journal per live document window. Destruction releases ownership, but
// never discards unsaved work: only an explicit save/discard calls clear().
class RecoveryStore {
  public:
    explicit RecoveryStore(QString directory);
    void write(const Model &model);
    void clear();
    QList<RecoveryEntry> available() const;
    RecoveryScan scan() const;
    QString recover(const QString &id, Model &destination);
    QString sessionId() const { return id; }

  private:
    QString directory, id;
    std::unique_ptr<QLockFile> owner;
    QString pathFor(const QString &session) const;
    QJsonObject read(const QString &session) const;
};
