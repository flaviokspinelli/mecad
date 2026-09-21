#include "model.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
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
    void suppressionPreservesHistoryAndDependencies() {
        Model m;const auto base=m.add("box",{{"w",20},{"h",10},{"d",5}});
        const auto moved=m.add("transform",{{"source",base},{"x",4}});
        const auto copied=m.add("copy",{{"source",moved},{"x",30}});
        const auto independent=m.add("sphere",{{"r",2}});
        QTemporaryDir dir;m.save(dir.filePath("original.mcad"));const auto original=m.json();
        m.suppress(moved,true);QVERIFY(m.get(moved).suppressed);QVERIFY(m.get(copied).inactive);
        QVERIFY(!m.get(copied).suppressed);QVERIFY(m.get(copied).shape.IsNull());
        QVERIFY(!m.get(base).inactive);QVERIFY(!m.consumed(base));QVERIFY(!m.get(independent).inactive);
        QCOMPARE(m.bodies().size(),size_t(2));QCOMPARE(m.features.size(),size_t(4));
        QCOMPARE(m.json()["version"].toInt(),4);const auto suppressed=m.json();
        m.suppress(moved,true);QCOMPARE(m.json(),suppressed);
        QVERIFY(m.undo());QCOMPARE(m.json(),original);QVERIFY(!m.dirty);
        QVERIFY(m.redo());QCOMPARE(m.json(),suppressed);
        m.save(dir.filePath("suppressed.mcad"));Model loaded;loaded.load(m.filePath);QCOMPARE(loaded.json(),suppressed);
        QVERIFY(loaded.get(copied).inactive);QCOMPARE(loaded.bodies().size(),size_t(2));
        QVERIFY_THROWS_EXCEPTION(std::exception,loaded.exportStep(dir.filePath("invalid.step"),copied));
        QVERIFY(!QFile::exists(dir.filePath("invalid.step")));
        loaded.suppress(moved,false);QCOMPARE(loaded.json(),original);QVERIFY(!loaded.get(copied).inactive);
        QVERIFY(std::abs(Model::volume(loaded.get(copied).shape)-1000)<1e-7);
        m.setParameters({{"size","3 mm"}});m.setExpression(independent,"r","size");QCOMPARE(m.json()["version"].toInt(),4);
        m.suppress(base,true);m.suppress(moved,false);QVERIFY(m.get(moved).inactive);
        m.suppress(base,false);QVERIFY(!m.get(copied).inactive);
    }
    void failedReactivationIsAtomic() {
        Model m;auto sk=m.add("sketch",{{"profile","rectangle"},{"w",10},{"h",10}});
        auto solid=m.add("extrude",{{"source",sk},{"d",5}});m.suppress(solid,true);
        auto invalid=m.get(solid).p;invalid["d"]=0;m.edit(solid,invalid,"Temporarily invalid");
        const auto before=m.json();QVERIFY_THROWS_EXCEPTION(std::exception,m.suppress(solid,false));
        QCOMPARE(m.json(),before);QVERIFY(m.get(solid).inactive);QVERIFY(m.get(solid).suppressed);
        invalid["d"]=8;m.edit(solid,invalid,"Fixed");m.suppress(solid,false);
        QVERIFY(std::abs(Model::volume(m.get(solid).shape)-800)<1e-7);
        auto malformed=m.json();auto list=malformed["features"].toArray();auto item=list[0].toObject();
        item["suppressed"]="yes";list[0]=item;malformed["features"]=list;malformed["version"]=4;
        const auto good=m.json();QVERIFY_THROWS_EXCEPTION(std::exception,m.loadJson(malformed));QCOMPARE(m.json(),good);
    }
    void expressionsDriveSketchAndAngles() {
        Model m;m.setParameters({{"width","20 mm"},{"turn","90 deg"}});
        auto sk=m.add("sketch",{{"profile","rectangle"},{"w",20},{"h",10}});
        m.setExpression(sk,"w","width");
        m.edit(sk,m.get(sk).p,m.get(sk).name);
        auto body=m.add("extrude",{{"source",sk},{"d",5}});
        auto rotated=m.add("transform",{{"source",body},{"axis","Z"},{"angle",0}});
        m.setExpression(rotated,"angle","turn");
        QVERIFY(std::abs(m.get(rotated).p["angle"].toDouble()-90)<1e-10);
        Bnd_Box bounds;BRepBndLib::Add(m.get(rotated).shape,bounds);
        double x0,y0,z0,x1,y1,z1;bounds.Get(x0,y0,z0,x1,y1,z1);
        QVERIFY(std::abs((x1-x0)-10)<1e-5);QVERIFY(std::abs((y1-y0)-20)<1e-5);
        const auto before=m.json();
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setExpression(rotated,"angle","2 mm"));
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setExpression(sk,"w","pi * rad"));QCOMPARE(m.json(),before);
        m.setParameters({{"width","30 mm"},{"turn","pi * rad"}});
        QVERIFY(std::abs(Model::volume(m.get(rotated).shape)-1500)<1e-6);
        QCOMPARE(m.get(rotated).p["angle"].toDouble(),180.);
        Model loaded;loaded.loadJson(m.json());QCOMPARE(loaded.json(),m.json());
        QVERIFY(m.undo());QCOMPARE(m.json(),before);
        const auto circle=m.add("sketch",{{"profile","circle"},{"r",2}});
        m.setExpression(circle,"r","width/4");QCOMPARE(m.get(circle).p["r"].toDouble(),5.);
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setExpression(circle,"w","1 mm"));
    }
    void nativeVersionFixtures() {
        const QStringList paths{QFINDTESTDATA("fixtures/v1-basic.mcad"),QFINDTESTDATA("fixtures/v2-constrained.mcad"),QFINDTESTDATA("fixtures/v3-parameters.mcad"),QFINDTESTDATA("fixtures/v4-suppressed.mcad")};
        const QVector<double> volumes{6000,500,2000,6};
        for(int i=0;i<paths.size();++i) {
            QVERIFY(!paths[i].isEmpty());QFile fixture(paths[i]);QVERIFY(fixture.open(QIODevice::ReadOnly));
            const auto expected=QJsonDocument::fromJson(fixture.readAll()).object();Model m;m.load(paths[i]);
            QCOMPARE(m.json(),expected);QCOMPARE(m.json()["version"].toInt(),i+1);
            QVERIFY(std::abs(Model::volume(m.features.back().shape)-volumes[i])<1e-6);
            m.rebuild();QCOMPARE(m.json(),expected);
            QTemporaryDir dir;m.save(dir.filePath("saved.mcad"));Model loaded;loaded.load(m.filePath);QCOMPARE(loaded.json(),expected);
            auto future=expected;future["version"]=999;
            const auto path=m.filePath;QVERIFY_THROWS_EXCEPTION(std::exception,m.loadJson(future));
            QCOMPARE(m.json(),expected);QCOMPARE(m.filePath,path);QVERIFY(!m.dirty);
        }
    }
    void graphAtDocumentLimitAndFailureRecovery() {
        QJsonArray nodes;
        for(int i=0;i<2000;++i) nodes.append(QJsonObject{{"id",QString::number(i)},
            {"parameters",i==0?QJsonObject{}:QJsonObject{{"source",QString::number(i-1)}}}});
        DependencyGraph graph(nodes);graph.requireHistoryOrder();QCOMPARE(graph.order().size(),2000);
        QCOMPARE(graph.dependents("0").size(),1999);QCOMPARE(graph.order().back(),QString("1999"));
        auto root=nodes[0].toObject();root["parameters"]=QJsonObject{{"source","1999"}};nodes[0]=root;
        QVERIFY_THROWS_EXCEPTION(std::exception,DependencyGraph{nodes});
        Model model;auto base=model.add("box",{{"w",10},{"h",10},{"d",10}});
        auto tool=model.add("cylinder",{{"r",2},{"d",12},{"x",5},{"y",5},{"z",-1}});
        auto cut=model.add("boolean",{{"target",base},{"tool",tool},{"mode","common"}});
        const auto before=model.json();const auto shape=model.get(cut).shape;
        auto invalid=model.get(tool).p;invalid["x"]=100;
        // An empty intersection is rejected, preserving all three nodes and their shapes.
        QVERIFY_THROWS_EXCEPTION(std::exception,model.edit(tool,invalid,"Outside"));
        QCOMPARE(model.json(),before);QVERIFY(model.get(cut).shape.IsSame(shape));
        QVERIFY(model.undo());QVERIFY(model.redo());QCOMPARE(model.json(),before);
    }
    void chamferModesAndPersistence() {
        for(const auto &mode:QStringList{"equal","two","angle"}) {
            Model m;auto box=m.add("box",{{"w",30},{"h",30},{"d",10}});
            TopTools_IndexedMapOfShape edges;TopExp::MapShapes(m.get(box).shape,TopAbs_EDGE,edges);
            GProp_GProps length;BRepGProp::LinearProperties(edges(1),length);
            const auto before=m.json();
            QJsonObject p{{"source",box},{"edges",QJsonArray{0}},{"d",2},{"d2",3},{"angle",45},{"mode",mode},{"side","first"},{"sourceEdgeCount",edges.Extent()}};
            auto id=m.add("chamfer",p);
            const double removed=(mode=="two"?3.:2.)*length.Mass();
            QVERIFY(std::abs(Model::volume(m.get(id).shape)-(9000-removed))<1e-5);
            QVERIFY(BRepCheck_Analyzer(m.get(id).shape).IsValid());
            QVERIFY(m.consumed(box));QCOMPARE(m.bodies().size(),size_t(1));
            const auto good=m.json();QVERIFY(m.undo());QCOMPARE(m.json(),before);QVERIFY(m.redo());QCOMPARE(m.json(),good);
            QTemporaryDir dir;m.save(dir.filePath("chamfer.mcad"));Model loaded;loaded.load(m.filePath);QCOMPARE(loaded.json(),good);
            p["d"]=100;QVERIFY_THROWS_EXCEPTION(std::exception,m.edit(id,p,"Invalid"));QCOMPARE(m.json(),good);QVERIFY(!m.dirty);
            p["d"]=1;p["side"]="second";m.edit(id,p,"Chamfer edited");QCOMPARE(m.features.size(),size_t(2));
            m.setExpression(id,"d","1.5 mm");QCOMPARE(m.get(id).p["d"].toDouble(),1.5);
        }
    }
    void chamferRejectsBadInputs() {
        Model m;auto box=m.add("box",{{"w",10},{"h",10},{"d",10}});const auto before=m.json();
        const QJsonObject base{{"source",box},{"edges",QJsonArray{0}},{"d",1}};
        QVector<QJsonObject> invalid;
        for(const auto &edges:QVector<QJsonArray>{{},{-1},{999},{0.5}}){auto p=base;p["edges"]=edges;invalid.append(p);}
        for(double distance:{0.,-1.,1e8}){auto p=base;p["d"]=distance;invalid.append(p);}
        for(double angle:{0.,90.,-1.}){auto p=base;p["mode"]="angle";p["angle"]=angle;invalid.append(p);}
        auto p=base;p["mode"]="unknown";invalid.append(p);
        p=base;p["sourceEdgeCount"]=13;invalid.append(p);
        for(const auto &parameters:invalid){QVERIFY_THROWS_EXCEPTION(std::exception,m.add("chamfer",parameters));QCOMPARE(m.json(),before);}
    }
    void expressionsOnSupportedFeatures() {
        Model m;m.setParameters({{"size","2 mm"}});
        auto cylinder=m.add("cylinder",{{"r",1},{"d",1}});
        m.setExpression(cylinder,"r","size");m.setExpression(cylinder,"d","size*3");
        QVERIFY(std::abs(Model::volume(m.get(cylinder).shape)-24*std::acos(-1))<1e-6);
        auto sphere=m.add("sphere",{{"r",1}});m.setExpression(sphere,"r","size");
        QVERIFY(std::abs(Model::volume(m.get(sphere).shape)-32*std::acos(-1)/3)<1e-6);
        auto sketch=m.add("sketch",{{"profile","rectangle"},{"w",10},{"h",10}});
        m.constrainSketch(sketch,sketch::Relation::Fixed,"p0",{},{0,0});
        auto extrusion=m.add("extrude",{{"source",sketch},{"d",1}});m.setExpression(extrusion,"d","-size");
        QVERIFY(std::abs(Model::volume(m.get(extrusion).shape)-200)<1e-6);
        auto box=m.add("box",{{"w",20},{"h",20},{"d",20}});
        auto fillet=m.add("fillet",{{"source",box},{"r",1},{"edges",QJsonArray{0}}});
        m.setExpression(fillet,"r","size");const double volume=Model::volume(m.get(fillet).shape);
        m.setParameters({{"size","3 mm"}});
        QVERIFY(Model::volume(m.get(fillet).shape)<volume);
        const auto saved=m.json();m.rebuild();QCOMPARE(m.json(),saved);
        Model restored;restored.loadJson(saved);QCOMPARE(restored.json(),saved);
        QVERIFY(BRepCheck_Analyzer(restored.get(fillet).shape).IsValid());
    }
    void namedParametersDriveGeometry() {
        Model m; m.setParameters({{"width","2 cm"},{"depth","width/2"}});
        auto box=m.add("box",{{"w",1},{"h",10},{"d",10},{"expressions",QJsonObject{{"w","width"},{"d","depth"}}}});
        QCOMPARE(Model::volume(m.get(box).shape),2000.);
        QCOMPARE(m.json()["version"].toInt(),3);
        const auto before=m.json();
        m.setParameters({{"width","3 cm"},{"depth","width/2"}});
        QVERIFY(std::abs(Model::volume(m.get(box).shape)-4500)<1e-7);
        QVERIFY(m.undo());QCOMPARE(m.json(),before);QVERIFY(m.redo());
        QTemporaryDir dir;m.save(dir.filePath("parameters.mcad"));
        Model loaded;loaded.load(m.filePath);QCOMPARE(loaded.json(),m.json());
        const auto saved=m.json();
        auto invalid=saved;invalid["namedParameters"]=QJsonObject{{"width",3}};
        QVERIFY_THROWS_EXCEPTION(std::exception,loaded.loadJson(invalid));QCOMPARE(loaded.json(),saved);
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setParameters({{"width","-1 mm"},{"depth","width/2"}}));
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setParameters({{"width","depth"},{"depth","width"}}));
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setParameters({}));
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setExpression(box,"w","3 deg"));
        QVERIFY_THROWS_EXCEPTION(std::exception,m.setExpression(box,"source","width"));
        QCOMPARE(m.json(),saved);QVERIFY(!m.dirty);
        auto p=m.get(box).p;p["w"]=12;
        QVERIFY_THROWS_EXCEPTION(std::exception,m.edit(box,p,"Box"));QCOMPARE(m.json(),saved);
        p.remove("expressions");
        QVERIFY_THROWS_EXCEPTION(std::exception,m.edit(box,p,"Box"));QCOMPARE(m.json(),saved);
        m.setExpression(box,"w","");p=m.get(box).p;p["w"]=12;m.edit(box,p,"Box");
        QCOMPARE(m.get(box).p["w"].toDouble(),12.);
        auto downgrade=saved;downgrade["version"]=2;
        QVERIFY_THROWS_EXCEPTION(std::exception,loaded.loadJson(downgrade));QCOMPARE(loaded.json(),saved);
        m.clear();QVERIFY(m.parameters().empty());QCOMPARE(m.json()["version"].toInt(),1);
    }
    void constrainedSketchDocumentRoundTrip() {
        Model m;
        auto sk=m.add("sketch",{{"profile","rectangle"},{"w",10},{"h",10}},"Constrained");
        const auto v1=m.json(); QCOMPARE(v1["version"].toInt(),1);
        auto fix=m.constrainSketch(sk,sketch::Relation::Fixed,"p0",{},{0,0});
        auto width=m.constrainSketch(sk,sketch::Relation::DistanceX,"p0","p1",{20,0});
        m.constrainSketch(sk,sketch::Relation::DistanceY,"p0","p3",{0,30});
        QCOMPARE(m.sketchSystem(sk).solve().degreesOfFreedom,0);
        auto body=m.add("extrude",{{"source",sk},{"d",2}});
        QVERIFY(std::abs(Model::volume(m.get(body).shape)-1200)<1e-5);
        const auto before=m.json(); QCOMPARE(before["version"].toInt(),2);
        QTemporaryDir dir; QVERIFY(dir.isValid());
        m.save(dir.filePath("constraints.mcad"));
        Model loaded; loaded.load(m.filePath); QCOMPARE(loaded.json(),before);
        loaded.rebuild(); QCOMPARE(loaded.json(),before);
        QVERIFY(std::abs(Model::volume(loaded.get(body).shape)-1200)<1e-5);
        QVERIFY_THROWS_EXCEPTION(std::exception,m.constrainSketch(sk,sketch::Relation::DistanceX,"p0","p1",{25,0}));
        QCOMPARE(m.json(),before); QVERIFY(!m.dirty);
        m.removeSketchConstraint(sk,width); QCOMPARE(m.sketchSystem(sk).solve().degreesOfFreedom,1);
        QVERIFY(m.undo()); QCOMPARE(m.json(),before); QVERIFY(!m.dirty);
        auto stale=before; stale["version"]=1;
        QVERIFY_THROWS_EXCEPTION(std::exception,loaded.loadJson(stale)); QCOMPARE(loaded.json(),before);
        auto points=m.get(sk).p["points"].toArray(); points[2]=QJsonArray{999,999};
        auto p=m.get(sk).p; p["points"]=points;
        m.edit(sk,p,m.get(sk).name); // Fully constrained: direct edits must not break dimensions.
        QCOMPARE(m.json(),before); QVERIFY(!m.dirty);
        QVERIFY(std::abs(Model::volume(m.get(body).shape)-1200)<1e-5);
        QVERIFY(!fix.isEmpty());
    }
    void constrainedSketchRejectsInvalidContours() {
        Model m; auto sk=m.add("sketch",{{"profile","rectangle"},{"w",10},{"h",10}});
        const auto before=m.json();
        QVERIFY_THROWS_EXCEPTION(std::exception,m.constrainSketch(sk,sketch::Relation::Coincident,"p0","p1"));
        QCOMPARE(m.json(),before);
        auto fixed=m.constrainSketch(sk,sketch::Relation::Fixed,"p0",{},{0,0});
        const auto good=m.json();
        QVERIFY(!m.sketchEntityId(sk,"edge",0).isEmpty());
        QVERIFY(!m.sketchEntityId(sk,"vertex",0).isEmpty());
        QVERIFY_THROWS_EXCEPTION(std::exception,m.sketchEntityId(sk,"edge",999));
        QVERIFY_THROWS_EXCEPTION(std::exception,m.editSketchElements(sk,{0},{},{},true));
        QCOMPARE(m.json(),good);
        auto p=m.get(sk).p; auto points=p["points"].toArray(); points[2]=QJsonArray{20,15}; p["points"]=points;
        m.edit(sk,p,m.get(sk).name);
        const auto system=m.sketchSystem(sk); QCOMPARE(system.constraints.back().id,fixed);
        QCOMPARE(system.points.front().position,QPointF(0,0));
        QVERIFY(std::abs(system.points[1].position.x()-system.points[2].position.x())<1e-8);
        Model restored; restored.loadJson(m.json()); QCOMPARE(restored.json(),m.json());
        const auto safe=m.json();
        auto crossing=m.sketchSystem(sk); crossing.constraints.clear();
        crossing.points[0].position={0,0}; crossing.points[1].position={10,10};
        crossing.points[2].position={0,10}; crossing.points[3].position={10,0};
        p=m.get(sk).p; p["constraintSystem"]=crossing.json();
        QVERIFY_THROWS_EXCEPTION(std::exception,m.edit(sk,p,"Crossing")); QCOMPARE(m.json(),safe);
        for(int i=0;i<4;++i) crossing.points[i].position={double(i),0};
        p["constraintSystem"]=crossing.json();
        QVERIFY_THROWS_EXCEPTION(std::exception,m.edit(sk,p,"Collinear")); QCOMPARE(m.json(),safe);
    }
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
        auto future = original; future["version"] = 999; invalid.append(future);
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
