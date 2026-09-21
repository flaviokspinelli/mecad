#!/bin/sh
set -eu
project_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$project_dir"
destination="dist/MecaCAD-0.2.25.app"
test ! -e "$destination" || { echo "Package already exists: $destination"; exit 1; }
# Shipping a consolidated app requires the full validation gate.
sh "$project_dir/scripts/check.sh" release
qt_prefix="$(brew --prefix qtbase)"
package_dir="$(mktemp -d "$project_dir/build/package-0.2.XXXXXX")"
cmake --install build --prefix "$package_dir"
bundle="$package_dir/MecaCAD.app"
"$qt_prefix/bin/macdeployqt" "$bundle" -always-overwrite -verbose=1
for framework in QtCore QtDBus QtGui QtOpenGL QtOpenGLWidgets QtWidgets; do
    binary="$bundle/Contents/Frameworks/$framework.framework/Versions/A/$framework"
    if test -f "$binary"; then
        install_name_tool -id "@rpath/$framework.framework/Versions/A/$framework" "$binary"
    fi
done
codesign --force --deep --sign - "$bundle"
codesign --verify --deep --strict "$bundle"
# Keep prior packages intact, including an app that may currently be running.
test ! -e "$destination" || { echo "Package already exists: $destination"; exit 1; }
mkdir -p dist
ditto "$bundle" "$destination"
