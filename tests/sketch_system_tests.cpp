#include "sketch_system.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
#include <limits>
#include <algorithm>
#include <cmath>
using namespace sketch;

class SketchSystemTests : public QObject {
    Q_OBJECT
    static System rectangle() {
        System s;
        s.points = {{"a",{1,2}},{"b",{13,1}},{"c",{12,9}},{"d",{-1,8}}};
        s.lines = {{"ab","a","b"},{"bc","b","c"},{"cd","c","d"},{"da","d","a"}};
        s.constraints = {{"h1",Relation::Horizontal,"ab",{},{}},{"v1",Relation::Vertical,"bc",{},{}},
            {"h2",Relation::Horizontal,"cd",{},{}},{"v2",Relation::Vertical,"da",{},{}},
            {"origin",Relation::Fixed,"a",{},{0,0}},{"width",Relation::DistanceX,"a","b",{10,0}},
            {"height",Relation::DistanceY,"a","d",{0,6}}};
        return s;
    }
    static bool near(QPointF actual, QPointF expected) {
        return std::hypot(actual.x()-expected.x(),actual.y()-expected.y()) < 1e-8;
    }
private slots:
    void localPointMobility() {
        System s;s.points={{"a",{0,0}},{"b",{10,0}}};s.lines={{"ab","a","b"}};
        auto result=s.solve();QCOMPARE(result.pointDegreesOfFreedom["a"],2);QCOMPARE(result.pointDegreesOfFreedom["b"],2);
        s.constraints={{"fixed",Relation::Fixed,"a",{},{0,0}},{"horizontal",Relation::Horizontal,"ab",{},{}}};
        result=s.solve();QCOMPARE(result.degreesOfFreedom,1);
        QCOMPARE(result.pointDegreesOfFreedom["a"],0);QCOMPARE(result.pointDegreesOfFreedom["b"],1);
        s.constraints.append({"width",Relation::DistanceX,"b","a",{10,0}});
        result=s.solve();QCOMPARE(result.degreesOfFreedom,0);QCOMPARE(result.pointDegreesOfFreedom["b"],0);
        s.constraints={{"coincident",Relation::Coincident,"a","b",{}}};
        result=s.solve();QCOMPARE(result.degreesOfFreedom,2);
        QCOMPARE(result.pointDegreesOfFreedom["a"],2);QCOMPARE(result.pointDegreesOfFreedom["b"],2);
        s.constraints.append({"fixedA",Relation::Fixed,"a",{},{0,0}});
        s.constraints.append({"fixedB",Relation::Fixed,"b",{},{1,0}});
        result=s.solve();QVERIFY(!result.consistent);QVERIFY(result.pointDegreesOfFreedom.empty());
    }
    void unconstrainedAndEmpty() {
        System empty;
        auto result = empty.solve(); QVERIFY(result.consistent); QCOMPARE(result.degreesOfFreedom,0);
        empty.points = {{"p",{2,3}},{"q",{-5,7}}};
        auto free = empty.solve(); QVERIFY(free.consistent); QCOMPARE(free.degreesOfFreedom,4);
        QCOMPARE(free.positions["p"],QPointF(2,3)); QCOMPARE(free.positions["q"],QPointF(-5,7));
    }
    void closestSolutionAndDegreesOfFreedom() {
        System s;
        s.points = {{"a",{0,0}},{"b",{10,4}}}; s.lines = {{"line","a","b"}};
        s.constraints = {{"horizontal",Relation::Horizontal,"line",{},{}}};
        auto result = s.solve(); QVERIFY(result.consistent); QCOMPARE(result.degreesOfFreedom,3);
        QVERIFY(near(result.positions["a"],{0,2})); QVERIFY(near(result.positions["b"],{10,2}));
        s.constraints.append({"coincident",Relation::Coincident,"a","b",{}});
        result = s.solve(); QVERIFY(result.consistent); QCOMPARE(result.degreesOfFreedom,2);
        QVERIFY(near(result.positions["a"],{5,2})); QVERIFY(near(result.positions["b"],{5,2}));
        // Algebraic coincidence may collapse a line: CAD validity is a separate gate.
    }
    void fullyConstrainedRectangleAndStableIds() {
        auto s = rectangle(); const auto before = s.json();
        const auto r = s.solve(); QVERIFY(r.consistent); QCOMPARE(r.degreesOfFreedom,0);
        QVERIFY(r.maximumResidual < 1e-8);
        QVERIFY(near(r.positions["a"],{0,0})); QVERIFY(near(r.positions["b"],{10,0}));
        QVERIFY(near(r.positions["c"],{10,6})); QVERIFY(near(r.positions["d"],{0,6}));
        QCOMPARE(s.json(),before);
        const auto solved = s.solved();
        QCOMPARE(solved.lines[0].id,QString("ab")); QCOMPARE(solved.points[0].id,QString("a"));
        QCOMPARE(solved.constraints[0].id,QString("h1"));
        QVERIFY(near(solved.points[2].position,{10,6}));
        s.constraints[5].value.setX(25);
        QVERIFY(near(s.solve().positions["c"],{25,6}));
    }
    void redundantVersusConflicting() {
        auto s = rectangle();
        s.constraints.append({"sameWidth",Relation::DistanceX,"a","b",{10,0}});
        auto r = s.solve(); QVERIFY(r.consistent); QCOMPARE(r.degreesOfFreedom,0);
        QVERIFY(r.redundantConstraints.contains("sameWidth"));
        s.constraints.back().value.setX(12);
        const auto before = s.json(); r = s.solve();
        QVERIFY(!r.consistent); QCOMPARE(r.degreesOfFreedom,-1); QVERIFY(r.positions.empty());
        QVERIFY(r.conflictCandidates.contains("width")); QVERIFY(r.conflictCandidates.contains("sameWidth"));
        QVERIFY_THROWS_EXCEPTION(std::exception,s.solved()); QCOMPARE(s.json(),before);
        s.constraints.removeLast(); QVERIFY(s.solve().consistent);
    }
    void fixedPointConflictsAndSelfRelations() {
        System s; s.points = {{"p",{0,0}}};
        s.constraints = {{"self",Relation::Coincident,"p","p",{}},{"fix",Relation::Fixed,"p",{},{3,4}}};
        auto r = s.solve(); QVERIFY(r.consistent); QCOMPARE(r.degreesOfFreedom,0);
        QVERIFY(r.redundantConstraints.contains("self")); QVERIFY(near(r.positions["p"],{3,4}));
        s.constraints.append({"otherFix",Relation::Fixed,"p",{},{3,5}});
        r = s.solve(); QVERIFY(!r.consistent); QVERIFY(r.conflictCandidates.contains("fix"));
        QVERIFY(r.conflictCandidates.contains("otherFix"));
    }
    void persistenceAndStrictValidation() {
        const auto s = rectangle();
        auto document = QJsonDocument::fromJson(QJsonDocument(s.json()).toJson()).object();
        auto loaded = System::fromJson(document); QCOMPARE(loaded.json(),s.json());
        QCOMPARE(loaded.solve().positions,s.solve().positions);
        auto invalid = document; invalid["version"] = 1.5;
        QVERIFY_THROWS_EXCEPTION(std::exception,System::fromJson(invalid));
        invalid = document; invalid["unknown"] = 1;
        QVERIFY_THROWS_EXCEPTION(std::exception,System::fromJson(invalid));
        invalid = document; invalid["points"] = QJsonArray{42};
        QVERIFY_THROWS_EXCEPTION(std::exception,System::fromJson(invalid));
        auto broken = s; broken.lines[0].end = "missing";
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
        broken = s; broken.points[0].id = broken.lines[0].id;
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
        broken = s; broken.constraints[0].relation = Relation(99);
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
        broken = s; broken.constraints[0].second = "a";
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
        broken = s; broken.points[0].position.setX(std::numeric_limits<double>::quiet_NaN());
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
        broken = s; broken.constraints[0].value.setX(1);
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
        broken = s; broken.constraints[0].first = "a";
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
        for (int i=0; i<129; ++i) broken.points.append({QString("extra%1").arg(i),{}});
        QVERIFY_THROWS_EXCEPTION(std::exception,broken.solve());
    }
    void orderScaleAndIndependentComponents() {
        for (double scale : {1e-4,1.0,10000.0}) {
            auto s = rectangle();
            for (auto &p : s.points) p.position *= scale;
            for (auto &c : s.constraints) c.value *= scale;
            const auto expected = s.solve(); QVERIFY(expected.consistent);
            std::reverse(s.points.begin(),s.points.end());
            std::reverse(s.constraints.begin(),s.constraints.end());
            auto actual = s.solve(); QVERIFY(actual.consistent);
            for (auto it=expected.positions.begin(); it!=expected.positions.end(); ++it)
                QVERIFY(near(actual.positions[it.key()],it.value()));
            s.points.append({"unrelated",{33,44}});
            actual = s.solve(); QCOMPARE(actual.degreesOfFreedom,2);
            QCOMPARE(actual.positions["unrelated"],QPointF(33,44));
        }
    }
    void connectedChainAtLimit() {
        System s;
        for (int i=0;i<128;++i) {
            s.points.append({QString::number(i),{double(i),double(i%7)}});
            if (i) s.constraints.append({QString("c%1").arg(i),Relation::Coincident,QString::number(i-1),QString::number(i),{}});
        }
        auto r=s.solve(); QVERIFY(r.consistent); QCOMPARE(r.degreesOfFreedom,2);
        for (const auto &position : r.positions) QVERIFY(near(position,r.positions["0"]));
        s.constraints.append({"fix",Relation::Fixed,"0",{},{100,200}});
        r=s.solve(); QVERIFY(r.consistent); QCOMPARE(r.degreesOfFreedom,0);
        for (const auto &position : r.positions) QVERIFY(near(position,{100,200}));
    }
};
QTEST_GUILESS_MAIN(SketchSystemTests)
#include "sketch_system_tests.moc"
