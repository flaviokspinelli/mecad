#include "model.h"
#include <BRepCheck_Analyzer.hxx>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <StlAPI_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <cmath>

class CoreTests : public QObject {
    Q_OBJECT
  private slots:
    void importStlMeshes() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Model source;
        auto box = source.add("box", {{"w", 20}, {"h", 30}, {"d", 10}});
        auto binary = dir.filePath("peça.STL");
        source.exportStl(binary, box);
        auto ascii = dir.filePath("ascii.stl");
        StlAPI_Writer writer;
        writer.ASCIIMode() = true;
        QVERIFY(writer.Write(source.get(box).shape, ascii.toUtf8().constData()));
        for (const auto &path : {binary, ascii}) {
            Model model;
            auto id = model.importStl(path);
            QCOMPARE(model.get(id).type, QString("mesh"));
            QCOMPARE(model.triangles().size(), size_t(12));
            QVERIFY(model.undo());
            QVERIFY(model.features.empty());
            QVERIFY(model.redo());
            auto saved = dir.filePath("mesh.mcad");
            model.save(saved);
            QVERIFY(QFile::remove(path));
            Model loaded;
            loaded.load(saved);
            QCOMPARE(loaded.triangles().size(), size_t(12));
            loaded.exportStl(dir.filePath("roundtrip.stl"));
            Model again;
            again.importStl(dir.filePath("roundtrip.stl"));
            QCOMPARE(again.triangles().size(), size_t(12));
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, loaded.exportStep(dir.filePath("mesh.step")));
        }
        QFile invalid(dir.filePath("invalid.stl"));
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("not an STL");
        invalid.close();
        Model unchanged;
        unchanged.add("box", {{"w", 1}, {"h", 1}, {"d", 1}});
        auto before = unchanged.json();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, unchanged.importStl(invalid.fileName()));
        QCOMPARE(unchanged.json(), before);
    }
    void parametricRoundTrip() {
        Model m;
        auto s = m.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"w", 40}, {"h", 30}});
        auto e = m.add("extrude", {{"source", s}, {"d", 10}});
        QVERIFY(std::abs(Model::volume(m.get(e).shape) - 12000) < .001);
        auto p = m.get(s).p;
        p["w"] = 60;
        m.edit(s, p, "Base");
        QVERIFY(std::abs(Model::volume(m.get(e).shape) - 18000) < .001);
        QVERIFY(m.undo());
        QVERIFY(std::abs(Model::volume(m.get(e).shape) - 12000) < .001);
        QVERIFY(m.redo());
        QVERIFY(std::abs(Model::volume(m.get(e).shape) - 18000) < .001);
        QTemporaryDir dir;
        auto file = dir.filePath("part.mcad");
        m.save(file);
        Model restored;
        restored.load(file);
        QCOMPARE(restored.features.size(), size_t(2));
        QVERIFY(std::abs(Model::volume(restored.get(e).shape) - 18000) < .001);
    }
    void rollbackAndDependencies() {
        Model m;
        auto b = m.add("box", {{"w", 10}, {"h", 20}, {"d", 30}});
        auto before = m.json();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, m.edit(b, {{"w", -1}}, "Bad"));
        QCOMPARE(m.json(), before);
        auto hole = m.add("hole", {{"target", b}, {"u", 5}, {"v", 10}, {"offset", -1}, {"r", 2}, {"d", 40}});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, m.remove(b));
        QVERIFY(std::abs(Model::volume(m.get(hole).shape) - (6000 - M_PI * 4 * 30)) < .001);
        QCOMPARE(m.bodies().size(), size_t(1));
    }
    void exports() {
        Model m;
        auto s = m.add("sketch", {{"profile", "circle"}, {"plane", "XY"}, {"r", 10}});
        auto e = m.add("extrude", {{"source", s}, {"d", 20}});
        QTemporaryDir dir;
        auto step = dir.filePath("peça.step"), stl = dir.filePath("part.stl"), dxf = dir.filePath("part.dxf");
        m.exportStep(step);
        m.exportStl(stl);
        m.exportDxf(dxf, s);
        Model imported;
        auto id = imported.importStep(step);
        QVERIFY(std::abs(Model::volume(imported.get(id).shape) - 2000 * M_PI) < .01);
        QVERIFY(BRepCheck_Analyzer(imported.get(id).shape).IsValid());
        TopoDS_Shape shape;
        StlAPI_Reader reader;
        QVERIFY(reader.Read(shape, stl.toUtf8().constData()));
        QVERIFY(!shape.IsNull());
        QFile f(dxf);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.readAll().contains("CIRCLE"));
        auto data = imported.json();
        Model again;
        again.loadJson(data);
        QVERIFY(std::abs(Model::volume(again.get(id).shape) - 2000 * M_PI) < .01);
    }
    void invalidLoadIsAtomic() {
        Model m;
        m.add("sphere", {{"r", 5}});
        auto before = m.json();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, m.loadJson({{"version", 999}}));
        QCOMPARE(m.json(), before);
    }
    void operations() {
        Model m;
        auto a = m.add("box", {{"w", 20}, {"h", 20}, {"d", 20}});
        auto b = m.add("cylinder", {{"x", 10}, {"y", 10}, {"r", 3}, {"d", 20}});
        auto c = m.add("boolean", {{"target", a}, {"tool", b}, {"mode", "cut"}});
        QVERIFY(std::abs(Model::volume(m.get(c).shape) - (8000 - 180 * M_PI)) < .001);
        auto copy = m.add("copy", {{"source", c}, {"x", 30}, {"angle", 45}});
        QCOMPARE(m.bodies().size(), size_t(2));
        QVERIFY(std::abs(Model::volume(m.get(copy).shape) - Model::volume(m.get(c).shape)) < .001);
        QVERIFY(!m.triangles().empty());
    }
    void planesAndProfiles() {
        for (auto plane : {"XY", "XZ", "YZ"}) {
            Model m;
            auto s = m.add("sketch",
                           {{"profile", "polyline"},
                            {"plane", plane},
                            {"points", QJsonArray{QJsonArray{0, 0}, QJsonArray{20, 0}, QJsonArray{0, 20}}},
                            {"closed", true}});
            auto e = m.add("extrude", {{"source", s}, {"d", -5}});
            QVERIFY(std::abs(Model::volume(m.get(e).shape) - 1000) < .001);
        }
    }
    void openProfilesRejected() {
        Model m;
        auto s = m.add("sketch", {{"profile", "polyline"},
                                  {"points", QJsonArray{QJsonArray{0, 0}, QJsonArray{10, 10}}},
                                  {"closed", false}});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, m.add("extrude", {{"source", s}, {"d", 10}}));
        QCOMPARE(m.features.size(), size_t(1));
    }
    void revolveAndArc() {
        Model m;
        auto s = m.add("sketch",
                       {{"profile", "rectangle"}, {"plane", "XZ"}, {"x", 10}, {"y", 0}, {"w", 5}, {"h", 20}});
        auto r = m.add("revolve", {{"source", s}, {"angle", 360}, {"axis", 0}});
        QVERIFY(std::abs(Model::volume(m.get(r).shape) - M_PI * (225 - 100) * 20) < .01);
        auto a = m.add("sketch", {{"profile", "arc"},
                                  {"plane", "XY"},
                                  {"x1", 10},
                                  {"y1", 0},
                                  {"xm", 0},
                                  {"ym", 10},
                                  {"x2", -10},
                                  {"y2", 0}});
        QTemporaryDir dir;
        m.exportDxf(dir.filePath("arc.dxf"), a);
        QVERIFY(QFileInfo(dir.filePath("arc.dxf")).size() > 100);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
