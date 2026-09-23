#include "window.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    app.setOrganizationName("Mecad");
    app.setApplicationName("Mecad");
    app.setApplicationVersion("0.2.25");
    Window window;
    // macOS delivers Finder double-clicks as QFileOpenEvent to QApplication,
    // not directly to the main window. Let Window route that event to openPath.
    app.installEventFilter(&window);
    window.show();
    auto args = app.arguments();
    if (args.contains("--demo"))
        window.demo();
    int example = args.indexOf("--export-example");
    if (example >= 0 && example + 1 < args.size()) {
        QDir dir(args[example + 1]);
        dir.mkpath(".");
        window.model.save(dir.filePath("Mounting-bracket.mcad"));
        window.model.exportStep(dir.filePath("Mounting-bracket.step"));
        window.model.exportStl(dir.filePath("Mounting-bracket.stl"));
        if (!window.model.features.empty())
            window.model.exportDxf(dir.filePath("Base-profile.dxf"), window.model.features.front().id);
    }
    for (auto arg : args)
        if (arg.endsWith(".mcad", Qt::CaseInsensitive) || arg.endsWith(".stl", Qt::CaseInsensitive) ||
            arg.endsWith(".step", Qt::CaseInsensitive) || arg.endsWith(".stp", Qt::CaseInsensitive)) {
            try {
                window.openPath(arg);
            } catch (const std::exception &e) {
                qWarning() << e.what();
            }
        }
    int index = args.indexOf("--screenshot");
    if (index >= 0 && index + 1 < args.size())
        QTimer::singleShot(1800, &window, [&window, &app, args, index] {
            window.grab().save(args[index + 1]);
            window.model.dirty = false;
            app.quit();
        });
    return app.exec();
}
