#pragma once
#include "model.h"
#include "viewport.h"
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QTabBar>
#include <QTimer>
#include <QTreeWidget>
#include <functional>

class Window : public QMainWindow {
    Q_OBJECT
  public:
    explicit Window();
    void openPath(const QString &path);
    void demo();
    Model model;

  protected:
    void closeEvent(QCloseEvent *) override;

  private:
    Viewport *canvas;
    QTreeWidget *tree;
    QListWidget *timeline;
    QDockWidget *properties;
    QWidget *propertyBody;
    QFormLayout *propertyForm;
    QLineEdit *featureName;
    QTabBar *tabs;
    QWidget *ribbon;
    QLabel *documentTitle, *status;
    QString selected, recoveryPath;
    QMap<QString, QDoubleSpinBox *> fields;
    QMap<QString, QAction *> commands;
    bool refreshing = false;
    void run(const std::function<void()> &fn);
    void refresh(bool fit = false);
    void select(const QString &id);
    void buildRibbon();
    void buildProperties();
    void applyProperties();
    void primitive(const QString &type);
    void startSketch();
    void finishSketch();
    void sketchTool(const QString &type);
    void exactSketch();
    void extrude(bool revolve = false);
    void booleanOp(const QString &mode);
    void transform(bool copy);
    void hole();
    void fillet();
    void measure();
    void exportFile(const QString &format);
    void save(bool as = false);
    void open();
    bool canLeave();
    void autosave();
    void search();
    QAction *command(QString key, QString label, QString shortcut, std::function<void()> fn);
};
