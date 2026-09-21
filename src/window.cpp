#include "window.h"
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <Standard_Failure.hxx>

namespace {
QString displayType(const QString &s) {
    static QMap<QString, QString> names = {
        {"box", "Box"},       {"cylinder", "Cylinder"},    {"sphere", "Sphere"},
        {"sketch", "Sketch"}, {"extrude", "Extrude"},      {"revolve", "Revolve"},
        {"hole", "Hole"},     {"boolean", "Combine"},      {"transform", "Move"},
        {"copy", "Copy"},     {"import", "Imported body"}, {"fillet", "Fillet"}};
    return names.value(s, s);
}
QString symbol(const QString &s) {
    static QMap<QString, QString> m = {{"sketch", "▱"},   {"extrude", "⬡"},   {"box", "□"},
                                       {"cylinder", "◉"}, {"sphere", "○"},    {"hole", "⊙"},
                                       {"boolean", "⊞"},  {"transform", "↗"}, {"copy", "▣"},
                                       {"fillet", "◜"},   {"revolve", "↻"},   {"import", "↓"}};
    return m.value(s, "◇");
}
QIcon icon(const QString &kind) {
    if (kind == "new")
        return qApp->style()->standardIcon(QStyle::SP_FileIcon);
    if (kind == "open")
        return qApp->style()->standardIcon(QStyle::SP_DirOpenIcon);
    if (kind == "save" || kind == "saveas")
        return qApp->style()->standardIcon(QStyle::SP_DialogSaveButton);
    if (kind == "undo")
        return qApp->style()->standardIcon(QStyle::SP_ArrowBack);
    if (kind == "redo")
        return qApp->style()->standardIcon(QStyle::SP_ArrowForward);
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#38576c"), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(QColor("#d4e6ed"));
    if (kind == "sketch" || kind == "rectangle" || kind == "polyline" || kind == "arc") {
        p.drawPolygon(QPolygonF{{10, 40}, {33, 49}, {54, 25}, {30, 17}});
        p.setPen(QPen(QColor("#dc873a"), 5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(25, 33, 46, 10);
        p.setPen(QPen(QColor("#38576c"), 2));
        p.drawLine(24, 33, 22, 40);
    } else if (kind == "circle" || kind == "hole" || kind == "sphere") {
        p.drawEllipse(QRectF(12, 12, 40, 40));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(kind == "hole" ? 22 : 25, kind == "hole" ? 22 : 12, kind == "hole" ? 20 : 14,
                             kind == "hole" ? 20 : 40));
    } else if (kind == "measure" || kind == "dimension") {
        p.save();
        p.translate(32, 32);
        p.rotate(-35);
        p.drawRoundedRect(QRectF(-25, -9, 50, 18), 2, 2);
        for (int x = -19; x < 22; x += 7)
            p.drawLine(x, -9, x, x % 2 ? -1 : 3);
        p.restore();
    } else if (kind == "fillet") {
        p.drawPath([] {
            QPainterPath path;
            path.moveTo(12, 51);
            path.lineTo(12, 32);
            path.quadTo(12, 12, 32, 12);
            path.lineTo(51, 12);
            path.lineTo(51, 51);
            path.closeSubpath();
            return path;
        }());
        p.setPen(QPen(QColor("#dc873a"), 3));
        p.drawArc(QRectF(12, 12, 40, 40), 90 * 16, 90 * 16);
    } else if (kind == "revolve") {
        p.drawRect(QRectF(29, 19, 12, 29));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#398caf"), 3));
        p.drawArc(QRectF(10, 8, 44, 44), 30 * 16, 300 * 16);
        p.drawLine(52, 21, 55, 9);
        p.drawLine(52, 21, 40, 19);
    } else if (kind == "fit") {
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(19, 19, 26, 26));
        for (int x : {10, 54})
            for (int y : {10, 54}) {
                p.drawLine(x, y, x + (x == 10 ? 10 : -10), y);
                p.drawLine(x, y, x, y + (y == 10 ? 10 : -10));
            }
    } else if (kind == "transform" || kind == "copy") {
        p.setPen(QPen(QColor("#398caf"), 3));
        p.drawLine(12, 49, 51, 10);
        p.drawLine(51, 10, 51, 27);
        p.drawLine(51, 10, 34, 10);
        p.drawLine(12, 49, 12, 34);
        p.drawLine(12, 49, 27, 49);
    } else if (kind == "cylinder") {
        p.drawRect(QRectF(15, 19, 34, 29));
        p.drawEllipse(QRectF(15, 39, 34, 15));
        p.drawEllipse(QRectF(15, 11, 34, 15));
    } else {
        p.drawPolygon(QPolygonF{{12, 22}, {32, 11}, {52, 22}, {52, 45}, {32, 56}, {12, 45}});
        p.setBrush(QColor("#a9c9d7"));
        p.drawPolygon(QPolygonF{{32, 33}, {52, 22}, {52, 45}, {32, 56}});
        p.drawLine(12, 22, 32, 33);
        p.drawLine(32, 33, 32, 56);
        if (kind == "extrude") {
            p.setPen(QPen(QColor("#dc873a"), 3));
            p.drawLine(32, 27, 32, 4);
            p.drawLine(32, 4, 25, 11);
            p.drawLine(32, 4, 39, 11);
        }
    }
    return QIcon(pix);
}
class Form : public QDialog {
  public:
    QFormLayout *layout;
    QMap<QString, QDoubleSpinBox *> nums;
    QMap<QString, QComboBox *> combos;
    Form(QWidget *parent, QString title) : QDialog(parent) {
        setWindowTitle(title);
        setMinimumWidth(340);
        layout = new QFormLayout(this);
        layout->setContentsMargins(22, 20, 22, 20);
        layout->setSpacing(13);
    }
    void number(QString key, QString label, double val, double min = -100000, double max = 100000,
                QString unit = " mm") {
        auto *s = new QDoubleSpinBox;
        s->setRange(min, max);
        s->setDecimals(3);
        s->setValue(val);
        s->setSuffix(unit);
        s->setKeyboardTracking(false);
        nums[key] = s;
        layout->addRow(label, s);
    }
    void choice(QString key, QString label, const QList<QPair<QString, QString>> &items,
                QString current = {}) {
        auto *c = new QComboBox;
        for (auto &i : items)
            c->addItem(i.first, i.second);
        int index = c->findData(current);
        if (index >= 0)
            c->setCurrentIndex(index);
        combos[key] = c;
        layout->addRow(label, c);
    }
    void note(QString text) {
        auto *l = new QLabel(text);
        l->setWordWrap(true);
        l->setStyleSheet("color:#64748b;font-size:12px;");
        layout->addRow(l);
    }
    QJsonObject values() const {
        QJsonObject p;
        for (auto it = nums.begin(); it != nums.end(); ++it)
            p[it.key()] = it.value()->value();
        for (auto it = combos.begin(); it != combos.end(); ++it)
            p[it.key()] = it.value()->currentData().toString();
        return p;
    }
    bool acceptForm() {
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addRow(buttons);
        if (parentWidget()) {
            auto r = parentWidget()->geometry();
            move(r.right() - width() - 35, r.top() + 180);
        }
        return exec() == Accepted;
    }
};
QList<QPair<QString, QString>> planes = {{"XY — Top", "XY"}, {"XZ — Front", "XZ"}, {"YZ — Right", "YZ"}};
QString withExtension(QString path, QString ext) {
    if (!path.endsWith("." + ext, Qt::CaseInsensitive))
        path += "." + ext;
    return path;
}
} // namespace
QAction *Window::command(QString key, QString label, QString shortcut, std::function<void()> fn) {
    auto *a = new QAction(icon(key), label, this);
    a->setObjectName(key);
    if (!shortcut.isEmpty())
        a->setShortcut(QKeySequence(shortcut));
    connect(a, &QAction::triggered, this, [this, fn] { run(fn); });
    addAction(a);
    commands[key] = a;
    return a;
}
Window::Window() {
    setObjectName("MecaCAD");
    resize(1440, 920);
    setMinimumSize(1100, 720);
    setDockOptions(QMainWindow::AnimatedDocks);
    setWindowTitle("MecaCAD");
    qApp->setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f3f5f7"));
    palette.setColor(QPalette::WindowText, QColor("#243548"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, QColor("#243548"));
    palette.setColor(QPalette::Button, QColor("#edf1f5"));
    palette.setColor(QPalette::ButtonText, QColor("#243548"));
    palette.setColor(QPalette::Highlight, QColor("#258bb8"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(palette);
    qApp->setStyleSheet(R"(
        QMainWindow,QDialog{background:#f3f5f7;color:#243548;} QWidget{font-family:'Helvetica Neue','Segoe UI';font-size:13px;}
        QMenuBar,QMenu,QToolBar{background:#fafbfc;color:#243548;} QMenu{border:1px solid #c8d1dc;padding:6px;} QMenu::item{padding:8px 32px 8px 14px;} QMenu::item:selected{background:#dcecf6;} QMenu::item:disabled{color:#a3aeb9;}
        QToolBar{border:0;spacing:8px;padding:5px;} QToolButton{border:1px solid transparent;border-radius:5px;padding:5px;color:#293f50;} QToolButton:hover{background:#e2edf4;border-color:#c5dbe9;} QToolButton:pressed{background:#c7e2f1;} QToolButton::menu-indicator{width:0;}
        QTabBar::tab{padding:11px 20px;color:#68798b;background:transparent;border-bottom:3px solid transparent;font-size:11px;font-weight:600;} QTabBar::tab:selected{color:#217ca9;border-bottom:3px solid #2589b7;} QTabBar::tab:disabled{color:#a1adbb;}
        QDockWidget{font-weight:600;} QDockWidget::title{background:#e5eaf0;padding:9px;color:#42546b;} QTreeWidget{background:#f4f6f9;border:0;outline:0;color:#34475c;padding:7px;font-size:12px;} QTreeWidget::item{height:29px;} QTreeWidget::item:selected{background:#d3e9f6;color:#173c58;border-radius:4px;}
        QLineEdit,QDoubleSpinBox,QComboBox{background:white;border:1px solid #cbd5df;border-radius:4px;padding:7px;color:#263a50;min-height:17px;} QLineEdit:focus,QDoubleSpinBox:focus,QComboBox:focus{border-color:#268bb8;} QPushButton{background:#e6edf3;border:1px solid #cbd5df;border-radius:4px;padding:8px 13px;color:#2c4359;} QPushButton:hover{background:#d6e7f2;} QPushButton:default{background:#2489b6;color:white;border-color:#2489b6;}
        QStatusBar{background:#edf1f5;color:#5d7186;border-top:1px solid #d2dbe4;font-size:11px;} QListWidget{background:#f0f3f7;border:0;outline:0;} QListWidget::item{border:1px solid #ced9e4;border-radius:5px;background:#fafcfe;margin:5px;padding:4px;color:#536c81;} QListWidget::item:selected{background:#d5ebf7;border-color:#318fb9;color:#174c6b;} QScrollBar:horizontal{height:9px;background:#e2e8f0;} QScrollBar::handle:horizontal{background:#aabac9;border-radius:4px;min-width:30px;}
    )");
    auto *file = menuBar()->addMenu("File");
    auto *edit = menuBar()->addMenu("Edit");
    auto *viewMenu = menuBar()->addMenu("View");
    auto *help = menuBar()->addMenu("Help");
    file->addAction(command("new", "New Design", "Ctrl+N", [this] {
        if (canLeave()) {
            model.clear();
            selected.clear();
            finishSketch();
            refresh();
        }
    }));
    file->addAction(command("open", "Open…", "Ctrl+O", [this] { open(); }));
    file->addAction(command("save", "Save", "Ctrl+S", [this] { save(); }));
    file->addAction(command("saveas", "Save As…", "Ctrl+Shift+S", [this] { save(true); }));
    file->addSeparator();
    file->addAction(command("import", "Import STEP…", "", [this] {
        auto path = QFileDialog::getOpenFileName(this, "Import STEP", {}, "STEP (*.step *.stp)");
        if (!path.isEmpty()) {
            selected = model.importStep(path);
            refresh(true);
        }
    }));
    auto *exports = file->addMenu("Export");
    for (auto fmt : {"STEP", "STL", "DXF"})
        exports->addAction(
            command(QString("export") + fmt, QString(fmt) + "…", "", [this, fmt] { exportFile(fmt); }));
    file->addSeparator();
    file->addAction("Open example — mounting bracket", this, [this] {
        run([this] {
            if (canLeave())
                demo();
        });
    });
    edit->addAction(command("undo", "Undo", "Ctrl+Z", [this] {
        model.undo();
        refresh();
    }));
    edit->addAction(command("redo", "Redo", "Ctrl+Shift+Z", [this] {
        model.redo();
        refresh();
    }));
    edit->addAction(command("delete", "Delete", "Backspace", [this] {
        if (!selected.isEmpty()) {
            model.remove(selected);
            selected.clear();
            refresh();
        }
    }));
    auto *deleteKey = new QAction(this);
    deleteKey->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(deleteKey, &QAction::triggered, commands["delete"], &QAction::trigger);
    addAction(deleteKey);
    command("sketch", "Create Sketch", "", [this] { startSketch(); });
    command("finish", "Finish Sketch", "", [this] { finishSketch(); });
    command("rectangle", "2-Point Rectangle", "R", [this] { sketchTool("rectangle"); });
    command("circle", "Center Diameter Circle", "C", [this] { sketchTool("circle"); });
    command("polyline", "Line", "L", [this] { sketchTool("polyline"); });
    command("arc", "3-Point Arc", "", [this] { sketchTool("arc"); });
    command("dimension", "Sketch Dimension", "D", [this] {
        if (selected.isEmpty())
            exactSketch();
        else {
            buildProperties();
            properties->show();
            if (!fields.isEmpty()) {
                fields.first()->setFocus();
                fields.first()->selectAll();
            }
        }
    });
    command("exact", "Create by dimensions…", "", [this] { exactSketch(); });
    command("extrude", "Extrude", "E", [this] { extrude(); });
    command("revolve", "Revolve", "", [this] { extrude(true); });
    command("hole", "Hole", "H", [this] { hole(); });
    for (auto type : {"box", "cylinder", "sphere"})
        command(type, displayType(type), "", [this, type] { primitive(type); });
    command("transform", "Move / Copy", "M", [this] { transform(false); });
    command("copy", "Create Copy", "", [this] { transform(true); });
    command("boolean", "Combine", "", [this] { booleanOp("join"); });
    command("cut", "Combine — Cut", "", [this] { booleanOp("cut"); });
    command("common", "Combine — Intersect", "", [this] { booleanOp("common"); });
    command("fillet", "Fillet", "", [this] { fillet(); });
    command("measure", "Measure", "I", [this] { measure(); });
    command("search", "Design Shortcuts", "S", [this] { search(); });
    command("fit", "Fit", "F", [this] { canvas->fit(); });
    viewMenu->addAction(commands["fit"]);
    for (auto name : {"iso", "top", "front", "right"})
        viewMenu->addAction(QString(name).toUpper(), this, [this, name] { canvas->view(name); });
    auto *theme = viewMenu->addAction("Light canvas");
    theme->setCheckable(true);
    connect(theme, &QAction::toggled, this, [this](bool v) {
        canvas->light = v;
        canvas->update();
    });
    help->addAction("Quick start", this, [this] {
        QMessageBox::information(
            this, "MecaCAD — Quick start",
            "1. Create Sketch → choose XY, XZ or YZ.\n2. Draw a rectangle (R), circle (C) or line (L).\n3. "
            "Select the sketch in the Browser; edit dimensions on the right.\n4. Finish Sketch → Extrude "
            "(E).\n5. Save and use File → Export.\n\nMiddle drag: pan · Shift+middle drag: orbit · Scroll: zoom · F: "
            "fit\nLine: Enter finishes; Shift+Enter closes the profile.\n\nVersion 0.1: one profile per "
            "sketch; dimensions drive rectangles/circles. General sketch constraints, face attachment, "
            "assemblies and simulation are not yet available.");
    });
    auto *bar = addToolBar("Application");
    bar->setMovable(false);
    bar->setIconSize({20, 20});
    auto *brand = new QLabel("  MECA<span style='color:#238fb7'>CAD</span>  ");
    brand->setStyleSheet("font-size:17px;font-weight:700;letter-spacing:1px;");
    bar->addWidget(brand);
    for (auto key : {"new", "open", "save", "undo", "redo"})
        bar->addAction(commands[key]);
    bar->addSeparator();
    documentTitle = new QLabel("Untitled");
    documentTitle->setStyleSheet("padding:0 22px;color:#4b6177;");
    bar->addWidget(documentTitle);
    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);
    auto *badge = new QLabel("LOCAL  ·  v0.1   ");
    badge->setStyleSheet("font-size:10px;color:#72899e;font-weight:600;");
    bar->addWidget(badge);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *tabrow = new QWidget;
    auto *tablayout = new QHBoxLayout(tabrow);
    tablayout->setContentsMargins(12, 0, 12, 0);
    auto *workspace = new QToolButton;
    workspace->setText("DESIGN  ▾");
    workspace->setStyleSheet("font-size:12px;font-weight:600;padding:13px 15px;");
    auto *workspaces = new QMenu(workspace);
    workspaces->addAction("Design");
    for (auto name : {"Render", "Animation", "Simulation", "Manufacture", "Drawing", "Electronics"}) {
        auto *a = workspaces->addAction(QString(name) + " — planned");
        a->setEnabled(false);
    }
    workspace->setMenu(workspaces);
    workspace->setPopupMode(QToolButton::InstantPopup);
    tablayout->addWidget(workspace);
    tabs = new QTabBar;
    for (auto title : {"SOLID", "SURFACE", "MESH", "SHEET METAL", "TOOLS"})
        tabs->addTab(title);
    for (int i = 1; i < 5; ++i)
        tabs->setTabEnabled(i, false);
    tablayout->addWidget(tabs);
    tablayout->addStretch();
    layout->addWidget(tabrow);
    ribbon = new QWidget;
    ribbon->setObjectName("ribbon");
    ribbon->setStyleSheet("#ribbon{background:#fafbfc;border-bottom:1px solid #cdd8e3;}");
    layout->addWidget(ribbon);
    layout->removeWidget(tabrow);
    layout->removeWidget(ribbon);
    auto *header = new QWidget;
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    headerLayout->addWidget(tabrow);
    headerLayout->addWidget(ribbon);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addToolBarBreak();
    auto *headerBar = addToolBar("Design commands");
    headerBar->setObjectName("designToolbar");
    headerBar->setMovable(false);
    headerBar->setStyleSheet("QToolBar{padding:0;border:0;}");
    headerBar->addWidget(header);
    canvas = new Viewport(&model);
    layout->addWidget(canvas, 1);
    auto *navigation = new QWidget;
    navigation->setStyleSheet("background:#e7edf3;");
    auto *nav = new QHBoxLayout(navigation);
    nav->setContentsMargins(8, 2, 8, 2);
    nav->addStretch();
    for (auto name : {"iso", "top", "front", "right"}) {
        auto *b = new QToolButton;
        b->setText(QString(name).toUpper());
        connect(b, &QToolButton::clicked, this, [this, name] { canvas->view(name); });
        nav->addWidget(b);
    }
    auto *fit = new QToolButton;
    fit->setDefaultAction(commands["fit"]);
    nav->addWidget(fit);
    nav->addStretch();
    auto *snap = new QCheckBox("Snap 1 mm");
    snap->setChecked(true);
    connect(snap, &QCheckBox::toggled, this, [this](bool v) { canvas->snap = v; });
    nav->addWidget(snap);
    layout->addWidget(navigation);
    auto *history = new QWidget;
    auto *historyLayout = new QHBoxLayout(history);
    historyLayout->setContentsMargins(14, 4, 12, 4);
    auto *historyTitle = new QLabel("HISTORY\nParametric");
    historyTitle->setStyleSheet("color:#7b8ea1;font-size:10px;");
    historyLayout->addWidget(historyTitle);
    timeline = new QListWidget;
    timeline->setFlow(QListView::LeftToRight);
    timeline->setWrapping(false);
    timeline->setFixedHeight(70);
    timeline->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    timeline->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    historyLayout->addWidget(timeline, 1);
    layout->addWidget(history);
    setCentralWidget(central);
    auto *browser = new QDockWidget("BROWSER", this);
    browser->setObjectName("browserDock");
    browser->setFeatures(QDockWidget::NoDockWidgetFeatures);
    browser->setMinimumWidth(215);
    browser->setMaximumWidth(380);
    tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    browser->setWidget(tree);
    addDockWidget(Qt::LeftDockWidgetArea, browser);
    properties = new QDockWidget("PROPERTIES", this);
    properties->setObjectName("propertiesDock");
    properties->setMinimumWidth(250);
    properties->setMaximumWidth(350);
    properties->setFeatures(QDockWidget::DockWidgetClosable);
    propertyBody = new QWidget;
    propertyForm = new QFormLayout(propertyBody);
    propertyForm->setContentsMargins(14, 18, 14, 18);
    propertyForm->setSpacing(12);
    auto *propertyScroll = new QScrollArea;
    propertyScroll->setWidgetResizable(true);
    propertyScroll->setFrameShape(QFrame::NoFrame);
    propertyScroll->setWidget(propertyBody);
    properties->setWidget(propertyScroll);
    addDockWidget(Qt::RightDockWidgetArea, properties);
    properties->hide();
    status = new QLabel("Ready");
    statusBar()->addWidget(status, 1);
    statusBar()->addPermanentWidget(new QLabel("  mm   |   Offline   "));
    canvas->onSelect = [this](QString id) { select(id); };
    canvas->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(canvas, &QWidget::customContextMenuRequested, this, [this](QPoint position) {
        QMenu menu;
        if (canvas->sketchMode) {
            for (auto key : {"polyline", "rectangle", "circle", "dimension", "finish"})
                menu.addAction(commands[key]);
        } else {
            for (auto key : {"sketch", "extrude", "hole", "transform", "measure"})
                menu.addAction(commands[key]);
        }
        menu.addSeparator();
        menu.addAction(commands["fit"]);
        menu.addAction(commands["undo"]);
        menu.exec(canvas->mapToGlobal(position));
    });
    canvas->onHint = [this](QString s) { status->setText(s); };
    canvas->onProfile = [this](QJsonObject p) {
        run([&] {
            selected = model.add("sketch", p, "Sketch " + QString::number(model.features.size() + 1));
            refresh();
            status->setText("Sketch criado. Ajuste as dimensões à direita ou continue desenhando.");
        });
    };
    connect(tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (!refreshing && tree->currentItem())
            select(tree->currentItem()->data(0, Qt::UserRole).toString());
    });
    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int) {
        if (refreshing)
            return;
        QString id = item->data(0, Qt::UserRole).toString();
        if (!id.isEmpty())
            run([&] {
                model.toggle(id);
                refresh();
            });
    });
    connect(tree, &QTreeWidget::customContextMenuRequested, this, [this](QPoint pos) {
        auto *item = tree->itemAt(pos);
        if (!item)
            return;
        select(item->data(0, Qt::UserRole).toString());
        if (selected.isEmpty())
            return;
        QMenu menu;
        menu.addAction("Edit Feature", this, [this] { properties->show(); });
        if (model.get(selected).type == "sketch") {
            menu.addAction("Edit Sketch", this, [this] {
                const auto &p = model.get(selected).p;
                canvas->plane = p["plane"].toString("XY");
                canvas->planeOffset = p["offset"].toDouble();
                canvas->sketchMode = true;
                canvas->setTool({});
                canvas->view("top");
                buildRibbon();
                properties->show();
            });
            menu.addAction(commands["extrude"]);
            menu.addAction(commands["exportDXF"]);
        } else {
            menu.addAction(commands["transform"]);
            menu.addAction(commands["copy"]);
            menu.addAction(commands["exportSTEP"]);
        }
        menu.addSeparator();
        menu.addAction(commands["delete"]);
        menu.exec(tree->viewport()->mapToGlobal(pos));
    });
    connect(timeline, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { select(item->data(Qt::UserRole).toString()); });
    connect(timeline, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { properties->show(); });
    recoveryPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/recovery.mcad";
    QDir().mkpath(QFileInfo(recoveryPath).absolutePath());
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Window::autosave);
    timer->start(30000);
    buildRibbon();
    refresh();
    QTimer::singleShot(150, this, [this] {
        if (QFile::exists(recoveryPath) && model.features.empty()) {
            if (QMessageBox::question(
                    this, "Recover project",
                    "Foi encontrado um projeto de uma sessão interrompida. Deseja recuperá-lo?") ==
                QMessageBox::Yes)
                run([this] {
                    model.load(recoveryPath);
                    model.filePath.clear();
                    model.dirty = true;
                    refresh(true);
                });
        }
    });
}
void Window::run(const std::function<void()> &fn) {
    try {
        fn();
    } catch (const Standard_Failure &e) {
        QMessageBox::warning(this, "Geometry", QString::fromUtf8(e.GetMessageString()));
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "MecaCAD", QString::fromUtf8(e.what()));
    }
}
void Window::buildRibbon() {
    if (auto *old = ribbon->layout()) {
        while (auto *item = old->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        delete old;
    }
    auto *row = new QHBoxLayout(ribbon);
    row->setContentsMargins(12, 7, 12, 5);
    row->setSpacing(10);
    auto group = [&](QString title, QStringList visible, QStringList menuItems, QStringList planned = {}) {
        auto *container = new QWidget;
        auto *vertical = new QVBoxLayout(container);
        vertical->setContentsMargins(3, 0, 3, 0);
        vertical->setSpacing(0);
        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(0);
        for (auto key : visible) {
            auto *b = new QToolButton;
            b->setDefaultAction(commands[key]);
            b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            b->setIconSize({32, 32});
            b->setMinimumSize(std::max(65, b->fontMetrics().horizontalAdvance(commands[key]->text()) + 18),
                              57);
            buttons->addWidget(b);
        }
        vertical->addLayout(buttons);
        auto *drop = new QToolButton;
        drop->setText(title + "  ▾");
        drop->setStyleSheet("font-size:10px;padding:3px;");
        auto *menu = new QMenu(drop);
        for (auto key : menuItems)
            menu->addAction(commands[key]);
        if (!planned.isEmpty())
            menu->addSeparator();
        for (auto text : planned) {
            auto *a = menu->addAction(text + "  (planned)");
            a->setEnabled(false);
        }
        drop->setMenu(menu);
        drop->setPopupMode(QToolButton::InstantPopup);
        vertical->addWidget(drop, 0, Qt::AlignHCenter);
        row->addWidget(container);
        auto *line = new QFrame;
        line->setFrameShape(QFrame::VLine);
        line->setStyleSheet("color:#d5dfe7;");
        row->addWidget(line);
    };
    if (canvas->sketchMode) {
        tabs->setTabText(0, "SKETCH");
        group("CREATE", {"polyline", "rectangle", "circle"},
              {"polyline", "rectangle", "circle", "arc", "exact"}, {"Spline", "Polygon", "Slot", "Text"});
        group("MODIFY", {"dimension"}, {"dimension"}, {"Trim", "Extend", "Offset", "Mirror"});
        group("CONSTRAINTS", {}, {},
              {"Coincident", "Horizontal / Vertical", "Parallel", "Perpendicular", "Tangent", "Equal"});
        row->addStretch();
        auto *finish = new QToolButton;
        finish->setDefaultAction(commands["finish"]);
        finish->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        finish->setIconSize({32, 32});
        finish->setStyleSheet("background:#dceee0;border:1px solid #b1d6ba;padding:10px;color:#2f6641;");
        row->addWidget(finish);
    } else {
        tabs->setTabText(0, "SOLID");
        group("CREATE", {"sketch", "extrude", "revolve"},
              {"sketch", "extrude", "revolve", "hole", "box", "cylinder", "sphere"},
              {"Sweep", "Loft", "Pattern", "Mirror"});
        group("MODIFY", {"fillet", "transform"}, {"fillet", "transform", "copy", "boolean", "cut", "common"},
              {"Chamfer", "Shell", "Draft", "Scale", "Split Body"});
        group("ASSEMBLE", {}, {}, {"New Component", "Joint", "As-Built Joint"});
        group("CONSTRUCT", {}, {}, {"Offset Plane", "Midplane", "Axis"});
        group("INSPECT", {"measure"}, {"measure"}, {"Section Analysis", "Interference"});
        group("INSERT", {}, {"import"});
        group("SELECT", {}, {"search"});
        row->addStretch();
    }
}
void Window::refresh(bool fit) {
    refreshing = true;
    tree->clear();
    timeline->clear();
    bool exists = false;
    for (auto &f : model.features)
        if (f.id == selected)
            exists = true;
    if (!exists)
        selected.clear();
    QString title = model.filePath.isEmpty() ? "Untitled" : QFileInfo(model.filePath).completeBaseName();
    documentTitle->setText(title + (model.dirty ? " *" : ""));
    setWindowTitle(title + (model.dirty ? " *" : "") + " — MecaCAD");
    auto *root = new QTreeWidgetItem(tree, {"◉  " + title});
    auto *settings = new QTreeWidgetItem(root, {"Document Settings"});
    new QTreeWidgetItem(settings, {"Units: mm"});
    auto *views = new QTreeWidgetItem(root, {"Named Views"});
    for (auto s : {"Top", "Front", "Right", "Home"})
        new QTreeWidgetItem(views, {s});
    auto *origin = new QTreeWidgetItem(root, {"Origin"});
    for (auto s : {"XY", "XZ", "YZ"})
        new QTreeWidgetItem(origin, {s});
    auto *bodies = new QTreeWidgetItem(root, {"Bodies"});
    auto *sketches = new QTreeWidgetItem(root, {"Sketches"});
    auto *history = new QTreeWidgetItem(root, {"Features"});
    for (auto &f : model.features) {
        bool consumed = model.consumed(f.id);
        auto *parent = f.type == "sketch" ? sketches : (consumed ? history : bodies);
        auto *item = new QTreeWidgetItem(parent, {f.name});
        item->setIcon(0, icon(f.type));
        item->setData(0, Qt::UserRole, f.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, f.visible ? Qt::Checked : Qt::Unchecked);
        if (consumed)
            item->setForeground(0, QColor("#8d9aaa"));
        if (f.id == selected)
            tree->setCurrentItem(item);
        auto *step = new QListWidgetItem(icon(f.type), QString::number(timeline->count() + 1), timeline);
        step->setData(Qt::UserRole, f.id);
        step->setToolTip(f.name + " — " + displayType(f.type) + "\nDouble-click to edit");
        step->setSizeHint({66, 45});
        if (f.id == selected)
            timeline->setCurrentItem(step);
    }
    root->setExpanded(true);
    bodies->setExpanded(true);
    sketches->setExpanded(true);
    refreshing = false;
    canvas->selected = selected;
    canvas->refresh();
    if (fit)
        canvas->fit();
    buildProperties();
    status->setText(QString("%1 bodies  ·  %2 operations  ·  millimeters")
                        .arg(model.bodies().size())
                        .arg(model.features.size()));
}
void Window::select(const QString &id) {
    selected = id;
    canvas->selected = id;
    canvas->update();
    refreshing = true;
    QTreeWidgetItemIterator it(tree);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id && !id.isEmpty())
            tree->setCurrentItem(*it);
        ++it;
    }
    for (int i = 0; i < timeline->count(); ++i)
        timeline->item(i)->setSelected(timeline->item(i)->data(Qt::UserRole) == id);
    refreshing = false;
    buildProperties();
}
void Window::buildProperties() {
    fields.clear();
    while (propertyForm->rowCount())
        propertyForm->removeRow(0);
    if (selected.isEmpty()) {
        properties->hide();
        return;
    }
    const auto &f = model.get(selected);
    properties->setWindowTitle(f.type == "sketch" ? "SKETCH DIMENSIONS" : "EDIT FEATURE");
    featureName = new QLineEdit(f.name);
    propertyForm->addRow("Name", featureName);
    static QMap<QString, QString> labels = {{"x", "X"},         {"y", "Y"},
                                            {"z", "Z"},         {"w", "Width"},
                                            {"h", "Height"},    {"d", "Distance"},
                                            {"r", "Radius"},    {"offset", "Plane offset"},
                                            {"angle", "Angle"}, {"axis", "Axis position"},
                                            {"u", "Center U"},  {"v", "Center V"},
                                            {"x1", "Start X"},  {"y1", "Start Y"},
                                            {"xm", "Mid X"},    {"ym", "Mid Y"},
                                            {"x2", "End X"},    {"y2", "End Y"}};
    for (auto key : {"x", "y", "z", "w", "h", "r", "d", "angle", "axis", "u", "v", "offset", "x1", "y1", "xm",
                     "ym", "x2", "y2"})
        if (f.p[key].isDouble()) {
            auto *spin = new QDoubleSpinBox;
            spin->setRange(-100000, 100000);
            spin->setDecimals(3);
            spin->setSuffix(QString(key) == "angle" ? " °" : " mm");
            spin->setValue(f.p[key].toDouble());
            spin->setKeyboardTracking(false);
            fields[key] = spin;
            propertyForm->addRow(labels.value(key, key), spin);
        }
    if (f.type == "sketch") {
        auto *note =
            new QLabel("Plane: " + f.p["plane"].toString() + "\nProfile: " + f.p["profile"].toString() +
                       "\nDimensions drive the profile.");
        note->setWordWrap(true);
        note->setStyleSheet("color:#74869a;font-size:11px;");
        propertyForm->addRow(note);
        if (f.p["profile"] == "polyline") {
            auto *edit = new QPushButton("Edit points…");
            propertyForm->addRow(edit);
            connect(edit, &QPushButton::clicked, this, [this] {
                run([this] {
                    auto &feature = model.get(selected);
                    QStringList lines;
                    for (auto v : feature.p["points"].toArray()) {
                        auto a = v.toArray();
                        lines << QString::number(a[0].toDouble()) + ", " + QString::number(a[1].toDouble());
                    }
                    bool ok;
                    auto text = QInputDialog::getMultiLineText(
                        this, "Sketch points", "One point per line: X, Y (mm)", lines.join('\n'), &ok);
                    if (!ok)
                        return;
                    QJsonArray points;
                    for (auto line : text.split('\n', Qt::SkipEmptyParts)) {
                        auto pair = line.split(',');
                        bool xok = false, yok = false;
                        double x = pair.value(0).trimmed().toDouble(&xok),
                               y = pair.value(1).trimmed().toDouble(&yok);
                        if (pair.size() != 2 || !xok || !yok)
                            throw std::runtime_error("Use X, Y em cada linha.");
                        points.append(QJsonArray{x, y});
                    }
                    auto p = feature.p;
                    p["points"] = points;
                    model.edit(selected, p, feature.name);
                    refresh();
                });
            });
        }
    } else {
        double v = Model::volume(f.shape);
        auto *l = new QLabel(QString("Volume: %1 cm³").arg(v / 1000, 0, 'f', 3));
        l->setStyleSheet("color:#71859a;font-size:11px;");
        propertyForm->addRow(l);
    }
    auto *apply = new QPushButton("Apply");
    apply->setDefault(true);
    propertyForm->addRow(apply);
    connect(apply, &QPushButton::clicked, this, [this] { run([this] { applyProperties(); }); });
    if (f.type == "sketch") {
        auto *extrude = new QPushButton("Extrude  ·  E");
        propertyForm->addRow(extrude);
        connect(extrude, &QPushButton::clicked, commands["extrude"], &QAction::trigger);
    }
    properties->show();
}
void Window::applyProperties() {
    auto p = model.get(selected).p;
    for (auto it = fields.begin(); it != fields.end(); ++it)
        p[it.key()] = it.value()->value();
    model.edit(selected, p, featureName->text());
    refresh();
}
void Window::primitive(const QString &type) {
    Form f(this, displayType(type));
    f.number("x", "X", 0);
    f.number("y", "Y", 0);
    f.number("z", "Z", 0);
    if (type == "box") {
        f.number("w", "Width", 40, .001);
        f.number("h", "Length", 30, .001);
    } else
        f.number("r", "Radius", 10, .001);
    if (type != "sphere")
        f.number("d", "Height", 10, .001);
    if (f.acceptForm()) {
        selected =
            model.add(type, f.values(), displayType(type) + " " + QString::number(model.features.size() + 1));
        refresh(true);
    }
}
void Window::startSketch() {
    Form f(this, "Create Sketch");
    f.choice("plane", "Plane", planes, canvas->plane);
    f.number("offset", "Offset", 0);
    f.note("Escolha um plano de origem. Cada perfil criado será um sketch independente.");
    if (!f.acceptForm())
        return;
    auto p = f.values();
    canvas->plane = p["plane"].toString();
    canvas->planeOffset = p["offset"].toDouble();
    canvas->sketchMode = true;
    canvas->view(canvas->plane == "XY" ? "top" : canvas->plane == "XZ" ? "front" : "right");
    canvas->setTool("rectangle");
    buildRibbon();
    status->setText("Sketch: escolha uma ferramenta e clique na área de desenho.");
}
void Window::finishSketch() {
    canvas->setTool({});
    canvas->sketchMode = false;
    buildRibbon();
    canvas->view("iso");
    canvas->update();
}
void Window::sketchTool(const QString &type) {
    if (!canvas->sketchMode)
        startSketch();
    if (canvas->sketchMode) {
        canvas->setTool(type);
        status->setText(type == "arc" ? "Arco: clique início, ponto intermediário e fim."
                        : type == "polyline"
                            ? "Linha: clique nos vértices; feche clicando no início ou Shift+Enter."
                            : "Clique em dois pontos. Depois ajuste as dimensões no painel.");
    }
}
void Window::exactSketch() {
    Form f(this, "Sketch — dimensions");
    f.choice("profile", "Profile", {{"Rectangle", "rectangle"}, {"Circle", "circle"}});
    f.choice("plane", "Plane", planes, canvas->plane);
    f.number("x", "X", 0);
    f.number("y", "Y", 0);
    f.number("offset", "Plane offset", canvas->planeOffset);
    f.number("w", "Rectangle width", 40, .001);
    f.number("h", "Rectangle height", 30, .001);
    f.number("r", "Circle radius", 10, .001);
    if (f.acceptForm()) {
        auto p = f.values();
        if (p["profile"] == "circle") {
            p.remove("w");
            p.remove("h");
        } else
            p.remove("r");
        selected = model.add("sketch", p, "Sketch " + QString::number(model.features.size() + 1));
        refresh(true);
    }
}
void Window::extrude(bool revolve) {
    QList<QPair<QString, QString>> sketches, bodies = {{"New Body", ""}};
    for (auto &f : model.features)
        if (f.type == "sketch")
            sketches.append({f.name, f.id});
    for (auto i : model.bodies())
        bodies.append({model.features[i].name, model.features[i].id});
    if (sketches.empty())
        throw std::runtime_error("Crie um sketch fechado antes de extrudar.");
    Form f(this, revolve ? "Revolve" : "Extrude");
    f.choice("source", "Profile", sketches, selected);
    if (revolve) {
        f.number("angle", "Angle", 360, .01, 360, " °");
        f.number("axis", "Vertical axis U", 0);
    } else
        f.number("d", "Distance", 10);
    f.choice("target", "Body", bodies);
    f.choice("mode", "Operation", {{"Join / New Body", "join"}, {"Cut", "cut"}});
    f.note(revolve ? "Revolução em torno do eixo vertical local do sketch."
                   : "Distância positiva segue a normal do plano. Valores negativos invertem a direção.");
    if (f.acceptForm()) {
        auto p = f.values();
        if (p["mode"] == "cut" && p["target"].toString().isEmpty())
            throw std::runtime_error("Escolha um corpo para o corte.");
        selected = model.add(revolve ? "revolve" : "extrude", p, revolve ? "Revolve" : "Extrude");
        finishSketch();
        refresh(true);
    }
}
void Window::booleanOp(const QString &mode) {
    QList<QPair<QString, QString>> bodies;
    for (auto i : model.bodies())
        bodies.append({model.features[i].name, model.features[i].id});
    if (bodies.size() < 2)
        throw std::runtime_error("Crie dois corpos para combinar.");
    Form f(this, "Combine");
    f.choice("target", "Target Body", bodies, selected);
    f.choice("tool", "Tool Body", bodies, bodies.last().second);
    f.choice("mode", "Operation", {{"Join", "join"}, {"Cut", "cut"}, {"Intersect", "common"}}, mode);
    if (f.acceptForm()) {
        auto p = f.values();
        if (p["target"] == p["tool"])
            throw std::runtime_error("Selecione dois corpos diferentes.");
        selected = model.add("boolean", p, "Combine");
        refresh();
    }
}
void Window::transform(bool copy) {
    QList<QPair<QString, QString>> bodies;
    for (auto i : model.bodies())
        bodies.append({model.features[i].name, model.features[i].id});
    if (bodies.empty())
        throw std::runtime_error("Crie um corpo primeiro.");
    Form f(this, copy ? "Create Copy" : "Move / Copy");
    f.choice("source", "Body", bodies, selected);
    f.number("x", "X distance", copy ? 50 : 0);
    f.number("y", "Y distance", 0);
    f.number("z", "Z distance", 0);
    f.choice("axis", "Rotation axis", {{"X", "X"}, {"Y", "Y"}, {"Z", "Z"}}, "Z");
    f.number("angle", "Rotation", 0, -36000, 36000, " °");
    f.note("Rotação em torno da origem global, seguida da translação.");
    if (f.acceptForm()) {
        selected = model.add(copy ? "copy" : "transform", f.values(), copy ? "Copy" : "Move");
        refresh(true);
    }
}
void Window::hole() {
    QList<QPair<QString, QString>> bodies;
    for (auto i : model.bodies())
        bodies.append({model.features[i].name, model.features[i].id});
    if (bodies.empty())
        throw std::runtime_error("Crie um corpo primeiro.");
    Form f(this, "Hole");
    f.choice("target", "Body", bodies, selected);
    f.choice("plane", "Direction", planes, "XY");
    f.number("u", "Center U", 10);
    f.number("v", "Center V", 10);
    f.number("r", "Radius", 3, .001);
    f.number("offset", "Start offset", -1);
    f.number("d", "Depth", 100, .001);
    f.note("Posição em coordenadas do plano escolhido; o furo segue a normal positiva.");
    if (f.acceptForm()) {
        selected = model.add("hole", f.values(), "Hole");
        refresh();
    }
}
void Window::fillet() {
    if (selected.isEmpty() || model.get(selected).type == "sketch")
        throw std::runtime_error("Selecione um corpo.");
    Form f(this, "Fillet — all edges");
    f.number("r", "Radius", 1, .001);
    f.note("Aplica o raio em todas as arestas do corpo selecionado.");
    if (f.acceptForm()) {
        auto p = f.values();
        p["source"] = selected;
        selected = model.add("fillet", p, "Fillet");
        refresh();
    }
}
void Window::measure() {
    if (selected.isEmpty())
        throw std::runtime_error("Selecione um objeto para medir.");
    auto &f = model.get(selected);
    Bnd_Box b;
    BRepBndLib::AddOptimal(f.shape, b);
    double x, y, z, X, Y, Z;
    b.Get(x, y, z, X, Y, Z);
    QMessageBox::information(this, "Measure",
                             QString("%1\n\nBounding box\nX: %2 mm\nY: %3 mm\nZ: %4 mm\n\nVolume: %5 "
                                     "cm³\n\nOrigin min: (%6, %7, %8) mm")
                                 .arg(f.name)
                                 .arg(X - x, 0, 'f', 3)
                                 .arg(Y - y, 0, 'f', 3)
                                 .arg(Z - z, 0, 'f', 3)
                                 .arg(Model::volume(f.shape) / 1000, 0, 'f', 3)
                                 .arg(x, 0, 'f', 3)
                                 .arg(y, 0, 'f', 3)
                                 .arg(z, 0, 'f', 3));
}
void Window::exportFile(const QString &format) {
    QString id = selected;
    if (!id.isEmpty() && model.get(id).type == "sketch" && format != "DXF")
        id.clear();
    if (format == "DXF" && (id.isEmpty() || model.get(id).type != "sketch"))
        throw std::runtime_error("Selecione um sketch no Browser para exportar DXF.");
    auto extension = format.toLower();
    auto path = QFileDialog::getSaveFileName(this, "Export " + format, QString("part.") + extension,
                                             format + " (*." + extension + ")");
    if (path.isEmpty())
        return;
    path = withExtension(path, extension);
    if (format == "STEP")
        model.exportStep(path, id);
    else if (format == "STL")
        model.exportStl(path, id);
    else
        model.exportDxf(path, id);
    status->setText("Exportado: " + path);
}
void Window::save(bool as) {
    QString path = model.filePath;
    if (as || path.isEmpty())
        path = QFileDialog::getSaveFileName(this, "Save Design", path.isEmpty() ? "Untitled.mcad" : path,
                                            "MecaCAD (*.mcad)");
    if (path.isEmpty())
        return;
    model.save(withExtension(path, "mcad"));
    QFile::remove(recoveryPath);
    refresh();
}
bool Window::canLeave() {
    if (!model.dirty)
        return true;
    auto answer = QMessageBox::question(this, "Unsaved changes", "Salvar as alterações antes de continuar?",
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel)
        return false;
    if (answer == QMessageBox::Save) {
        save();
        return !model.dirty;
    }
    QFile::remove(recoveryPath);
    return true;
}
void Window::open() {
    if (!canLeave())
        return;
    auto path = QFileDialog::getOpenFileName(this, "Open Design", {}, "MecaCAD (*.mcad)");
    if (!path.isEmpty())
        openPath(path);
}
void Window::openPath(const QString &path) {
    model.load(path);
    selected.clear();
    finishSketch();
    refresh(true);
}
void Window::autosave() {
    if (!model.dirty)
        return;
    QSaveFile file(recoveryPath);
    if (!file.open(QIODevice::WriteOnly)) {
        status->setText("Não foi possível salvar a recuperação automática.");
        return;
    }
    auto data = QJsonDocument(model.json()).toJson();
    if (file.write(data) != data.size() || !file.commit())
        status->setText("Falha na recuperação automática: " + file.errorString());
}
void Window::closeEvent(QCloseEvent *e) {
    bool ok = false;
    run([&] { ok = canLeave(); });
    if (ok) {
        QFile::remove(recoveryPath);
        e->accept();
    } else
        e->ignore();
}
void Window::search() {
    QStringList names;
    QMap<QString, QAction *> actions;
    for (auto it = commands.begin(); it != commands.end(); ++it) {
        QString label = it.value()->text();
        if (!it.value()->shortcut().isEmpty())
            label += "    [" + it.value()->shortcut().toString(QKeySequence::NativeText) + "]";
        names << label;
        actions[label] = it.value();
    }
    names.sort();
    bool ok;
    auto text = QInputDialog::getItem(this, "Design Shortcuts", "Search command", names, 0, true, &ok);
    if (ok && actions.contains(text))
        actions[text]->trigger();
}
void Window::demo() {
    model.clear();
    QString s = model.add(
        "sketch", {{"profile", "rectangle"}, {"plane", "XY"}, {"x", 0}, {"y", 0}, {"w", 80}, {"h", 50}},
        "Base profile");
    QString b = model.add("extrude", {{"source", s}, {"d", 8}}, "Base · 8 mm");
    QString upright =
        model.add("box", {{"x", 0}, {"y", 42}, {"z", 8}, {"w", 80}, {"h", 8}, {"d", 42}}, "Vertical plate");
    b = model.add("boolean", {{"target", b}, {"tool", upright}, {"mode", "join"}}, "Bracket");
    for (double x : {12., 68.})
        b = model.add(
            "hole",
            {{"target", b}, {"plane", "XY"}, {"u", x}, {"v", 15}, {"offset", -1}, {"d", 10}, {"r", 3.2}},
            "Mounting hole Ø6.4");
    b = model.add(
        "hole", {{"target", b}, {"plane", "XZ"}, {"u", 40}, {"v", 30}, {"offset", -51}, {"d", 12}, {"r", 10}},
        "Motor opening Ø20");
    selected = b;
    finishSketch();
    refresh(true);
}
