#!/bin/sh
set -eu
project_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$project_dir"
qt_prefix="$(brew --prefix qtbase)"
cmake --install build --prefix dist
"$qt_prefix/bin/macdeployqt" dist/MecaCAD.app -always-overwrite -verbose=1
for framework in QtCore QtDBus QtGui QtOpenGL QtOpenGLWidgets QtWidgets; do
    binary="dist/MecaCAD.app/Contents/Frameworks/$framework.framework/Versions/A/$framework"
    if test -f "$binary"; then
        install_name_tool -id "@rpath/$framework.framework/Versions/A/$framework" "$binary"
    fi
done
codesign --force --deep --sign - dist/MecaCAD.app
codesign --verify --deep --strict dist/MecaCAD.app
ditto -c -k --sequesterRsrc --keepParent dist/MecaCAD.app dist/MecaCAD-0.1-macOS-arm64.zip
