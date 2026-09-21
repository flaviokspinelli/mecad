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
    void historyReorderingIsTransactional() {
        Model model;
        auto base = model.add("box",{{"w",10},{"h",10},{"d",10}},"Base");
        auto independent = model.add("sphere",{{"r",2}},"Sphere");
        auto copy = model.add("copy",{{"source",base},{"x",20}},"Copy");
        const auto before = model.json();
        model.moveFeature(independent,0);
        QCOMPARE(model.features.front().id,independent);
        QCOMPARE(model.get(copy).p["source"].toString(),base);
        QVERIFY(std::abs(Model::volume(model.get(copy).shape)-1000)<.001);
        const auto reordered = model.json();
        QVERIFY_THROWS_EXCEPTION(std::exception,model.moveFeature(base,2));
        QCOMPARE(model.json(),reordered);
        QVERIFY_THROWS_EXCEPTION(std::exception,model.moveFeature(base,-1));
        QVERIFY_THROWS_EXCEPTION(std::exception,model.moveFeature("missing",0));
        QVERIFY(model.undo()); QCOMPARE(model.json(),before);
        model.moveFeature(base,0); // No-op must retain redo.
        QVERIFY(model.redo()); QCOMPARE(model.json(),reordered);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        model.save(directory.filePath("reordered.mcad"));
        Model restored; restored.load(model.filePath);
        QCOMPARE(restored.json(),reordered);
        QCOMPARE(restored.dependencyGraph().dependencies(copy),QStringList({base}));
    }
    void dependencyGraphValidation() {
        auto node = [](QString id, QJsonObject parameters = {}) {
            return QJsonObject{{"id",id},{"parameters",parameters}};
        };
        DependencyGraph graph(QJsonArray{node("d",{{"source","b"},{"target","c"}}),
            node("b",{{"source","a"},{"support","a"}}),node("c",{{"source","a"}}),node("a"),node("e")});
        QCOMPARE(graph.order(),QStringList({"a","b","c","d","e"}));
        QCOMPARE(graph.dependencies("b"),QStringList({"a"}));
        QCOMPARE(graph.dependents("a"),QStringList({"b","c","d"}));
        QCOMPARE(graph.dependents("a",false),QStringList({"b","c"}));
        QVERIFY(graph.dependents("e").empty());
        QVERIFY_THROWS_EXCEPTION(std::exception,graph.requireHistoryOrder());
        QVERIFY_THROWS_EXCEPTION(std::exception,graph.dependencies("missing"));
        QVERIFY_THROWS_EXCEPTION(std::exception,DependencyGraph(QJsonArray{node("a"),node("a")}));
        QVERIFY_THROWS_EXCEPTION(std::exception,DependencyGraph(QJsonArray{node("a",{{"source","missing"}})}));
        QVERIFY_THROWS_EXCEPTION(std::exception,DependencyGraph(QJsonArray{node("a",{{"source",12}})}));
        try {
            DependencyGraph cycle(QJsonArray{node("a",{{"source","b"}}),node("b",{{"source","a"}})});
            QFAIL("Cycle accepted");
        } catch (const std::exception &error) {
            const QString message = QString::fromUtf8(error.what());
            QVERIFY(message.contains("a → b → a"));
        }
        DependencyGraph ordered(QJsonArray{node("a"),node("b",{{"source","a"}})});
        ordered.requireHistoryOrder();
    }
    void failedRebuildPreservesGeometry() {
        Model model;
        auto box = model.add("box",{{"w",10},{"h",10},{"d",10}},"Base");
        auto copy = model.add("copy",{{"source",box},{"x",20}},"Copy");
        const auto before = model.json();
        const auto baseShape = model.get(box).shape;
        const auto copyShape = model.get(copy).shape;
        try {
            model.edit(box,{{"w",-10}},"Base");
            QFAIL("Invalid dimension accepted");
        } catch (const std::exception &error) {
            QVERIFY(QString::fromUtf8(error.what()).contains("Base [" + box + "]"));
        }
        QCOMPARE(model.json(),before);
        QVERIFY(model.get(box).shape.IsSame(baseShape));
        QVERIFY(model.get(copy).shape.IsSame(copyShape));
        auto invalid = before;
        auto features = invalid["features"].toArray();
        auto feature = features[1].toObject();
        feature["parameters"] = QJsonObject{{"source",box},{"angle",1e9}};
        features[1] = feature; invalid["features"] = features;
        QVERIFY_THROWS_EXCEPTION(std::exception,model.commit(invalid));
        QVERIFY(model.get(box).shape.IsSame(baseShape));
        QVERIFY(model.get(copy).shape.IsSame(copyShape));
        QCOMPARE(model.json(),before);
        QCOMPARE(model.dependencyGraph().dependents(box),QStringList({copy}));
        QVERIFY(model.undo()); QCOMPARE(model.features.size(),size_t(1));
        QVERIFY(model.redo()); QCOMPARE(model.json(),before);
    }
    void savedStateTracksUndoRedo() {
        Model model;
        QVERIFY(!model.dirty);
        const auto id = model.add("box", {{"w",10}}, "Box");
        QVERIFY(model.dirty);
        QVERIFY(model.undo()); QVERIFY(!model.dirty);
        QVERIFY(model.redo()); QVERIFY(model.dirty);
        QTemporaryDir directory; QVERIFY(directory.isValid());
        model.save(directory.filePath("saved.mcad"));
        const auto saved = model.json();
        QVERIFY(!model.dirty);
        model.edit(id, {{"w",20}}, "Box"); QVERIFY(model.dirty);
        QVERIFY(model.undo()); QCOMPARE(model.json(),saved); QVERIFY(!model.dirty);
        QVERIFY(model.redo()); QVERIFY(model.dirty);
        model.save(directory.filePath("updated.mcad")); QVERIFY(!model.dirty);
        QVERIFY(model.undo()); QVERIFY(model.dirty);
        QVERIFY(model.redo()); QVERIFY(!model.dirty);
        model.clear(); QVERIFY(!model.dirty);
    }
    void legacyV1FixtureRoundTrip() {
        const auto path = QFINDTESTDATA("fixtures/v1-basic.mcad");
        QVERIFY(!path.isEmpty());
        QFile fixture(path); QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto expected = QJsonDocument::fromJson(fixture.readAll()).object();
        Model m; m.load(path);
        QCOMPARE(m.json(), expected);
        QVERIFY(std::abs(Model::volume(m.get("base-extrude").shape)-6000)<.001);
        QTemporaryDir dir; QVERIFY(dir.isValid());
        m.save(dir.filePath("saved.mcad"));
        Model reopened; reopened.load(dir.filePath("saved.mcad"));
        QCOMPARE(reopened.json(), expected);
        QVERIFY(!reopened.dirty);
    }
    void invalidOperationModesAreAtomic() {
        Model m;
        auto sketch = m.add("sketch", {{"profile","rectangle"},{"w",10},{"h",10}});
        const auto before = m.json();
        QVERIFY_THROWS_EXCEPTION(std::exception, m.add("extrude", {{"source",sketch},{"d",10},{"mode","ctu"}}));
        QCOMPARE(m.json(), before);
        QVERIFY_THROWS_EXCEPTION(std::exception, m.add("sketch", {{"profile","rectangle"},{"plane","YX"}}));
        QCOMPARE(m.json(), before);
        QVERIFY_THROWS_EXCEPTION(std::exception, m.add("sketch", {{"profile","rectangle"},{"closed","false"}}));
        QCOMPARE(m.json(), before);
        auto body = m.add("extrude", {{"source",sketch},{"d",10}});
        const auto solid = m.json();
        QVERIFY_THROWS_EXCEPTION(std::exception, m.add("transform", {{"source",body},{"angle",90},{"axis","W"}}));
        QCOMPARE(m.json(), solid);
        QVERIFY(m.undo()); QCOMPARE(m.json(), before);
        QVERIFY(m.redo()); QCOMPARE(m.json(), solid);
    }
    void rejectedDocumentsPreserveSession() {
        Model m;
        auto id = m.add("box", {{"w",10},{"h",20},{"d",30}});
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        m.save(dir.filePath("original.mcad"));
        const auto original = m.json();
        const auto path = m.filePath;
        m.edit(id, {{"w",20},{"h",20},{"d",30}}, "Changed");
        const auto edited = m.json();
        QVERIFY(m.undo());
        const bool dirty = m.dirty;
        QList<QJsonObject> invalid;
        auto wrongVersion = original; wrongVersion["version"] = 1.5; invalid.append(wrongVersion);
        auto future = original; future["version"] = 2; invalid.append(future);
        auto units = original; units["units"] = "in"; invalid.append(units);
        auto extra = original; extra["unrecognizedData"] = true; invalid.append(extra);
        const auto feature = original["features"].toArray().first().toObject();
        for (const auto &key : {"parameters", "visible", "name"}) {
            auto changed = feature; changed[key] = QJsonArray{1};
            auto root = original; root["features"] = QJsonArray{changed}; invalid.append(root);
        }
        auto duplicate = original; duplicate["features"] = QJsonArray{feature,feature}; invalid.append(duplicate);
        for (auto target : {QString("missing"), id}) {
            auto changed = feature; changed["type"] = "transform";
            changed["parameters"] = QJsonObject{{"source",target}};
            auto root = original; root["features"] = QJsonArray{changed}; invalid.append(root);
        }
        auto unknown = feature; unknown["newMetadata"] = 1;
        auto root = original; root["features"] = QJsonArray{unknown}; invalid.append(root);
        for (const auto &document : invalid) {
            QVERIFY_THROWS_EXCEPTION(std::exception, m.loadJson(document));
            QCOMPARE(m.json(), original); QCOMPARE(m.filePath, path); QCOMPARE(m.dirty, dirty);
            QVERIFY_THROWS_EXCEPTION(std::exception, m.commit(document));
            QCOMPARE(m.json(), original);
        }
        // Failure must preserve redo, not merely the visible shape.
        QVERIFY(m.redo()); QCOMPARE(m.json(), edited);
        QVERIFY(m.undo()); QCOMPARE(m.json(), original);
    }
    void noOpCommandsPreserveHistory() {
        Model m;
        auto id = m.add("box", {{"w",10},{"h",10},{"d",10}}, "Box");
        QTemporaryDir dir; QVERIFY(dir.isValid());
        m.save(dir.filePath("part.mcad"));
        const auto clean = m.json();
        m.edit(id, m.get(id).p, m.get(id).name);
        m.commit(clean);
        QVERIFY(!m.dirty);
        m.edit(id, {{"w",20},{"h",10},{"d",10}}, "Box");
        const auto changed = m.json();
        QVERIFY(m.undo()); QCOMPARE(m.json(), clean);
        m.edit(id, m.get(id).p, m.get(id).name);
        QVERIFY_THROWS_EXCEPTION(std::exception, m.remove("missing"));
        QVERIFY(m.redo()); QCOMPARE(m.json(), changed);
        QVERIFY(m.undo()); QCOMPARE(m.json(), clean);
        QVERIFY(m.undo()); QVERIFY(m.features.empty());
        QVERIFY(!m.undo());
    }
    void failedSaveAndParsePreserveSession() {
        Model m;
        auto id = m.add("box", {{"w",10}});
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto path = dir.filePath("original.mcad");
        m.save(path);
        QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes = original.readAll(); original.close();
        m.edit(id, {{"w",20}}, "Changed");
        const auto before = m.json();
        QVERIFY_THROWS_EXCEPTION(std::exception, m.save(dir.filePath("missing/part.mcad")));
        QCOMPARE(m.json(), before); QCOMPARE(m.filePath, path); QVERIFY(m.dirty);
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), bytes); original.close();
        QFile broken(dir.filePath("broken.mcad")); QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("{invalid"); broken.close();
        QVERIFY_THROWS_EXCEPTION(std::exception, m.load(broken.fileName()));
        QCOMPARE(m.json(), before); QCOMPARE(m.filePath, path); QVERIFY(m.dirty);
        QVERIFY(m.undo()); QCOMPARE(m.get(id).p["w"].toDouble(), 10.);
    }
    void associativeFaceSketchCut() {
        Model model;
        auto body = model.add("box", {{"w",30},{"h",30},{"d",10}});
        QString plane; int face = -1;
        for (const auto &triangle : model.triangles()) {
            auto candidate = Model::facePlane(model.get(body).shape, triangle.face);
            if (Model::planeNormal(candidate).z() > .99) { plane=candidate; face=triangle.face; break; }
        }
        QVERIFY(face >= 0);
        auto center = Model::planeCoordinates(plane, {15,15,10});
        auto sketch = model.add("sketch", {{"profile","rectangle"},{"plane",plane},
            {"x",center.x()-5},{"y",center.y()-5},{"w",10},{"h",10},
            {"support",body},{"supportFace",face},{"supportFaceCount",6}});
        auto cut = model.add("extrude", {{"source",sketch},{"target",body},{"mode","cut"},{"d",-2}});
        QVERIFY(std::abs(Model::volume(model.get(cut).shape)-8800)<.001);
        auto before = model.json();
        model.edit(body, {{"w",30},{"h",30},{"d",20}}, "Box");
        QVERIFY(std::abs(Model::volume(model.get(cut).shape)-17800)<.001);
        QVERIFY(std::abs(Model::planePoint(model.get(sketch).p["plane"].toString(),0,0).z()-20)<.001);
        auto changed = model.json();
        QVERIFY(model.undo()); QCOMPARE(model.json(),before);
        QVERIFY(model.redo()); QCOMPARE(model.json(),changed);
        QTemporaryDir dir;
        model.save(dir.filePath("parametric.mcad"));
        Model reopened; reopened.load(dir.filePath("parametric.mcad"));
        QCOMPARE(reopened.json(), changed);
        QVERIFY_THROWS_EXCEPTION(std::exception, model.remove(body));
        QCOMPARE(model.json(),changed);
        model.deleteBody(cut);
        QVERIFY(model.bodies().empty());
        QVERIFY(model.undo()); QCOMPARE(model.json(),changed);
    }
    void selectedEdgeFilletPersistence() {
        Model model;
        auto box = model.add("box", {{"w",30},{"h",30},{"d",10}});
        auto before = model.json();
        QVERIFY_THROWS_EXCEPTION(std::exception, model.add("fillet", {{"source",box},{"r",2},{"edges",QJsonArray{999}}}));
        QCOMPARE(model.json(), before);
        auto fillet = model.add("fillet", {{"source",box},{"r",2},{"edges",QJsonArray{0}}});
        QVERIFY(BRepCheck_Analyzer(model.get(fillet).shape).IsValid());
        const double volume = Model::volume(model.get(fillet).shape);
        QVERIFY(volume < 9000 && volume > 8800);
        auto good = model.json();
        QVERIFY_THROWS_EXCEPTION(std::exception, model.edit(fillet, {{"source",box},{"r",100},{"edges",QJsonArray{0}}}, "Fillet"));
        QCOMPARE(model.json(), good);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        model.save(dir.filePath("part.mcad"));
        Model loaded; loaded.load(dir.filePath("part.mcad"));
        QVERIFY(std::abs(Model::volume(loaded.get(fillet).shape)-volume)<.001);
        loaded.exportStep(dir.filePath("part.step"));
        Model imported; imported.importStep(dir.filePath("part.step"));
        QVERIFY(std::abs(Model::volume(imported.features.back().shape)-volume)<.001);
        QVERIFY(model.undo()); QCOMPARE(model.json(), before);
        QVERIFY(model.redo()); QCOMPARE(model.json(), good);
    }
    void planarFaceFrames() {
        Model model;
        auto box = model.add("box", {{"w",30},{"h",30},{"d",10}});
        auto body = model.add("transform", {{"source",box},{"axis","Y"},{"angle",30},{"x",15}});
        int top = -1;
        QString plane;
        for (const auto &triangle : model.triangles()) {
            auto candidate = Model::facePlane(model.get(body).shape, triangle.face);
            if (Model::planeNormal(candidate).z() > .8) { top=triangle.face; plane=candidate; break; }
        }
        QVERIFY(top >= 0);
        auto point = Model::planePoint(plane, 4, 5);
        auto local = Model::planeCoordinates(plane, point);
        QVERIFY(std::abs(local.x()-4)<1e-4 && std::abs(local.y()-5)<1e-4);
        auto sketch = model.add("sketch", {{"plane",plane},{"profile","rectangle"},{"w",4},{"h",5}});
        auto extrude = model.add("extrude", {{"source",sketch},{"d",2}});
        QVERIFY(std::abs(Model::volume(model.get(extrude).shape)-40)<.001);
        Model loaded;
        loaded.loadJson(model.json());
        QVERIFY(std::abs(Model::volume(loaded.get(extrude).shape)-40)<.001);
        model.editSketchElements(sketch, {0,1,2,3}, {}, Model::planePoint(plane,1,2)-Model::planePoint(plane,0,0), false);
        QVERIFY(std::abs(Model::volume(model.get(extrude).shape)-40)<.001);
    }
    void extrudeCutValidation() {
        Model model;
        auto body = model.add("box", {{"w",30},{"h",30},{"d",10}});
        auto sketch = model.add("sketch", {{"profile","rectangle"},{"plane","XY"},{"offset",10},
                                          {"x",10},{"y",10},{"w",10},{"h",10}});
        auto before = model.json();
        QVERIFY_THROWS_EXCEPTION(std::exception, model.add("extrude", {{"source",sketch},{"d",-5},{"mode","cut"}}));
        QCOMPARE(model.json(), before);
        QVERIFY_THROWS_EXCEPTION(std::exception, model.add("extrude", {{"source",sketch},{"d",5},{"mode","cut"},{"target",body}}));
        QCOMPARE(model.json(), before);
        auto cut = model.add("extrude", {{"source",sketch},{"d",-5},{"mode","cut"},{"target",body}});
        QVERIFY(std::abs(Model::volume(model.get(cut).shape)-8500)<.01);
        QVERIFY(model.undo());
        QCOMPARE(model.json(), before);
        auto through = model.add("extrude", {{"source",sketch},{"d",-20},{"mode","cut"},{"target",body}});
        QVERIFY(std::abs(Model::volume(model.get(through).shape)-8000)<.01);
    }
    void sketchElementEditing() {
        for (auto plane : {"XY", "XZ", "YZ"}) {
            Model model;
            auto id = model.add("sketch", {{"profile", "rectangle"}, {"plane", plane}, {"w", 20}, {"h", 20}});
            auto original = model.json();
            model.editSketchElements(id, {0,1}, {}, Model::planePoint(plane, 3, 4), false);
            QCOMPARE(model.get(id).p["profile"].toString(), QString("polyline"));
            QVERIFY(model.get(id).p["closed"].toBool());
            auto points = model.get(id).p["points"].toArray();
            QCOMPARE(points[1].toArray(), (QJsonArray{23,4})); // Shared corner moves once.
            QVERIFY(model.undo());
            QCOMPARE(model.json(), original);
            model.editSketchElements(id, {}, {0}, Model::planePoint(plane, 1, 2), false);
            auto vertexPoints = model.get(id).p["points"].toArray();
            QCOMPARE(vertexPoints[0].toArray(), (QJsonArray{1,2}));
            QCOMPARE(vertexPoints[1].toArray(), (QJsonArray{20,0}));
            QVERIFY(model.undo());
            QCOMPARE(model.json(), original);
            model.editSketchElements(id, {0}, {}, {}, true);
            QVERIFY(!model.get(id).p["closed"].toBool());
            QCOMPARE(model.get(id).p["points"].toArray().size(), 4);
            QVERIFY(model.undo());
            model.editSketchElements(id, {0,2}, {}, {}, true);
            QCOMPARE(model.features.size(), size_t(2));
            QVERIFY(model.undo());
            QCOMPARE(model.json(), original);
            model.editSketchElements(id, {}, {0}, {}, true);
            QCOMPARE(model.get(id).p["points"].toArray().size(), 3);
            QVERIFY(model.undo());
            auto solid = model.add("extrude", {{"source", id}, {"d", 10}});
            auto before = model.json();
            QVERIFY_THROWS_EXCEPTION(std::exception, model.editSketchElements(id, {0}, {}, {}, true));
            QCOMPARE(model.json(), before);
            model.editSketchElements(id, {0,1,2,3}, {}, Model::planePoint(plane, 2, 3), false);
            QVERIFY(std::abs(Model::volume(model.get(solid).shape)-4000)<.01);
            QVERIFY(model.undo());
            QCOMPARE(model.json(), before);
        }
    }
    void atomicBatchEdit() {
        Model model;
        auto a = model.add("box", {{"w",10},{"d",10},{"h",10}});
        auto b = model.add("box", {{"w",10},{"d",10},{"h",10},{"x",30}});
        auto before = model.json();
        Model work = model;
        work.deleteBody(a);
        work.deleteBody(b);
        model.commit(work.json());
        QVERIFY(model.bodies().empty());
        QVERIFY(model.undo());
        QCOMPARE(model.json(), before);
        QVERIFY(model.redo());
        QVERIFY(model.bodies().empty());
    }
    void deleteBodyVersusRollback() {
        Model model;
        auto sketch = model.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"w", 20}, {"h", 30}});
        auto extrude = model.add("extrude", {{"source", sketch}, {"d", 10}});
        auto move = model.add("transform", {{"source", extrude}, {"x", 10}});
        auto copy = model.add("copy", {{"source", extrude}, {"x", 50}});
        auto before = model.json();
        model.deleteBody(move);
        QCOMPARE(model.bodies().size(), size_t(1));
        QCOMPARE(model.features[model.bodies()[0]].id, copy);
        QVERIFY(model.consumed(extrude));
        QTemporaryDir dir;
        model.save(dir.filePath("deleted.mcad"));
        Model loaded;
        loaded.load(dir.filePath("deleted.mcad"));
        QCOMPARE(loaded.bodies().size(), size_t(1));
        QVERIFY(model.undo());
        QCOMPARE(model.json(), before);
        model.remove(move);
        QCOMPARE(model.bodies().size(), size_t(2));
        QVERIFY(!model.consumed(extrude));
        QVERIFY(model.undo());
        model.deleteBody(move);
        model.deleteBody(copy);
        QVERIFY(model.bodies().empty());
        QVERIFY(model.triangles().empty());
    }
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
            auto beforeMove = loaded.triangles().front().a;
            auto moved = loaded.add("transform", {{"source", id}, {"x", 12}, {"y", -3}, {"z", 7}});
            QVERIFY(loaded.isMesh(moved));
            QVERIFY((loaded.triangles().front().a - beforeMove - QVector3D(12, -3, 7)).length() < .001);
            loaded.save(saved);
            Model reopened;
            reopened.load(saved);
            QVERIFY(reopened.isMesh(moved));
            QCOMPARE(reopened.triangles().front().a, loaded.triangles().front().a);
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, loaded.exportStep(dir.filePath("mesh.step")));
            loaded.deleteBody(moved);
            QVERIFY(loaded.bodies().empty());
            QVERIFY(loaded.triangles().empty());
            QVERIFY(loaded.undo());
            QCOMPARE(loaded.bodies().size(), size_t(1));
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
