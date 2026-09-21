#!/bin/sh
# Focused, windowless checks by default. GUI suites require an explicit mode.
set -eu
project_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$project_dir"
mode="${1:-core}"
if test "$#" -gt 0; then shift; fi
case "$mode" in
    core)
        cmake --build build --target core_tests -j "${MECACAD_BUILD_JOBS:-6}"
        if test "$#" -eq 0; then
            set -- rejectedDocumentsPreserveSession noOpCommandsPreserveHistory \
                failedSaveAndParsePreserveSession legacyV1FixtureRoundTrip invalidOperationModesAreAtomic \
                invalidLoadIsAtomic atomicBatchEdit rollbackAndDependencies associativeFaceSketchCut
        fi
        cd build
        exec ./core_tests "$@"
        ;;
    recovery)
        cmake --build build --target recovery_tests -j "${MECACAD_BUILD_JOBS:-6}"
        cd build
        exec ./recovery_tests "$@"
        ;;
    ui)
        if test "$#" -eq 0; then
            echo 'Informe os testes de interface a executar. Para a suíte completa, use release.' >&2
            exit 2
        fi
        echo 'Estes testes de interface abrirão janelas.' >&2
        cmake --build build --target ui_tests -j "${MECACAD_BUILD_JOBS:-6}"
        cd build
        exec ./ui_tests "$@"
        ;;
    release)
        if test "$#" -ne 0; then echo 'release não aceita filtros.' >&2; exit 2; fi
        echo 'Validação de entrega: as suítes completas, incluindo janelas de interface, serão executadas.' >&2
        cmake --build build -j "${MECACAD_BUILD_JOBS:-6}"
        exec ctest --test-dir build --output-on-failure --timeout 120
        ;;
    *)
        echo 'Uso: sh scripts/check.sh [core [teste... ] | recovery [teste... ] | ui teste... | release]' >&2
        exit 2
        ;;
esac
