#!/usr/bin/env bash
# Кросс-сборка FolderView.exe (windows-amd64) в Linux-контейнере
# с помощью clang-cl + lld-link + llvm-rc и Windows SDK/CRT из xwin.
set -euo pipefail

# --- Параметры (через переменные окружения) -------------------------------
CONFIGURATION="${CONFIGURATION:-Release}"   # Release | Debug
VERSION="${VERSION:-}"                       # например 1.2.3
ZIP_NAME="${ZIP_NAME:-}"

SRC_ROOT="${SRC_ROOT:-/src/FolderView}"
PROJECT_DIR="$SRC_ROOT/FolderView"
TEST_DIR="${TEST_DIR:-$SRC_ROOT/tests}"
OUT_DIR="${OUT_DIR:-/out}"
BUILD_DIR="${BUILD_DIR:-/tmp/folderview-build}"
LICENSE_FILE="${LICENSE_FILE:-$SRC_ROOT/../LICENSE}"
NOTICES_FILE="${NOTICES_FILE:-$SRC_ROOT/../THIRD-PARTY-NOTICES.txt}"

XWIN_DIR="${XWIN_DIR:-/xwin}"
ZLIB_DIR="${ZLIB_DIR:-/opt/zlib}"
TARGET="x86_64-pc-windows-msvc"

# --- Подготовка каталогов --------------------------------------------------
rm -rf "$BUILD_DIR" "$OUT_DIR"
mkdir -p "$BUILD_DIR" "$OUT_DIR"

# --- Нативные C-регрессии ---------------------------------------------------
# Платформонезависимые части (разбор ZIP из памяти, расчёты декодирования и
# кэша вьювера) проверяются нативно до Windows-сборки. UBSan в trap-режиме
# не требует рантайма: UB (переполнения, сдвиги, выход за границы массивов)
# роняет тест.
NATIVE_TEST_FLAGS=(
    -std=c17 -Wall -Wextra -Werror -pedantic
    -fsanitize=undefined -fsanitize-trap=undefined
    -I "$PROJECT_DIR"
)

echo "[build] native ZipFormat regression tests"
clang "${NATIVE_TEST_FLAGS[@]}" \
    "$PROJECT_DIR/ZipFormat.c" "$TEST_DIR/ZipFormatTests.c" \
    -o "$BUILD_DIR/zip-format-tests"
"$BUILD_DIR/zip-format-tests"

echo "[build] native ViewerPlan regression tests"
clang "${NATIVE_TEST_FLAGS[@]}" \
    "$PROJECT_DIR/ViewerPlan.c" "$TEST_DIR/ViewerPlanTests.c" \
    -o "$BUILD_DIR/viewer-plan-tests"
"$BUILD_DIR/viewer-plan-tests"

# --- Рабочая копия FolderView.rc + прошивка версии --------------------------
# Исходный ресурс хранится в UTF-8. Версию штампуем в рабочей копии, чтобы не
# изменять файл в дереве исходников.
RC_SRC="$PROJECT_DIR/FolderView.rc"
RC_WORK="$BUILD_DIR/FolderView.rc"
cp "$RC_SRC" "$RC_WORK"

if [ -n "${VERSION// /}" ]; then
    echo "[build] Stamping version $VERSION into FolderView.rc"
    VERSION="$VERSION" python3 - "$RC_WORK" <<'PY'
import os, re, sys

path = sys.argv[1]
version = os.environ["VERSION"].strip()

m = re.match(r"^(\d+)\.(\d+)\.(\d+)", version)
if not m:
    raise SystemExit(f"Version '{version}' must start with major.minor.patch")
file_version = "{0},{1},{2},0".format(*m.groups())

with open(path, "r", encoding="utf-8") as f:
    text = f.read()

text = re.sub(r"FILEVERSION\s+\d+,\d+,\d+,\d+", f"FILEVERSION {file_version}", text)
text = re.sub(r"PRODUCTVERSION\s+\d+,\d+,\d+,\d+", f"PRODUCTVERSION {file_version}", text)
text = re.sub(r'VALUE "FileVersion", "[^"]*\\0"', f'VALUE "FileVersion", "{version}\\\\0"', text)
text = re.sub(r'VALUE "ProductVersion", "[^"]*\\0"', f'VALUE "ProductVersion", "{version}\\\\0"', text)

with open(path, "w", encoding="utf-8") as f:
    f.write(text)
PY
fi

# --- Флаги компиляции ------------------------------------------------------
INCLUDE_FLAGS=(
    /imsvc "$XWIN_DIR/crt/include"
    /imsvc "$XWIN_DIR/sdk/include/ucrt"
    /imsvc "$XWIN_DIR/sdk/include/um"
    /imsvc "$XWIN_DIR/sdk/include/shared"
    /imsvc "$XWIN_DIR/sdk/include/winrt"
    /I "$ZLIB_DIR/include"
)

COMMON_DEFS=(/DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE /DZ_SOLO /DNO_GZIP)

if [ "$CONFIGURATION" = "Debug" ]; then
    CONFIG_FLAGS=(/Od /D_DEBUG /Z7)
else
    CONFIG_FLAGS=(/O2 /DNDEBUG)
fi

CL_FLAGS=(
    "--target=$TARGET"
    /nologo /c /TC /Zl /GS- /std:c17 /utf-8 /W3
    /clang:-ffreestanding
    "${CONFIG_FLAGS[@]}"
    "${COMMON_DEFS[@]}"
    "${INCLUDE_FLAGS[@]}"
)

# --- Компиляция исходников -------------------------------------------------
SOURCES=(
    FolderView.c
    WicDecoder.c
    ViewerPlan.c
    ZipArchive.c
    ZipFormat.c
    Trace.c
    NoCrt.c
)

OBJS=()
for src in "${SOURCES[@]}"; do
    obj="$BUILD_DIR/${src%.*}.obj"
    echo "[build] clang-cl $src"
    clang-cl "${CL_FLAGS[@]}" "$PROJECT_DIR/$src" "/Fo$obj"
    OBJS+=("$obj")
done

# --- Компиляция ресурсов ---------------------------------------------------
# llvm-rc не запускает C-препроцессор для #include, поэтому препроцессируем
# через clang (с RC_INVOKED), затем компилируем результат в .res.
echo "[build] preprocess + llvm-rc FolderView.rc"
RC_PP="$BUILD_DIR/FolderView.pp.rc"

clang --target="$TARGET" -DRC_INVOKED -E -P -xc \
    -fms-compatibility -fms-extensions \
    -I "$PROJECT_DIR" \
    -isystem "$XWIN_DIR/crt/include" \
    -isystem "$XWIN_DIR/sdk/include/ucrt" \
    -isystem "$XWIN_DIR/sdk/include/um" \
    -isystem "$XWIN_DIR/sdk/include/shared" \
    "$RC_WORK" -o "$RC_PP"

llvm-rc /nologo /C 65001 /I "$PROJECT_DIR" /fo "$BUILD_DIR/FolderView.res" --no-preprocess "$RC_PP"

# --- Линковка --------------------------------------------------------------
echo "[build] lld-link FolderView.exe (no CRT)"
LINK_LIBS=(
    kernel32.lib user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib
    uuid.lib comdlg32.lib comctl32.lib advapi32.lib shlwapi.lib
    windowscodecs.lib d2d1.lib
    "$ZLIB_DIR/zlibstatic.lib"
)

lld-link /nologo /subsystem:windows /machine:x64 /entry:FolderViewEntry \
    /nodefaultlib /opt:ref /opt:icf /dynamicbase /nxcompat /highentropyva \
    "/libpath:$XWIN_DIR/crt/lib/x86_64" \
    "/libpath:$XWIN_DIR/sdk/lib/ucrt/x86_64" \
    "/libpath:$XWIN_DIR/sdk/lib/um/x86_64" \
    "/out:$OUT_DIR/FolderView.exe" \
    "${OBJS[@]}" \
    "$BUILD_DIR/FolderView.res" \
    "${LINK_LIBS[@]}"

# Проверяем не только состав ZIP, но и таблицу импортов PE.
IMPORTS="$(llvm-readobj --coff-imports "$OUT_DIR/FolderView.exe")"
echo "$IMPORTS"
for forbidden_import in msvcp vcruntime ucrtbase api-ms-win-crt zlib1; do
    if grep -qi "$forbidden_import" <<<"$IMPORTS"; then
        echo "[build] ERROR: executable imports forbidden runtime: $forbidden_import" >&2
        exit 1
    fi
done

# --- Упаковка --------------------------------------------------------------
if [ -z "${ZIP_NAME// /}" ]; then
    if [ -n "${VERSION// /}" ]; then
        ZIP_NAME="FolderView-${VERSION}-win-x64.zip"
    else
        ZIP_NAME="FolderView-win-x64.zip"
    fi
fi

STAGE_DIR="$BUILD_DIR/package"
mkdir -p "$STAGE_DIR"
cp "$LICENSE_FILE" "$OUT_DIR/LICENSE"
cp "$NOTICES_FILE" "$OUT_DIR/THIRD-PARTY-NOTICES.txt"
cp "$OUT_DIR/FolderView.exe" \
    "$OUT_DIR/LICENSE" \
    "$OUT_DIR/THIRD-PARTY-NOTICES.txt" \
    "$STAGE_DIR/"

ZIP_PATH="$OUT_DIR/$ZIP_NAME"
( cd "$STAGE_DIR" && zip -q -X "$ZIP_PATH" \
    FolderView.exe LICENSE THIRD-PARTY-NOTICES.txt )

# Контроль содержимого пакета
for required in FolderView.exe LICENSE THIRD-PARTY-NOTICES.txt; do
    if ! unzip -Z1 "$ZIP_PATH" | grep -Fxq "$required"; then
        echo "[build] ERROR: package is missing required file: $required" >&2
        exit 1
    fi
done
for forbidden in FolderView.pdb msvcp140.dll vcruntime140.dll vcruntime140_1.dll zlib1.dll; do
    if unzip -l "$ZIP_PATH" | grep -q "$forbidden"; then
        echo "[build] ERROR: package contains forbidden file: $forbidden" >&2
        exit 1
    fi
done

# SHA256
HASH="$(cd "$OUT_DIR" && sha256sum "$ZIP_NAME" | cut -d' ' -f1)"
printf '%s  %s\n' "$HASH" "$ZIP_NAME" > "$ZIP_PATH.sha256"

echo "[build] Done: $ZIP_PATH"
echo "[build] SHA256: $HASH"
