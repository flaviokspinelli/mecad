#include "recovery.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

class RecoveryTests : public QObject {
    Q_OBJECT
  private slots:
    void everyNativeVersionRestores() {
        for(int version=1;version<=3;++version) {
            QTemporaryDir directory;QString id;QJsonObject expected;
            {
                RecoveryStore writer(directory.path());id=writer.sessionId();Model m;
                auto sketch=m.add("sketch",{{"profile","rectangle"},{"w",10},{"h",10}});
                if(version>=2)m.constrainSketch(sketch,sketch::Relation::Fixed,"p0",{},{0,0});
                auto solid=m.add("extrude",{{"source",sketch},{"d",2}});
                if(version==3){m.setParameters({{"depth","3 mm"}});m.setExpression(solid,"d","depth");}
                QCOMPARE(m.json()["version"].toInt(),version);expected=m.json();writer.write(m);
            }
            RecoveryStore reader(directory.path());Model restored;reader.recover(id,restored);
            QCOMPARE(restored.json(),expected);QVERIFY(restored.dirty);QVERIFY(restored.filePath.isEmpty());
            restored.rebuild();QCOMPARE(restored.json(),expected);
        }
    }
    void unreadableJournalIsReportedAndPreserved() {
        QTemporaryDir directory;QString id;
        {RecoveryStore writer(directory.path());id=writer.sessionId();Model m;m.add("box",{});writer.write(m);}
        QFile journal(directory.filePath(id+".json"));QVERIFY(journal.open(QIODevice::WriteOnly));
        journal.write("{broken");journal.close();
        RecoveryStore reader(directory.path());const auto scan=reader.scan();
        QVERIFY(scan.entries.empty());QCOMPARE(scan.warnings.size(),1);QVERIFY(scan.warnings.front().contains(id));
        Model current;const auto before=current.json();
        QVERIFY_THROWS_EXCEPTION(std::exception,reader.recover(id,current));QCOMPARE(current.json(),before);
        QVERIFY(journal.open(QIODevice::ReadOnly));QCOMPARE(journal.readAll(),QByteArray("{broken"));
    }
    void failedWriteKeepsPreviousCopy() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto root = directory.filePath("journals");
        RecoveryStore writer(root);
        Model model; auto id=model.add("box", {{"w",10}});
        writer.write(model);
        const auto filename = writer.sessionId()+".json";
        QFile file(QDir(root).filePath(filename)); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto before=file.readAll(); file.close();
        QVERIFY(QDir(directory.path()).rename("journals","unavailable"));
        model.edit(id, {{"w",20}}, "Changed");
        QVERIFY_THROWS_EXCEPTION(std::exception, writer.write(model));
        QFile previous(directory.filePath("unavailable/"+filename));
        QVERIFY(previous.open(QIODevice::ReadOnly)); QCOMPARE(previous.readAll(),before);
        previous.close();
        QVERIFY(QDir(directory.path()).rename("unavailable","journals"));
    }
    void dirtyDestinationIsNotReplaced() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QString id;
        {
            RecoveryStore source(directory.path()); id=source.sessionId();
            Model model; model.add("box", {{"w",10}}); source.write(model);
        }
        RecoveryStore reader(directory.path());
        Model current; current.add("sphere", {{"r",3}});
        const auto before=current.json(); reader.write(current);
        QFile own(QDir(directory.path()).filePath(reader.sessionId()+".json"));
        QVERIFY(own.open(QIODevice::ReadOnly)); const auto bytes=own.readAll(); own.close();
        QVERIFY_THROWS_EXCEPTION(std::exception, reader.recover(id,current));
        QCOMPARE(current.json(),before); QVERIFY(current.dirty);
        QVERIFY(own.open(QIODevice::ReadOnly)); QCOMPARE(own.readAll(),bytes);
        QCOMPARE(reader.available().size(),1);
    }
    void sessionsStayIsolated() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        RecoveryStore reader(directory.path());
        QString firstId, secondId;
        {
            RecoveryStore first(directory.path()), second(directory.path());
            firstId = first.sessionId(); secondId = second.sessionId();
            QVERIFY(firstId != secondId);
            Model a, b;
            a.add("box", {{"w",10},{"h",10},{"d",10}});
            b.add("box", {{"w",20},{"h",10},{"d",10}});
            first.write(a); second.write(b);
            QVERIFY(reader.available().empty());
            Model untouched;
            QVERIFY_THROWS_EXCEPTION(std::exception, reader.recover(firstId, untouched));
            first.clear();
            QVERIFY(QFile::exists(QDir(directory.path()).filePath(secondId+".json")));
        }
        QCOMPARE(reader.available().size(), 1);
        QCOMPARE(reader.available().front().id, secondId);
        Model restored;
        reader.recover(secondId, restored);
        QVERIFY(restored.dirty); QVERIFY(restored.filePath.isEmpty());
        QVERIFY(std::abs(Model::volume(restored.features.front().shape)-2000)<.001);
        auto recoveredState = restored.json();
        restored.edit(restored.features.front().id, {{"w",25},{"h",10},{"d",10}}, "Edited");
        QVERIFY(restored.undo()); QCOMPARE(restored.json(),recoveredState);
        QVERIFY(restored.dirty); // No on-disk saved baseline exists for this recovered copy.
        QVERIFY(reader.available().empty());
        reader.clear();
        QVERIFY(QDir(directory.path()).entryList({"*.json"}, QDir::Files).empty());
    }
    void corruptRecoveryIsPreserved() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QString id;
        {
            RecoveryStore writer(directory.path()); id=writer.sessionId();
            Model model; model.add("box", {{"w",10}}); writer.write(model);
        }
        const auto path = QDir(directory.path()).filePath(id+".json");
        QFile file(path); QVERIFY(file.open(QIODevice::ReadWrite));
        auto root = QJsonDocument::fromJson(file.readAll()).object();
        root["document"] = QJsonObject{{"format","MecaCAD"},{"version",999}};
        file.resize(0); file.seek(0); file.write(QJsonDocument(root).toJson()); file.close();
        RecoveryStore reader(directory.path());
        Model current; current.add("sphere", {{"r",3}}); current.dirty=false;
        const auto before = current.json();
        QVERIFY_THROWS_EXCEPTION(std::exception, reader.recover(id,current));
        QCOMPARE(current.json(),before); QVERIFY(!current.dirty);
        QVERIFY(QFile::exists(path));
        QVERIFY_THROWS_EXCEPTION(std::exception, reader.recover("../../outside", current));
        QCOMPARE(current.json(), before);
    }
    void forcedTerminationRestoresWithoutOverwritingOriginal() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QProcess writer;
        writer.start(QCoreApplication::applicationFilePath(), {"--writer",directory.path()});
        QVERIFY(writer.waitForStarted(10000));
        QByteArray output;
        QElapsedTimer timeout; timeout.start();
        while (!output.contains("READY") && timeout.elapsed() < 10000) {
            writer.waitForReadyRead(200);
            output += writer.readAllStandardOutput();
        }
        QVERIFY2(output.contains("READY"), writer.readAllStandardError().constData());
        RecoveryStore reader(QDir(directory.path()).filePath("recoveries"));
        QVERIFY(reader.available().empty());
        const auto originalPath = QDir(directory.path()).filePath("original.mcad");
        QFile original(originalPath); QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes = original.readAll(); original.close();
        writer.kill(); QVERIFY(writer.waitForFinished(10000));
        auto entries = reader.available(); QCOMPARE(entries.size(),1);
        QCOMPARE(entries.front().originalPath, originalPath);
        Model restored;
        QCOMPARE(reader.recover(entries.front().id, restored), originalPath);
        QVERIFY(restored.dirty); QVERIFY(restored.filePath.isEmpty());
        QVERIFY(std::abs(Model::volume(restored.features.front().shape)-2000)<.001);
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(),bytes);
        reader.clear();
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc,argv);
    const auto args = app.arguments();
    if (args.size()==3 && args[1]=="--writer") {
        RecoveryStore store(QDir(args[2]).filePath("recoveries"));
        Model model;
        auto id = model.add("box", {{"w",10},{"h",10},{"d",10}});
        model.save(QDir(args[2]).filePath("original.mcad"));
        model.edit(id, {{"w",20},{"h",10},{"d",10}}, "Changed");
        store.write(model);
        QTextStream(stdout) << "READY\n" << Qt::flush;
        return app.exec();
    }
    RecoveryTests tests;
    return QTest::qExec(&tests,argc,argv);
}
#include "recovery_tests.moc"
