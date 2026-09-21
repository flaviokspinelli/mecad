#include "window.h"
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QMouseEvent>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>

class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void sketchDragGestures() {
        Model model;
        Viewport v(&model);
        v.resize(900, 600);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        v.onProfile = [&](QJsonObject p) {
            model.add("sketch", p, "Dragged sketch");
            v.refresh();
        };
        auto move = [&](QPoint position, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
            QMouseEvent event(QEvent::MouseMove, QPointF(position), QPointF(v.mapToGlobal(position)),
                              Qt::NoButton, Qt::LeftButton, modifiers);
            QApplication::sendEvent(&v, &event);
        };
        const QPoint start(400, 350), end(550, 250);
        for (auto plane : {"XY", "XZ", "YZ"})
            for (auto tool : {"rectangle", "circle"}) {
                model.clear();
                v.refresh();
                v.sketchMode = true;
                v.plane = plane;
                v.view("top");
                v.setTool(tool);
                QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, start);
                QCOMPARE(model.features.size(), size_t(0));
                move(end);
                QCOMPARE(model.features.size(), size_t(0));
                QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, end);
                QCOMPARE(model.features.size(), size_t(1));
                QCOMPARE(model.features.back().p["profile"].toString(), QString(tool));
                QCOMPARE(model.features.back().p["plane"].toString(), QString(plane));
                QVERIFY(model.features.back().p[tool == QString("circle") ? "r" : "w"].toDouble() > 0);
            }
        model.clear();
        v.refresh();
        v.plane = "XY";
        v.view("top");
        v.setTool("rectangle");
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, start);
        move(end);
        QTest::keyClick(&v, Qt::Key_Escape);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(model.features.empty());
        v.setTool("rectangle");
        QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, end);
        QCOMPARE(model.features.size(), size_t(1));
        v.setTool("polyline");
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, start);
        move(end);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, end);
        QTest::keyClick(&v, Qt::Key_Return);
        QCOMPARE(model.features.size(), size_t(2));
        QCOMPARE(model.features.back().p["points"].toArray().size(), 2);
        v.setTool("rectangle");
        QTest::mousePress(&v, Qt::LeftButton, Qt::AltModifier, start);
        move(end, Qt::AltModifier);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::AltModifier, end);
        QCOMPARE(model.features.size(), size_t(2));
    }
    void viewCubeNavigation() {
        Model model;
        model.add("box", {{"x", 0}, {"y", 0}, {"z", 0}, {"w", 50}, {"h", 30}, {"d", 20}}, "Cube test");
        Viewport viewport(&model);
        viewport.resize(900, 600);
        viewport.refresh();
        viewport.fit();
        viewport.show();
        QVERIFY(QTest::qWaitForWindowExposed(&viewport));
        auto *v = &viewport;
        const auto original = model.json();
        auto targetPixel = [&](QVector3D desired) {
            desired.normalize();
            for (int y = 0; y < 115; ++y)
                for (int x = v->width() - 115; x < v->width(); ++x)
                    if ((v->cubeDirectionAt(QPointF(x, y)) - desired).length() < .01)
                        return QPoint(x, y);
            return QPoint(-1, -1);
        };
        for (auto target : {QVector3D(1, -1, 1), QVector3D(1, -1, 0), QVector3D(0, 0, 1)}) {
            v->view("iso");
            QTest::qWait(80);
            v->grab();
            auto pixel = targetPixel(target);
            QVERIFY(pixel.x() >= 0);
            const auto before = v->cameraDirection();
            QTest::mouseMove(v, pixel);
            QTest::qWait(30);
            viewport.grab().save(QDir::currentPath() + "/cube-hover-test.png");
            QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, pixel);
            QVERIFY(v->isViewAnimating());
            QVERIFY((v->cameraDirection() - before).length() < .05);
            QTRY_VERIFY_WITH_TIMEOUT((v->cameraDirection() - before).length() > .005, 1000);
            QTRY_VERIFY(!v->isViewAnimating());
            QVERIFY((v->cameraDirection() - target.normalized()).length() < .001);
            QCOMPARE(model.json(), original);
        }
        v->view("iso");
        const auto homeDirection = v->cameraDirection();
        v->view("top");
        v->grab();
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, QPoint(v->width() - 59, 110));
        QVERIFY(v->isViewAnimating());
        QTRY_VERIFY(!v->isViewAnimating());
        QVERIFY((v->cameraDirection() - homeDirection).length() < .001);
        viewport.grab().save(QDir::currentPath() + "/cube-home-icon.png");
        v->view("front", true);
        QTest::qWait(60);
        v->view("right", true);
        QTRY_VERIFY(!v->isViewAnimating());
        QVERIFY((v->cameraDirection() - QVector3D(1, 0, 0)).length() < .001);
        v->view("front", true);
        QTest::qWait(40);
        auto position = QPointF(v->rect().center());
        QTest::mousePress(v, Qt::MiddleButton, Qt::ShiftModifier, position.toPoint());
        QMouseEvent drag(QEvent::MouseMove, position + QPointF(10, 5),
                         v->mapToGlobal(position.toPoint()) + QPoint(10, 5), Qt::NoButton, Qt::MiddleButton,
                         Qt::ShiftModifier);
        QApplication::sendEvent(v, &drag);
        QTest::mouseRelease(v, Qt::MiddleButton, Qt::ShiftModifier, (position + QPointF(10, 5)).toPoint());
        QVERIFY(!v->isViewAnimating());
        v->sketchMode = true;
        v->plane = "XY";
        v->viewDirection({1, 1, 1});
        QTRY_VERIFY(!v->isViewAnimating());
        QVERIFY((v->cameraDirection() - QVector3D(0, 0, 1)).length() < .001);
        viewport.close();
    }
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
