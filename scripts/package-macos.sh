#!/bin/sh
set -eu
# Disabled at the user's request: versioned bundles and staging copies filled disk.
# Do not restore packaging without new explicit authorization. Development and
# tests reuse build/; the current dist/MecaCAD-0.2.25.app must remain intact.
echo 'Empacotamento desativado pelo usuário para economizar disco.' >&2
echo 'Reutilize build/ para compilar e testar. Novos pacotes exigem autorização.' >&2
exit 2
