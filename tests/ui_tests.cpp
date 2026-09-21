#include "window.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QMouseEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTableWidget>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QTextBrowser>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QProcess>
#include <QTextStream>
#include <QtTest>

class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void usageGuideUsesCurrentShortcuts() {
        QTemporaryDir dir;Window window(dir.filePath("recovery"),false);const auto before=window.model.json();
        window.findChild<QAction *>("fit")->setShortcut(QKeySequence("Ctrl+G"));
        window.findChild<QAction *>("rectangle")->setShortcut({});
        bool current=false,limits=false;
        QTimer::singleShot(100,[&]{auto *dialog=window.findChild<QDialog *>("usageGuide");if(!dialog)return;
            auto *tabs=dialog->findChild<QTabWidget *>("guideTabs");QCOMPARE(tabs->count(),4);
            const auto keys=qobject_cast<QTextBrowser *>(tabs->widget(2))->toPlainText();
            current=keys.contains(QKeySequence("Ctrl+G").toString(QKeySequence::NativeText)) && keys.contains("Sem atalho");
            limits=qobject_cast<QTextBrowser *>(tabs->widget(3))->toPlainText().contains("não equivale ao Fusion");
            dialog->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("usage-guide.png"));dialog->reject();
        });
        window.findChild<QAction *>("help_guide")->trigger();QVERIFY(current);QVERIFY(limits);QCOMPARE(window.model.json(),before);
    }
    void configurableShortcutsPersistAndRejectConflicts() {
        QTemporaryDir dir;const auto path=dir.filePath("preferences.ini");
        {
            Window window(dir.filePath("recovery"),false,path);const auto before=window.model.json();
            for(bool apply:{false,true}) {
                bool checked=false;
                QTimer::singleShot(100,[&]{
                    auto *dialog=window.findChild<QDialog *>("shortcutPreferences");if(!dialog)return;
                    auto *fit=dialog->findChild<QKeySequenceEdit *>("shortcut_fit");
                    auto *buttons=dialog->findChild<QDialogButtonBox *>();
                    fit->setKeySequence(QKeySequence("E"));
                    checked=!buttons->button(QDialogButtonBox::Ok)->isEnabled();
                    fit->setKeySequence(QKeySequence("Ctrl+S"));checked=checked && !buttons->button(QDialogButtonBox::Ok)->isEnabled();
                    fit->setKeySequence(QKeySequence("G"));checked=checked && buttons->button(QDialogButtonBox::Ok)->isEnabled();
                    if(apply)buttons->button(QDialogButtonBox::Ok)->click();else dialog->reject();
                });
                window.findChild<QAction *>("configure_shortcuts")->trigger();QVERIFY(checked);
                QCOMPARE(window.findChild<QAction *>("fit")->shortcut(),QKeySequence(apply?"G":"F"));
                QCOMPARE(window.model.json(),before);
            }
        }
        Window reopened(dir.filePath("recovery"),false,path);
        QCOMPARE(reopened.findChild<QAction *>("fit")->shortcut(),QKeySequence("G"));
        reopened.show();QVERIFY(QTest::qWaitForWindowExposed(&reopened));reopened.raise();reopened.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&reopened));auto *v=reopened.findChild<Viewport *>();v->setFocus();
        QSignalSpy invoked(reopened.findChild<QAction *>("fit"),&QAction::triggered);
        v->span=127;QTest::keyClick(v,Qt::Key_F);QCOMPARE(invoked.count(),0);QCOMPARE(v->span,127.f);
        QTest::keyClick(v,Qt::Key_G);QCOMPARE(invoked.count(),1);
        QTimer::singleShot(100,[&]{auto *dialog=reopened.findChild<QDialog *>("shortcutPreferences");
            auto *buttons=dialog->findChild<QDialogButtonBox *>();buttons->button(QDialogButtonBox::RestoreDefaults)->click();buttons->button(QDialogButtonBox::Ok)->click();});
        reopened.findChild<QAction *>("configure_shortcuts")->trigger();
        QCOMPARE(reopened.findChild<QAction *>("fit")->shortcut(),QKeySequence("F"));
        QSettings broken(path,QSettings::IniFormat);broken.setValue("keyboard/shortcuts",QVariantMap{{"fit","E"}});broken.sync();
        Window fallback(dir.filePath("recovery"),false,path);
        QCOMPARE(fallback.findChild<QAction *>("fit")->shortcut(),QKeySequence("F"));
    }
    void displayPreferencesPersistWithoutDocumentChanges() {
        QTemporaryDir dir;const auto settings=dir.filePath("preferences.ini");
        {
            Window window(dir.filePath("recovery"),false,settings);
            const auto before=window.model.json();auto *v=window.findChild<Viewport *>();
            QVERIFY(!v->light);QVERIFY(v->showEdges);QVERIFY(v->snap);QVERIFY(v->smartSnap);
            window.findChild<QAction *>("lightCanvas")->setChecked(true);
            for(const auto &key:QStringList{"visibleEdges","gridSnap","smartSnap"})window.findChild<QAction *>(key)->setChecked(false);
            QVERIFY(v->light);QVERIFY(!v->showEdges);QVERIFY(!v->snap);QVERIFY(!v->smartSnap);
            QCOMPARE(window.model.json(),before);QVERIFY(!window.model.dirty);
        }
        {
            Window window(dir.filePath("recovery"),false,settings);auto *v=window.findChild<Viewport *>();
            QVERIFY(v->light);QVERIFY(!v->showEdges);QVERIFY(!v->snap);QVERIFY(!v->smartSnap);
            window.model.add("box",{{"w",10},{"h",20},{"d",30}});
            const auto before=window.model.json();window.findChild<QAction *>("lightCanvas")->setChecked(false);
            QCOMPARE(window.model.json(),before);QVERIFY(window.model.undo());QVERIFY(window.model.features.empty());
        }
        QSettings invalid(settings,QSettings::IniFormat);invalid.setValue("sketch/gridSnap","not-a-bool");invalid.sync();
        Window fallback(dir.filePath("recovery"),false,settings);
        QVERIFY(fallback.findChild<Viewport *>()->snap);
        QVERIFY(!fallback.findChild<Viewport *>()->light);
    }
    void displayPreferenceWriteFailureIsReported() {
        QTemporaryDir dir;
        Window window(dir.filePath("recovery"),false,dir.path());
        const auto before=window.model.json();window.findChild<QAction *>("lightCanvas")->setChecked(true);
        QVERIFY(window.findChild<Viewport *>()->light);QCOMPARE(window.model.json(),before);
        bool warning=false;for(auto *label:window.findChildren<QLabel *>())
            if(label->text().contains("não foi possível salvá-la"))warning=true;
        QVERIFY(warning);
    }
    void commandSearchFiltersAndKeyboard() {
        QTemporaryDir dir;Window window(dir.filePath("recovery"),false);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto before=window.model.json();bool searched=false;
        QTimer::singleShot(100,[&]{
            auto *dialog=window.findChild<QDialog *>("commandSearch");if(!dialog)return;
            auto *input=dialog->findChild<QLineEdit *>("commandSearchInput");
            auto *list=dialog->findChild<QListWidget *>("commandSearchResults");
            input->setText("RESTRICOES revisar");
            searched=list->count()==1 && list->item(0)->data(Qt::UserRole).toString()=="constraint_remove";
            input->setText("zzzz-no-command");QCOMPARE(list->count(),0);
            QTest::keyClick(input,Qt::Key_Return);QVERIFY(dialog->isVisible());
            input->clear();QVERIFY(list->count()>2);QTest::keyClick(input,Qt::Key_Down);QCOMPARE(list->currentRow(),1);
            QTest::keyClick(input,Qt::Key_Up);QCOMPARE(list->currentRow(),0);
            dialog->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("command-search.png"));
            QTest::keyClick(input,Qt::Key_Escape);
        });
        window.findChild<QAction *>("search")->trigger();QVERIFY(searched);QCOMPARE(window.model.json(),before);
        auto *fit=window.findChild<QAction *>("fit");QSignalSpy executed(fit,&QAction::triggered);
        for(bool enabled:{false,true}) {
            fit->setEnabled(enabled);
            QTimer::singleShot(100,[&]{
                auto *dialog=window.findChild<QDialog *>("commandSearch");auto *input=dialog->findChild<QLineEdit *>("commandSearchInput");
                input->setText("fit");QTest::keyClick(input,Qt::Key_Return);
                if(!enabled){QVERIFY(dialog->isVisible());dialog->reject();}
            });
            window.findChild<QAction *>("search")->trigger();QCOMPARE(executed.count(),enabled?1:0);
        }
        QCOMPARE(window.model.json(),before);
    }
    void sketchMobilityUpdatesAfterUndo() {
        QTemporaryDir dir;Window window(dir.filePath("recovery"),false);
        auto id=window.model.add("sketch",{{"profile","rectangle"},{"w",30},{"h",20}});
        window.model.constrainSketch(id,sketch::Relation::Fixed,"p0",{},{0,0});
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();
        v->onEditSketch(id);v->refresh();v->fit();
        QCOMPARE(v->constraintDiagnostics[id].degreesOfFreedom,2);
        QCOMPARE(v->constraintDiagnostics[id].pointDegreesOfFreedom["p0"],0);
        QCOMPARE(v->constraintDiagnostics[id].pointDegreesOfFreedom["p2"],2);
        window.model.constrainSketch(id,sketch::Relation::DistanceX,"p1","p0",{30,0});
        window.model.constrainSketch(id,sketch::Relation::DistanceY,"p3","p0",{0,20});v->refresh();
        QCOMPARE(v->constraintDiagnostics[id].degreesOfFreedom,0);
        for(auto freedom:v->constraintDiagnostics[id].pointDegreesOfFreedom)QCOMPARE(freedom,0);
        window.findChild<QAction *>("undo")->trigger();v->onEditSketch(id);
        QCOMPARE(v->constraintDiagnostics[id].degreesOfFreedom,1);
        QCOMPARE(v->constraintDiagnostics[id].pointDegreesOfFreedom["p2"],1);
        v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("sketch-mobility.png"));
        window.findChild<QAction *>("redo")->trigger();QCOMPARE(v->constraintDiagnostics[id].degreesOfFreedom,0);
    }
    void inlineProfileExpressionsAndUnits() {
        QTemporaryDir dir;Window window(dir.filePath("recovery"),false);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();
        window.model.setParameters({{"medida","40 mm"}});
        for(bool circle:{false,true}) {
            const auto id=window.model.add("sketch",circle?QJsonObject{{"profile","circle"},{"r",10}}:
                QJsonObject{{"profile","rectangle"},{"w",20},{"h",10}});
            v->onEditSketch(id);v->refresh();v->fit();const QString field=circle?"r":"w";
            auto edit=[&]() -> QLineEdit * {
                v->grab();for(const auto &target:v->dimensions)if(target.key==field){v->editDimension(target);return v->dimensionEditor;}
                return nullptr;
            };
            const auto initial=window.model.json();
            auto *input=edit();QVERIFY(input);QTest::keyClick(input,Qt::Key_Escape);QCOMPARE(window.model.json(),initial);
            input=edit();QVERIFY(input);input->setText("medida / 2");QTest::keyClick(input,Qt::Key_Return);
            QCOMPARE(window.model.get(id).p[field].toDouble(),circle?10.:20.);
            window.model.setParameters({{"medida","60 mm"}});v->refresh();
            QCOMPARE(window.model.get(id).p[field].toDouble(),circle?15.:30.);
            const auto before=window.model.json();input=edit();QVERIFY(input);
            QVERIFY(input->text().contains("medida"));QTest::keyClick(input,Qt::Key_Return);QCOMPARE(window.model.json(),before);
            input=edit();QVERIFY(input);input->setText("2,5 cm");QTest::keyClick(input,Qt::Key_Return);
            QCOMPARE(window.model.get(id).p[field].toDouble(),circle?12.5:25.);
            const auto valid=window.model.json();
            for(const auto &bad:QStringList{"-2 mm","90 deg","unknown_parameter",""}) {
                input=edit();QVERIFY(input);input->setText(bad);QTest::keyClick(input,Qt::Key_Return);
                QCOMPARE(window.model.json(),valid);QVERIFY(v->dimensionEditor);QTest::keyClick(input,Qt::Key_Escape);
            }
            input=edit();QVERIFY(input);input->setText("30");QTest::keyClick(input,Qt::Key_Return);
            QCOMPARE(window.model.get(id).p[field].toDouble(),circle?15.:30.);
            window.findChild<QAction *>("undo")->trigger();QCOMPARE(window.model.json(),valid);
            window.findChild<QAction *>("redo")->trigger();
            window.model.setParameters({{"medida","40 mm"}});
        }
        window.model.save(dir.filePath("expressions.mcad"));Model reopened;reopened.load(window.model.filePath);
        QCOMPARE(reopened.json(),window.model.json());
    }
    void conflictingSketchConstraintIsExplained() {
        QTemporaryDir dir;Model source;
        const auto id=source.add("sketch",{{"profile","polyline"},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{20,10}}}});
        source.constrainSketch(id,sketch::Relation::Fixed,"p0",{},{0,0});
        source.constrainSketch(id,sketch::Relation::Fixed,"p1",{},{20,10});
        source.save(dir.filePath("conflict.mcad"));
        Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();v->onSelect(id);
        v->selectedDetails={{id,"edge",0,{{0,0,0},{20,10,0}}}};
        const auto before=window.model.json();bool explained=false,highlighted=false;
        QTimer::singleShot(100,[&]{
            auto *dialog=window.findChild<QDialog *>("constraintConflict");if(!dialog)return;
            auto *list=dialog->findChild<QListWidget *>("constraintConflictList");
            explained=list->count()==3 && list->currentItem()->text().startsWith("NOVA");
            highlighted=v->selectedDetails.size()==1 && v->selectedDetails.front().geometry.size()==2;
            dialog->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("constraint-conflict.png"));
            dialog->reject();
        });
        window.findChild<QAction *>("constraint_horizontal")->trigger();
        QVERIFY(explained);QVERIFY(highlighted);QCOMPARE(window.model.json(),before);
        QCOMPARE(v->selectedDetails.front().index,0);
        bool reviewed=false;
        QTimer::singleShot(100,[&]{
            auto *dialog=window.findChild<QDialog *>("constraintConflict");if(!dialog)return;
            QTimer::singleShot(100,[&]{
                auto *review=window.findChild<QDialog *>("constraintReview");
                reviewed=review && review->isVisible();if(review)review->reject();
            });
            dialog->findChild<QPushButton *>("reviewConflictingConstraints")->click();
        });
        window.findChild<QAction *>("constraint_horizontal")->trigger();
        QVERIFY(reviewed);QCOMPARE(window.model.json(),before);
    }
    void reviewRedundantSketchConstraints() {
        QTemporaryDir dir;Model source;
        const auto id=source.add("sketch",{{"profile","rectangle"},{"w",30},{"h",20}});
        const auto redundant=source.constrainSketch(id,sketch::Relation::Horizontal,"e0");
        source.save(dir.filePath("review.mcad"));
        Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();v->onSelect(id);
        const auto before=window.model.json();
        for(bool accept : {false,true}) {
            bool located=false,labelled=false;
            QTimer::singleShot(100,[&] {
                auto *dialog=window.findChild<QDialog *>("constraintReview");
                if(!dialog)return;
                auto *list=dialog->findChild<QListWidget *>("constraintReviewList");
                for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toString()==redundant) {
                    list->setCurrentRow(row);labelled=list->item(row)->text().contains("Redundante");
                }
                located=v->selectedDetails.size()==1 && v->selectedDetails.front().kind=="edge" &&
                    v->selectedDetails.front().geometry.size()==2;
                window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("constraint-review.png"));
                dialog->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("constraint-review-panel.png"));
                if(accept)dialog->findChild<QPushButton *>("removeReviewedConstraint")->click();else dialog->reject();
            });
            window.findChild<QAction *>("constraint_remove")->trigger();
            QVERIFY(located);QVERIFY(labelled);
            if(!accept)QCOMPARE(window.model.json(),before);
            else {
                QVERIFY(!window.model.sketchSystem(id).solve().redundantConstraints.contains(redundant));
                window.findChild<QAction *>("undo")->trigger();QCOMPARE(window.model.json(),before);
            }
        }
    }
    void planarMoveHandles() {
        QTemporaryDir dir;
        for (bool stl : {false,true}) {
            Model source;source.add("box",{{"w",40},{"h",30},{"d",20}});
            if(stl){source.exportStl(dir.filePath("plane.stl"));source.clear();source.importStl(dir.filePath("plane.stl"));}
            source.save(dir.filePath("plane.mcad"));
            Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();
            const auto before=window.model.json();
            for(int normal=0;normal<3;++normal) {
                bool started=false, live=false, locked=false;
                QTimer::singleShot(150,[&] {
                    v->snap=false;
                    const auto start=v->movePlanePolygon(normal).boundingRect().center().toPoint();
                    QTest::mousePress(v,Qt::LeftButton,Qt::NoModifier,start);
                    started=v->draggingMoveFree && v->movePlaneNormal==normal;
                    const auto previous=v->mesh.front().a;
                    const auto end=start+QPoint(45,-35);
                    QMouseEvent move(QEvent::MouseMove,end,v->mapToGlobal(end),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
                    QApplication::sendEvent(v,&move);QTest::qWait(90);
                    live=(v->mesh.front().a-previous).length()>1;
                    locked=std::abs(v->moveDistances[normal])<1e-6;
                    QTest::mouseRelease(v,Qt::LeftButton,Qt::NoModifier,end);
                    if(normal==2)window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("planar-move.png"));
                    v->onAcceptCommand();
                });
                window.findChild<QAction *>("transform")->trigger();
                QVERIFY(started);QVERIFY(live);QVERIFY(locked);
                QCOMPARE(window.model.features.size(),size_t(2));
                QCOMPARE(window.model.features.back().p[QString("xyz")[normal]].toDouble(),0.);
                window.findChild<QAction *>("undo")->trigger();QCOMPARE(window.model.json(),before);
            }
        }
    }
    void planarDragPreservesNormalCoordinate() {
        Model model;Viewport v(&model);v.resize(1000,700);v.view("iso");
        v.moveHandleActive=true;v.snap=true;
        for(int normal=0;normal<3;++normal) {
            v.draggingMoveFree=true;v.movePlaneNormal=normal;
            v.moveStartDistances={1.234f,2.345f,3.456f};v.moveDragInverse=v.matrix().inverted();
            const QPoint p(100,100);
            QMouseEvent event(QEvent::MouseMove,p,p,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(&v,&event);
            QCOMPARE(v.moveDistances[normal],v.moveStartDistances[normal]);
            QVERIFY((v.moveDistances-v.moveStartDistances).length()>1);
        }
        v.draggingMoveFree=false;v.moveDistances={};v.view("top");
        for(int normal : {0,1})
            QVERIFY(v.movePlaneAt(v.movePlanePolygon(normal).boundingRect().center())!=normal);
        v.movePlaneNormal=0;v.draggingMoveFree=true;v.moveDragInverse=v.matrix().inverted();
        const auto before=v.moveDistances;
        QMouseEvent edgeOn(QEvent::MouseMove,QPointF(300,200),QPointF(300,200),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&v,&edgeOn);QCOMPARE(v.moveDistances,before);
    }
    void pickRotationPivotVertex() {
        QTemporaryDir dir;
        for(bool stl : {false,true}) {
        Model source;
        source.add("box",{{"w",40},{"h",30},{"d",20}});
        if(stl){source.exportStl(dir.filePath("pick.stl"));source.clear();source.importStl(dir.filePath("pick.stl"));}
        source.save(dir.filePath("pick.mcad"));
        Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();
        const auto before=window.model.json();const auto filter=v->selectionFilter;
        bool picked=false;
        QTimer::singleShot(150,[&] {
            auto *button=window.findChild<QPushButton *>("pickMovePivot");
            if(!button){v->onCancelCommand();return;}
            button->click();QTest::qWait(40);
            const QVector3D corner(0,0,20);
            QTest::mouseClick(v,Qt::LeftButton,Qt::NoModifier,v->project(corner).toPoint());
            QTest::qWait(70);
            picked=!button->isChecked() && (v->handleOrigin-corner).length()<.001;
            v->onAcceptCommand();
        });
        window.findChild<QAction *>("transform")->trigger();
        QVERIFY(picked);QCOMPARE(window.model.json(),before);
        QCOMPARE(v->selectionFilter,filter);QVERIFY(!v->commandSelectSubelements);
        QVERIFY(!v->commandPickMeshVertices);
        bool choosing=false;
        QTimer::singleShot(100,[&] {
            window.findChild<QPushButton *>("pickMovePivot")->click();
            choosing=v->commandPickMeshVertices;
            v->onCancelCommand();
        });
        window.findChild<QAction *>("transform")->trigger();
        QVERIFY(choosing);QVERIFY(!v->commandPickMeshVertices);QCOMPARE(window.model.json(),before);
        }
    }
    void meshPivotPickingVisibility() {
        QTemporaryDir dir;Model model;model.add("box",{{"w",40},{"h",30},{"d",20}});
        model.exportStl(dir.filePath("points.stl"));model.clear();const auto id=model.importStl(dir.filePath("points.stl"));
        Viewport v(&model);v.resize(1000,700);v.refresh();v.view("top");v.selectionFilter="vertex";
        const QVector3D point(0,0,20);
        QVERIFY(v.pickDetail(v.project(point)).kind!="vertex");
        v.commandPickMeshVertices=true;
        for(float span : {60.f,120.f,240.f}) {
            v.span=span;
            const auto hit=v.pickDetail(v.project(point)+QPointF(1,1));
            QCOMPARE(hit.feature,id);QCOMPARE(hit.kind,QString("vertex"));
            QVERIFY((hit.geometry.front()-point).length()<1e-4);
            QVERIFY(v.pickDetail(v.project(point),true).kind!="vertex");
        }
        model.add("box",{{"x",-5},{"y",-5},{"z",30},{"w",50},{"h",40},{"d",5}});
        v.setModel(&model);
        const auto hidden=v.pickDetail(v.project(point));
        QVERIFY(hidden.feature!=id || hidden.kind!="vertex");
    }
    void customRotationPivot() {
        QTemporaryDir dir;
        for (bool stl : {false, true}) {
            Model source;
            source.add("box", {{"x",100},{"y",50},{"z",20},{"w",40},{"h",30},{"d",20}});
            if(stl) { source.exportStl(dir.filePath("pivot.stl")); source.clear(); source.importStl(dir.filePath("pivot.stl")); }
            source.save(dir.filePath("pivot.mcad"));
            Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);
            auto *v=window.findChild<Viewport *>();
            const auto before=window.model.json();
            for (bool cancel : {true,false}) {
                bool configured=false;
                QTimer::singleShot(100,[&] {
                    auto *custom=window.findChild<QPushButton *>("customMovePivot");
                    if(!custom){v->onCancelCommand();return;}
                    custom->click();
                    window.findChild<QDoubleSpinBox *>("movePivot_px")->setValue(100);
                    window.findChild<QDoubleSpinBox *>("movePivot_py")->setValue(50);
                    window.findChild<QDoubleSpinBox *>("movePivot_pz")->setValue(20);
                    v->onRotateAngle(90); QTest::qWait(80);
                    configured=(v->handleOrigin-QVector3D(100,50,20)).length()<.001;
                    if(cancel)v->onCancelCommand();else v->onAcceptCommand();
                });
                window.findChild<QAction *>("transform")->trigger();
                QVERIFY(configured);
                if(cancel) {QCOMPARE(window.model.json(),before);continue;}
                const auto p=window.model.features.back().p;
                QCOMPARE(p["px"].toDouble(),100.);QCOMPARE(p["py"].toDouble(),50.);
                QVector3D center;const auto mesh=window.model.triangles();
                for(const auto &t:mesh)center+=t.a+t.b+t.c;
                center/=mesh.size()*3;
                QVERIFY((center-QVector3D(85,70,30)).length()<.01);
                const auto after=window.model.json();
                window.model.save(dir.filePath("rotated.mcad"));Model reopened;reopened.load(dir.filePath("rotated.mcad"));
                QCOMPARE(reopened.json(),after);
                window.findChild<QAction *>("undo")->trigger();QCOMPARE(window.model.json(),before);
                window.findChild<QAction *>("redo")->trigger();QCOMPARE(window.model.json(),after);
            }
        }
    }
    void arcIntersectionDoesNotExtendArc() {
        for(const auto &plane:QStringList{"XY","XZ","YZ"}) {
            Model model;
            model.add("sketch",{{"profile","arc"},{"plane",plane},{"x1",5},{"y1",0},{"xm",0},{"ym",5},{"x2",-5},{"y2",0}});
            model.add("sketch",{{"profile","circle"},{"plane",plane},{"x",6},{"r",5}});
            Viewport v(&model);v.resize(1000,700);v.sketchMode=true;v.plane=plane;v.snap=false;v.span=40;v.view("top");
            const auto pixel=v.project(Model::planePoint(plane,3,4));
            QVERIFY(QLineF(v.sketchPoint(pixel+QPointF(.5,-.5)),{3,4}).length()<1e-5);QCOMPARE(v.magnetLabel,QString("Interseção"));
            v.magnetLabel.clear();v.sketchPoint(v.project(Model::planePoint(plane,3,-4)));
            QVERIFY(v.magnetLabel!=QString("Interseção"));
        }
    }
    void curvedIntersectionsSnapOnSketchPlanes() {
        for(const auto &plane:QStringList{"XY","XZ","YZ"}) {
            Model model;
            model.add("sketch",{{"profile","circle"},{"plane",plane},{"offset",8},{"r",5}});
            model.add("sketch",{{"profile","circle"},{"plane",plane},{"offset",8},{"x",6},{"r",5}});
            model.add("sketch",{{"profile","polyline"},{"plane",plane},{"offset",8},
                {"points",QJsonArray{QJsonArray{-10,3},QJsonArray{10,3}}}});
            Viewport v(&model);v.resize(1000,700);v.sketchMode=true;v.plane=plane;v.planeOffset=8;v.snap=false;v.smartSnap=true;
            const auto before=model.json();
            v.view("top");
            for(float span:{40.f,120.f}) {
                v.span=span;
                for(const auto &point:QList<QPointF>{{3,4},{3,-4},{-4,3},{4,3}}) {
                    v.magnetLabel.clear();const auto pixel=v.project(Model::planePoint(plane,point.x(),point.y(),8));
                    QVERIFY(QLineF(v.sketchPoint(pixel+QPointF(.5,-.5)),point).length()<1e-5);
                    QCOMPARE(v.magnetLabel,QString("Interseção"));
                }
            }
            v.smartSnap=false;v.magnetLabel.clear();const auto point=QPointF(3,4);
            const auto pixel=v.project(Model::planePoint(plane,3,4,8));
            QVERIFY(QLineF(v.sketchPoint(pixel+QPointF(3,2)),point).length()>1e-3);QVERIFY(v.magnetLabel.isEmpty());
            QCOMPARE(model.json(),before);
        }
    }
    void namedSketchDimensionFromSelection() {
        QTemporaryDir dir;Model source;source.setParameters({{"largura","30 mm"}});
        const auto sk=source.add("sketch",{{"profile","rectangle"},{"w",20},{"h",10},{"plane","XY"}});
        source.save(dir.filePath("source.mcad"));Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();v->sketchMode=true;v->onSelect(sk);v->fit();v->view("top");v->selectionFilter="edge";
        QTest::mouseClick(v,Qt::LeftButton,Qt::NoModifier,v->project({10,0,0}).toPoint());QCOMPARE(v->selectedDetail.kind,QString("edge"));
        const auto before=window.model.json();
        QTimer watchdog;watchdog.setInterval(4000);connect(&watchdog,&QTimer::timeout,[&]{for(auto *d:window.findChildren<QDialog *>())if(d->isVisible())d->reject();});watchdog.start();
        QTimer::singleShot(100,[&]{for(auto *dialog:window.findChildren<QInputDialog *>())dialog->reject();});
        window.findChild<QAction *>("constraint_distance_x")->trigger();QCOMPARE(window.model.json(),before);
        QTimer::singleShot(100,[&]{for(auto *dialog:window.findChildren<QInputDialog *>()){dialog->setTextValue("largura");dialog->accept();}});
        window.findChild<QAction *>("constraint_distance_x")->trigger();
        const auto system=window.model.sketchSystem(sk);QCOMPARE(system.constraints.size(),5);
        const auto constraint=system.constraints.back();QCOMPARE(constraint.relation,sketch::Relation::DistanceX);QCOMPARE(constraint.value.x(),30.);
        QCOMPARE(window.model.get(sk).p.value("expressions").toObject().value("constraint:"+constraint.id).toString(),QString("largura"));
        window.findChild<QAction *>("undo")->trigger();QCOMPARE(window.model.json(),before);
        window.findChild<QAction *>("redo")->trigger();QCOMPARE(window.model.sketchSystem(sk).constraints.back().value.x(),30.);
        v->onSelect(sk);v->update();QTest::qWait(80);QVERIFY(!v->dimensions.empty());
        auto target=v->dimensions.back();QVERIFY(target.key.startsWith("constraint:"));
        QTest::mouseClick(v,Qt::LeftButton,Qt::NoModifier,target.rect.center().toPoint());
        auto *editor=v->findChild<QLineEdit *>("inlineDimension");QVERIFY(editor);QCOMPARE(editor->text(),QString("largura"));
        editor->setText("largura / 2");QTest::keyClick(editor,Qt::Key_Return);
        QCOMPARE(window.model.sketchSystem(sk).constraints.back().value.x(),15.);
        const auto dimensionState=window.model.json();v->update();QTest::qWait(80);
        QTest::mouseClick(v,Qt::LeftButton,Qt::NoModifier,v->dimensions.back().rect.center().toPoint());
        editor=v->findChild<QLineEdit *>("inlineDimension");QVERIFY(editor);editor->setText("0 mm");QTest::keyClick(editor,Qt::Key_Return);
        QCOMPARE(window.model.json(),dimensionState);QVERIFY(editor->isVisible());QTest::keyClick(editor,Qt::Key_Escape);
        window.model.save(dir.filePath("result.mcad"));QVERIFY(window.close());
    }
    void dynamicOperationEdgesAndFilletReedit() {
        for(bool chamfer:{false,true}) {
            QTemporaryDir dir;Model source;auto body=source.add("box",{{"w",30},{"h",30},{"d",10}});
            source.save(dir.filePath("source.mcad"));Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);
            window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();v->fit();v->view("iso");v->onSelect(body);
            bool selectedTwo=false,toggledOff=false;
            QTimer watchdog;watchdog.setInterval(4000);connect(&watchdog,&QTimer::timeout,[&]{if(v->onCancelCommand)v->onCancelCommand();});watchdog.start();
            QTimer::singleShot(200,[&]{
                auto *choose=window.findChild<QPushButton *>("chooseOperationEdges");if(!choose)return;
                choose->setChecked(true);
                QTest::mouseClick(v,Qt::LeftButton,Qt::NoModifier,v->project({30,0,5}).toPoint());
                QTest::mouseClick(v,Qt::LeftButton,Qt::ShiftModifier,v->project({15,0,10}).toPoint());
                selectedTwo=v->selectedDetails.size()==2;
                QTest::mouseClick(v,Qt::LeftButton,Qt::ShiftModifier,v->project({15,0,10}).toPoint());
                toggledOff=v->selectedDetails.size()==1;
                choose->setChecked(false);if(v->onAcceptCommand)v->onAcceptCommand();
            });
            auto *action=window.findChild<QAction *>(chamfer?"chamfer":"fillet");action->trigger();
            QVERIFY(selectedTwo);QVERIFY(toggledOff);QCOMPARE(window.model.features.size(),size_t(2));
            auto id=window.model.features.back().id;QCOMPARE(window.model.get(id).p["edges"].toArray().size(),1);
            const auto before=window.model.json();v->onSelect(id);
            QTimer::singleShot(200,[&]{if(v->onAcceptCommand)v->onAcceptCommand();});action->trigger();
            QCOMPARE(window.model.json(),before); // Same feature and unchanged parameters.
            v->onSelect(id);QTimer::singleShot(200,[&]{
                auto *value=window.findChild<QDoubleSpinBox *>(chamfer?"chamferDistance":"filletRadius");if(value)value->setValue(2);
                if(v->onCancelCommand)v->onCancelCommand();
            });action->trigger();QCOMPARE(window.model.json(),before);
            window.model.save(dir.filePath("result.mcad"));QVERIFY(window.close());
        }
    }
    void suppressionFromHistory() {
        QTemporaryDir dir;Model source;auto base=source.add("box",{{"w",10},{"h",10},{"d",10}});
        auto moved=source.add("transform",{{"source",base},{"x",10}});source.save(dir.filePath("source.mcad"));
        Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();v->onSelect(moved);
        window.findChild<QAction *>("suppress")->trigger();
        QVERIFY(window.model.get(moved).suppressed);QVERIFY(!window.model.consumed(base));
        auto *timeline=window.findChild<QListWidget *>("timeline");
        QVERIFY(timeline->item(1)->toolTip().contains("suprimida"));
        QVERIFY(timeline->item(1)->font().strikeOut());
        window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("suppression-history.png"));
        QVERIFY(!v->mesh.empty());
        for(const auto &triangle:v->mesh)QCOMPARE(window.model.features[triangle.feature].id,base);
        window.findChild<QAction *>("suppress")->trigger();QCOMPARE(window.model.json(),source.json());
        QVERIFY(!window.model.dirty);
        window.findChild<QAction *>("undo")->trigger();QVERIFY(window.model.get(moved).suppressed);
        window.findChild<QAction *>("redo")->trigger();QCOMPARE(window.model.json(),source.json());
        QVERIFY(window.close());
    }
    void expressionBoundExtrusionEditIsNoOp() {
        QTemporaryDir dir;Model source;
        auto sketch=source.add("sketch",{{"profile","rectangle"},{"w",10},{"h",10}});
        auto body=source.add("extrude",{{"source",sketch},{"d",1}});
        source.setExpression(body,"d","3 mm / 7");source.save(dir.filePath("source.mcad"));
        Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));auto *v=window.findChild<Viewport *>();v->onSelect(body);
        bool previewCreated=false,handleDisabled=false;double previewDistance=0;QString feedback;
        QTimer watchdog;watchdog.setInterval(4000);connect(&watchdog,&QTimer::timeout,[&]{if(v->onCancelCommand)v->onCancelCommand();});watchdog.start();
        QTimer::singleShot(200,[&]{
            previewCreated=v->model!=&window.model;handleDisabled=!v->handleActive;
            previewDistance=v->model->get(body).p["d"].toDouble();
            for(auto *label:window.findChildren<QLabel *>())if(label->isVisible())feedback+=label->text()+"\n";
            if(v->onAcceptCommand)v->onAcceptCommand();
        });
        window.findChild<QAction *>("extrude")->trigger();QVERIFY2(previewCreated,qPrintable(feedback));QVERIFY(handleDisabled);
        QVERIFY(std::abs(previewDistance-3./7)<1e-14);
        QCOMPARE(window.model.json(),source.json());QVERIFY(!window.model.dirty);QVERIFY(window.close());
    }
    void autosaveTimerSurvivesProcessCrash() {
        QTemporaryDir dir;QVERIFY(dir.isValid());QProcess child;
        child.start(QCoreApplication::applicationFilePath(),{"--recovery-ui-writer",dir.path()});
        QVERIFY(child.waitForStarted(10000));QByteArray output;QElapsedTimer timeout;timeout.start();
        while(!output.contains("READY") && timeout.elapsed()<15000) {
            child.waitForReadyRead(200);output+=child.readAllStandardOutput();
        }
        QVERIFY2(output.contains("READY"),child.readAllStandardError().constData());
        QFile original(dir.filePath("original.mcad"));QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes=original.readAll();original.close();
        child.kill();QVERIFY(child.waitForFinished(10000));
        Window window(dir.filePath("recoveries"),false);window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTimer::singleShot(100,[&]{for(auto *d:window.findChildren<QInputDialog *>())d->accept();});
        window.findChild<QAction *>("recover")->trigger();
        QVERIFY(window.model.dirty);QVERIFY(window.model.filePath.isEmpty());
        QCOMPARE(window.model.parameters().value("width"),QString("20 mm"));
        QVERIFY(std::abs(Model::volume(window.model.features.front().shape)-2000)<1e-6);
        QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),bytes);original.close();
        window.model.save(dir.filePath("recovered.mcad"));QVERIFY(window.close());
        QVERIFY(QDir(dir.filePath("recoveries")).entryList({"*.json"},QDir::Files).empty());
    }
    void unavailableRecoveryStaysVisible() {
        QTemporaryDir dir;QFile notDirectory(dir.filePath("blocked"));QVERIFY(notDirectory.open(QIODevice::WriteOnly));notDirectory.close();
        Window window(notDirectory.fileName(),false);window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *notice=window.findChild<QLabel *>("recoveryStatus");QVERIFY(notice);QVERIFY(notice->isVisible());
        QVERIFY(notice->text().contains("indisponível"));QVERIFY(!notice->toolTip().isEmpty());
        window.findChild<QAction *>("undo")->trigger();QVERIFY(notice->text().contains("indisponível"));
        QVERIFY(window.close());
    }
    void chamferPreviewEditAndCancel() {
        QTemporaryDir dir;Model source;source.add("box",{{"w",30},{"h",30},{"d",10}});
        source.save(dir.filePath("box.mcad"));Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v=window.findChild<Viewport *>();v->refresh();v->fit();v->view("iso");v->selectionFilter="edge";
        QTest::mouseClick(v,Qt::LeftButton,Qt::NoModifier,v->project({30,0,5}).toPoint());
        QCOMPARE(v->selectedDetail.kind,QString("edge"));
        const auto before=window.model.json();bool preview=false,blocked=false;
        QTimer watchdog;watchdog.setInterval(4000);connect(&watchdog,&QTimer::timeout,[&]{if(v->onCancelCommand)v->onCancelCommand();});watchdog.start();
        QTimer::singleShot(200,[&]{
            preview=v->model!=&window.model && window.model.json()==before && v->model->features.size()==2;
            auto *distance=window.findChild<QDoubleSpinBox *>("chamferDistance");if(!distance)return;
            distance->setValue(100);if(v->onAcceptCommand)v->onAcceptCommand();
            blocked=bool(v->onCancelCommand) && window.model.json()==before;
            distance->setValue(2);window.findChild<QComboBox *>("chamferMode")->setCurrentIndex(1);
            window.findChild<QDoubleSpinBox *>("chamferDistance2")->setValue(3);
            if(v->onAcceptCommand)v->onAcceptCommand();
        });
        window.findChild<QAction *>("chamfer")->trigger();QVERIFY(preview);QVERIFY(blocked);
        QCOMPARE(window.model.features.size(),size_t(2));QCOMPARE(window.model.features.back().p["mode"].toString(),QString("two"));
        const auto id=window.model.features.back().id;const auto good=window.model.json();
        v->onSelect(id); // Reopen the existing operation instead of adding another one.
        QTimer::singleShot(200,[&]{
            auto *distance=window.findChild<QDoubleSpinBox *>("chamferDistance");if(distance)distance->setValue(1);
            if(v->onAcceptCommand)v->onAcceptCommand();
        });
        window.findChild<QAction *>("chamfer")->trigger();QCOMPARE(window.model.features.size(),size_t(2));
        QCOMPARE(window.model.get(id).p["d"].toDouble(),1.);
        window.findChild<QAction *>("undo")->trigger();QCOMPARE(window.model.json(),good);
        window.findChild<QAction *>("redo")->trigger();const auto edited=window.model.json();v->onSelect(id);
        QTimer::singleShot(200,[&]{
            window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("chamfer-preview.png"));
            if(v->onCancelCommand)v->onCancelCommand();
        });
        window.findChild<QAction *>("chamfer")->trigger();QCOMPARE(window.model.json(),edited);
        window.model.save(dir.filePath("chamfer.mcad"));Model loaded;loaded.load(window.model.filePath);
        QCOMPARE(loaded.json(),edited);QVERIFY(window.close());
    }
    void namedParametersEditing() {
        QTemporaryDir dir;Model source;
        const auto id=source.add("box",{{"w",10},{"h",10},{"d",10}});
        source.save(dir.filePath("source.mcad"));
        Window window(dir.filePath("recovery"),false);window.openPath(source.filePath);
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTimer watchdog;watchdog.setInterval(4000);connect(&watchdog,&QTimer::timeout,[&]{
            for(auto *d:window.findChildren<QDialog *>())if(d->isVisible())d->reject();
        });watchdog.start();
        QTimer::singleShot(100,[&]{
            auto *d=window.findChild<QDialog *>("parameterEditor");if(!d)return;
            auto *t=d->findChild<QTableWidget *>("parameterTable");t->setRowCount(1);
            t->setItem(0,0,new QTableWidgetItem("largura"));t->setItem(0,1,new QTableWidgetItem("2 cm"));
            d->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("parameter-editor.png"));
            QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        window.findChild<QAction *>("parameters")->trigger();
        QCOMPARE(window.model.parameters().value("largura"),QString("2 cm"));
        auto *timeline=window.findChild<QListWidget *>("timeline");
        QTest::mouseClick(timeline->viewport(),Qt::LeftButton,Qt::NoModifier,timeline->visualItemRect(timeline->item(0)).center());
        QTimer::singleShot(100,[&]{
            auto *d=window.findChild<QDialog *>("expressionEditor");if(!d)return;
            d->findChild<QComboBox *>("expressionField")->setCurrentText("w");
            d->findChild<QLineEdit *>("expressionInput")->setText("largura");
            QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        window.findChild<QAction *>("expression")->trigger();
        QCOMPARE(window.model.get(id).p["w"].toDouble(),20.);
        const auto before=window.model.json();
        QTimer::singleShot(100,[&]{
            auto *d=window.findChild<QDialog *>("parameterEditor");if(!d)return;
            d->findChild<QTableWidget *>("parameterTable")->item(0,1)->setText("30 mm");
            d->reject();
        });
        window.findChild<QAction *>("parameters")->trigger();QCOMPARE(window.model.json(),before);
        QTimer::singleShot(100,[&]{
            auto *d=window.findChild<QDialog *>("parameterEditor");if(!d)return;
            d->findChild<QTableWidget *>("parameterTable")->item(0,1)->setText("30 mm");
            QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        window.findChild<QAction *>("parameters")->trigger();QCOMPARE(window.model.get(id).p["w"].toDouble(),30.);
        window.findChild<QAction *>("undo")->trigger();QCOMPARE(window.model.json(),before);
        window.findChild<QAction *>("redo")->trigger();QCOMPARE(window.model.get(id).p["w"].toDouble(),30.);
        window.model.save(dir.filePath("result.mcad"));Model loaded;loaded.load(window.model.filePath);
        QCOMPARE(loaded.json(),window.model.json());QVERIFY(window.close());
    }
    void parameterValuesAndErrorsBeforeApply() {
        QTemporaryDir dir;Window window(dir.filePath("recovery"),false);
        window.model.setParameters({{"length","2 cm"},{"area","length * length"}});
        const auto before=window.model.json();window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        bool initialValues=false,invalidBlocked=false,recovered=false,readOnly=false;
        QTimer::singleShot(100,[&]{
            auto *dialog=window.findChild<QDialog *>("parameterEditor");if(!dialog)return;
            auto *table=dialog->findChild<QTableWidget *>("parameterTable");
            auto *ok=dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
            auto *error=dialog->findChild<QLabel *>("parameterError");
            initialValues=table->item(0,2)->text()=="400 mm^2" && table->item(1,2)->text()=="20 mm";
            readOnly=!(table->item(0,2)->flags() & Qt::ItemIsEditable);
            table->item(1,1)->setText("area");
            invalidBlocked=!ok->isEnabled() && !error->text().isEmpty() && table->item(0,2)->text().isEmpty();
            table->item(1,1)->setText("3 cm");
            recovered=ok->isEnabled() && error->text().isEmpty() && table->item(0,2)->text()=="900 mm^2";
            dialog->reject();
        });
        window.findChild<QAction *>("parameters")->trigger();
        QVERIFY(initialValues);QVERIFY(readOnly);QVERIFY(invalidBlocked);QVERIFY(recovered);
        QCOMPARE(window.model.json(),before);window.model.save(dir.filePath("clean.mcad"));QVERIFY(window.close());
    }
    void sketchConstraintsSelectionAndPersistence() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        Model source;
        auto id=source.add("sketch",{{"profile","polyline"},{"closed",false},{"plane","XY"},
            {"points",QJsonArray{QJsonArray{0,0},QJsonArray{20,5}}}},"Line");
        source.save(dir.filePath("line.mcad"));
        Window window(dir.filePath("recovery"),false); window.openPath(source.filePath);
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v=window.findChild<Viewport *>(); QVERIFY(v);
        v->onEditSketch(id);
        v->view("top"); v->fit(); v->selectionFilter="edge";
        QTest::qWait(300);
        QTest::mouseClick(v,Qt::LeftButton,Qt::NoModifier,v->project({10,2.5,0}).toPoint());
        QCOMPARE(v->selectedDetail.kind,QString("edge"));
        QToolButton *horizontal=nullptr;
        for(auto *button:window.findChildren<QToolButton *>())
            if(button->defaultAction()==window.findChild<QAction *>("constraint_horizontal")) horizontal=button;
        QVERIFY(horizontal); QVERIFY(horizontal->isVisible());
        QTest::mouseClick(horizontal,Qt::LeftButton);
        QCOMPARE(window.model.json()["version"].toInt(),2);
        auto system=window.model.sketchSystem(id);
        QCOMPARE(system.constraints.size(),1);
        QVERIFY(std::abs(system.points[0].position.y()-system.points[1].position.y())<1e-8);
        QVERIFY(window.findChild<QLabel *>("sketchConstraintStatus"));
        window.findChild<QDockWidget *>("propertiesDock")->show();
        window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("sketch-constraints.png"));
        auto constrained=window.model.json();
        window.findChild<QAction *>("undo")->trigger(); QCOMPARE(window.model.json(),source.json());
        window.findChild<QAction *>("redo")->trigger(); QCOMPARE(window.model.json(),constrained);
        auto *timeline=window.findChild<QListWidget *>("timeline");
        QTest::mouseClick(timeline->viewport(),Qt::LeftButton,Qt::NoModifier,timeline->visualItemRect(timeline->item(0)).center());
        QTimer::singleShot(100,[&]{window.findChild<QDialog *>("constraintReview")->reject();});
        window.findChild<QAction *>("constraint_remove")->trigger(); QCOMPARE(window.model.json(),constrained);
        QTimer::singleShot(100,[&]{window.findChild<QDialog *>("constraintReview")->accept();});
        window.findChild<QAction *>("constraint_remove")->trigger();
        QVERIFY(window.model.sketchSystem(id).constraints.empty());
        window.findChild<QAction *>("undo")->trigger(); QCOMPARE(window.model.json(),constrained);
        window.model.save(dir.filePath("constrained.mcad"));
        Model reopened; reopened.load(window.model.filePath); QCOMPARE(reopened.json(),constrained);
        QVERIFY(window.close());
    }
    void historyDependenciesAndReorder() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        Model source;
        auto base = source.add("box",{{"w",10},{"h",10},{"d",10}},"Base");
        auto sphere = source.add("sphere",{{"r",2}},"Independent sphere");
        auto copy = source.add("copy",{{"source",base},{"x",20}},"Dependent copy");
        source.save(directory.filePath("source.mcad"));
        Window window(directory.filePath("recovery"),false);
        window.openPath(source.filePath);
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *timeline = window.findChild<QListWidget *>("timeline"); QVERIFY(timeline);
        QTest::mouseClick(timeline->viewport(),Qt::LeftButton,Qt::NoModifier,
            timeline->visualItemRect(timeline->item(1)).center());
        window.findChild<QAction *>("historyEarlier")->trigger();
        QCOMPARE(window.model.features.front().id,sphere);
        const auto before = window.model.json();
        QTest::mouseClick(timeline->viewport(),Qt::LeftButton,Qt::NoModifier,
            timeline->visualItemRect(timeline->item(1)).center());
        QString report;
        QTimer::singleShot(100,[&] {
            for (auto *dialog : window.findChildren<QMessageBox *>()) {
                report = dialog->text(); dialog->accept();
            }
        });
        window.findChild<QAction *>("dependencies")->trigger();
        QVERIFY(report.contains("Dependent copy"));
        QVERIFY(!report.contains("Independent sphere"));
        QCOMPARE(window.model.json(),before);
        QString failure;
        QTimer::singleShot(100,[&] {
            for (auto *dialog : window.findChildren<QMessageBox *>()) {
                failure = dialog->text(); dialog->accept();
            }
        });
        window.findChild<QAction *>("historyLater")->trigger();
        QVERIFY(failure.contains("futura"));
        QCOMPARE(window.model.json(),before);
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.json(),source.json());
        QVERIFY(!window.model.dirty);
        QVERIFY(window.close());
    }
    void isolatedRecoveryDialog() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto root = directory.filePath("recoveries");
        const auto originalPath = directory.filePath("original.mcad");
        QJsonObject expected;
        {
            RecoveryStore writer(root);
            Model source;
            auto id = source.add("box", {{"w",10},{"h",10},{"d",10}});
            source.save(originalPath);
            source.edit(id, {{"w",20},{"h",10},{"d",10}}, "Changed");
            expected = source.json(); writer.write(source);
        }
        Window window(root, false);
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto empty = window.model.json();
        QTimer::singleShot(100, [&] {
            for (auto *dialog : window.findChildren<QInputDialog *>()) dialog->reject();
        });
        window.findChild<QAction *>("recover")->trigger();
        QCOMPARE(window.model.json(), empty);
        QCOMPARE(QDir(root).entryList({"*.json"},QDir::Files).size(),1);
        QTimer::singleShot(100, [&] {
            for (auto *dialog : window.findChildren<QInputDialog *>()) dialog->accept();
        });
        window.findChild<QAction *>("recover")->trigger();
        QCOMPARE(window.model.json(),expected);
        QVERIFY(window.model.dirty); QVERIFY(window.model.filePath.isEmpty());
        Model original; original.load(originalPath);
        QVERIFY(std::abs(Model::volume(original.features.front().shape)-1000)<.001);
        QCOMPARE(QDir(root).entryList({"*.json"},QDir::Files).size(),1);
        // Simulate the persistence step, then exercise the normal clean-close path.
        window.model.save(directory.filePath("recovered.mcad"));
        QVERIFY(window.close());
        QVERIFY(QDir(root).entryList({"*.json"},QDir::Files).empty());
    }
    void selectedEdgeFilletAndMeasurement() {
        Window window;
        auto body = window.model.add("box", {{"w",30},{"h",30},{"d",10}});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v = window.findChild<Viewport *>();
        v->refresh(); v->fit(); v->view("iso");
        v->selectionFilter = "edge";
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, v->project({30,0,5}).toPoint());
        QCOMPARE(v->selectedDetail.kind, QString("edge"));
        auto before = window.model.json();
        bool preview = false, blocked = false;
        QTimer::singleShot(180, [&] {
            preview = v->model->features.size() == 2 && window.model.json() == before;
            auto *radius = window.findChild<QDoubleSpinBox *>("filletRadius");
            if (radius) radius->setValue(100);
            if (v->onAcceptCommand) v->onAcceptCommand();
            blocked = bool(v->onCancelCommand) && v->model->json() == before;
            if (radius) radius->setValue(2);
            if (v->onAcceptCommand) v->onAcceptCommand();
        });
        window.findChild<QAction *>("fillet")->trigger();
        QVERIFY(preview); QVERIFY(blocked);
        QCOMPARE(window.model.features.size(), size_t(2));
        QCOMPARE(window.model.features.back().p["edges"].toArray().size(), 1);
        auto good = window.model.json();
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.json(), before);
        window.findChild<QAction *>("redo")->trigger();
        QCOMPARE(window.model.json(), good);
        window.findChild<QAction *>("undo")->trigger();
        v->selectionFilter = "face";
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, v->project({15,15,10}).toPoint());
        QString measure;
        QTimer::singleShot(100, [&] {
            for (auto *dialog : window.findChildren<QMessageBox *>()) { measure = dialog->text(); dialog->accept(); }
        });
        window.findChild<QAction *>("measure")->trigger();
        QVERIFY(measure.contains("900.0000"));
        v->selectionFilter = "vertex";
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, v->project({30,0,0}).toPoint());
        QTest::mouseClick(v, Qt::LeftButton, Qt::ShiftModifier, v->project({30,0,10}).toPoint());
        QCOMPARE(v->selectedDetails.size(), 2);
        QTimer::singleShot(100, [&] {
            for (auto *dialog : window.findChildren<QMessageBox *>()) { measure = dialog->text(); dialog->accept(); }
        });
        window.findChild<QAction *>("measure")->trigger();
        QCOMPARE(measure, QString("Distância mínima: 10.0000 mm"));
        v->onSelect(body);
        QTimer::singleShot(180, [&] {
            window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("fillet-preview-test.png"));
            if (v->onCancelCommand) v->onCancelCommand();
        });
        window.findChild<QAction *>("fillet")->trigger();
        QCOMPARE(window.model.json(), before);
    }
    void frontFaceProfilePriority() {
        Window window;
        auto body = window.model.add("box", {{"w",30},{"h",30},{"d",30}});
        auto sketch = window.model.add("sketch", {{"profile","rectangle"},{"plane","XZ"},
                                                 {"x",10},{"y",5},{"w",10},{"h",15}});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v = window.findChild<Viewport *>();
        v->refresh(); v->fit(); v->view("iso");
        auto pixel = v->project({15,0,12}).toPoint();
        QCOMPARE(v->pickDetail(pixel).feature, sketch);
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, pixel);
        QCOMPARE(v->selectedDetail.feature, sketch);
        QCOMPARE(v->selectedDetail.kind, QString("object"));
        v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("front-profile-selection-test.png"));
        v->selectionFilter = "face";
        auto face = v->pickDetail(pixel);
        QCOMPARE(face.feature, body);
        QCOMPARE(face.kind, QString("face"));
        QVERIFY(Model::planeNormal(Model::facePlane(window.model.get(body).shape, face.index)).y() < -.99);
        v->selectionFilter = "auto";
        window.model.edit(sketch, {{"profile","rectangle"},{"plane","XZ"},{"offset",-30},
                                  {"x",10},{"y",5},{"w",10},{"h",15}}, "Hidden sketch");
        v->refresh();
        QVERIFY(v->pickDetail(v->project({15,30,12})).feature != sketch);
    }
    void faceSelectionAndSketch() {
        Window window;
        auto body = window.model.add("box", {{"w",30},{"h",30},{"d",10}});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *v = window.findChild<Viewport *>();
        v->refresh(); v->fit(); v->view("iso");
        auto pixel = v->project({15,15,10}).toPoint();
        auto face = v->pickDetail(pixel);
        QCOMPARE(face.kind, QString("face"));
        QCOMPARE(face.feature, body);
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, pixel);
        QCOMPARE(v->selectedDetail.kind, QString("face"));
        v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("face-selection-test.png"));
        window.findChild<QAction *>("sketch")->trigger();
        QVERIFY(v->sketchMode);
        QVERIFY(v->plane.startsWith("FACE:"));
        QVERIFY(Model::planeNormal(v->plane).z() > .99);
        auto local = Model::planeCoordinates(v->plane, {15,15,10});
        auto back = v->planeAt(v->project({15,15,10}), false);
        QVERIFY(QLineF(local,back).length()<.001);
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, v->project(Model::planePoint(v->plane,local.x()-3,local.y()-3)).toPoint());
        QTest::mouseClick(v, Qt::LeftButton, Qt::NoModifier, v->project(Model::planePoint(v->plane,local.x()+3,local.y()+3)).toPoint());
        QCOMPARE(window.model.features.size(), size_t(2));
        auto sketch = window.model.features.back().id;
        QCOMPARE(window.model.get(sketch).p["support"].toString(), body);
        window.findChild<QAction *>("finish")->trigger();
        auto target = v->pickDetail(v->project({15,15,10}));
        QCOMPARE(target.feature, sketch);
        QCOMPARE(target.kind, QString("object"));
        QCOMPARE(v->pickDetail(v->project({25,25,10})).kind, QString("face"));
        v->onSelect(sketch);
        bool acceptedProfile = false;
        QTimer::singleShot(180, [&] {
            acceptedProfile = v->handleActive && v->handleDistance == 0;
            v->onCancelCommand();
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(acceptedProfile);
        const auto beforeCut = window.model.json();
        v->onSelect(sketch);
        bool cutPreview = false;
        QTimer::singleShot(180, [&] {
            for (auto *combo : window.findChildren<QComboBox *>()) {
                int index = combo->findData("cut");
                if (index >= 0) combo->setCurrentIndex(index);
            }
            if (v->onHandleDistance) v->onHandleDistance(-5);
            QTest::qWait(100);
            cutPreview = v->model->features.size() == 3 && window.model.json() == beforeCut;
            window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("face-sketch-cut-workflow.png"));
            if (v->onAcceptCommand) v->onAcceptCommand();
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(cutPreview);
        QCOMPARE(window.model.features.size(), size_t(3));
        double removed = window.model.get(sketch).p["w"].toDouble() * window.model.get(sketch).p["h"].toDouble() * 5;
        QVERIFY(std::abs(Model::volume(window.model.features.back().shape)-(9000-removed)) < .01);
        auto result = window.model.json();
        window.findChild<QAction *>("undo")->trigger();
        QCOMPARE(window.model.json(), beforeCut);
        window.findChild<QAction *>("redo")->trigger();
        QCOMPARE(window.model.json(), result);
        QTemporaryDir dir;
        window.model.save(dir.filePath("face-cut.mcad"));
        window.openPath(dir.filePath("face-cut.mcad"));
        QCOMPARE(window.model.json(), result);
        window.model.exportStep(dir.filePath("face-cut.step"));
        window.model.exportStl(dir.filePath("face-cut.stl"));
        QVERIFY(QFileInfo(dir.filePath("face-cut.step")).size() > 0);
        QVERIFY(QFileInfo(dir.filePath("face-cut.stl")).size() > 0);
    }
    void extrudeCutPreview() {
        Window window;
        auto body = window.model.add("box", {{"w",30},{"h",30},{"d",10}});
        auto sketch = window.model.add("sketch", {{"profile","rectangle"},{"plane","XY"},{"offset",10},
                                                 {"x",10},{"y",10},{"w",10},{"h",10}});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        // Events below target widgets directly; desktop focus is not a prerequisite.
        auto *v = window.findChild<Viewport *>();
        v->refresh(); v->fit(); v->onSelect(sketch);
        auto before = window.model.json();
        bool preview = false, invalidCleared = false, keptOpen = false;
        QTimer::singleShot(180, [&] {
            for (auto *combo : window.findChildren<QComboBox *>()) {
                int index = combo->findData("cut");
                if (index >= 0) combo->setCurrentIndex(index);
            }
            v->onHandleDistance(-5);
            QTest::qWait(100);
            if (v->model->features.size() == 3)
                preview = std::abs(Model::volume(v->model->features.back().shape)-8500)<.01;
            v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("extrude-cut-test.png"));
            v->onHandleDistance(5);
            QTest::qWait(100);
            invalidCleared = v->model->json() == before;
            v->onAcceptCommand();
            keptOpen = bool(v->onHandleDistance);
            if (v->onHandleDistance) {
                v->onHandleDistance(-5);
                v->onAcceptCommand();
            }
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(preview); QVERIFY(invalidCleared); QVERIFY(keptOpen);
        QCOMPARE(window.model.features.back().p["target"].toString(), body);
        QVERIFY(std::abs(Model::volume(window.model.features.back().shape)-8500)<.01);
        QVERIFY(window.model.undo());
        QCOMPARE(window.model.json(), before);
    }
    void editSelectedElementsAndBodies() {
        Window window;
        auto id = window.model.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"w", 20}, {"h", 20}, {"offset",10}});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        // Events below target widgets directly; desktop focus is not a prerequisite.
        auto *v = window.findChild<Viewport *>();
        v->refresh();
        v->fit();
        auto chooseEdges = [&](int count) {
            v->onSelect(id);
            for (int i=0; i<count; ++i)
                v->selectedDetails.append({id,"edge",i,{{0,0,10},{20,20,10}}});
            v->selectedDetail = v->selectedDetails.back();
        };
        auto before = window.model.json();
        chooseEdges(2);
        bool previewed = false;
        QTimer::singleShot(180, [&] {
            if (!v->onMoveTranslation) { if (v->onCancelCommand) v->onCancelCommand(); return; }
            v->onMoveTranslation({3,4,0});
            QTest::qWait(100);
            previewed = v->model->get(id).p["profile"] == "polyline" && window.model.json() == before;
            v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("sketch-elements-move-test.png"));
            v->onAcceptCommand();
        });
        window.findChild<QAction *>("transform")->trigger();
        QVERIFY(previewed);
        QVERIFY(window.model.undo());
        QCOMPARE(window.model.json(), before);
        window.model.add("box", {{"w",30},{"h",30},{"d",10}});
        auto contourOnBody = window.model.json();
        v->refresh();
        v->fit();
        chooseEdges(4);
        bool recognized = false;
        QTimer::singleShot(180, [&] {
            recognized = v->handleActive && v->handleDistance == 0;
            v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("contour-extrude-test.png"));
            if (v->onCancelCommand) v->onCancelCommand();
        });
        window.findChild<QAction *>("extrude")->trigger();
        QVERIFY(recognized);
        QCOMPARE(window.model.json(), contourOnBody);
        QVERIFY(window.model.undo());
        QCOMPARE(window.model.json(), before);
        chooseEdges(1);
        window.findChild<QAction *>("delete")->trigger();
        QVERIFY(!window.model.get(id).p["closed"].toBool());
        QVERIFY(window.model.undo());
        QCOMPARE(window.model.json(), before);
        auto a = window.model.add("box", {{"w",10},{"d",10},{"h",10}});
        auto b = window.model.add("box", {{"w",10},{"d",10},{"h",10},{"x",30}});
        auto bodiesBefore = window.model.json();
        v->refresh();
        v->onSelect(b);
        v->selectedDetails = {{a,"object",-1,{}},{b,"object",-1,{}}};
        bool moved = false;
        QTimer::singleShot(180, [&] {
            if (!v->onMoveTranslation) { if (v->onCancelCommand) v->onCancelCommand(); return; }
            v->onMoveTranslation({5,6,7});
            v->onRotateAngle(90);
            QTest::qWait(100);
            moved = v->model->bodies().size() == 2 && window.model.json() == bodiesBefore;
            v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("batch-move-test.png"));
            v->onAcceptCommand();
        });
        window.findChild<QAction *>("transform")->trigger();
        QVERIFY(moved);
        QCOMPARE(window.model.features.size(), size_t(5));
        auto &first = window.model.features[3], &second = window.model.features[4];
        QCOMPARE(first.p["px"], second.p["px"]);
        QCOMPARE(first.p["angle"].toDouble(), 90.);
        QVERIFY(window.model.undo());
        QCOMPARE(window.model.json(), bodiesBefore);
        v->refresh();
        v->onSelect(b);
        v->selectedDetails = {{a,"object",-1,{}},{b,"object",-1,{}}};
        window.findChild<QAction *>("delete")->trigger();
        QVERIFY(window.model.bodies().empty());
        QVERIFY(window.model.undo());
        QCOMPARE(window.model.json(), bodiesBefore);
    }
    void areaSelection() {
        Model model;
        auto a = model.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"w", 20}, {"h", 20}});
        auto b = model.add("sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"x", 40}, {"w", 20}, {"h", 20}});
        Viewport v(&model);
        v.resize(1000, 700);
        v.view("top");
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        v.fit();
        auto box = [&](double x1, double y1, double x2, double y2) {
            return QRectF(v.project({float(x1), float(y1), 0}), v.project({float(x2), float(y2), 0})).normalized();
        };
        auto area = box(-2, -2, 22, 22);
        QCOMPARE(v.pickArea(area, false).size(), 1);
        QCOMPARE(v.pickArea(area, false).front().feature, a);
        auto partial = box(19, -2, 22, 22);
        QVERIFY(v.pickArea(partial, false).empty());
        QCOMPARE(v.pickArea(partial, true).size(), 1);
        v.selectionFilter = "edge";
        QCOMPARE(v.pickArea(area, false).size(), 4);
        v.selectionFilter = "vertex";
        QCOMPARE(v.pickArea(area, false).size(), 4);
        v.selectionFilter = "auto";
        auto drag = [&](QRectF rect, Qt::KeyboardModifiers modifiers) {
            auto start = rect.topLeft().toPoint(), end = rect.bottomRight().toPoint();
            QTest::mousePress(&v, Qt::LeftButton, modifiers, start);
            QMouseEvent move(QEvent::MouseMove, end, v.mapToGlobal(end), Qt::NoButton, Qt::LeftButton, modifiers);
            QApplication::sendEvent(&v, &move);
            QVERIFY(v.areaDragging);
            v.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("area-selection-test.png"));
            QTest::mouseRelease(&v, Qt::LeftButton, modifiers, end);
        };
        drag(area, Qt::NoModifier);
        QCOMPARE(v.selectedDetails.size(), 1);
        drag(box(38, -2, 62, 22), Qt::ShiftModifier);
        QCOMPARE(v.selectedDetails.size(), 2);
        QTest::keyClick(&v, Qt::Key_Escape);
        QVERIFY(v.selectedDetails.empty());
    }
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
        v.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("shift-selection-test.png"));
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
        // Events below target widgets directly; desktop focus is not a prerequisite.
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
            v->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("extrude-grid-test.png"));
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
            v.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("selected-edge-test.png"));
            QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, pixel(0, 0).toPoint());
            QCOMPARE(v.selectedDetail.kind, QString("vertex"));
            v.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("selected-vertex-test.png"));
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
                    window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("rotation-ring-test.png"));
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
                    window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("free-move-test.png"));
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
        window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("imported-stl-test.png"));
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
                    v.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("polygon-preview-test.png"));
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
        window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("inline-dimension-test.png"));
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
            v.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("smart-snap-test.png"));
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
            v.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("plane-layout-test.png"));
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
            viewport.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("cube-hover-test.png"));
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
        viewport.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("cube-home-icon.png"));
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
        viewport.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("cube-drag-test.png"));
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
        window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("plane-selection-test.png"));
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
        image.save(QDir(QCoreApplication::applicationDirPath()).filePath("sketch-test.png"));
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
            window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("extrude-live-drag.png"));
            QTest::mouseRelease(v, Qt::LeftButton, Qt::NoModifier, end);
            dragged = v->handleDistance > before + 5;
            QTest::qWait(80);
            window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("extrude-panel-test.png"));
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
            window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("move-panel-test.png"));
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
    const auto args=app.arguments();
    if(args.size()==3 && args[1]=="--recovery-ui-writer") {
        Window window(QDir(args[2]).filePath("recoveries"),false);window.show();
        window.model.setParameters({{"width","10 mm"}});
        window.model.add("box",{{"w",10},{"h",10},{"d",10},{"expressions",QJsonObject{{"w","width"}}}});
        window.model.save(QDir(args[2]).filePath("original.mcad"));
        window.model.setParameters({{"width","20 mm"}});
        // Accelerate the real production timer, not a direct RecoveryStore::write call.
        auto *timer=window.findChild<QTimer *>("recoveryTimer");if(!timer)return 2;timer->start(50);
        QTimer ready;
        QObject::connect(&ready,&QTimer::timeout,[&]{
            if(QDir(QDir(args[2]).filePath("recoveries")).entryList({"*.json"},QDir::Files).isEmpty())return;
            ready.stop();QTextStream(stdout)<<"READY\n"<<Qt::flush;
        });ready.start(50);return app.exec();
    }
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "ui_tests.moc"
