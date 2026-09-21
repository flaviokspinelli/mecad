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
        auto dialogAccept = [] {
            QTimer::singleShot(100, [] {
                for (auto *widget : QApplication::topLevelWidgets())
                    if (auto *dialog = qobject_cast<QDialog *>(widget); dialog && dialog->isVisible())
                        dialog->accept();
            });
        };
        dialogAccept();
        window.findChild<QAction *>("sketch")->trigger();
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
        dialogAccept();
        window.findChild<QAction *>("extrude")->trigger();
        QCOMPARE(window.model.features.size(), size_t(2));
        QCOMPARE(window.model.bodies().size(), size_t(1));
        const auto &p = window.model.get(sketch).p;
        double expected = p["w"].toDouble() * p["h"].toDouble() * 10;
        QVERIFY(std::abs(Model::volume(window.model.features.back().shape) - expected) < .01);
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.features.size(), size_t(1));
        window.findChild<QAction *>("redo")->trigger();
        QCOMPARE(window.model.features.size(), size_t(2));
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
