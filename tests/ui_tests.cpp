#include "window.h"
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>

class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void sketchToSolid() {
        Window window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v = window.findChild<Viewport *>();
        QVERIFY(v);
        window.findChild<QAction *>("sketch")->trigger();
        QVERIFY(v->choosingPlane);
        QTest::qWait(100);
        window.grab().save(QDir::currentPath() + "/plane-selection-test.png");
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier,
                          v->project(Model::planePoint("XY", 10, 10)).toPoint());
        QVERIFY(v->sketchMode);
        QCOMPARE(v->tool, QString("rectangle"));
        QTest::qWait(100);
        QPoint p1(v->width() / 2 - 70, v->height() / 2 + 45), p2(v->width() / 2 + 70, v->height() / 2 - 45);
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, p1);
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, p2);
        QCOMPARE(window.model.features.size(), size_t(1));
        auto sketch = window.model.features.front().id;
        QVERIFY(window.model.features.front().p["w"].toDouble() > 0);
        auto image = v->grab().toImage();
        QVERIFY(!image.isNull());
        image.save(QDir::currentPath() + "/sketch-test.png");
        window.findChild<QAction *>("finish")->trigger();
        QVERIFY(!v->sketchMode);
        bool dragged = false;
        QTimer::singleShot(180, [&] {
            QVERIFY(v->handleActive);
            QCOMPARE(window.model.features.size(), size_t(1));
            double before = v->handleDistance;
            auto start = v->project(v->handleOrigin + v->handleAxis * before).toPoint();
            auto end = v->project(v->handleOrigin + v->handleAxis * (before + 12)).toPoint();
            QTest::mousePress(v, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(v, end, 30);
            QTest::mouseRelease(v, Qt::LeftButton, Qt::NoModifier, end);
            dragged = v->handleDistance > before + 5;
            QTest::qWait(80);
            window.grab().save(QDir::currentPath() + "/extrude-panel-test.png");
            QTest::keyClick(v, Qt::Key_Return);
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(dragged);
        QCOMPARE(window.model.features.size(), size_t(2));
        QCOMPARE(window.model.bodies().size(), size_t(1));
        const auto &p = window.model.get(sketch).p;
        double expected =
            p["w"].toDouble() * p["h"].toDouble() * window.model.features.back().p["d"].toDouble();
        QVERIFY(std::abs(Model::volume(window.model.features.back().shape) - expected) < .01);
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.features.size(), size_t(1));
        window.findChild<QAction *>("redo")->trigger();
        QCOMPARE(window.model.features.size(), size_t(2));
        auto beforeCancel = window.model.json();
        QTimer::singleShot(180, [&] {
            QVERIFY(v->handleActive);
            QTest::keyClick(v, Qt::Key_Escape);
        });
        window.findChild<QAction *>("extrude")->trigger();
        QCOMPARE(window.model.json(), beforeCancel);
        bool moved = false;
        QTimer::singleShot(180, [&] {
            QVERIFY(v->moveHandleActive);
            QCOMPARE(window.model.json(), beforeCancel);
            auto start = v->project(v->moveHandleTip(0)).toPoint();
            auto end = v->project(v->moveHandleTip(0) + QVector3D(15, 0, 0)).toPoint();
            QTest::mousePress(v, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(v, end, 30);
            QTest::mouseRelease(v, Qt::LeftButton, Qt::NoModifier, end);
            QTest::qWait(80);
            moved = v->moveDistances.x() > 10;
            window.grab().save(QDir::currentPath() + "/move-panel-test.png");
            QTest::keyClick(v, Qt::Key_Return);
        });
        window.findChild<QAction *>("transform")->trigger();
        QVERIFY(moved);
        QCOMPARE(window.model.features.size(), size_t(3));
        QVERIFY(window.model.features.back().p["x"].toDouble() > 10);
        auto movedState = window.model.json();
        QTimer::singleShot(180, [&] {
            QVERIFY(v->moveHandleActive);
            QTest::keyClick(v, Qt::Key_Escape);
        });
        window.findChild<QAction *>("transform")->trigger();
        QCOMPARE(window.model.json(), movedState);
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.json(), beforeCancel);
        QTemporaryDir dir;
        window.model.save(dir.filePath("ui.mcad"));
        window.openPath(dir.filePath("ui.mcad"));
        QCOMPARE(window.model.features.size(), size_t(2));
        window.model.dirty = false;
        window.close();
    }
};
int main(int argc, char **argv) {
    QSurfaceFormat f;
    f.setVersion(3, 2);
    f.setProfile(QSurfaceFormat::CoreProfile);
    f.setDepthBufferSize(24);
    f.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(f);
    QApplication app(argc, argv);
    app.setOrganizationName("MecaCADTests");
    app.setApplicationName("MecaCADTests");
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "ui_tests.moc"
