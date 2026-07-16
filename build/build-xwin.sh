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
OUT_DIR="${OUT_DIR:-/out}"
BUILD_DIR="${BUILD_DIR:-/tmp/folderview-build}"

XWIN_DIR="${XWIN_DIR:-/xwin}"
ZLIB_DIR="${ZLIB_DIR:-/opt/zlib}"
TARGET="x86_64-pc-windows-msvc"

# --- Подготовка каталогов --------------------------------------------------
rm -rf "$BUILD_DIR" "$OUT_DIR"
mkdir -p "$BUILD_DIR" "$OUT_DIR"

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
    ZipArchive.c
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
cp "$OUT_DIR/FolderView.exe" "$STAGE_DIR/"

ZIP_PATH="$OUT_DIR/$ZIP_NAME"
( cd "$STAGE_DIR" && zip -q -X "$ZIP_PATH" FolderView.exe )

# Контроль содержимого пакета
if ! unzip -l "$ZIP_PATH" | grep -q "FolderView.exe"; then
    echo "[build] ERROR: package is missing FolderView.exe" >&2
    exit 1
fi
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
