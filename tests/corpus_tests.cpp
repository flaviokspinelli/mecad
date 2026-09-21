#include "model.h"
#include <BRepCheck_Analyzer.hxx>
#include <QTemporaryDir>
#include <QtTest>
#include <numbers>

// Synthetic fixtures only. No user/company project is copied into the corpus.
class CorpusTests:public QObject {
    Q_OBJECT
    void roundTrip(Model &model,const QString &body,double expected) {
        QVERIFY(BRepCheck_Analyzer(model.get(body).shape).IsValid());
        QVERIFY(std::abs(Model::volume(model.get(body).shape)-expected)<1e-5);
        QCOMPARE(model.bodies().size(),size_t(1));
        const auto saved=model.json();QTemporaryDir dir;QVERIFY(dir.isValid());
        model.save(dir.filePath("synthetic.mcad"));
        Model loaded;loaded.load(model.filePath);QCOMPARE(loaded.json(),saved);
        loaded.rebuild();QCOMPARE(loaded.json(),saved);
        QVERIFY(std::abs(Model::volume(loaded.get(body).shape)-expected)<1e-5);
        model.exportStep(dir.filePath("synthetic.step"),body);
        Model step;auto imported=step.importStep(dir.filePath("synthetic.step"));
        QVERIFY(std::abs(Model::volume(step.get(imported).shape)-expected)<1e-4);
    }
private slots:
    void motorBracket() {
        Model m;
        auto base=m.add("box",{{"w",80},{"h",50},{"d",5}},"Base");
        auto wall=m.add("box",{{"w",80},{"h",5},{"d",40},{"y",45},{"z",5}},"Flange");
        auto body=m.add("boolean",{{"target",base},{"tool",wall},{"mode","join"}},"Bracket");
        for(double x:{10.,70.}) {
            auto drill=m.add("cylinder",{{"r",3},{"d",7},{"x",x},{"y",10},{"z",-1}},"Mount hole tool");
            body=m.add("boolean",{{"target",body},{"tool",drill},{"mode","cut"}},"Mount hole");
        }
        roundTrip(m,body,36000-90*std::numbers::pi);
        const auto original=m.json();QVERIFY(m.undo());QVERIFY(m.redo());QCOMPARE(m.json(),original);
    }
    void sensorEnclosure() {
        Model m;m.setParameters({{"wall","2 mm"},{"width","60 mm"}});
        auto outside=m.add("box",{{"w",60},{"h",40},{"d",25},{"expressions",QJsonObject{{"w","width"}}}},"Outside");
        auto cavity=m.add("box",{{"w",56},{"h",36},{"d",23},{"x",2},{"y",2},{"z",2},
            {"expressions",QJsonObject{{"w","width - 2 * wall"}}}},"Cavity");
        auto body=m.add("boolean",{{"target",outside},{"tool",cavity},{"mode","cut"}},"Enclosure");
        roundTrip(m,body,60*40*25-56*36*23);
        const auto before=m.json();m.setParameters({{"wall","2 mm"},{"width","70 mm"}});
        roundTrip(m,body,70*40*25-66*36*23);
        QVERIFY(m.undo());QCOMPARE(m.json(),before);
    }
    void shaftSpacer() {
        Model m;auto outside=m.add("cylinder",{{"r",12},{"d",10}},"Outside");
        auto bore=m.add("cylinder",{{"r",5},{"d",12},{"z",-1}},"Bore");
        auto body=m.add("boolean",{{"target",outside},{"tool",bore},{"mode","cut"}},"Shaft spacer");
        roundTrip(m,body,1190*std::numbers::pi);
        const auto before=m.json();auto invalid=m.get(bore).p;invalid["r"]=-1;
        QVERIFY_THROWS_EXCEPTION(std::exception,m.edit(bore,invalid,"Invalid bore"));QCOMPARE(m.json(),before);
    }
};
QTEST_APPLESS_MAIN(CorpusTests)
#include "corpus_tests.moc"
