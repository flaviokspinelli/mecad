#include "window.h"
#include <QApplication>
#include <QComboBox>
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
    void shiftSelection() {
        Model model;
        auto id = model.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"w", 40}, {"h", 30}});
        Viewport v(&model);
        v.resize(1000, 700);
        v.view("top");
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        v.fit();
        auto click = [&](double x, double y, Qt::KeyboardModifiers modifiers) {
            QTest::mouseClick(&v, Qt::LeftButton, modifiers, v.project({float(x), float(y), 0}).toPoint());
        };
        // Emulate the Browser callback clearing its single selection state.
        v.onSelect = [&](QString owner) {
            v.selected = owner;
            v.selectedDetail = {};
            v.selectedDetails.clear();
        };
        click(20, 0, Qt::NoModifier);
        click(40, 15, Qt::ShiftModifier);
        click(0, 0, Qt::ShiftModifier);
        QCOMPARE(v.selectedDetails.size(), 3);
        QCOMPARE(v.selectedDetails[0].kind, QString("edge"));
        QCOMPARE(v.selectedDetails[1].kind, QString("edge"));
        QCOMPARE(v.selectedDetails[2].kind, QString("vertex"));
        v.grab().save(QDir::currentPath() + "/shift-selection-test.png");
        click(40, 15, Qt::ShiftModifier);
        QCOMPARE(v.selectedDetails.size(), 2);
        QTest::mouseClick(&v, Qt::LeftButton, Qt::ShiftModifier, QPoint(10, 400));
        QCOMPARE(v.selectedDetails.size(), 2);
        click(40, 30, Qt::NoModifier);
        QCOMPARE(v.selectedDetails.size(), 1);
        QCOMPARE(v.selectedDetail.kind, QString("vertex"));
        click(40, 30, Qt::ShiftModifier);
        QVERIFY(v.selectedDetails.empty());
        QVERIFY(v.selected.isEmpty());
        click(0, 0, Qt::ShiftModifier);
        click(20, 0, Qt::ShiftModifier);
        QTest::keyClick(&v, Qt::Key_Escape);
        QVERIFY(v.selectedDetails.empty());
    }
    void extrusionStartsAtZero() {
        Window window;
        auto sketch = window.model.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"},
                                                  {"w", 40}, {"h", 30}});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *v = window.findChild<Viewport *>();
        v->refresh();
        v->fit();
        v->onSelect(sketch);
        auto original = window.model.json();
        bool zero = false, live = false, returnedToZero = false;
        QTimer::singleShot(180, [&] {
            zero = v->handleActive && v->handleDistance == 0 && v->mesh.empty() &&
                   v->model->json() == original;
            v->onHandleDistance(15);
            QTest::qWait(100);
            live = !v->mesh.empty() && window.model.json() == original;
            v->grab().save(QDir::currentPath() + "/extrude-grid-test.png");
            v->onHandleDistance(0);
            QTest::qWait(100);
            returnedToZero = v->mesh.empty() && v->handleActive;
            QTest::keyClick(v, Qt::Key_Return);
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(zero);
        QVERIFY(live);
        QVERIFY(returnedToZero);
        QCOMPARE(window.model.json(), original);
    }
    void preciseSubelementSelection() {
        Model model;
        Viewport v(&model);
        v.resize(1000, 700);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        for (auto plane : {"XY", "XZ", "YZ"}) {
            model.clear();
            v.selected.clear();
            v.sketchMode = true;
            v.plane = plane;
            v.view("top");
            auto id = model.add(
                "sketch", {{"profile", "rectangle"}, {"plane", plane}, {"w", 40}, {"h", 30}, {"offset", 7}});
            v.refresh();
            v.fit();
            auto pixel = [&](double x, double y) { return v.project(Model::planePoint(plane, x, y, 7)); };
            auto vertex = v.pickDetail(pixel(0, 0) + QPointF(2, 2));
            QCOMPARE(vertex.feature, id);
            QCOMPARE(vertex.kind, QString("vertex"));
            auto edge = v.pickDetail(pixel(20, 0) + QPointF(0, 2));
            QCOMPARE(edge.feature, id);
            QCOMPARE(edge.kind, QString("edge"));
            QCOMPARE(v.pickDetail(pixel(20, 15)).kind, QString("object"));
            QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, pixel(20, 0).toPoint());
            QCOMPARE(v.selectedDetail.kind, QString("edge"));
            QVERIFY(v.hasSubselection());
            v.grab().save(QDir::currentPath() + "/selected-edge-test.png");
            QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, pixel(0, 0).toPoint());
            QCOMPARE(v.selectedDetail.kind, QString("vertex"));
            v.grab().save(QDir::currentPath() + "/selected-vertex-test.png");
            v.selectionFilter = "edge";
            QVERIFY(v.pickDetail(pixel(20, 15)).feature.isEmpty());
            v.selectionFilter = "object";
            QCOMPARE(v.pickDetail(pixel(0, 0)).kind, QString("object"));
            v.selectionFilter = "auto";
            v.sketchMode = false;
            v.view("iso");
            QCOMPARE(v.pickDetail(pixel(0, 0)).kind, QString("vertex"));
            QCOMPARE(v.pickDetail(pixel(20, 0)).kind, QString("edge"));
            v.zoomBy(2);
            QCOMPARE(v.pickDetail(pixel(20, 0) + QPointF(0, 2)).kind, QString("edge"));
            QTest::keyClick(&v, Qt::Key_Escape);
            QVERIFY(v.selected.isEmpty());
        }
        model.clear();
        v.selected.clear();
        v.sketchMode = false;
        v.view("top");
        auto behind = model.add("sketch", {{"profile", "rectangle"},
                                           {"plane", "XY"},
                                           {"x", 5},
                                           {"y", 5},
                                           {"w", 20},
                                           {"h", 20},
                                           {"offset", -5}});
        auto box = model.add("box", {{"w", 40}, {"h", 30}, {"d", 10}});
        v.refresh();
        v.fit();
        auto center = v.project({15, 15, 10});
        QCOMPARE(v.pickDetail(center).feature, box);
        // Hidden sketch vertices and bottom edges cannot steal the click.
        QCOMPARE(v.pickDetail(v.project({5, 5, -5})).feature, box);
        auto corner = v.pickDetail(v.project({0, 0, 10}));
        QCOMPARE(corner.kind, QString("vertex"));
        QVERIFY(std::abs(corner.geometry[0].z() - 10) < .001);
        auto edge = v.pickDetail(v.project({20, 0, 10}));
        QCOMPARE(edge.kind, QString("edge"));
        auto front = model.add(
            "sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"w", 40}, {"h", 30}, {"offset", 20}});
        v.refresh();
        QCOMPARE(v.pickDetail(center).feature, front);
        model.toggle(front);
        v.refresh();
        QCOMPARE(v.pickDetail(center).feature, box);
    }
    void separateDeleteAndRollback() {
        QTemporaryDir dir;
        Window window;
        auto box = window.model.add("box", {{"w", 40}, {"h", 30}, {"d", 20}});
        auto move = window.model.add("transform", {{"source", box}, {"x", 15}});
        window.model.save(dir.filePath("delete.mcad"));
        window.openPath(dir.filePath("delete.mcad"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v = window.findChild<Viewport *>();
        v->onSelect(move);
        window.findChild<QAction *>("delete")->trigger();
        QVERIFY(window.model.bodies().empty());
        QVERIFY(v->mesh.empty());
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.bodies().size(), size_t(1));
        v->onSelect(move);
        window.findChild<QAction *>("rollback")->trigger();
        QCOMPARE(window.model.features.size(), size_t(1));
        QCOMPARE(window.model.features[0].id, box);
        QVERIFY(!v->mesh.empty());
    }
    void rotationRing() {
        QTemporaryDir dir;
        for (bool stl : {false, true}) {
            Window window;
            window.model.add("box", {{"x", 100}, {"y", 50}, {"z", 20}, {"w", 40}, {"h", 30}, {"d", 20}});
            if (stl) {
                window.model.exportStl(dir.filePath("rotation.stl"));
                window.model.clear();
                window.model.importStl(dir.filePath("rotation.stl"));
            }
            window.model.save(dir.filePath("rotation.mcad"));
            window.openPath(dir.filePath("rotation.mcad"));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto *v = window.findChild<Viewport *>();
            const auto original = window.model.json();
            for (QString axis : {"X", "Y", "Z"}) {
                bool started = false, previewChanged = false;
                QTimer::singleShot(180, [&] {
                    auto *button = window.findChild<QPushButton *>("rotateMode");
                    if (!button) {
                        QTest::keyClick(v, Qt::Key_Escape);
                        return;
                    }
                    button->click();
                    for (auto *combo : button->parentWidget()->findChildren<QComboBox *>())
                        if (combo->findData("X") >= 0)
                            combo->setCurrentIndex(combo->findData(axis));
                    QTest::qWait(70);
                    auto center = v->project(v->handleOrigin + v->moveDistances);
                    auto start = (center + QPointF(85, 0)).toPoint();
                    QTest::mousePress(v, Qt::LeftButton, Qt::NoModifier, start);
                    started = v->draggingRotation;
                    auto before = v->mesh.front().a;
                    for (int step = 1; step <= 20; ++step) {
                        double angle = -M_PI / 2 * step / 20;
                        QPoint point =
                            (center + QPointF(85 * std::cos(angle), 85 * std::sin(angle))).toPoint();
                        QMouseEvent move(QEvent::MouseMove, QPointF(point), QPointF(v->mapToGlobal(point)),
                                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(v, &move);
                        QTest::qWait(10);
                    }
                    previewChanged = (v->mesh.front().a - before).length() > 1;
                    window.grab().save(QDir::currentPath() + "/rotation-ring-test.png");
                    QTest::mouseRelease(v, Qt::LeftButton, Qt::NoModifier,
                                        (center + QPointF(0, -85)).toPoint());
                    QTest::keyClick(v, Qt::Key_Return);
                });
                window.findChild<QAction *>("transform")->trigger();
                QVERIFY(started);
                QVERIFY(previewChanged);
                QCOMPARE(window.model.features.size(), size_t(2));
                auto params = window.model.features.back().p;
                QVERIFY(std::abs(params["angle"].toDouble() - 90) < 2);
                QCOMPARE(params["axis"].toString(), axis);
                QVector3D center;
                auto triangles = window.model.triangles();
                for (auto triangle : triangles)
                    center += triangle.a + triangle.b + triangle.c;
                center /= triangles.size() * 3;
                QVERIFY((center - QVector3D(120, 65, 30)).length() < .01);
                window.findChild<QAction *>("undo")->trigger();
                QCOMPARE(window.model.json(), original);
            }
        }
    }
    void repeatExtrudeEditsExistingBody() {
        QTemporaryDir dir;
        Window window;
        auto sketch =
            window.model.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"w", 40}, {"h", 30}});
        auto solid =
            window.model.add("extrude", {{"source", sketch}, {"d", 10}, {"target", ""}, {"mode", "join"}});
        window.model.save(dir.filePath("extrude.mcad"));
        window.openPath(dir.filePath("extrude.mcad"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v = window.findChild<Viewport *>();
        v->onSelect(solid);
        auto original = window.model.json();
        bool singlePreview = false;
        QTimer::singleShot(180, [&] {
            singlePreview = v->model->bodies().size() == 1 && v->model->features.size() == 2;
            v->onHandleDistance(25);
            QTest::qWait(90);
            singlePreview = singlePreview && v->model->bodies().size() == 1;
            QTest::keyClick(v, Qt::Key_Return);
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(singlePreview);
        QCOMPARE(window.model.features.size(), size_t(2));
        QCOMPARE(window.model.bodies().size(), size_t(1));
        QCOMPARE(window.model.get(solid).p["d"].toDouble(), 25.);
        QVERIFY(std::abs(Model::volume(window.model.get(solid).shape) - 30000) < .01);
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.json(), original);
        v->onSelect(solid);
        QTimer::singleShot(180, [&] {
            v->onHandleDistance(50);
            QTest::qWait(90);
            QTest::keyClick(v, Qt::Key_Escape);
        });
        window.findChild<QAction *>("extrude")->trigger();
        QCOMPARE(window.model.json(), original);
        v->onSelect({});
        bool noAutomaticPreview = false;
        QTimer::singleShot(180, [&] {
            noAutomaticPreview = !v->handleActive && v->model->json() == original;
            QTest::keyClick(v, Qt::Key_Return);
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(noAutomaticPreview);
        QCOMPARE(window.model.json(), original);
    }
    void freeMoveLivePreview() {
        QTemporaryDir dir;
        for (bool stl : {false, true}) {
            Window window;
            window.model.add("box", {{"w", 40}, {"h", 30}, {"d", 20}});
            if (stl) {
                window.model.exportStl(dir.filePath("move.stl"));
                window.model.clear();
                window.model.importStl(dir.filePath("move.stl"));
            }
            window.model.save(dir.filePath("move.mcad"));
            window.openPath(dir.filePath("move.mcad"));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto *v = window.findChild<Viewport *>();
            auto original = window.model.json();
            for (bool center : {true, false}) {
                int updates = 0;
                bool started = false;
                QTimer::singleShot(180, [&] {
                    auto start = v->project(center ? v->handleOrigin : QVector3D(10, 10, 20)).toPoint();
                    QTest::mousePress(v, Qt::LeftButton, Qt::NoModifier, start);
                    started = v->draggingMoveFree;
                    auto lastPoint = v->mesh.front().a;
                    for (int step = 1; step <= 35; ++step) {
                        QPoint point = start + QPoint(step * 2, -step);
                        QMouseEvent move(QEvent::MouseMove, QPointF(point), QPointF(v->mapToGlobal(point)),
                                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(v, &move);
                        QTest::qWait(10);
                        if ((v->mesh.front().a - lastPoint).length() > .01)
                            ++updates;
                        lastPoint = v->mesh.front().a;
                    }
                    QCOMPARE(window.model.json(), original);
                    window.grab().save(QDir::currentPath() + "/free-move-test.png");
                    QTest::mouseRelease(v, Qt::LeftButton, Qt::NoModifier, start + QPoint(70, -35));
                    QTest::keyClick(v, center ? Qt::Key_Return : Qt::Key_Escape);
                });
                window.findChild<QAction *>("transform")->trigger();
                QVERIFY(started);
                QVERIFY(updates >= 3);
                if (center) {
                    QCOMPARE(window.model.features.size(), size_t(2));
                    auto p = window.model.features.back().p;
                    QVERIFY(std::abs(p["x"].toDouble()) + std::abs(p["y"].toDouble()) +
                                std::abs(p["z"].toDouble()) >
                            1);
                    window.findChild<QAction *>("undo")->trigger();
                }
                QCOMPARE(window.model.json(), original);
            }
        }
    }
    void importedMeshViewport() {
        QTemporaryDir dir;
        Model source;
        source.add("box", {{"w", 20}, {"h", 30}, {"d", 10}});
        source.exportStl(dir.filePath("box.stl"));
        Window window;
        auto id = window.model.importStl(dir.filePath("box.stl"));
        window.model.save(dir.filePath("mesh.mcad"));
        window.openPath(dir.filePath("mesh.mcad"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v = window.findChild<Viewport *>();
        QCOMPARE(v->mesh.size(), size_t(12));
        window.grab().save(QDir::currentPath() + "/imported-stl-test.png");
        QVERIFY(v->span > 1);
        QCOMPARE(window.model.get(id).type, QString("mesh"));
        window.openPath(dir.filePath("box.stl"));
        QCOMPARE(window.model.features.size(), size_t(1));
        QVERIFY(window.model.filePath.isEmpty());
        QCOMPARE(v->mesh.size(), size_t(12));
    }
    void regularPolygons() {
        Model model;
        Viewport v(&model);
        v.resize(900, 600);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        v.sketchMode = true;
        v.snap = false;
        v.smartSnap = false;
        QString id;
        v.onProfile = [&](QJsonObject p) { id = model.add("sketch", p); };
        for (auto plane : {"XY", "XZ", "YZ"}) {
            for (int sides : {3, 5, 6, 8}) {
                model.clear();
                v.plane = plane;
                v.planeOffset = 7;
                v.view("top");
                v.setTool("polygon");
                v.polygonSides = sides;
                QPoint center(430, 340), vertex(530, 310);
                QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, center);
                QMouseEvent move(QEvent::MouseMove, QPointF(vertex), QPointF(v.mapToGlobal(vertex)),
                                 Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(&v, &move);
                if (sides == 6 && QString(plane) == "XY")
                    v.grab().save(QDir::currentPath() + "/polygon-preview-test.png");
                QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, vertex);
                QCOMPARE(model.features.size(), size_t(1));
                auto p = model.get(id).p;
                QCOMPARE(p["points"].toArray().size(), sides);
                QVERIFY(p["closed"].toBool());
                QCOMPARE(p["offset"].toDouble(), 7.);
                auto points = p["points"].toArray();
                double edge = 0;
                for (int i = 0; i < sides; ++i) {
                    auto a = points[i].toArray(), b = points[(i + 1) % sides].toArray();
                    double length =
                        QLineF({a[0].toDouble(), a[1].toDouble()}, {b[0].toDouble(), b[1].toDouble()})
                            .length();
                    if (i == 0)
                        edge = length;
                    QVERIFY(std::abs(length - edge) < 1e-8);
                }
                auto solid = model.add("extrude", {{"source", id}, {"d", 10}});
                double area = sides * edge * edge / (4 * std::tan(M_PI / sides));
                QVERIFY(std::abs(Model::volume(model.get(solid).shape) - area * 10) < .001);
            }
        }
        model.clear();
        v.setTool("polygon");
        v.polygonSides = 5;
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, {430, 340});
        QPoint vertex(530, 310);
        QMouseEvent move(QEvent::MouseMove, QPointF(vertex), QPointF(v.mapToGlobal(vertex)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&v, &move);
        QTest::keyClick(&v, Qt::Key_Up);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, vertex);
        QCOMPARE(model.get(id).p["points"].toArray().size(), 6);
        QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, {430, 340});
        QTest::keyClick(&v, Qt::Key_Escape);
        QCOMPARE(model.features.size(), size_t(1));
    }
    void inlineDimensions() {
        Window window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.raise();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *v = window.findChild<Viewport *>();
        auto id = window.model.add(
            "sketch",
            {{"profile", "rectangle"}, {"plane", "XY"}, {"x", -20}, {"y", -15}, {"w", 40}, {"h", 30}});
        v->onEditSketch(id);
        v->refresh();
        v->fit();
        auto openDimension = [&](QString key) -> QLineEdit * {
            v->grab();
            for (const auto &target : v->dimensions) {
                if (target.key == key) {
                    QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, target.rect.center().toPoint());
                    return v->dimensionEditor.data();
                }
            }
            return nullptr;
        };
        v->setTool("rectangle");
        auto *editor = openDimension("w");
        QVERIFY(editor);
        QVERIFY(editor->hasFocus());
        QTest::keyClicks(editor, "55,5");
        window.grab().save(QDir::currentPath() + "/inline-dimension-test.png");
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(window.model.get(id).p["w"].toDouble(), 55.5);
        QCOMPARE(window.model.features.size(), size_t(1));
        QVERIFY(window.model.undo());
        QCOMPARE(window.model.get(id).p["w"].toDouble(), 40.);
        QVERIFY(window.model.redo());
        QCOMPARE(window.model.get(id).p["w"].toDouble(), 55.5);
        editor = openDimension("h");
        QVERIFY(editor);
        QTest::keyClicks(editor, "99");
        QTest::keyClick(editor, Qt::Key_Escape);
        QCOMPARE(window.model.get(id).p["h"].toDouble(), 30.);
        editor = openDimension("w");
        QVERIFY(editor);
        QTest::keyClicks(editor, "-2");
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(v->dimensionEditor);
        QCOMPARE(window.model.get(id).p["w"].toDouble(), 55.5);
        QTest::keyClick(editor, Qt::Key_Escape);
        auto solid = window.model.add("extrude", {{"source", id}, {"d", 10}});
        editor = openDimension("h");
        QVERIFY(editor);
        QTest::keyClicks(editor, "20");
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(std::abs(Model::volume(window.model.get(solid).shape) - 55.5 * 20 * 10) < .01);
        auto circle = window.model.add(
            "sketch", {{"profile", "circle"}, {"plane", "XY"}, {"x", 0}, {"y", 0}, {"r", 10}});
        v->onEditSketch(circle);
        v->refresh();
        editor = openDimension("r");
        QVERIFY(editor);
        QCOMPARE(editor->text(), QString("20"));
        QTest::keyClicks(editor, "30");
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(window.model.get(circle).p["r"].toDouble(), 15.);
    }
    void smartSketchSnapping() {
        Model model;
        Viewport v(&model);
        v.resize(900, 600);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        v.sketchMode = true;
        v.setTool("rectangle");
        for (auto plane : {"XY", "XZ", "YZ"}) {
            model.clear();
            v.selected.clear();
            v.plane = plane;
            v.planeOffset = 8;
            v.view("top");
            auto rectangle = model.add("sketch", {{"profile", "rectangle"},
                                                  {"plane", plane},
                                                  {"offset", 8},
                                                  {"x", 10.25},
                                                  {"y", 12.75},
                                                  {"w", 30},
                                                  {"h", 20}});
            model.add("sketch", {{"profile", "circle"},
                                 {"plane", plane},
                                 {"offset", 8},
                                 {"x", 60.25},
                                 {"y", 42.75},
                                 {"r", 8}});
            model.add("sketch", {{"profile", "polyline"},
                                 {"plane", plane},
                                 {"offset", 8},
                                 {"points", QJsonArray{QJsonArray{80, 10}, QJsonArray{100, 10}}}});
            model.add("sketch", {{"profile", "polyline"},
                                 {"plane", plane},
                                 {"offset", 8},
                                 {"points", QJsonArray{QJsonArray{86, 0}, QJsonArray{86, 40}}}});
            v.refresh();
            v.fit();
            v.setTool("rectangle");
            auto screen = [&](QPointF p) { return v.project(Model::planePoint(plane, p.x(), p.y(), 8)); };
            for (auto point : {QPointF(10.25, 12.75), QPointF(25.25, 12.75), QPointF(60.25, 42.75),
                               QPointF(86, 10), QPointF(0, 0)}) {
                v.magnetLabel.clear();
                QVERIFY(QLineF(v.sketchPoint(screen(point) + QPointF(3, -2)), point).length() < .0001);
                QVERIFY(!v.magnetLabel.isEmpty());
            }
            v.magnetLabel.clear();
            auto vertical = v.sketchPoint(screen({25.25, 70}) + QPointF(3, 0));
            QCOMPARE(vertical.x(), 25.25);
            QVERIFY(v.magnetLabel.contains("Vertical"));
            v.magnetLabel.clear();
            auto horizontal = v.sketchPoint(screen({58, 12.75}) + QPointF(0, 3));
            QCOMPARE(horizontal.y(), 12.75);
            QVERIFY(v.magnetLabel.contains("Horizontal"));
            v.magnetLabel.clear();
            v.sketchPoint(screen({10.25, 12.75}));
            QCOMPARE(v.sketchPoint(screen({10.25, 12.75}) + QPointF(12, 0)), QPointF(10.25, 12.75));
            v.smartSnap = false;
            QVERIFY(QLineF(v.sketchPoint(screen({10.25, 12.75})), QPointF(10.25, 12.75)).length() > .1);
            QVERIFY(v.magnetLabel.isEmpty());
            v.smartSnap = true;
            for (float zoom : {.5f, 2.f}) {
                v.zoomBy(zoom);
                v.magnetLabel.clear();
                QCOMPARE(v.sketchPoint(screen({10.25, 12.75}) + QPointF(8, 0)), QPointF(10.25, 12.75));
            }
            auto hidden = model.add("sketch", {{"profile", "circle"},
                                               {"plane", plane},
                                               {"offset", 8},
                                               {"x", 125.4},
                                               {"y", 73.6},
                                               {"r", 4}});
            model.get(hidden).visible = false;
            model.add("sketch", {{"profile", "circle"},
                                 {"plane", plane},
                                 {"offset", 18},
                                 {"x", 125.4},
                                 {"y", 73.6},
                                 {"r", 4}});
            v.snap = false;
            v.magnetLabel.clear();
            QVERIFY(QLineF(v.sketchPoint(screen({125.4, 73.6})), QPointF(125.4, 73.6)).length() < .001);
            QVERIFY(v.magnetLabel.isEmpty());
            v.snap = true;
            QJsonObject created;
            v.onProfile = [&](QJsonObject p) { created = p; };
            auto start = (screen({10.25, 12.75}) + QPointF(3, -2)).toPoint();
            auto end = (screen({60.25, 42.75}) + QPointF(3, -2)).toPoint();
            v.setTool("rectangle");
            QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, start);
            QMouseEvent move(QEvent::MouseMove, QPointF(end), QPointF(v.mapToGlobal(end)), Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&v, &move);
            QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, end);
            QCOMPARE(created["x"].toDouble(), 10.25);
            QCOMPARE(created["y"].toDouble(), 12.75);
            QCOMPARE(created["w"].toDouble(), 50.);
            QCOMPARE(created["h"].toDouble(), 30.);
            v.selected = rectangle;
            v.cursor = v.sketchPoint(screen({58, 12.75}) + QPointF(0, 3));
            v.grab().save(QDir::currentPath() + "/smart-snap-test.png");
            v.onProfile = {};
        }
    }
    void adjacentPlaneSelection() {
        Model model;
        Viewport v(&model);
        v.resize(900, 600);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        QString chosen;
        double offset = 1;
        v.onPlaneChosen = [&](QString name, double distance) {
            chosen = name;
            offset = distance;
        };
        for (int i = 0; i < 3; ++i) {
            v.choosingPlane = true;
            v.view("iso");
            v.grab();
            QCOMPARE(v.planeRegions.size(), 6);
            auto xy = v.planeRegions[0].second.boundingRect().center();
            auto xz = v.planeRegions[2].second.boundingRect().center();
            auto yz = v.planeRegions[1].second.boundingRect().center();
            QVERIFY(xy.y() > xz.y());
            QVERIFY(xy.y() > yz.y());
            QVERIFY(yz.x() < xz.x());
            v.grab().save(QDir::currentPath() + "/plane-layout-test.png");
            const auto face = v.planeRegions[i + 3];
            auto position = face.second.boundingRect().center();
            // Labels stay selectable even where translucent planes overlap.
            QVERIFY(v.planeRegions[i].second.containsPoint(position, Qt::OddEvenFill));
            QVERIFY(v.planeRegions[2].second.contains(v.project(Model::planePoint("XZ", 40, 0))));
            QVERIFY(v.planeRegions[0].second.contains(v.project(Model::planePoint("XY", 40, 0))));
            QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, position.toPoint());
            QCOMPARE(chosen, face.first);
            QCOMPARE(offset, 0.);
            QVERIFY(!v.choosingPlane);
        }
    }
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
        v->setTool("rectangle");
        v->grab();
        auto cubePoint = targetPixel({0, 0, 1});
        QVERIFY(cubePoint.x() >= 0);
        auto beforeOrbit = v->cameraDirection();
        int accidentalProfiles = 0;
        v->onProfile = [&](QJsonObject) { ++accidentalProfiles; };
        QTest::mousePress(v, Qt::LeftButton, Qt::NoModifier, cubePoint);
        auto dragEnd = cubePoint + QPoint(-130, 75);
        QMouseEvent cubeDrag(QEvent::MouseMove, QPointF(dragEnd), QPointF(v->mapToGlobal(dragEnd)),
                             Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(v, &cubeDrag);
        QVERIFY((v->cameraDirection() - beforeOrbit).length() > .1);
        QCOMPARE(static_cast<QWidget *>(v)->cursor().shape(), Qt::ClosedHandCursor);
        QTest::mouseRelease(v, Qt::LeftButton, Qt::NoModifier, dragEnd);
        QVERIFY(!v->isViewAnimating());
        QCOMPARE(v->plane, QString("XY"));
        QVERIFY(v->sketchMode);
        QCOMPARE(accidentalProfiles, 0);
        QCOMPARE(model.json(), original);
        viewport.grab().save(QDir::currentPath() + "/cube-drag-test.png");
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
                          v->planeRegions[3].second.boundingRect().center().toPoint());
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
        int liveMeshUpdates = 0;
        bool liveShrink = false;
        QTimer::singleShot(180, [&] {
            QVERIFY(v->handleActive);
            QCOMPARE(window.model.features.size(), size_t(1));
            double before = v->handleDistance;
            auto start = v->project(v->handleOrigin + v->handleAxis * before).toPoint();
            auto end = v->project(v->handleOrigin + v->handleAxis * (before + 12)).toPoint();
            QTest::mousePress(v, Qt::LeftButton, Qt::NoModifier, start);
            auto meshExtent = [&] {
                double extent = -1e10;
                for (const auto &triangle : v->mesh)
                    for (auto point : {triangle.a, triangle.b, triangle.c})
                        extent = std::max(
                            extent, double(QVector3D::dotProduct(point - v->handleOrigin, v->handleAxis)));
                return extent;
            };
            double lastExtent = meshExtent();
            // Keep moving faster than the preview interval, without releasing.
            // The actual rendered solid must update, not only its arrow.
            for (int step = 1; step <= 40; ++step) {
                double distance = before + (step <= 20 ? step * 1.2 : 24 - (step - 20) * .6);
                auto point = v->project(v->handleOrigin + v->handleAxis * distance).toPoint();
                QMouseEvent move(QEvent::MouseMove, QPointF(point), QPointF(v->mapToGlobal(point)),
                                 Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(v, &move);
                QTest::qWait(10);
                double extent = meshExtent();
                if (std::abs(extent - lastExtent) > .01) {
                    ++liveMeshUpdates;
                    if (extent < lastExtent)
                        liveShrink = true;
                    lastExtent = extent;
                }
            }
            window.grab().save(QDir::currentPath() + "/extrude-live-drag.png");
            QTest::mouseRelease(v, Qt::LeftButton, Qt::NoModifier, end);
            dragged = v->handleDistance > before + 5;
            QTest::qWait(80);
            window.grab().save(QDir::currentPath() + "/extrude-panel-test.png");
            QTest::keyClick(v, Qt::Key_Return);
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(dragged);
        QVERIFY(liveMeshUpdates >= 2);
        QVERIFY(liveShrink);
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
        // Undo/redo clears selection; choose the extrusion explicitly now that
        // the command no longer silently falls back to the first sketch.
        v->onSelect(window.model.features.back().id);
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
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("MecaCADTests");
    app.setApplicationName("MecaCADTests");
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "ui_tests.moc"
