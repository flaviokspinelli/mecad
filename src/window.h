#pragma once
#include "model.h"
#include "viewport.h"
#include "recovery.h"
#include <QDialog>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QTabBar>
#include <QTimer>
#include <QTreeWidget>
#include <functional>

class Window : public QMainWindow {
    Q_OBJECT
  public:
    explicit Window(QString recoveryDirectory = {}, bool promptRecovery = true, QString preferencesFile = {});
    void openPath(const QString &path);
    void demo();
    Model model;

  protected:
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

  private:
    Viewport *canvas;
    QTreeWidget *tree;
    QListWidget *timeline;
    QDockWidget *properties;
    QDockWidget *browser;
    QWidget *navigation;
    QWidget *propertyBody;
    QFormLayout *propertyForm;
    QLineEdit *featureName;
    QTabBar *tabs;
    QWidget *ribbon;
    QLabel *documentTitle, *status, *recoveryStatus;
    QString selected;
    std::unique_ptr<RecoveryStore> recovery;
    QMap<QString, QDoubleSpinBox *> fields;
    QMap<QString, QAction *> commands;
    bool refreshing = false;
    std::unique_ptr<QSettings> preferences;
    void savePreference(const QString &key, bool value);
    QMap<QString,QString> defaultShortcuts;
    QString validateShortcuts(const QMap<QString,QString> &values) const;
    void configureShortcuts();
    void showHelp();
    QPointer<QDialog> activeCommand;
    std::function<void(QString)> commandSelection;
    QString pendingSketchTool = "rectangle";
    void run(const std::function<void()> &fn);
    void refresh(bool fit = false);
    void select(const QString &id);
    void buildRibbon();
    void positionPanels();
    void buildProperties();
    void applyProperties();
    void primitive(const QString &type);
    void startSketch();
    void finishSketch();
    void sketchTool(const QString &type);
    void exactSketch();
    bool checkSketchConstraint(const QString &owner, sketch::Relation relation,
                               const QString &first, const QString &second, QPointF value);
    void extrude(bool revolve = false);
    void booleanOp(const QString &mode);
    void transform(bool copy);
    void hole();
    void fillet(bool chamfer = false);
    void measure();
    void exportFile(const QString &format);
    void save(bool as = false);
    void open();
    bool canLeave();
    void autosave();
    void clearRecovery();
    void recoverProject();
    void search();
    QAction *command(QString key, QString label, QString shortcut, std::function<void()> fn);
};
