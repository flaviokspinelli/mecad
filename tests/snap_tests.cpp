#include "snap_intersections.h"
#include <QtTest>
#include <limits>
class SnapTests:public QObject {
    Q_OBJECT
private slots:
    void arcLimits(){
        const snapping::Circle circle{{0,0},5};
        QVERIFY(snapping::onArc(circle,{5,0},{0,5},{-5,0},{3,4}));
        QVERIFY(!snapping::onArc(circle,{5,0},{0,5},{-5,0},{3,-4}));
        QVERIFY(snapping::onArc(circle,{-5,0},{0,5},{5,0},{3,4}));
        QVERIFY(!snapping::onArc(circle,{-5,0},{0,5},{5,0},{3,-4}));
        QVERIFY(snapping::onArc(circle,{5,0},{-5,0},{0,-5},{-3,-4}));
        QVERIFY(!snapping::onArc(circle,{5,0},{-5,0},{0,-5},{3,-4}));
        QVERIFY(snapping::onArc(circle,{5,0},{0,5},{-5,0},{5,0}));
        QVERIFY(!snapping::onArc(circle,{5,0},{0,5},{-5,0},{0,2}));
    }
    void segmentCrossings(){
        auto points=snapping::intersections(QLineF(-10,3,10,3),{{0,0},5});QCOMPARE(points.size(),2);
        QVERIFY(QLineF(points[0],{-4,3}).length()<1e-8);QVERIFY(QLineF(points[1],{4,3}).length()<1e-8);
        QCOMPARE(snapping::intersections(QLineF(0,3,10,3),{{0,0},5}).size(),1);
        QVERIFY(snapping::intersections(QLineF(6,0,10,0),{{0,0},5}).empty());
        QVERIFY(snapping::intersections(QLineF(0,0,0,0),{{0,0},5}).empty());
    }
    void tangentAndDisjoint(){
        auto points=snapping::intersections(QLineF(-10,5,10,5),{{0,0},5});QCOMPARE(points.size(),1);QCOMPARE(points[0],QPointF(0,5));
        QVERIFY(snapping::intersections(QLineF(-10,5.001,10,5.001),{{0,0},5}).empty());
        QCOMPARE(snapping::intersections(snapping::Circle{{0,0},5},{{10,0},5}).size(),1);
        QCOMPARE(snapping::intersections(snapping::Circle{{0,0},5},{{3,0},2}).size(),1);
        QVERIFY(snapping::intersections(snapping::Circle{{0,0},5},{{10.001,0},5}).empty());
        QVERIFY(snapping::intersections(snapping::Circle{{0,0},5},{{1,0},2}).empty());
        QVERIFY(snapping::intersections(snapping::Circle{{0,0},5},{{0,0},5}).empty());
    }
    void circleCrossings(){
        const auto points=snapping::intersections(snapping::Circle{{0,0},5},{{6,0},5});QCOMPARE(points.size(),2);
        QVERIFY(QLineF(points[0],{3,4}).length()<1e-8);QVERIFY(QLineF(points[1],{3,-4}).length()<1e-8);
        const auto shifted=snapping::intersections(snapping::Circle{{1e6,-1e6},5},{{1e6+6,-1e6},5});
        QCOMPARE(shifted.size(),2);QVERIFY(QLineF(shifted[0],{1e6+3,-1e6+4}).length()<1e-8);
        QVERIFY(snapping::intersections(QLineF(-1,0,1,0),{{0,0},-1}).empty());
        QVERIFY(snapping::intersections(snapping::Circle{{0,0},5},{{0,0},std::numeric_limits<double>::infinity()}).empty());
    }
};
QTEST_APPLESS_MAIN(SnapTests)
#include "snap_tests.moc"
