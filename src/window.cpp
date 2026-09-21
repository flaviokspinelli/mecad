#include "window.h"
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <GProp_GProps.hxx>
#include <Bnd_Box.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <QSet>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
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
#include <QTableWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <Standard_Failure.hxx>

namespace {
QString displayType(const QString &s) {
    static QMap<QString, QString> names = {
        {"box", "Box"},       {"cylinder", "Cylinder"},    {"sphere", "Sphere"},
        {"sketch", "Sketch"}, {"extrude", "Extrude"},      {"revolve", "Revolve"},
        {"hole", "Hole"},     {"boolean", "Combine"},      {"transform", "Move"},
        {"copy", "Copy"},     {"import", "Imported body"}, {"fillet", "Fillet"},
        {"mesh", "STL mesh"}, {"chamfer", "Chamfer"}};
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
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink("#d2dce6"), blue("#72d1fa"), side("#3e9ccc"), top("#edf2f6"), muted("#9babbd");
    p.setPen(QPen(ink, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    auto line = [&](QPointF a, QPointF b) { p.drawLine(a, b); };
    auto cube = [&](double x, double y, double w, double h) {
        QPolygonF topFace{{x, y + 9}, {x + 9, y}, {x + w, y}, {x + w - 9, y + 9}};
        QPolygonF front{{x, y + 9}, {x + w - 9, y + 9}, {x + w - 9, y + h}, {x, y + h}};
        QPolygonF right{{x + w - 9, y + 9}, {x + w, y}, {x + w, y + h - 9}, {x + w - 9, y + h}};
        p.setPen(Qt::NoPen);
        p.setBrush(top);
        p.drawPolygon(topFace);
        p.setBrush(blue);
        p.drawPolygon(front);
        p.setBrush(side);
        p.drawPolygon(right);
        p.setPen(QPen(ink, 1));
    };
    if (kind == "eye" || kind == "hidden") {
        p.setBrush(ink);
        QPainterPath path;
        path.moveTo(6, 32);
        path.quadTo(32, 7, 58, 32);
        path.quadTo(32, 57, 6, 32);
        p.drawPath(path);
        p.setBrush(QColor("#394957"));
        p.drawEllipse(QPointF(32, 32), 9, 9);
        if (kind == "hidden") {
            p.setPen(QPen(QColor("#364353"), 9));
            line({8, 8}, {56, 56});
            p.setPen(QPen(muted, 4));
            line({8, 8}, {56, 56});
        }
    } else if (kind == "folder" || kind == "open") {
        p.setBrush(muted);
        p.drawPolygon(QPolygonF{{6, 18}, {26, 18}, {31, 24}, {57, 24}, {57, 52}, {6, 52}});
    } else if (kind == "new") {
        p.setBrush(muted);
        p.drawPolygon(QPolygonF{{16, 7}, {39, 7}, {50, 19}, {50, 57}, {16, 57}});
        p.setBrush(top);
        p.drawPolygon(QPolygonF{{39, 7}, {39, 19}, {50, 19}});
    } else if (kind == "save" || kind == "saveas") {
        p.setBrush(muted);
        p.drawRect(11, 8, 43, 47);
        p.setBrush(QColor("#364353"));
        p.drawRect(19, 8, 25, 18);
        p.drawRect(19, 36, 27, 19);
        p.setPen(QPen(ink, 3));
        line({38, 11}, {38, 22});
    } else if (kind == "undo" || kind == "redo") {
        if (kind == "redo") {
            p.translate(64, 0);
            p.scale(-1, 1);
        }
        p.setPen(QPen(muted, 5));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(51, 46);
        path.cubicTo(54, 17, 27, 17, 13, 24);
        p.drawPath(path);
        line({13, 24}, {23, 11});
        line({13, 24}, {28, 29});
    } else if (kind == "home") {
        p.setBrush(muted);
        p.drawPolygon(QPolygonF{{6, 31},
                                {32, 8},
                                {58, 31},
                                {50, 31},
                                {50, 55},
                                {37, 55},
                                {37, 40},
                                {27, 40},
                                {27, 55},
                                {14, 55},
                                {14, 31}});
    } else if (kind == "grid" || kind == "configure") {
        p.setBrush(muted);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                p.drawRect(9 + i * 16, 9 + j * 16, 11, 11);
    } else if (kind == "gear") {
        p.setPen(QPen(muted, 8));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(32, 32), 16, 16);
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(32, 32);
            p.rotate(i * 45);
            line({0, 18}, {0, 25});
            p.restore();
        }
    } else if (kind == "sketch" || kind == "exact") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(muted, 2, Qt::DashLine));
        p.drawRect(7, 8, 40, 43);
        p.setPen(QPen(top, 2));
        line({15, 42}, {38, 42});
        line({38, 42}, {38, 17});
        p.setPen(QPen(QColor("#58c89a"), 6));
        line({47, 36}, {47, 60});
        line({35, 48}, {59, 48});
    } else if (kind == "rectangle") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(blue, 3));
        p.drawRect(9, 16, 45, 32);
        for (auto q : {QPointF(9, 16), QPointF(54, 48)}) {
            p.setBrush(top);
            p.drawRect(QRectF(q - QPointF(3, 3), QSizeF(6, 6)));
        }
    } else if (kind == "polygon") {
        p.setPen(QPen(blue, 3));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(QPolygonF{{10, 20}, {32, 8}, {54, 20}, {54, 44}, {32, 56}, {10, 44}});
        p.setBrush(top);
        p.drawEllipse(QPointF(32, 32), 3, 3);
    } else if (kind == "polyline" || kind == "trim") {
        p.setPen(QPen(blue, 3));
        line({9, 49}, {25, 15});
        line({25, 15}, {55, 35});
        p.setBrush(top);
        for (auto q : {QPointF(9, 49), QPointF(25, 15), QPointF(55, 35)})
            p.drawRect(QRectF(q - QPointF(3, 3), QSizeF(6, 6)));
    } else if (kind == "circle" || kind == "sphere") {
        p.setBrush(kind == "sphere" ? blue : QColor(Qt::transparent));
        p.setPen(QPen(ink, 3));
        p.drawEllipse(9, 9, 46, 46);
        p.setPen(QPen(side, 2));
        p.drawEllipse(23, 9, 18, 46);
    } else if (kind == "arc") {
        p.setPen(QPen(blue, 3));
        p.setBrush(Qt::NoBrush);
        p.drawArc(8, 9, 47, 46, 15 * 16, 150 * 16);
    } else if (kind == "measure" || kind == "dimension") {
        p.setPen(QPen(QColor("#e6bd74"), 3));
        line({5, 40}, {58, 40});
        for (int x = 6; x < 59; x += 7)
            line({double(x), 40}, {double(x), double(x % 2 ? 30 : 25)});
        p.setPen(QPen(muted, 2));
        line({7, 18}, {7, 52});
        line({57, 18}, {57, 52});
    } else if(kind=="chamfer") {
        p.setPen(Qt::NoPen);p.setBrush(top);
        p.drawPolygon(QPolygonF{{8,53},{8,28},{28,9},{53,9},{53,53}});
        p.setPen(QPen(blue,5));p.drawLine(8,28,28,9);
    } else if (kind == "fillet") {
        p.setPen(Qt::NoPen);
        p.setBrush(top);
        QPainterPath path;
        path.moveTo(8, 53);
        path.lineTo(8, 28);
        path.quadTo(8, 9, 28, 9);
        path.lineTo(50, 9);
        path.lineTo(57, 16);
        path.lineTo(57, 53);
        path.closeSubpath();
        p.drawPath(path);
        p.setBrush(blue);
        QPainterPath edge;
        edge.moveTo(8, 28);
        edge.quadTo(8, 9, 28, 9);
        edge.lineTo(35, 16);
        edge.quadTo(16, 16, 16, 35);
        edge.closeSubpath();
        p.drawPath(edge);
    } else if (kind == "revolve") {
        p.setPen(QPen(top, 9));
        p.setBrush(Qt::NoBrush);
        p.drawArc(9, 12, 46, 30, 0, 180 * 16);
        p.setPen(QPen(blue, 14));
        p.drawArc(9, 20, 46, 30, 0, 180 * 16);
        p.setBrush(side);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF{{8, 32}, {22, 26}, {22, 48}, {8, 42}});
    } else if (kind == "cylinder") {
        p.setPen(Qt::NoPen);
        p.setBrush(blue);
        p.drawRect(12, 17, 40, 31);
        p.drawEllipse(12, 39, 40, 17);
        p.setBrush(top);
        p.drawEllipse(12, 9, 40, 17);
    } else if (kind == "transform" || kind == "pan") {
        p.setPen(QPen(top, 4));
        line({32, 5}, {32, 59});
        line({5, 32}, {59, 32});
        for (int a = 0; a < 4; ++a) {
            p.save();
            p.translate(32, 32);
            p.rotate(90 * a);
            line({0, -27}, {-7, -19});
            line({0, -27}, {7, -19});
            p.restore();
        }
    } else if (kind == "orbit") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ink, 3));
        p.drawEllipse(7, 21, 50, 23);
        p.drawEllipse(24, 6, 16, 51);
        line({51, 21}, {59, 22});
    } else if (kind == "fit" || kind == "search") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kind == "search" ? QColor("#62c895") : ink, 2, Qt::DashLine));
        p.drawRect(8, 8, 42, 41);
        if (kind == "search") {
            p.setBrush(top);
            p.setPen(Qt::NoPen);
            p.drawPolygon(QPolygonF{{30, 27}, {53, 42}, {43, 45}, {48, 58}, {41, 61}, {35, 47}, {28, 54}});
        } else {
            p.setPen(QPen(ink, 2));
            p.drawRect(19, 18, 22, 22);
        }
    } else if (kind == "zoom") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ink, 4));
        p.drawEllipse(8, 7, 33, 33);
        line({36, 37}, {56, 57});
    } else if (kind == "display") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ink, 3));
        p.drawRect(6, 9, 52, 35);
        line({32, 44}, {32, 55});
        line({19, 56}, {45, 56});
    } else if (kind == "plane") {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#f2b77c"));
        p.drawPolygon(QPolygonF{{30, 7}, {55, 22}, {35, 53}, {10, 38}});
        p.setBrush(QColor("#82d2bf"));
        p.drawPolygon(QPolygonF{{10, 20}, {34, 6}, {34, 52}, {10, 59}});
    } else if (kind == "joint") {
        p.setPen(QPen(ink, 6));
        line({14, 48}, {46, 16});
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(5, 35, 21, 21);
        p.drawEllipse(36, 6, 21, 21);
    } else if (kind == "component") {
        cube(5, 25, 29, 32);
        cube(29, 7, 29, 32);
    } else if (kind == "copy" || kind == "boolean" || kind == "cut") {
        cube(5, 20, 34, 36);
        cube(26, 6, 33, 37);
        if (kind == "cut") {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#364353"));
            p.drawRect(23, 30, 18, 20);
        }
    } else if (kind == "hole") {
        cube(7, 5, 48, 52);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#364353"));
        p.drawEllipse(17, 26, 23, 23);
        p.setBrush(side);
        p.drawEllipse(20, 27, 15, 21);
    } else if (kind == "finish") {
        p.setPen(QPen(QColor("#70cba4"), 7));
        line({9, 34}, {25, 49});
        line({25, 49}, {55, 14});
    } else if (kind == "import") {
        cube(9, 25, 31, 32);
        p.setPen(QPen(blue, 5));
        line({45, 5}, {45, 33});
        line({45, 33}, {36, 24});
        line({45, 33}, {54, 24});
    } else if (kind == "constraint_horizontal" || kind == "constraint_vertical") {
        p.setPen(QPen(blue,5,Qt::SolidLine,Qt::RoundCap));
        if(kind=="constraint_horizontal") { p.drawLine(12,25,52,25); p.drawLine(12,39,52,39); }
        else { p.drawLine(25,12,25,52); p.drawLine(39,12,39,52); }
    } else if (kind == "constraint_fixed") {
        p.setPen(QPen(ink,3)); p.drawArc(QRectF(22,9,20,28),0,180*16);
        p.setBrush(blue); p.drawRoundedRect(QRectF(15,28,34,27),3,3);
        p.setPen(QPen(side,4)); p.drawLine(32,36,32,47);
    } else if (kind == "constraint_coincident") {
        p.setPen(QPen(ink,3)); p.drawLine(8,12,32,32); p.drawLine(32,32,56,48);
        p.setBrush(blue); p.drawEllipse(QPointF(32,32),8,8);
    } else if (kind == "parallel" || kind == "constraint" || kind == "offset" || kind == "tangent") {
        p.setPen(QPen(blue, 4));
        line({15, 49}, {31, 12});
        line({33, 49}, {49, 12});
    } else {
        cube(7, 5, 48, 53);
        if (kind == "extrude") {
            p.setPen(QPen(top, 3));
            line({59, 48}, {59, 6});
            line({59, 6}, {52, 14});
        }
    }
    return QIcon(pix);
}
class Form : public QDialog {
  public:
    std::function<bool()> validate;
    void accept() override {
        if (!validate || validate())
            QDialog::accept();
    }
    QFormLayout *layout;
    QMap<QString, QDoubleSpinBox *> nums;
    QMap<QString, QComboBox *> combos;
    Form(QWidget *parent, QString title) : QDialog(parent) {
        setWindowTitle(title);
        if (auto *host = parent->findChild<Viewport *>()) {
            setParent(host);
            setWindowFlags(Qt::Widget);
        } else
            setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setFixedWidth(290);
        layout = new QFormLayout(this);
        layout->setContentsMargins(12, 10, 12, 12);
        layout->setSpacing(8);
        auto *heading = new QLabel("−   " + title.toUpper());
        heading->setStyleSheet(
            "color:#dbe5ef;border-bottom:1px solid #526374;padding-bottom:7px;font-size:11px;");
        layout->addRow(heading);
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
        l->setStyleSheet("color:#aebfce;font-size:11px;");
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
            move(parentWidget()->width() - width() - 12, 116);
        }
        QEventLoop loop;
        connect(this, &QDialog::finished, &loop, &QEventLoop::quit);
        setWindowModality(Qt::NonModal);
        show();
        raise();
        loop.exec();
        return result() == Accepted;
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
    connect(a, &QAction::triggered, this, [this, fn, key] {
        if (activeCommand && key != "fit") {
            activeCommand->raise();
            return;
        }
        if (key != "fit")
            for (auto *dialog : findChildren<QDialog *>())
                if (dialog->isVisible()) {
                    dialog->raise();
                    return;
                }
        run([&] {
            if ((key == "extrude" || key == "revolve") && canvas->hasSubselection()) {
                const auto items = canvas->selectedDetails;
                QString owner;
                QSet<int> edges;
                bool complete = !items.empty();
                for (const auto &item : items) {
                    if (owner.isEmpty()) owner = item.feature;
                    complete = complete && item.kind == "edge" && item.feature == owner;
                    edges.insert(item.index);
                }
                if (complete && model.get(owner).type == "sketch") {
                    TopTools_IndexedMapOfShape topology;
                    TopExp::MapShapes(model.get(owner).shape, TopAbs_EDGE, topology);
                    const auto p = model.get(owner).p;
                    bool closed = p["profile"] == "rectangle" || p["profile"] == "circle" || p["closed"].toBool();
                    if (closed && edges.size() == topology.Extent())
                        select(owner);
                }
            }
            const QStringList bodyCommands = {"delete", "rollback", "transform", "copy",
                                              "fillet", "hole",     "boolean",   "cut",
                                              "common", "extrude",  "revolve",   "dimension"};
            if(!selected.isEmpty() && model.get(selected).inactive &&
               (bodyCommands.contains(key) || key=="chamfer" || key=="sketch" || key=="measure"))
                throw std::runtime_error("Reative a etapa e suas dependências antes de operar sobre sua geometria.");
            if (key != "delete" && key != "transform" && key != "fillet" && key != "chamfer" &&
                (canvas->hasSubselection() || canvas->selectedDetails.size() > 1) && bodyCommands.contains(key))
                throw std::runtime_error(
                    "Há subelementos ou vários itens selecionados. Esta ferramenta ainda atua em um objeto inteiro; "
                    "selecione o objeto no Browser ou use o filtro Objetos / perfis.");
            fn();
        });
    });
    addAction(a);
    commands[key] = a;
    return a;
}
Window::Window(QString recoveryDirectory, bool promptRecovery) {
    setObjectName("MecaCAD");
    resize(1440, 920);
    setMinimumSize(1100, 720);
    setDockOptions(QMainWindow::AnimatedDocks);
    setWindowTitle("MecaCAD");
    qApp->setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#364353"));
    palette.setColor(QPalette::WindowText, QColor("#d9e0e8"));
    palette.setColor(QPalette::Base, QColor("#303d4b"));
    palette.setColor(QPalette::Text, QColor("#d9e0e8"));
    palette.setColor(QPalette::Button, QColor("#364353"));
    palette.setColor(QPalette::ButtonText, QColor("#d9e0e8"));
    palette.setColor(QPalette::Highlight, QColor("#357899"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(palette);
    qApp->setStyleSheet(R"(
        QWidget{font-family:'Helvetica Neue';font-size:12px;color:#dce3eb;}
        QMainWindow,QDialog{background:#364353;}
        QDialog{border:1px solid #566879;}
        QMenuBar,QMenu{background:#303d4c;color:#e0e6ed;} QMenu{border:1px solid #526576;padding:4px;}
        QMenu::item{padding:6px 30px 6px 12px;} QMenu::item:selected{background:#42667f;} QMenu::item:disabled{color:#8292a3;}
        QToolBar{border:0;spacing:2px;padding:0;background:#202b37;}
        QToolButton{border:1px solid transparent;border-radius:0;padding:2px;color:#dce3eb;}
        QToolButton:hover{background:#455e73;border-color:#708a9f;} QToolButton:pressed,QToolButton:checked{background:#345c75;border-color:#55b8e3;}
        QToolButton::menu-indicator{width:0;}
        QTabBar::tab{padding:3px 17px;color:#dce2e9;background:transparent;border:0;border-bottom:2px solid transparent;font-size:10px;font-weight:600;}
        QTabBar::tab:selected{border-bottom:2px solid #d8e3ee;} QTabBar::tab:disabled{color:#a2adbb;}
        QDockWidget{font-weight:500;background:transparent;} QDockWidget::title{background:#354353;padding:4px 8px;color:#d6dfe8;border:1px solid #49596a;font-size:11px;}
        QTreeWidget{background:transparent;border:0;outline:0;color:#dae2ec;padding:2px 0;font-size:11px;}
        QTreeWidget::item{height:21px;padding:0;} QTreeWidget::item:selected{background:#293747;color:#e5eef6;}
        QTreeWidget::item:hover{background:#42566b;}
        QLineEdit,QDoubleSpinBox,QComboBox{background:#293644;border:1px solid #526477;border-radius:0;padding:3px 5px;color:#e0e8f0;min-height:18px;selection-background-color:#367998;}
        QLineEdit:focus,QDoubleSpinBox:focus,QComboBox:focus{border-color:#54bfea;}
        QComboBox QAbstractItemView{background:#2d3d4d;color:#e1e9f2;selection-background-color:#44657f;}
        QPushButton{background:#364353;border:1px solid #718499;border-radius:1px;padding:4px 12px;color:#e0e6ef;}
        QPushButton:hover{background:#496379;} QPushButton:default{background:#34556d;border-color:#56aacf;}
        QCheckBox{spacing:4px;} QScrollArea{border:0;background:#364353;}
        QStatusBar{background:#364353;color:#99acbd;border:0;font-size:10px;min-height:0;}
        QListWidget{background:transparent;border:0;outline:0;}
        QListWidget::item{border:1px solid transparent;border-radius:0;background:transparent;margin:0;padding:1px;}
        QListWidget::item:selected{background:#4c6d84;border-color:#62b8df;}
        QListWidget::item:hover{background:#485b6c;}
        QScrollBar:horizontal{height:5px;background:#354352;} QScrollBar::handle:horizontal{background:#6f8294;min-width:24px;}
        QScrollBar:vertical{width:5px;background:#354352;} QScrollBar::handle:vertical{background:#6f8294;min-height:24px;}
        QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}
        QToolTip{background:#24313f;color:#e2eaf3;border:1px solid #6c8499;padding:6px;}
    )");
    auto *file = menuBar()->addMenu("File");
    auto *edit = menuBar()->addMenu("Edit");
    auto *viewMenu = menuBar()->addMenu("View");
    auto *help = menuBar()->addMenu("Help");
    file->addAction(command("new", "New Design", "Ctrl+N", [this] {
        if (canLeave()) {
            clearRecovery();
            model.clear();
            selected.clear();
            finishSketch();
            refresh();
        }
    }));
    file->addAction(command("open", "Open…", "Ctrl+O", [this] { open(); }));
    file->addAction(command("save", "Save", "Ctrl+S", [this] { save(); }));
    file->addAction(command("saveas", "Save As…", "Ctrl+Shift+S", [this] { save(true); }));
    file->addAction(command("recover", "Recover unsaved project…", "", [this] { recoverProject(); }));
    file->addSeparator();
    file->addAction(command("import", "Import STEP / STL…", "", [this] {
        auto path = QFileDialog::getOpenFileName(
            this, "Import STEP / STL", {},
            "CAD / Mesh (*.step *.stp *.stl *.STEP *.STP *.STL);;STL (*.stl *.STL);;STEP (*.step *.stp)");
        if (!path.isEmpty()) {
            bool mesh = QFileInfo(path).suffix().compare("stl", Qt::CaseInsensitive) == 0;
            selected = mesh ? model.importStl(path) : model.importStep(path);
            refresh(true);
            if (mesh)
                QMessageBox::information(this, "STL importado",
                                         "Importado como malha, usando milímetros.\n"
                                         "STL não registra unidades nem histórico paramétrico.\n"
                                         "A malha será incorporada ao salvar o projeto .mcad. Conversão para "
                                         "sólido ainda não disponível.");
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
    edit->addAction(command("delete", "Apagar peça", "Backspace", [this] {
        if (!canvas->selectedDetails.empty()) {
            const auto items = canvas->selectedDetails;
            for (const auto &item : items)
                if (item.kind == "face")
                    throw std::runtime_error("Apagar faces de um sólido ainda não está disponível. Selecione o corpo inteiro para apagar a peça.");
            Model work = model;
            QSet<QString> owners;
            for (const auto &item : items) owners.insert(item.feature);
            for (const auto &owner : owners) {
                QVector<int> edges, vertices;
                bool whole = false;
                for (const auto &item : items) if (item.feature == owner) {
                    if (item.kind == "object") whole = true;
                    else if (item.kind == "edge") edges.append(item.index);
                    else if (item.kind == "vertex") vertices.append(item.index);
                }
                if (!whole)
                    work.editSketchElements(owner, edges, vertices, {}, true);
                else if (work.get(owner).type == "sketch") work.remove(owner);
                else work.deleteBody(owner);
            }
            model.commit(work.json());
            selected.clear();
            refresh();
            return;
        }
        if (!selected.isEmpty()) {
            if (model.get(selected).type == "sketch")
                model.remove(selected);
            else
                model.deleteBody(selected);
            selected.clear();
            refresh();
        }
    }));
    auto *deleteKey = new QAction(this);
    deleteKey->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(deleteKey, &QAction::triggered, commands["delete"], &QAction::trigger);
    addAction(deleteKey);
    edit->addAction(command("rollback", "Voltar uma etapa da peça", "", [this] {
        if (selected.isEmpty())
            return;
        const auto parameters = model.get(selected).p;
        QString previous = parameters["target"].toString();
        if (previous.isEmpty())
            previous = parameters["source"].toString();
        if (previous.isEmpty())
            throw std::runtime_error(
                "Esta operação não possui uma etapa anterior. Use Apagar para remover a peça.");
        model.remove(selected);
        selected = previous;
        refresh();
    }));
    edit->addAction(command("dependencies", "Dependências da etapa…", "", [this] {
        if (selected.isEmpty()) return;
        const auto graph = model.dependencyGraph();
        auto names = [&](const QStringList &ids) {
            QStringList result;
            for (const auto &id : ids) result.append(model.get(id).name + " [" + id.left(8) + "]");
            return result.empty() ? QString("Nenhuma") : result.join("\n");
        };
        QMessageBox dialog(QMessageBox::Information, "Dependências da etapa", "", QMessageBox::Ok, this);
        dialog.setObjectName("dependencyReport");
        dialog.setTextFormat(Qt::PlainText);
        dialog.setText(model.get(selected).name + "\n\nEntradas diretas:\n" + names(graph.dependencies(selected)) +
            "\n\nEtapas dependentes (diretas e indiretas):\n" + names(graph.dependents(selected)));
        dialog.exec();
    }));
    for (int delta : {-1, 1}) {
        const QString key = delta < 0 ? "historyEarlier" : "historyLater";
        edit->addAction(command(key, delta < 0 ? "Mover etapa para antes" : "Mover etapa para depois", "", [this, delta] {
            if (canvas->sketchMode)
                throw std::runtime_error("Finalize o sketch antes de reordenar o histórico.");
            if (selected.isEmpty()) return;
            const auto iterator = std::find_if(model.features.begin(), model.features.end(),
                [this](const Feature &feature) { return feature.id == selected; });
            if (iterator == model.features.end()) return;
            const int destination = int(iterator - model.features.begin()) + delta;
            if (destination < 0 || destination >= int(model.features.size())) return;
            model.moveFeature(selected, destination);
            refresh();
        }));
    }
    edit->addAction(command("suppress", "Suprimir / reativar etapa", "", [this] {
        if(canvas->sketchMode)throw std::runtime_error("Finalize o sketch antes de suprimir uma etapa.");
        if(selected.isEmpty())return;
        const auto &feature=model.get(selected);
        if(feature.inactive && !feature.suppressed)
            throw std::runtime_error("Esta etapa depende de uma etapa suprimida. Reative a origem primeiro; consulte Dependências da etapa.");
        model.suppress(selected,!feature.suppressed);refresh();
    }));
    command("sketch", "Create Sketch", "", [this] { startSketch(); });
    command("finish", "Finish Sketch", "", [this] { finishSketch(); });
    command("rectangle", "2-Point Rectangle", "R", [this] { sketchTool("rectangle"); });
    command("circle", "Center Diameter Circle", "C", [this] { sketchTool("circle"); });
    command("polyline", "Line", "L", [this] { sketchTool("polyline"); });
    command("polygon", "Polygon — Polígono regular", "", [this] {
        bool ok = false;
        int sides = QInputDialog::getInt(this, "Polígono regular",
                                         "Número de lados (3 a 64):", canvas->polygonSides, 3, 64, 1, &ok);
        if (!ok)
            return;
        canvas->polygonSides = sides;
        sketchTool("polygon");
    });
    command("arc", "3-Point Arc", "", [this] { sketchTool("arc"); });
    command("dimension", "Sketch Dimension", "D", [this] {
        canvas->setTool({});
        if (selected.isEmpty())
            status->setText("Selecione um perfil e clique na cota para editar.");
        else {
            buildProperties();
            properties->show();
            if (!fields.isEmpty()) {
                fields.first()->setFocus();
                fields.first()->selectAll();
            }
        }
    });
    for (auto key : {QString("constraint_horizontal"),QString("constraint_vertical"),QString("constraint_fixed")}) {
        const QMap<QString,QString> labels{{"constraint_horizontal","Horizontal"},{"constraint_vertical","Vertical"},
            {"constraint_fixed","Fixar ponto"},{"constraint_coincident","Coincidente"}};
        command(key,labels[key],"",[this,key] {
            const auto items=canvas->selectedDetails;
            const bool pair=key=="constraint_coincident", line=key=="constraint_horizontal" || key=="constraint_vertical";
            if(items.size()!=(pair ? 2 : 1)) throw std::runtime_error("Selecione uma linha, um ponto para fixar, ou dois pontos com Shift para coincidência.");
            QString owner=items.front().feature;
            QStringList entities;
            for(const auto &item:items) {
                if(item.feature!=owner || item.kind!=(line ? "edge" : "vertex"))
                    throw std::runtime_error("Selecione os elementos exigidos no mesmo sketch.");
                entities.append(model.sketchEntityId(owner,item.kind,item.index));
            }
            auto relation=key=="constraint_horizontal" ? sketch::Relation::Horizontal :
                key=="constraint_vertical" ? sketch::Relation::Vertical : pair ? sketch::Relation::Coincident : sketch::Relation::Fixed;
            QPointF value;
            if(relation==sketch::Relation::Fixed)
                for(const auto &point:model.sketchSystem(owner).points) if(point.id==entities.front()) value=point.position;
            model.constrainSketch(owner,relation,entities.front(),pair ? entities.back() : QString(),value);
            selected=owner; canvas->selectedDetails.clear(); canvas->selectedDetail={}; refresh();
            status->setText(QString("Restrição aplicada · %1 graus de liberdade").arg(model.sketchSystem(owner).solve().degreesOfFreedom));
        });
    }
    command("constraint_remove","Remover restrição…","",[this] {
        if(selected.isEmpty()) throw std::runtime_error("Selecione um sketch com restrições.");
        auto system=model.sketchSystem(selected);
        if(system.constraints.empty()) throw std::runtime_error("O sketch não tem restrições.");
        const QStringList labels{"Coincidente","Horizontal","Vertical","Ponto fixo","Distância X","Distância Y"};
        QStringList choices;
        for(const auto &c:system.constraints) choices.append(labels[int(c.relation)]+" · "+c.first+" "+c.second+" ["+c.id+"]");
        bool ok=false;
        auto choice=QInputDialog::getItem(this,"Restrições do sketch","Restrição a remover",choices,0,false,&ok);
        if(!ok) return;
        model.removeSketchConstraint(selected,system.constraints[choices.indexOf(choice)].id); refresh();
    });
    for(bool vertical:{false,true})command(vertical?"constraint_distance_y":"constraint_distance_x",
        vertical?"Cota vertical":"Cota horizontal","",[this,vertical]{
        const auto items=canvas->selectedDetails;
        if(items.empty())throw std::runtime_error("Selecione uma linha ou dois vértices do mesmo sketch.");
        const auto owner=items.front().feature;const auto system=model.sketchSystem(owner);
        QString first,second;
        if(items.size()==1 && items.front().kind=="edge") {
            const auto id=model.sketchEntityId(owner,"edge",items.front().index);
            for(const auto &line:system.lines)if(line.id==id){first=line.end;second=line.start;break;}
        } else if(items.size()==2 && items[0].feature==items[1].feature && items[0].kind=="vertex" && items[1].kind=="vertex") {
            first=model.sketchEntityId(owner,"vertex",items[0].index);second=model.sketchEntityId(owner,"vertex",items[1].index);
        }
        if(first.isEmpty() || second.isEmpty())throw std::runtime_error("Selecione uma linha ou dois vértices do mesmo sketch.");
        QPointF a,b;for(const auto &point:system.points){if(point.id==first)a=point.position;if(point.id==second)b=point.position;}
        const double distance=vertical?a.y()-b.y():a.x()-b.x();bool ok=false;
        const auto formula=QInputDialog::getText(this,vertical?"Cota vertical":"Cota horizontal",
            "Distância com sinal (primeiro ponto menos segundo). Use mm ou uma fórmula do projeto.",QLineEdit::Normal,
            QString::number(distance,'g',12)+" mm",&ok);
        if(!ok)return;
        const auto value=parameters::evaluate(formula,parameters::resolve(model.parameters()));
        if(value.length!=1 || value.angle!=0)throw std::runtime_error("A cota deve resultar em comprimento.");
        Model work=model;const auto constraint=work.constrainSketch(owner,vertical?sketch::Relation::DistanceY:sketch::Relation::DistanceX,
            first,second,vertical?QPointF(0,value.value):QPointF(value.value,0));
        work.setExpression(owner,"constraint:"+constraint,formula);model.commit(work.json());selected=owner;refresh();
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
    command("chamfer", "Chamfer", "", [this] { fillet(true); });
    command("measure", "Measure", "I", [this] { measure(); });
    command("parameters", "Change Parameters", "", [this] {
        QDialog dialog(this); dialog.setObjectName("parameterEditor");dialog.setWindowTitle("Parâmetros do projeto");dialog.resize(650,420);
        auto *layout=new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel("Use unidades nas medidas: 20 mm, 2 cm, largura / 2. Nomes sem espaços.",&dialog));
        auto *table=new QTableWidget(0,3,&dialog);table->setObjectName("parameterTable");
        table->setHorizontalHeaderLabels({"Nome","Expressão","Valor calculado"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        const auto definitions=model.parameters();
        for(auto it=definitions.begin();it!=definitions.end();++it) {
            int row=table->rowCount();table->insertRow(row);
            table->setItem(row,0,new QTableWidgetItem(it.key()));table->setItem(row,1,new QTableWidgetItem(it.value()));
        }
        layout->addWidget(table);
        auto *row=new QHBoxLayout;layout->addLayout(row);
        auto *add=new QPushButton("Adicionar",&dialog);add->setObjectName("addParameter");row->addWidget(add);
        auto *remove=new QPushButton("Remover linha",&dialog);row->addWidget(remove);row->addStretch();
        connect(add,&QPushButton::clicked,&dialog,[table]{table->insertRow(table->rowCount());});
        connect(remove,&QPushButton::clicked,&dialog,[table]{if(table->currentRow()>=0)table->removeRow(table->currentRow());});
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
        auto *error=new QLabel(&dialog);error->setObjectName("parameterError");error->setWordWrap(true);
        error->setStyleSheet("color:#edc17e");layout->addWidget(error);
        auto readDefinitions=[&]{
            QMap<QString,QString> values;
            for(int i=0;i<table->rowCount();++i) {
                auto name=table->item(i,0)?table->item(i,0)->text().trimmed():QString();
                auto expression=table->item(i,1)?table->item(i,1)->text().trimmed():QString();
                if(name.isEmpty() && expression.isEmpty())continue;
                if(values.contains(name))throw std::runtime_error("Nome de parâmetro duplicado.");
                values[name]=expression;
            }
            return values;
        };
        auto updateValues=[&]{
            QSignalBlocker blocker(table);
            for(int i=0;i<table->rowCount();++i) {
                auto *item=new QTableWidgetItem;
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);table->setItem(i,2,item);
            }
            try {
                const auto resolved=parameters::resolve(readDefinitions());
                for(int i=0;i<table->rowCount();++i) {
                    const auto name=table->item(i,0)?table->item(i,0)->text().trimmed():QString();
                    if(!resolved.contains(name))continue;
                    const auto quantity=resolved[name];QStringList units;
                    if(quantity.length)units<< (quantity.length==1?QString("mm"):QString("mm^%1").arg(quantity.length));
                    if(quantity.angle)units<< (quantity.angle==1?QString("rad"):QString("rad^%1").arg(quantity.angle));
                    table->item(i,2)->setText(QString::number(quantity.value,'g',12)+(units.empty()?QString():" "+units.join(" · ")));
                }
                error->clear();buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
            } catch(const std::exception &e) {
                error->setText(QString::fromUtf8(e.what()));buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
            }
        };
        connect(table,&QTableWidget::cellChanged,&dialog,[&](int,int column){if(column<2)updateValues();});
        connect(table->model(),&QAbstractItemModel::rowsRemoved,&dialog,updateValues);
        updateValues();
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
            try {
                model.setParameters(readDefinitions());dialog.accept();
            } catch(const std::exception &e) {QMessageBox::warning(&dialog,"Parâmetros não aplicados",e.what());}
        });
        if(dialog.exec()==QDialog::Accepted)refresh();
    });
    command("expression", "Link Dimension to Expression", "", [this] {
        if(selected.isEmpty() || Model::expressionFields(model.get(selected).type,model.get(selected).p).isEmpty())
            throw std::runtime_error("Selecione no Browser um recurso com medidas editáveis.");
        const auto id=selected;
        QDialog dialog(this);dialog.setObjectName("expressionEditor");dialog.setWindowTitle("Expressão da medida");
        auto *layout=new QVBoxLayout(&dialog);auto *field=new QComboBox(&dialog);field->setObjectName("expressionField");
        for(const auto &key:Model::expressionFields(model.get(id).type,model.get(id).p)) {
            QString label=key;
            if(key.startsWith("constraint:"))for(const auto &constraint:model.sketchSystem(id).constraints)if(key=="constraint:"+constraint.id)
                label=QString("Cota %1: %2 − %3 [%4]").arg(constraint.relation==sketch::Relation::DistanceX?"X":"Y",constraint.first,constraint.second,constraint.id.left(8));
            field->addItem(label,key);
        }
        layout->addWidget(field);
        auto *input=new QLineEdit(&dialog);input->setObjectName("expressionInput");layout->addWidget(input);
        auto load=[&]{input->setText(model.get(id).p.value("expressions").toObject().value(field->currentData().toString()).toString());};
        connect(field,&QComboBox::currentTextChanged,&dialog,load);load();
        layout->addWidget(new QLabel("Comprimentos em mm/cm/m/in; ângulos em deg/rad.\nEx.: largura / 2 ou 90 deg.\nVazio desvincula e mantém a medida atual.",&dialog));
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
            try {model.setExpression(id,field->currentData().toString(),input->text());dialog.accept();}
            catch(const std::exception &e) {QMessageBox::warning(&dialog,"Expressão não aplicada",e.what());}
        });
        if(dialog.exec()==QDialog::Accepted)refresh();
    });
    command("search", "Design Shortcuts", "S", [this] { search(); });
    auto *selectionMenu = viewMenu->addMenu("Seleção");
    auto *selectionGroup = new QActionGroup(this);
    for (auto entry : QList<QPair<QString, QString>>{{"auto", "Automático"},
                                                     {"object", "Objetos / perfis"},
                                                     {"edge", "Linhas / arestas"},
                                                     {"face", "Faces"},
                                                     {"vertex", "Vértices"}}) {
        auto *action = command("select_" + entry.first, entry.second, "", [this, mode = entry.first] {
            canvas->selectionFilter = mode;
            canvas->selectedDetails.clear();
            canvas->selectedDetail = {};
            canvas->hoveredDetail = {};
            canvas->setTool({});
            select({});
            canvas->update();
        });
        action->setCheckable(true);
        action->setChecked(entry.first == "auto");
        selectionGroup->addAction(action);
        selectionMenu->addAction(action);
    }
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
            "(E).\n5. Save and use File → Export.\n\nMiddle drag: pan · Shift+middle drag: orbit · Scroll: "
            "zoom · F: "
            "fit\nLine: Enter finishes; Shift+Enter closes the profile.\n\nVersion 0.1: one profile per "
            "sketch; dimensions drive rectangles/circles. General sketch constraints, face attachment, "
            "assemblies and simulation are not yet available.");
    });
    auto *bar = addToolBar("Application");
    bar->setObjectName("applicationToolbar");
    bar->setMovable(false);
    bar->setIconSize({16, 16});
    bar->setFixedHeight(28);
    auto *files = new QToolButton;
    files->setIcon(icon("grid"));
    files->setToolTip("File / projects");
    files->setMenu(file);
    files->setPopupMode(QToolButton::InstantPopup);
    bar->addWidget(files);
    for (auto key : {"new", "save", "undo", "redo"})
        bar->addAction(commands[key]);
    auto *home = new QToolButton;
    home->setIcon(icon("home"));
    home->setToolTip("Open design");
    connect(home, &QToolButton::clicked, commands["open"], &QAction::trigger);
    bar->addWidget(home);
    documentTitle = new QLabel("Untitled");
    documentTitle->setObjectName("documentTab");
    documentTitle->setStyleSheet(
        "background:#364353;padding:5px 28px;color:#e1e8ef;font-size:11px;font-weight:600;");
    documentTitle->setMinimumWidth(380);
    documentTitle->setMaximumWidth(640);
    documentTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(documentTitle);
    auto *newTab = new QToolButton;
    newTab->setText("+");
    newTab->setToolTip("New Design");
    connect(newTab, &QToolButton::clicked, commands["new"], &QAction::trigger);
    bar->addWidget(newTab);
    auto *space = new QWidget;
    space->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(space);
    auto *local = new QLabel("MecaCAD   ·   Local   ");
    local->setStyleSheet("color:#9badbf;font-size:10px;");
    bar->addWidget(local);
    auto *helpButton = new QToolButton;
    helpButton->setText("?");
    helpButton->setToolTip("Help");
    helpButton->setMenu(help);
    helpButton->setPopupMode(QToolButton::InstantPopup);
    bar->addWidget(helpButton);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *header = new QWidget;
    header->setObjectName("commandHeader");
    header->setStyleSheet("#commandHeader{background:#364353;}");
    auto *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(10, 0, 6, 4);
    headerRow->setSpacing(10);
    auto *workspace = new QToolButton;
    workspace->setText("DESIGN ▾");
    workspace->setFixedSize(98, 54);
    workspace->setStyleSheet("border:1px solid #73899e;border-radius:3px;font-size:11px;font-weight:600;");
    auto *workspaces = new QMenu(workspace);
    workspaces->addAction("Design");
    for (auto name : {"Render", "Animation", "Simulation", "Manufacture", "Drawing", "Electronics"}) {
        auto *a = workspaces->addAction(QString(name) + " — planned");
        a->setEnabled(false);
    }
    workspace->setMenu(workspaces);
    workspace->setPopupMode(QToolButton::InstantPopup);
    headerRow->addWidget(workspace, 0, Qt::AlignBottom);
    auto *toolsColumn = new QVBoxLayout;
    toolsColumn->setContentsMargins(0, 0, 0, 0);
    toolsColumn->setSpacing(0);
    tabs = new QTabBar;
    tabs->setObjectName("workspaceTabs");
    tabs->setFixedHeight(22);
    for (auto title : {"SOLID", "SURFACE", "MESH", "SHEET METAL", "PLASTIC", "MANAGE", "UTILITIES"})
        tabs->addTab(title);
    for (int i = 1; i < tabs->count(); ++i)
        tabs->setTabEnabled(i, false);
    toolsColumn->addWidget(tabs, 0, Qt::AlignLeft);
    ribbon = new QWidget;
    ribbon->setObjectName("ribbon");
    ribbon->setFixedHeight(54);
    toolsColumn->addWidget(ribbon);
    headerRow->addLayout(toolsColumn, 1);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addToolBarBreak();
    auto *headerBar = addToolBar("Design commands");
    headerBar->setObjectName("designToolbar");
    headerBar->setMovable(false);
    headerBar->setStyleSheet("QToolBar{padding:0;border:0;background:#364353;}");
    headerBar->addWidget(header);
    canvas = new Viewport(&model);
    layout->addWidget(canvas, 1);
    navigation = new QWidget(canvas);
    navigation->setObjectName("navigationOverlay");
    navigation->setStyleSheet("#navigationOverlay{background:rgba(48,62,78,190);border-radius:3px;}");
    auto *nav = new QHBoxLayout(navigation);
    nav->setContentsMargins(5, 1, 5, 1);
    nav->setSpacing(0);
    auto navButton = [&](QString glyph, QString tip, std::function<void()> fn) {
        auto *b = new QToolButton;
        b->setIcon(icon(glyph));
        b->setIconSize({18, 18});
        b->setFixedSize(28, 25);
        b->setToolTip(tip);
        connect(b, &QToolButton::clicked, this, fn);
        nav->addWidget(b);
        return b;
    };
    navButton("orbit", "Orbit · Shift + middle drag", [this] {
        canvas->setTool({});
        canvas->navigationMode = "orbit";
    });
    navButton("home", "Home view", [this] {
        canvas->view("iso");
        canvas->fit();
    });
    navButton("pan", "Pan · middle drag", [this] {
        canvas->setTool({});
        canvas->navigationMode = "pan";
    });
    navButton("zoom", "Zoom in", [this] { canvas->zoomBy(.8f); });
    navButton("fit", "Fit · F", [this] { canvas->fit(); });
    auto *display = navButton("display", "Display settings", [] {});
    auto *displayMenu = new QMenu(display);
    displayMenu->addAction(theme);
    auto *edges = displayMenu->addAction("Visible edges");
    edges->setCheckable(true);
    edges->setChecked(true);
    connect(edges, &QAction::toggled, this, [this](bool yes) {
        canvas->showEdges = yes;
        canvas->update();
    });
    display->setMenu(displayMenu);
    display->setPopupMode(QToolButton::InstantPopup);
    auto *grid = navButton("grid", "Grid and snaps", [] {});
    auto *gridMenu = new QMenu(grid);
    auto *snap = gridMenu->addAction("Snap to 1 mm");
    snap->setCheckable(true);
    snap->setChecked(true);
    connect(snap, &QAction::toggled, this, [this](bool v) { canvas->snap = v; });
    auto *smartSnap = gridMenu->addAction("Encaixe inteligente");
    smartSnap->setObjectName("smartSnap");
    smartSnap->setToolTip("Atrai o cursor a pontos e alinhamentos; não cria restrições permanentes. Use Constraints para manter uma relação.");
    smartSnap->setCheckable(true);
    smartSnap->setChecked(true);
    connect(smartSnap, &QAction::toggled, this, [this](bool enabled) {
        canvas->smartSnap = enabled;
        canvas->update();
    });
    grid->setMenu(gridMenu);
    grid->setPopupMode(QToolButton::InstantPopup);
    auto *history = new QWidget;
    history->setObjectName("historyStrip");
    history->setFixedHeight(38);
    auto *historyLayout = new QHBoxLayout(history);
    historyLayout->setContentsMargins(8, 0, 8, 0);
    historyLayout->setSpacing(1);
    timeline = new QListWidget;
    timeline->setObjectName("timeline");
    timeline->setFlow(QListView::LeftToRight);
    timeline->setWrapping(false);
    timeline->setFixedHeight(34);
    timeline->setIconSize({18, 18});
    timeline->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    timeline->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto historyButton = [&](QString glyph, QString tip, int jump) {
        auto *b = new QToolButton;
        b->setText(glyph);
        b->setFixedSize(22, 25);
        b->setToolTip(tip);
        connect(b, &QToolButton::clicked, this, [this, jump] {
            if (timeline->count() == 0)
                return;
            int row = jump == -2  ? 0
                      : jump == 2 ? timeline->count() - 1
                                  : std::clamp(timeline->currentRow() + jump, 0, timeline->count() - 1);
            select(timeline->item(row)->data(Qt::UserRole).toString());
            timeline->scrollToItem(timeline->item(row));
        });
        historyLayout->addWidget(b);
    };
    historyButton("⏮", "Select first feature", -2);
    historyButton("◀", "Select previous feature", -1);
    auto *play = new QToolButton;
    play->setText("▶");
    play->setFixedSize(22, 25);
    play->setToolTip("History playback — planned");
    play->setEnabled(false);
    historyLayout->addWidget(play);
    historyButton("▶", "Select next feature", 1);
    historyButton("⏭", "Select last feature", 2);
    historyLayout->addSpacing(8);
    historyLayout->addWidget(timeline, 1);
    auto *historyHelp = new QToolButton;
    historyHelp->setText("⚙");
    historyHelp->setToolTip("Parametric history · select a feature to edit");
    historyHelp->setMenu(edit);
    historyHelp->setPopupMode(QToolButton::InstantPopup);
    historyLayout->addWidget(historyHelp);
    layout->addWidget(history);
    setCentralWidget(central);
    browser = new QDockWidget("BROWSER", canvas);
    browser->setObjectName("browserDock");
    browser->setFeatures(QDockWidget::NoDockWidgetFeatures);
    browser->setFixedWidth(264);
    tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    tree->setColumnCount(2);
    tree->setColumnWidth(0, 24);
    tree->setIconSize({14, 14});
    tree->setIndentation(14);
    tree->setUniformRowHeights(true);
    tree->setRootIsDecorated(false);
    tree->setTreePosition(1);
    tree->setRootIsDecorated(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    browser->setWidget(tree);
    properties = new QDockWidget("EDIT FEATURE", canvas);
    properties->setObjectName("propertiesDock");
    properties->setFixedWidth(280);
    properties->setFeatures(QDockWidget::DockWidgetClosable);
    propertyBody = new QWidget;
    propertyBody->setObjectName("propertyBody");
    propertyBody->setStyleSheet("#propertyBody{background:#364353;}");
    propertyForm = new QFormLayout(propertyBody);
    propertyForm->setContentsMargins(10, 10, 10, 10);
    propertyForm->setSpacing(7);
    auto *propertyScroll = new QScrollArea;
    propertyScroll->setWidgetResizable(true);
    propertyScroll->setFrameShape(QFrame::NoFrame);
    propertyScroll->setWidget(propertyBody);
    properties->setWidget(propertyScroll);
    properties->hide();
    status = new QLabel("Ready");
    statusBar()->addWidget(status, 1);
    statusBar()->show();
    canvas->installEventFilter(this);
    browser->show();
    navigation->show();
    QTimer::singleShot(0, this, [this] { positionPanels(); });
    canvas->onSelect = [this](QString id) {
        if (activeCommand) {
            if (commandSelection)
                commandSelection(id);
            return;
        }
        select(id);
    };
    canvas->onDimensionEdit = [this](QString id, QString key, double value) -> QString {
        if (activeCommand)
            return "Conclua ou cancele a operação atual.";
        try {
            const auto &feature = model.get(id);
            auto parameters = feature.p;
            auto name = feature.name;
            parameters[key] = value;
            model.edit(id, parameters, name);
            selected = id;
            refresh();
            return {};
        } catch (const Standard_Failure &error) {
            return QString::fromUtf8(error.GetMessageString());
        } catch (const std::exception &error) {
            return QString::fromUtf8(error.what());
        }
    };
    canvas->onEditSketch = [this](QString id) {
        if(model.get(id).inactive)return;
        if (activeCommand)
            return;
        const auto &p = model.get(id).p;
        canvas->plane = p["plane"].toString("XY");
        canvas->planeOffset = p["offset"].toDouble();
        canvas->sketchSupport = {p["support"].toString(), "face", p["supportFace"].toInt(-1), {}};
        canvas->sketchMode = true;
        canvas->setTool({});
        canvas->view("top");
        select(id);
        buildRibbon();
    };
    canvas->onDimensionExpression=[this](QString id,QString field,QString formula)->QString {
        if(activeCommand)return "Conclua ou cancele a operação atual.";
        try {
            bool numeric=false;formula.trimmed().replace(',','.').toDouble(&numeric);
            if(numeric)formula+=" mm";
            model.setExpression(id,field,formula);selected=id;refresh();return {};
        } catch(const std::exception &error){return QString::fromUtf8(error.what());}
          catch(const Standard_Failure &error){return QString::fromUtf8(error.GetMessageString());}
    };
    canvas->onPlaneChosen = [this](QString name, double offset) {
        canvas->plane = name;
        canvas->planeOffset = offset;
        canvas->sketchMode = true;
        canvas->view("top");
        canvas->setTool(pendingSketchTool);
        buildRibbon();
        status->setText("Sketch: clique para desenhar.");
    };
    canvas->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(canvas, &QWidget::customContextMenuRequested, this, [this](QPoint position) {
        QMenu menu;
        if (canvas->sketchMode) {
            for (auto key : {"polyline", "rectangle", "circle", "polygon", "dimension", "finish"})
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
            if (!canvas->sketchSupport.feature.isEmpty()) {
                p["support"] = canvas->sketchSupport.feature;
                p["supportFace"] = canvas->sketchSupport.index;
                TopTools_IndexedMapOfShape faces;
                TopExp::MapShapes(model.get(canvas->sketchSupport.feature).shape, TopAbs_FACE, faces);
                p["supportFaceCount"] = faces.Extent();
            }
            selected = model.add("sketch", p, "Sketch " + QString::number(model.features.size() + 1));
            refresh();
            status->setText("Sketch criado. Ajuste as dimensões à direita ou continue desenhando.");
        });
    };
    connect(tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (!refreshing && tree->currentItem())
            select(tree->currentItem()->data(0, Qt::UserRole).toString());
    });
    connect(tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
        if (activeCommand)
            return;
        QString id = item->data(0, Qt::UserRole).toString();
        if (column == 0 && !id.isEmpty())
            run([&] {
                model.toggle(id);
                refresh();
            });
        else if (id.isEmpty())
            item->setExpanded(!item->isExpanded());
    });
    connect(tree, &QTreeWidget::customContextMenuRequested, this, [this](QPoint pos) {
        if (activeCommand)
            return;
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
                if(model.get(selected).inactive)return;
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
        menu.addAction(commands["rollback"]);
        menu.addSeparator();
        menu.addAction(commands["dependencies"]);
        menu.addAction(commands["historyEarlier"]);
        menu.addAction(commands["historyLater"]);
        menu.addAction(commands["suppress"]);
        menu.exec(tree->viewport()->mapToGlobal(pos));
    });
    timeline->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(timeline, &QWidget::customContextMenuRequested, this, [this](QPoint position) {
        if (activeCommand || canvas->sketchMode) return;
        auto *item = timeline->itemAt(position);
        if (!item) return;
        select(item->data(Qt::UserRole).toString());
        QMenu menu;
        menu.addAction(commands["dependencies"]);
        menu.addAction(commands["historyEarlier"]);
        menu.addAction(commands["historyLater"]);
        menu.addAction(commands["suppress"]);
        menu.exec(timeline->viewport()->mapToGlobal(position));
    });
    connect(timeline, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { select(item->data(Qt::UserRole).toString()); });
    connect(timeline, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        if (!activeCommand)
            properties->show();
    });
    recoveryStatus=new QLabel(this);recoveryStatus->setObjectName("recoveryStatus");
    recoveryStatus->setAccessibleName("Estado da recuperação automática");
    statusBar()->addPermanentWidget(recoveryStatus);
    try {
        recovery = std::make_unique<RecoveryStore>(
            recoveryDirectory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/recoveries"
                                        : recoveryDirectory);
    } catch (const std::exception &error) {
        recoveryStatus->setText("Recuperação indisponível");
        recoveryStatus->setToolTip(QString::fromUtf8(error.what()));
    }
    auto *timer = new QTimer(this);
    timer->setObjectName("recoveryTimer");
    connect(timer, &QTimer::timeout, this, &Window::autosave);
    timer->start(30000);
    buildRibbon();
    refresh();
    QTimer::singleShot(150, this, [this, promptRecovery] {
        if (!promptRecovery) return;
        if(!recovery || !model.features.empty())return;
        const auto scan=recovery->scan();
        if(!scan.warnings.empty()) {
            recoveryStatus->setText("Recuperações com erro — File → Recover");
            recoveryStatus->setToolTip(scan.warnings.join('\n'));
        }
        if (!scan.entries.empty()) {
            if (QMessageBox::question(
                    this, "Recover project",
                    "Foi encontrado um projeto de uma sessão interrompida. Deseja recuperá-lo?") ==
                QMessageBox::Yes)
                run([this] { recoverProject(); });
        }
    });
}
void Window::positionPanels() {
    if (!canvas || !browser || !properties || !navigation)
        return;
    browser->setGeometry(4, 4, 264, std::max(100, canvas->height() - 48));
    properties->setGeometry(std::max(275, canvas->width() - 292), 116, 280,
                            std::min(440, std::max(120, canvas->height() - 155)));
    navigation->adjustSize();
    navigation->move((canvas->width() - navigation->width()) / 2, canvas->height() - 32);
    browser->raise();
    properties->raise();
    navigation->raise();
    if (activeCommand) {
        activeCommand->move(canvas->width() - activeCommand->width() - 12, 116);
        activeCommand->raise();
    }
}
bool Window::eventFilter(QObject *object, QEvent *event) {
    if (object == canvas && event->type() == QEvent::Resize)
        positionPanels();
    return QMainWindow::eventFilter(object, event);
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
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(5);
    auto group = [&](QString title, QStringList visible, QStringList menuItems,
                     QStringList planned = QStringList{}, QStringList preview = QStringList{}) {
        auto *container = new QWidget;
        auto *vertical = new QVBoxLayout(container);
        vertical->setContentsMargins(0, 0, 0, 0);
        vertical->setSpacing(0);
        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(1);
        for (auto key : visible) {
            auto *b = new QToolButton;
            b->setDefaultAction(commands[key]);
            b->setToolButtonStyle(Qt::ToolButtonIconOnly);
            b->setIconSize({29, 29});
            b->setFixedSize(32, 34);
            b->setToolTip(commands[key]->text() +
                          (commands[key]->shortcut().isEmpty()
                               ? ""
                               : " · " + commands[key]->shortcut().toString(QKeySequence::NativeText)));
            buttons->addWidget(b);
        }
        for (auto key : preview) {
            auto *b = new QToolButton;
            b->setIcon(icon(key));
            b->setIconSize({29, 29});
            b->setFixedSize(32, 34);
            b->setEnabled(false);
            b->setToolTip(key + " — planned");
            buttons->addWidget(b);
        }
        vertical->addLayout(buttons);
        auto *drop = new QToolButton;
        drop->setText(title + " ▾");
        drop->setFixedHeight(17);
        drop->setStyleSheet("font-size:10px;padding:0;");
        auto *menu = new QMenu(drop);
        for (auto key : menuItems)
            menu->addAction(commands[key]);
        if (!planned.isEmpty())
            menu->addSeparator();
        for (auto label : planned) {
            auto *a = menu->addAction(label + " — planned");
            a->setEnabled(false);
        }
        drop->setMenu(menu);
        drop->setPopupMode(QToolButton::InstantPopup);
        vertical->addWidget(drop, 0, Qt::AlignHCenter);
        row->addWidget(container);
        auto *line = new QFrame;
        line->setFrameShape(QFrame::VLine);
        line->setFixedHeight(44);
        line->setStyleSheet("color:#536171;");
        row->addWidget(line);
    };
    if (canvas->sketchMode) {
        tabs->setTabText(0, "SKETCH");
        group("CREATE", {"polyline", "rectangle", "circle", "polygon", "arc", "exact"},
              {"polyline", "rectangle", "circle", "polygon", "arc", "exact"}, {"Spline", "Slot", "Text"});
        group("MODIFY", {"dimension"}, {"dimension"}, {"Trim", "Extend", "Offset", "Mirror"},
              {"trim", "offset"});
        group("CONSTRAINTS", {"constraint_horizontal","constraint_vertical","constraint_fixed"},
              {"constraint_horizontal","constraint_vertical","constraint_fixed","constraint_distance_x","constraint_distance_y","constraint_remove"},
              {"Coincident", "Parallel", "Perpendicular", "Tangent", "Equal"});
        row->addStretch();
        auto *finish = new QToolButton;
        finish->setDefaultAction(commands["finish"]);
        finish->setIcon(icon("finish"));
        finish->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        finish->setIconSize({27, 27});
        finish->setStyleSheet("color:#b6e6ba;font-size:10px;");
        row->addWidget(finish);
    } else {
        tabs->setTabText(0, "SOLID");
        group("CREATE", {"sketch", "extrude", "revolve", "hole", "box", "cylinder"},
              {"sketch", "extrude", "revolve", "hole", "box", "cylinder", "sphere"},
              {"Sweep", "Loft", "Pattern", "Mirror"});
        group("MODIFY", {"fillet", "boolean", "cut", "copy", "transform"},
              {"fillet", "chamfer", "transform", "copy", "boolean", "cut", "common", "parameters", "expression"},
              {"Shell", "Draft", "Scale", "Split Body"});
        group("ASSEMBLE", {}, {}, {"New Component", "Joint", "As-Built Joint"}, {"joint", "component"});
        group("CONFIGURE", {}, {}, {"Configuration Table"}, {"configure"});
        group("CONSTRUCT", {}, {}, {"Offset Plane", "Midplane", "Axis"}, {"plane"});
        group("INSPECT", {"measure"}, {"measure"}, {"Section Analysis", "Interference"});
        group("INSERT", {"import"}, {"import"}, {"Canvas"});
        group("SELECT", {"search"},
              {"select_auto", "select_object", "select_edge", "select_vertex", "select_face", "search"});
        group("POSITION", {"transform"}, {"transform", "copy"});
        row->addStretch();
    }
}
void Window::refresh(bool fit) {
    if (!model.dirty) clearRecovery();
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
    auto *root = new QTreeWidgetItem(tree, {"", title});
    root->setIcon(0, icon("eye"));
    root->setIcon(1, icon("component"));
    auto *settings = new QTreeWidgetItem(root, {"", "Document Settings"});
    settings->setIcon(1, icon("gear"));
    new QTreeWidgetItem(settings, {"", "Units: mm"});
    auto *views = new QTreeWidgetItem(root, {"", "Named Views"});
    views->setIcon(1, icon("folder"));
    for (auto s : {"Top", "Front", "Right", "Home"})
        new QTreeWidgetItem(views, {"", s});
    auto *origin = new QTreeWidgetItem(root, {"", "Origin"});
    origin->setIcon(1, icon("folder"));
    for (auto s : {"XY", "XZ", "YZ"})
        new QTreeWidgetItem(origin, {"", s});
    auto *bodies = new QTreeWidgetItem(root, {"", "Bodies"});
    bodies->setIcon(1, icon("folder"));
    auto *sketches = new QTreeWidgetItem(root, {"", "Sketches"});
    sketches->setIcon(1, icon("folder"));
    auto *history = new QTreeWidgetItem(root, {"", "Features"});
    history->setIcon(1, icon("folder"));
    for (auto &f : model.features) {
        bool consumed = model.consumed(f.id);
        auto *parent = f.type == "sketch" ? sketches : ((f.inactive || consumed || f.type == "remove") ? history : bodies);
        const QString state=f.suppressed?" [suprimida]":(f.inactive?" [dependência suprimida]":"");
        auto *item = new QTreeWidgetItem(parent, {"", f.name+state});
        item->setIcon(0, icon(f.visible && !f.inactive ? "eye" : "hidden"));
        item->setIcon(1, icon(f.type));
        item->setData(0, Qt::UserRole, f.id);
        item->setToolTip(0, f.inactive?"Geometria inativa por supressão; reative a etapa de origem.":"Click to toggle visibility");
        if (consumed || f.inactive)
            item->setForeground(1, QColor("#9baab9"));
        if (f.id == selected)
            tree->setCurrentItem(item);
        auto *step = new QListWidgetItem(icon(f.type), "", timeline);
        step->setData(Qt::UserRole, f.id);
        step->setToolTip(f.name + state + " — " + displayType(f.type) + "\nDouble-click to edit");
        if(f.inactive){auto font=step->font();font.setStrikeOut(true);step->setFont(font);step->setText("−");}
        step->setSizeHint({25, 27});
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
    if (activeCommand) {
        if (commandSelection)
            commandSelection(id);
        return;
    }
    properties->hide();
    canvas->selectedDetails.clear();
    canvas->selectedDetail = {};
    canvas->hoveredDetail = {};
    refreshing = true;
    tree->clearSelection();
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
    if(f.inactive) {
        auto *note=new QLabel(f.suppressed?"Etapa suprimida. Reative para editar sua geometria.":"Etapa inativa por dependência suprimida. Reative a etapa de origem.");
        note->setWordWrap(true);propertyForm->addRow(note);
        auto *resume=new QPushButton("Suprimir / reativar etapa");propertyForm->addRow(resume);
        connect(resume,&QPushButton::clicked,commands["suppress"],&QAction::trigger);
        return;
    }
    properties->setWindowTitle(f.type == "sketch" ? "SKETCH DIMENSIONS" : "EDIT FEATURE");
    featureName = new QLineEdit(f.name);
    propertyForm->addRow("Name", featureName);
    static QMap<QString, QString> labels = {{"x", "X"},         {"y", "Y"},
                                            {"z", "Z"},         {"w", "Width"},
                                            {"h", "Height"},    {"d", "Distance"}, {"d2", "Distance 2"},
                                            {"r", "Radius"},    {"offset", "Plane offset"},
                                            {"angle", "Angle"}, {"axis", "Axis position"},
                                            {"u", "Center U"},  {"v", "Center V"},
                                            {"x1", "Start X"},  {"y1", "Start Y"},
                                            {"xm", "Mid X"},    {"ym", "Mid Y"},
                                            {"x2", "End X"},    {"y2", "End Y"}};
    for (auto key : {"x", "y", "z", "w", "h", "r", "d", "d2", "angle", "axis", "u", "v", "offset", "x1", "y1", "xm",
                     "ym", "x2", "y2"})
        if (f.p[key].isDouble()) {
            auto *spin = new QDoubleSpinBox;
            spin->setRange(-100000, 100000);
            spin->setDecimals(3);
            spin->setSuffix(QString(key) == "angle" ? " °" : " mm");
            spin->setValue(f.p[key].toDouble());
            spin->setKeyboardTracking(false);
            const auto expression=f.p["expressions"].toObject()[key].toString();
            if(!expression.isEmpty()) {
                spin->setReadOnly(true);
                spin->setToolTip("Controlado por: "+expression+"\nEdite em Modify → Link Dimension to Expression.");
            }
            fields[key] = spin;
            propertyForm->addRow(labels.value(key, key), spin);
        }
    if (f.type == "sketch") {
        if(f.p.contains("constraintSystem")) {
            const auto solved=model.sketchSystem(f.id).solve();
            auto *state=new QLabel(QString("%1 graus de liberdade · %2 restrições redundantes")
                .arg(solved.degreesOfFreedom).arg(solved.redundantConstraints.size()));
            state->setObjectName("sketchConstraintStatus"); state->setWordWrap(true); propertyForm->addRow(state);
            auto *remove=new QPushButton("Remover restrição…"); propertyForm->addRow(remove);
            connect(remove,&QPushButton::clicked,commands["constraint_remove"],&QAction::trigger);
        }
        auto *note =
            new QLabel("Plane: " + (f.p["plane"].toString().startsWith("FACE:") ? QString("Face da peça") : f.p["plane"].toString()) + "\nProfile: " + f.p["profile"].toString() +
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
    } else if (model.isMesh(f.id)) {
        propertyForm->addRow(
            new QLabel("Malha STL · unidades: mm\nVolume não calculado; não é um sólido paramétrico."));
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
    positionPanels();
}
void Window::applyProperties() {
    auto p = model.get(selected).p;
    for (auto it = fields.begin(); it != fields.end(); ++it)
        if(!model.get(selected).p.value("expressions").toObject().contains(it.key())) p[it.key()] = it.value()->value();
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
    pendingSketchTool = "rectangle";
    canvas->sketchSupport = {};
    if (canvas->selectedDetail.kind == "face" && canvas->selectedDetails.size() <= 1) {
        auto target = canvas->selectedDetail;
        auto plane = Model::facePlane(model.get(target.feature).shape, target.index);
        canvas->sketchSupport = target;
        canvas->choosingPlane = false;
        canvas->onPlaneChosen(plane, 0);
        return;
    }
    canvas->sketchMode = false;
    canvas->setTool({});
    canvas->choosingPlane = true;
    canvas->view("iso");
    properties->hide();
    canvas->update();
    status->setText("Selecione um plano ou uma face plana da peça, inclusive inclinada.");
}
void Window::finishSketch() {
    canvas->setTool({});
    canvas->sketchMode = false;
    canvas->choosingPlane = false;
    buildRibbon();
    canvas->view("iso");
    canvas->update();
}
void Window::sketchTool(const QString &type) {
    if (!canvas->sketchMode)
        startSketch();
    pendingSketchTool = type;
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
    auto availablePlanes = planes;
    if (canvas->plane.startsWith("FACE:"))
        availablePlanes.append({"Face selecionada", canvas->plane});
    f.choice("plane", "Plane", availablePlanes, canvas->plane);
    f.number("x", "X", 0);
    f.number("y", "Y", 0);
    f.number("offset", "Plane offset", canvas->planeOffset);
    f.number("w", "Rectangle width", 40, .001);
    f.number("h", "Rectangle height", 30, .001);
    f.number("r", "Circle radius", 10, .001);
    if (f.acceptForm()) {
        auto p = f.values();
        if (p["plane"].toString() == canvas->plane && !canvas->sketchSupport.feature.isEmpty()) {
            p["support"] = canvas->sketchSupport.feature;
            p["supportFace"] = canvas->sketchSupport.index;
            TopTools_IndexedMapOfShape faces;
            TopExp::MapShapes(model.get(canvas->sketchSupport.feature).shape, TopAbs_FACE, faces);
            p["supportFaceCount"] = faces.Extent();
        }
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
    const QString operation = revolve ? "revolve" : "extrude";
    const QString editing =
        !selected.isEmpty() && model.get(selected).type == operation ? selected : QString();
    const QJsonObject initial = editing.isEmpty() ? QJsonObject() : model.get(editing).p;
    const QString originalName = editing.isEmpty() ? QString() : model.get(editing).name;
    QList<QPair<QString, QString>> sketches, bodies = {{"New Body", ""}};
    for (auto &feature : model.features)
        if (!feature.inactive && feature.type == "sketch")
            sketches.append({feature.name, feature.id});
    for (auto i : model.bodies())
        if (model.features[i].id != editing && !model.isMesh(model.features[i].id))
            bodies.append({model.features[i].name, model.features[i].id});
    if (sketches.empty())
        throw std::runtime_error("Crie um perfil fechado antes de extrudar.");
    QString initialProfile = initial["source"].toString();
    if (editing.isEmpty() && !selected.isEmpty() && model.get(selected).type == "sketch")
        initialProfile = selected;
    sketches.prepend({"Select a profile…", ""});
    const auto originalTarget = initial["target"].toString();
    if (!originalTarget.isEmpty() && !bodies.contains({model.get(originalTarget).name, originalTarget}))
        bodies.append({model.get(originalTarget).name, originalTarget});
    Form panel(this, editing.isEmpty() ? (revolve ? "Revolve" : "Extrude")
                                       : (revolve ? "Edit Revolve" : "Edit Extrude"));
    panel.choice("source", "Profile", sketches, initialProfile);
    if (revolve) {
        panel.number("angle", "Angle", initial["angle"].toDouble(360), .01, 360, " °");
        panel.number("axis", "Axis position", initial["axis"].toDouble());
    } else
        panel.number("d", "Distance", initial["d"].toDouble(0));
    panel.choice("target", "Target body", bodies, originalTarget);
    panel.choice("mode", "Operation", {{"New Body / Join", "join"}, {"Cut", "cut"}},
                 initial["mode"].toString("join"));
    const auto bindings=initial["expressions"].toObject();
    const auto initialFormValues=panel.values();
    for(auto it=panel.nums.begin();it!=panel.nums.end();++it)if(bindings.contains(it.key())) {
        it.value()->setReadOnly(true);it.value()->setToolTip("Controlado por: "+bindings[it.key()].toString());
    }
    auto commandParameters=[&]{
        auto p=initial;const auto values=panel.values();
        for(auto it=values.begin();it!=values.end();++it) {
            if(bindings.contains(it.key()))continue;
            if(!editing.isEmpty() && !initial.contains(it.key()) && initialFormValues[it.key()]==it.value())continue;
            p[it.key()]=it.value();
        }
        return p;
    };
    auto chooseCutTarget = [&] {
        if (panel.combos["mode"]->currentData() == "cut" &&
            panel.combos["target"]->currentData().toString().isEmpty() && bodies.size() == 2)
            panel.combos["target"]->setCurrentIndex(1);
    };
    connect(panel.combos["mode"], qOverload<int>(&QComboBox::currentIndexChanged), &panel, chooseCutTarget);
    chooseCutTarget();
    panel.note(
        bindings.contains("d")
            ? "Distância controlada por fórmula. Use Link Dimension to Expression para editar ou remover o vínculo."
            : revolve
            ? "Selecione o perfil na área de desenho."
            : "Arraste a seta azul para definir a distância.\nClique em outro perfil para trocar a seleção.");
    auto *feedback = new QLabel;
    feedback->setWordWrap(true);
    feedback->setStyleSheet("color:#edc17e;font-size:11px;");
    panel.layout->addRow(feedback);
    Model preview = model;
    QString previousSelection = selected;
    canvas->sketchMode = false;
    canvas->choosingPlane = false;
    canvas->setTool({});
    canvas->view("iso");
    buildRibbon();
    properties->hide();
    activeCommand = &panel;
    auto updatePreview = [&] {
        try {
            const auto parameters = commandParameters();
            if (parameters["source"].toString().isEmpty()) {
                canvas->handleActive = false;
                canvas->selected = previousSelection;
                canvas->setModel(&model);
                feedback->setText("Selecione um perfil para extrudar. Nenhum corpo novo foi criado.");
                return;
            }
            const auto &sketch = model.get(parameters["source"].toString());
            Bnd_Box box;
            BRepBndLib::AddOptimal(sketch.shape, box);
            double x, y, z, X, Y, Z;
            box.Get(x, y, z, X, Y, Z);
            canvas->handleOrigin = QVector3D((x + X) / 2, (y + Y) / 2, (z + Z) / 2);
            canvas->handleAxis = Model::planeNormal(sketch.p["plane"].toString("XY"));
            canvas->handleDistance = parameters["d"].toDouble();
            canvas->handleActive = !revolve && !bindings.contains("d");
            if (!revolve && std::abs(canvas->handleDistance) < 1e-7) {
                canvas->selected = sketch.id;
                canvas->setModel(&model);
                feedback->setText(parameters["mode"] == "cut"
                    ? "Arraste a seta para dentro da peça. Se houver várias peças, clique no corpo a cortar ou escolha Target body."
                    : "Arraste a seta ou digite uma distância para iniciar a extrusão.");
                return;
            }
            if (parameters["mode"] == "cut" && parameters["target"].toString().isEmpty())
                throw std::runtime_error("Clique na peça a cortar ou escolha Target body.");
            preview = model;
            QString id = editing;
            if (editing.isEmpty())
                id = preview.add(operation, parameters, "Preview");
            else
                preview.edit(editing, parameters, originalName);
            canvas->selected = id;
            canvas->setModel(&preview);
            feedback->clear();
        } catch (const std::exception &error) {
            canvas->selected = panel.combos["source"]->currentData().toString();
            canvas->setModel(&model);
            feedback->setText(QString::fromUtf8(error.what()));
        } catch (const Standard_Failure &error) {
            canvas->selected = panel.combos["source"]->currentData().toString();
            canvas->setModel(&model);
            feedback->setText(QString::fromUtf8(error.GetMessageString()));
        }
    };
    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(35);
    connect(&debounce, &QTimer::timeout, &panel, updatePreview);
    // Throttle instead of restarting the timer: continuous dragging must render
    // intermediate solids, not wait until the pointer stops moving.
    auto schedulePreview = [&] {
        if (!debounce.isActive())
            debounce.start();
    };
    for (auto *spin : panel.nums)
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), &panel, schedulePreview);
    for (auto *combo : panel.combos)
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), &panel, schedulePreview);
    commandSelection = [&](QString id) {
        int index = panel.combos["source"]->findData(id);
        if (index >= 0)
            panel.combos["source"]->setCurrentIndex(index);
        else if (panel.combos["mode"]->currentData() == "cut") {
            int target = panel.combos["target"]->findData(id);
            if (target > 0)
                panel.combos["target"]->setCurrentIndex(target);
        }
    };
    canvas->onHandleDistance = [&](double distance) { if(!revolve && !bindings.contains("d"))panel.nums["d"]->setValue(distance); };
    panel.validate = [&] {
        const auto p = commandParameters();
        if (p["source"].toString().isEmpty() || (!revolve && std::abs(p["d"].toDouble()) < 1e-7))
            return true;
        try {
            Model check = model;
            if (editing.isEmpty()) check.add(operation, p);
            else check.edit(editing, p, originalName);
            return true;
        } catch (...) {
            updatePreview();
            return false;
        }
    };
    canvas->onCancelCommand = [&] { panel.reject(); };
    canvas->onAcceptCommand = [&] { panel.accept(); };
    QTimer::singleShot(0, &panel, updatePreview);
    bool accepted = panel.acceptForm();
    debounce.stop();
    activeCommand.clear();
    commandSelection = {};
    canvas->onHandleDistance = {};
    canvas->onCancelCommand = {};
    canvas->onAcceptCommand = {};
    canvas->handleActive = false;
    canvas->selected = previousSelection;
    canvas->setModel(&model);
    if (accepted) {
        const auto p = commandParameters();
        if (p["source"].toString().isEmpty() || (!revolve && std::abs(p["d"].toDouble()) < 1e-7)) {
            selected = previousSelection;
            refresh();
            return;
        }
        if (p["mode"] == "cut" && p["target"].toString().isEmpty())
            throw std::runtime_error("Escolha um corpo para o corte.");
        if (editing.isEmpty())
            selected = model.add(operation, p, revolve ? "Revolve" : "Extrude");
        else {
            selected = editing;
            if (p != initial)
                model.edit(editing, p, originalName);
        }
    } else
        selected = previousSelection;
    refresh();
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
    const auto items = canvas->selectedDetails;
    const bool subelements = canvas->hasSubselection();
    QStringList group;
    QString sketchPlane;
    for (const auto &item : items) {
        if (!group.contains(item.feature)) group.append(item.feature);
        if (subelements) {
            const auto &feature = model.get(item.feature);
            if (item.kind == "object" || feature.type != "sketch" ||
                (feature.p["profile"] != "rectangle" && feature.p["profile"] != "polyline"))
                throw std::runtime_error("Selecione apenas linhas ou vértices de sketches retos para editar. Não misture com corpos ou curvas.");
            auto plane = feature.p["plane"].toString("XY");
            if (!sketchPlane.isEmpty() && sketchPlane != plane)
                throw std::runtime_error("Mova subelementos de um mesmo plano por vez.");
            sketchPlane = plane;
        }
    }
    const bool grouped = subelements || group.size() > 1;
    QList<QPair<QString, QString>> bodies;
    for (auto i : model.bodies())
        bodies.append({model.features[i].name, model.features[i].id});
    if (grouped) {
        bodies.clear();
        for (const auto &id : group) {
            if (!subelements && (model.get(id).type == "sketch" || model.consumed(id)))
                throw std::runtime_error("Para mover peças juntas, selecione somente corpos finais.");
            bodies.append({model.get(id).name, id});
        }
    }
    if (bodies.empty())
        throw std::runtime_error("Crie um corpo primeiro.");
    Form panel(this, copy ? "Create Copy" : "Move / Copy");
    panel.choice("source", "Body", bodies, selected);
    if (grouped) {
        panel.combos["source"]->setEnabled(false);
        panel.note(subelements ? "Move os pontos selecionados e mantém as linhas conectadas. Movimento limitado ao plano do sketch."
                               : QString("%1 peças serão movidas/giradas juntas.").arg(group.size()));
    }
    panel.number("x", "X distance", copy ? 50 : 0);
    panel.number("y", "Y distance", 0);
    panel.number("z", "Z distance", 0);
    panel.choice("axis", "Rotation axis", {{"X", "X"}, {"Y", "Y"}, {"Z", "Z"}}, "Z");
    panel.number("angle", "Rotation", 0, -36000, 36000, " °");
    for (auto *spin : panel.nums)
        panel.layout->setRowVisible(spin, false);
    panel.layout->setRowVisible(panel.combos["axis"], false);
    auto *rotate = new QPushButton("Girar pelo mouse");
    rotate->setObjectName("rotateMode");
    rotate->setCheckable(true);
    rotate->setVisible(!subelements);
    panel.layout->addRow(rotate);
    connect(rotate, &QPushButton::toggled, &panel, [&](bool enabled) {
        canvas->rotationMode = enabled;
        panel.layout->setRowVisible(panel.combos["axis"], enabled);
        panel.layout->setRowVisible(panel.nums["angle"], enabled);
        rotate->setText(enabled ? "Voltar a mover" : "Girar pelo mouse");
        canvas->update();
        panel.adjustSize();
    });
    auto *precision = new QPushButton("Precise values / rotation ▸");
    precision->setCheckable(true);
    panel.layout->addRow(precision);
    connect(precision, &QPushButton::toggled, &panel, [&panel, precision](bool expanded) {
        for (auto *spin : panel.nums)
            panel.layout->setRowVisible(spin, expanded);
        panel.layout->setRowVisible(panel.combos["axis"], expanded);
        precision->setText(expanded ? "Precise values / rotation ▾" : "Precise values / rotation ▸");
        panel.adjustSize();
    });
    if (subelements) {
        rotate->setEnabled(false);
        panel.nums["angle"]->setEnabled(false);
        panel.combos["axis"]->setEnabled(false);
    }
    panel.note("Arraste a peça ou o centro das hastes para mover no plano da tela.\n"
               "Use as setas para restringir a X, Y ou Z. Enter confirma; Esc cancela.");
    auto *feedback = new QLabel;
    feedback->setWordWrap(true);
    feedback->setStyleSheet("color:#edc17e;font-size:11px;");
    panel.layout->addRow(feedback);
    Model preview = model;
    QString previousSelection = selected;
    properties->hide();
    canvas->setTool({});
    activeCommand = &panel;
    auto moveParameters = [&] {
        auto parameters = panel.values();
        Bnd_Box sourceBox;
        if (subelements) {
            for (const auto &item : items)
                for (auto point : item.geometry) sourceBox.Add(gp_Pnt(point.x(), point.y(), point.z()));
            auto delta = QVector3D(parameters["x"].toDouble(), parameters["y"].toDouble(), parameters["z"].toDouble());
            auto normal = Model::planeNormal(sketchPlane);
            delta -= normal * QVector3D::dotProduct(delta, normal);
            parameters["x"] = delta.x(); parameters["y"] = delta.y(); parameters["z"] = delta.z();
        } else if (grouped) {
            for (const auto &id : group) BRepBndLib::Add(model.get(id).shape, sourceBox);
        } else
            BRepBndLib::Add(model.get(parameters["source"].toString()).shape, sourceBox);
        double x, y, z, X, Y, Z;
        sourceBox.Get(x, y, z, X, Y, Z);
        parameters["px"] = (x + X) / 2;
        parameters["py"] = (y + Y) / 2;
        parameters["pz"] = (z + Z) / 2;
        return parameters;
    };
    auto applyMovement = [&](Model &work, QJsonObject parameters) {
        QStringList results;
        const auto sources = grouped ? group : QStringList{parameters["source"].toString()};
        for (const auto &source : sources) {
            if (subelements) {
                QVector<int> edges, vertices;
                for (const auto &item : items) if (item.feature == source) {
                    if (item.kind == "edge") edges.append(item.index);
                    if (item.kind == "vertex") vertices.append(item.index);
                }
                work.editSketchElements(source, edges, vertices,
                    {float(parameters["x"].toDouble()), float(parameters["y"].toDouble()), float(parameters["z"].toDouble())}, false);
                results.append(source);
            } else {
                parameters["source"] = source;
                results.append(work.add(copy ? "copy" : "transform", parameters, copy ? "Copy" : "Move"));
            }
        }
        return results;
    };
    auto updatePreview = [&] {
        try {
            auto parameters = moveParameters();
            preview = model;
            auto results = applyMovement(preview, parameters);
            QString id = results.last();
            Bnd_Box box;
            for (const auto &result : results) BRepBndLib::AddOptimal(preview.get(result).shape, box);
            double x, y, z, X, Y, Z;
            box.Get(x, y, z, X, Y, Z);
            canvas->moveDistances =
                QVector3D(parameters["x"].toDouble(), parameters["y"].toDouble(), parameters["z"].toDouble());
            canvas->handleOrigin = QVector3D((x + X) / 2, (y + Y) / 2, (z + Z) / 2) - canvas->moveDistances;
            canvas->moveHandleLength = std::max(10., std::max({X - x, Y - y, Z - z}) * .6);
            canvas->moveHandleActive = true;
            canvas->rotationAngle = parameters["angle"].toDouble();
            canvas->rotationAxis = parameters["axis"].toString();
            canvas->handleOrigin = QVector3D(parameters["px"].toDouble(), parameters["py"].toDouble(),
                                             parameters["pz"].toDouble());
            canvas->selected = id;
            canvas->setModel(&preview);
            canvas->selectedDetails.clear();
            for (const auto &result : results)
                canvas->selectedDetails.append({result, "object", -1, {}});
            feedback->clear();
        } catch (const std::exception &error) {
            feedback->setText(QString::fromUtf8(error.what()));
        } catch (const Standard_Failure &error) {
            feedback->setText(QString::fromUtf8(error.GetMessageString()));
        }
    };
    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(35);
    connect(&debounce, &QTimer::timeout, &panel, updatePreview);
    for (auto *spin : panel.nums)
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), &panel, [&] {
            if (!debounce.isActive())
                debounce.start();
        });
    for (auto *combo : panel.combos)
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), &panel, [&] {
            if (!debounce.isActive())
                debounce.start();
        });
    commandSelection = [&](QString id) {
        if (grouped) return;
        int index = panel.combos["source"]->findData(id);
        if (index >= 0)
            panel.combos["source"]->setCurrentIndex(index);
    };
    canvas->onMoveDistance = [&](int axis, double distance) {
        panel.nums[QString("xyz")[axis]]->setValue(distance);
    };
    canvas->onMoveTranslation = [&](QVector3D distances) {
        for (int axis = 0; axis < 3; ++axis)
            panel.nums[QString("xyz")[axis]]->setValue(distances[axis]);
    };
    canvas->onRotateAngle = [&](double angle) { panel.nums["angle"]->setValue(angle); };
    canvas->onCancelCommand = [&] { panel.reject(); };
    canvas->onAcceptCommand = [&] { panel.accept(); };
    QTimer::singleShot(0, &panel, updatePreview);
    bool accepted = panel.acceptForm();
    debounce.stop();
    activeCommand.clear();
    commandSelection = {};
    canvas->onMoveDistance = {};
    canvas->onMoveTranslation = {};
    canvas->onCancelCommand = {};
    canvas->onAcceptCommand = {};
    canvas->moveHandleActive = false;
    canvas->rotationMode = false;
    canvas->onRotateAngle = {};
    canvas->selected = previousSelection;
    canvas->setModel(&model);
    if (accepted) {
        auto parameters = moveParameters();
        if (copy || std::abs(parameters["x"].toDouble()) + std::abs(parameters["y"].toDouble()) +
                    std::abs(parameters["z"].toDouble()) + std::abs(parameters["angle"].toDouble()) > 1e-8) {
            Model work = model;
            selected = applyMovement(work, parameters).last();
            model.commit(work.json());
        }
    }
    else
        selected = previousSelection;
    refresh();
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
void Window::fillet(bool chamfer) {
    if (selected.isEmpty() || model.get(selected).type == "sketch" || model.isMesh(selected))
        throw std::runtime_error("Selecione arestas de um sólido CAD ou um corpo CAD inteiro.");
    QJsonArray edges;
    const auto previousSelection=selected;
    const auto previousDetails=canvas->selectedDetails;
    const auto previousDetail=canvas->selectedDetail;
    const QString operation=chamfer?"chamfer":"fillet", title=chamfer?"Chamfer":"Fillet";
    bool editing=model.get(selected).type==operation;
    for(const auto &item:canvas->selectedDetails)if(item.kind!="object")editing=false;
    const auto initial=editing?model.get(selected).p:QJsonObject{};
    for (const auto &item : canvas->selectedDetails) {
        if (item.feature != selected || (item.kind != "edge" && item.kind != "object"))
            throw std::runtime_error("Selecione arestas de um único corpo para aplicar o filete.");
        if (item.kind == "edge") edges.append(item.index);
    }
    if(editing)edges=initial["edges"].toArray();
    const QString source = editing?initial["source"].toString():selected;
    Form f(this, title);
    if(chamfer) {
        f.choice("mode","Type",{{"Equal distance","equal"},{"Two distances","two"},{"Distance and angle","angle"}},initial["mode"].toString("equal"));
        f.combos["mode"]->setObjectName("chamferMode");
        f.number("d","Distance",initial["d"].toDouble(1),.001);f.nums["d"]->setObjectName("chamferDistance");
        f.number("d2","Distance 2",initial["d2"].toDouble(1),.001);
        f.nums["d2"]->setObjectName("chamferDistance2");
        f.number("angle","Angle",initial["angle"].toDouble(45),.001,89.999," °");
        f.choice("side","Reference side",{{"First","first"},{"Second","second"}},initial["side"].toString("first"));
        auto fields=[&]{auto mode=f.combos["mode"]->currentData();f.nums["d2"]->setEnabled(mode=="two");
            f.nums["angle"]->setEnabled(mode=="angle");f.combos["side"]->setEnabled(mode!="equal");};
        connect(f.combos["mode"],qOverload<int>(&QComboBox::currentIndexChanged),&f,fields);fields();
    } else {f.number("r", "Radius", initial["r"].toDouble(1), .001);f.nums["r"]->setObjectName("filletRadius");}
    const auto initialValues=f.values();
    const auto bindings=initial["expressions"].toObject();
    for(auto it=f.nums.begin();it!=f.nums.end();++it)if(bindings.contains(it.key())) {
        it.value()->setReadOnly(true);it.value()->setToolTip("Controlado por: "+bindings[it.key()].toString());
    }
    bool allEdges=!chamfer && edges.empty();
    auto *choose=new QPushButton("Selecionar arestas");choose->setObjectName("chooseOperationEdges");choose->setCheckable(true);f.layout->addRow(choose);
    auto *count=new QLabel;count->setObjectName("operationEdgeCount");f.layout->addRow(count);
    f.note("Ative Selecionar arestas para ver o corpo original. Clique para substituir; Shift adiciona/remove. Desative para ver a prévia.");
    auto *feedback = new QLabel;
    feedback->setWordWrap(true);
    f.layout->addRow(feedback);
    auto parameters = [&] {
        auto p = initial;const auto values=f.values();
        for(auto it=values.begin();it!=values.end();++it) {
            if(bindings.contains(it.key()))continue;
            if(editing && initialValues[it.key()]==it.value())continue;
            p[it.key()]=it.value();
        }
        p["source"] = source;
        if(allEdges)p.remove("edges");else p["edges"] = edges;
        if(chamfer) {TopTools_IndexedMapOfShape topology;TopExp::MapShapes(model.get(source).shape,TopAbs_EDGE,topology);p["sourceEdgeCount"]=topology.Extent();}
        return p;
    };
    Model preview = model;
    Model selectionModel=model;
    if(editing)selectionModel.suppress(previousSelection,true);
    const auto previousFilter=canvas->selectionFilter;
    bool valid = false;
    auto updatePreview = [&] {
        valid = false;
        count->setText(allEdges?"Todas as arestas":QString("%1 aresta(s) selecionada(s)").arg(edges.size()));
        if(choose->isChecked()) {
            const auto details=canvas->selectedDetails;const auto detail=canvas->selectedDetail;
            canvas->selected=source;canvas->setModel(&selectionModel);
            canvas->selectedDetails=details;canvas->selectedDetail=detail;
            feedback->setText("Selecione arestas e desative Selecionar arestas para conferir o resultado.");
            return;
        }
        try {
            preview = model;
            QString id=previousSelection;
            if(editing)preview.edit(id,parameters(),model.get(id).name);
            else id=preview.add(operation, parameters(), title+" preview");
            canvas->selected = id;
            canvas->setModel(&preview);
            feedback->clear(); valid = true;
        } catch (const std::exception &error) {
            canvas->selected = source; canvas->setModel(&model);
            feedback->setText(QString::fromUtf8(error.what()));
        } catch (const Standard_Failure &) {
            canvas->selected = source; canvas->setModel(&model);
            feedback->setText("Não foi possível aplicar a operação. Reduza as medidas.");
        }
    };
    QTimer debounce;
    debounce.setSingleShot(true); debounce.setInterval(80);
    for(auto *input:f.nums)connect(input,qOverload<double>(&QDoubleSpinBox::valueChanged),&f,[&]{debounce.start();});
    for(auto *combo:f.combos)connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),&f,[&]{debounce.start();});
    connect(&debounce, &QTimer::timeout, &f, updatePreview);
    connect(choose,&QPushButton::toggled,&f,[&](bool choosing){
        canvas->commandSelectSubelements=choosing;canvas->selectionFilter=choosing?"edge":previousFilter;
        canvas->selectedDetails.clear();canvas->selectedDetail={};canvas->hoveredDetail={};
        if(choosing)for(auto edge:edges)canvas->selectedDetails.append({source,"edge",edge.toInt(),{}});
        updatePreview();
    });
    commandSelection=[&](QString){
        if(!choose->isChecked())return;
        QJsonArray picked;
        QVector<Viewport::SelectionTarget> details;
        for(const auto &item:canvas->selectedDetails) {
            if(item.feature!=source || item.kind!="edge")continue;
            if(!picked.contains(item.index)){picked.append(item.index);details.append(item);}
        }
        canvas->selectedDetails=details;canvas->selectedDetail=details.empty()?Viewport::SelectionTarget{}:details.back();
        edges=picked;allEdges=false;updatePreview();
    };
    f.validate = [&] { debounce.stop();choose->setChecked(false);updatePreview();return valid; };
    activeCommand = &f;
    canvas->onCancelCommand = [&] { f.reject(); };
    canvas->onAcceptCommand = [&] { f.accept(); };
    QTimer::singleShot(0, &f, updatePreview);
    bool accepted = f.acceptForm();
    debounce.stop(); activeCommand.clear();commandSelection={};
    canvas->commandSelectSubelements=false;canvas->selectionFilter=previousFilter;
    canvas->selectedDetails.clear();canvas->selectedDetail={};canvas->hoveredDetail={};
    canvas->onCancelCommand = {}; canvas->onAcceptCommand = {};
    canvas->selected = previousSelection; canvas->setModel(&model);
    if (accepted) {
        if(editing)model.edit(previousSelection,parameters(),model.get(previousSelection).name);
        else selected = model.add(operation, parameters(), title);
    }
    refresh();
    if(!accepted){canvas->selectedDetails=previousDetails;canvas->selectedDetail=previousDetail;canvas->update();}
}
void Window::measure() {
    auto shapeFor = [&](const Viewport::SelectionTarget &detail) {
        const auto &shape = model.get(detail.feature).shape;
        if (model.isMesh(detail.feature))
            throw std::runtime_error("Medição entre elementos exige geometria CAD, não STL.");
        if (detail.kind == "object") return shape;
        TopTools_IndexedMapOfShape elements;
        TopExp::MapShapes(shape, detail.kind == "face" ? TopAbs_FACE : detail.kind == "edge" ? TopAbs_EDGE : TopAbs_VERTEX, elements);
        if (detail.index < 0 || detail.index >= elements.Extent())
            throw std::runtime_error("Seleção inválida. Selecione novamente o elemento.");
        return TopoDS_Shape(elements(detail.index+1));
    };
    if (canvas->selectedDetails.size() == 2) {
        BRepExtrema_DistShapeShape distance(shapeFor(canvas->selectedDetails[0]), shapeFor(canvas->selectedDetails[1]));
        if (!distance.IsDone()) throw std::runtime_error("Não foi possível calcular a distância.");
        QMessageBox::information(this, "Measure", QString("Distância mínima: %1 mm").arg(distance.Value(), 0, 'f', 4));
        return;
    }
    if (canvas->selectedDetails.size() > 2)
        throw std::runtime_error("Selecione um elemento para medir, ou dois com Shift para medir a distância mínima.");
    if (canvas->selectedDetail.kind == "face") {
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(shapeFor(canvas->selectedDetail), properties);
        auto center = properties.CentreOfMass();
        QMessageBox::information(this, "Measure — Face", QString("Área: %1 mm²\nCentro: (%2, %3, %4) mm")
            .arg(properties.Mass(), 0, 'f', 4).arg(center.X(), 0, 'f', 3).arg(center.Y(), 0, 'f', 3).arg(center.Z(), 0, 'f', 3));
        return;
    }
    if (canvas->hasSubselection()) {
        const auto &detail = canvas->selectedDetail;
        if (detail.kind == "vertex" && !detail.geometry.empty()) {
            auto p = detail.geometry[0];
            QMessageBox::information(this, "Vértice",
                                     QString("X: %1 mm\nY: %2 mm\nZ: %3 mm")
                                         .arg(p.x(), 0, 'f', 3)
                                         .arg(p.y(), 0, 'f', 3)
                                         .arg(p.z(), 0, 'f', 3));
        } else {
            GProp_GProps properties;
            BRepGProp::LinearProperties(shapeFor(detail), properties);
            QMessageBox::information(this, "Aresta",
                                     QString("Comprimento: %1 mm").arg(properties.Mass(), 0, 'f', 4));
        }
        return;
    }
    if (selected.isEmpty())
        throw std::runtime_error("Selecione um objeto para medir.");
    auto &f = model.get(selected);
    Bnd_Box b;
    BRepBndLib::AddOptimal(f.shape, b);
    double x, y, z, X, Y, Z;
    b.Get(x, y, z, X, Y, Z);
    if (model.isMesh(f.id)) {
        QMessageBox::information(
            this, "Measure — STL",
            QString("%1\n\nX: %2 mm\nY: %3 mm\nZ: %4 mm\n\nVolume não calculado para malhas.")
                .arg(f.name)
                .arg(X - x, 0, 'f', 3)
                .arg(Y - y, 0, 'f', 3)
                .arg(Z - z, 0, 'f', 3));
        return;
    }
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
    clearRecovery();
    refresh();
}
bool Window::canLeave() {
    if (activeCommand) {
        activeCommand->raise();
        return false;
    }
    for (auto *dialog : findChildren<QDialog *>())
        if (dialog->isVisible()) {
            dialog->raise();
            return false;
        }
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
    return true;
}
void Window::open() {
    if (!canLeave())
        return;
    auto path = QFileDialog::getOpenFileName(
        this, "Open Design", {}, "MecaCAD / STL (*.mcad *.stl *.STL);;MecaCAD (*.mcad);;STL (*.stl *.STL)");
    if (!path.isEmpty())
        openPath(path);
}
void Window::openPath(const QString &path) {
    if (QFileInfo(path).suffix().compare("stl", Qt::CaseInsensitive) == 0) {
        Model imported;
        auto id = imported.importStl(path);
        model = std::move(imported);
        clearRecovery();
        selected = id;
        finishSketch();
        refresh(true);
        return;
    }
    model.load(path);
    clearRecovery();
    selected.clear();
    finishSketch();
    refresh(true);
}
void Window::autosave() {
    if (!recovery)
        return;
    if (!model.dirty) { clearRecovery(); return; }
    try {
        recovery->write(model);
        recoveryStatus->setText("Recuperação: "+QTime::currentTime().toString("HH:mm:ss"));
        recoveryStatus->setToolTip("Cópia automática local a cada 30 segundos. O arquivo original não foi alterado.");
    } catch (const std::exception &error) {
        recoveryStatus->setText("Falha na recuperação automática");
        recoveryStatus->setToolTip(QString::fromUtf8(error.what()));
    }
}
void Window::clearRecovery() {
    if (!recovery) return;
    try {
        recovery->clear();
        recoveryStatus->clear();recoveryStatus->setToolTip({});
    } catch (const std::exception &error) {
        recoveryStatus->setText("Recuperação antiga preservada");
        recoveryStatus->setToolTip(QString::fromUtf8(error.what()));
    }
}
void Window::recoverProject() {
    if (!recovery) throw std::runtime_error("A recuperação automática não está disponível nesta sessão.");
    const auto scan = recovery->scan();
    const auto entries = scan.entries;
    if(!scan.warnings.empty())
        QMessageBox::warning(this,"Recuperações preservadas com erro",
            "Estas cópias não puderam ser lidas e NÃO foram apagadas:\n\n"+scan.warnings.join('\n'));
    if (entries.empty()) {
        const auto legacy = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/recovery.mcad";
        QMessageBox::information(this, "Recover project", QFile::exists(legacy)
            ? "Há uma recuperação de versão antiga, preservada sem alteração. Use File → Open para abrir:\n" + legacy
            : "Não há recuperações disponíveis de sessões encerradas.");
        return;
    }
    QStringList labels;
    for (const auto &entry : entries)
        labels << QString("%1 — %2 — %3").arg(entry.originalPath.isEmpty() ? "Untitled" : entry.originalPath,
            entry.savedAt.toLocalTime().toString("dd/MM/yyyy HH:mm:ss"), entry.id.left(8));
    bool chosen = false;
    const auto label = QInputDialog::getItem(this, "Recover project",
        "Escolha a cópia. O original não será sobrescrito; a recuperação será um documento não salvo.",
        labels, 0, false, &chosen);
    if (!chosen || !canLeave()) return;
    Model restored;
    const auto original = recovery->recover(entries[labels.indexOf(label)].id, restored);
    model = std::move(restored);
    selected.clear();
    finishSketch();
    refresh(true);
    status->setText(original.isEmpty() ? "Projeto não salvo recuperado. Use Save As para salvar."
                                     : "Cópia recuperada de " + original + ". Use Save As; o original foi preservado.");
}
void Window::closeEvent(QCloseEvent *e) {
    if (activeCommand) {
        activeCommand->reject();
        e->ignore();
        return;
    }
    for (auto *dialog : findChildren<QDialog *>())
        if (dialog->isVisible()) {
            dialog->reject();
            e->ignore();
            return;
        }
    bool ok = false;
    run([&] { ok = canLeave(); });
    if (ok) {
        clearRecovery();
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
    clearRecovery();
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
