#!/bin/bash
# windows-install.sh - Install build dependencies for eepp on Windows (MinGW)
# Requires: scoop (https://scoop.sh)
# Usage: bash projects/mingw32/windows-install.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=== eepp Windows Build Dependencies ==="
echo ""

# -------------------------------------------------------
# 1. Check/install scoop packages
# -------------------------------------------------------
check_and_install() {
    local pkg="$1"
    if command -v "$pkg" &> /dev/null; then
        echo "[OK] $pkg found: $(command -v "$pkg")"
    else
        echo "[INSTALL] $pkg not found, installing via scoop..."
        scoop install "$pkg" || { echo "ERROR: Failed to install $pkg"; exit 1; }
    fi
}

echo "--- Checking tools ---"
check_and_install gcc
check_and_install g++
check_and_install premake5
check_and_install ninja

echo ""
echo "Tool versions:"
echo "  gcc:      $(gcc --version 2>&1 | head -1)"
echo "  premake5: $(premake5 --version 2>&1 | grep premake5)"
echo "  ninja:    $(ninja --version 2>&1)"
echo ""

# -------------------------------------------------------
# 2. Initialize git submodules
# -------------------------------------------------------
echo "--- Initializing git submodules ---"
cd "$PROJECT_ROOT"

SUBMODULES_MISSING=0
for dir in premake/premake-cmake premake/premake-ninja src/thirdparty/SOIL2 src/thirdparty/efsw; do
    if [ ! -f "$dir/.git" ] && [ -z "$(ls -A "$dir" 2>/dev/null)" ]; then
        SUBMODULES_MISSING=1
        break
    fi
done

if [ "$SUBMODULES_MISSING" -eq 1 ]; then
    echo "Submodules missing, initializing..."
    git submodule update --init --recursive
    echo "[OK] Submodules initialized"
else
    echo "[OK] Submodules already present"
fi

# -------------------------------------------------------
# 3. Fix MinGW 15.x missing sec_api/wconio_s.h
# -------------------------------------------------------
echo ""
echo "--- Checking MinGW headers ---"

GCC_PATH="$(which gcc)"
MINGW_BASE="$(dirname "$GCC_PATH")/.."
MINGW_INCLUDE="$MINGW_BASE/x86_64-w64-mingw32/include"
SEC_API_DIR="$MINGW_INCLUDE/sec_api"
WCONIO_HEADER="$SEC_API_DIR/wconio_s.h"

if [ -f "$WCONIO_HEADER" ]; then
    echo "[OK] sec_api/wconio_s.h exists"
else
    echo "[FIX] Creating missing sec_api/wconio_s.h..."
    mkdir -p "$SEC_API_DIR"
    cat > "$WCONIO_HEADER" << 'HEADER'
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _INC_WCONIO_S
#define _INC_WCONIO_S

#include <conio.h>

#if defined(__LIBMSVCRT__)
#define _SECIMP
#else
#ifndef _SECIMP
#define _SECIMP __declspec(dllimport)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WCONIO_S_DEFINED
#define _WCONIO_S_DEFINED
  _SECIMP errno_t __cdecl _cgetws_s(wchar_t *_Buffer, size_t _SizeInWords, size_t *_SizeRead);
  _SECIMP int __cdecl _cwprintf_s(const wchar_t *_Format, ...);
  _SECIMP int __cdecl _vcwprintf_s(const wchar_t *_Format, va_list _ArgList);
  _SECIMP int __cdecl _cwprintf_s_l(const wchar_t *_Format, _locale_t _Locale, ...);
  _SECIMP int __cdecl _vcwprintf_s_l(const wchar_t *_Format, _locale_t _Locale, va_list _ArgList);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _INC_WCONIO_S */
HEADER
    echo "[OK] sec_api/wconio_s.h created"
fi

# -------------------------------------------------------
# 4. Build MinGW 15.x compat object (fseeko64/ftello64)
# -------------------------------------------------------
echo ""
echo "--- Building MinGW 15.x compatibility shim ---"

COMPAT_SRC="$PROJECT_ROOT/mingw15_compat.c"
COMPAT_OBJ="$PROJECT_ROOT/mingw15_compat.o"

GCC_MAJOR="$(gcc -dumpversion | cut -d. -f1)"
if [ "$GCC_MAJOR" -ge 15 ]; then
    cat > "$COMPAT_SRC" << 'COMPAT'
/* MinGW 15.x compatibility shim
 * Provides __imp_ (dllimport) wrappers for functions that are no longer
 * exported from msvcrt in MinGW 15.x but are still referenced via dllimport. */
#include <stdio.h>

/* fseeko64/ftello64: defined in libmingwex.a as static symbols,
 * but code compiled against older headers expects __imp_ versions. */
int fseeko64(FILE *stream, long long offset, int whence);
long long ftello64(FILE *stream);

int (*__imp_fseeko64)(FILE*, long long, int) = fseeko64;
long long (*__imp_ftello64)(FILE*) = ftello64;

/* _setjmp: msvcrt exports __intrinsic_setjmp but not _setjmp.
 * Some libraries (freetype, libpng) reference __imp__setjmp. */
typedef long long SETJMP_FLOAT128[2];
int __intrinsic_setjmp(SETJMP_FLOAT128 *, void *);
int _setjmp(SETJMP_FLOAT128 *buf, void *frame) {
    return __intrinsic_setjmp(buf, frame);
}
void *__imp__setjmp = (void*)_setjmp;
COMPAT
    gcc -c -m64 -O2 -o "$COMPAT_OBJ" "$COMPAT_SRC" 2>/dev/null
    echo "[OK] mingw15_compat.o built (GCC $GCC_MAJOR.x fseeko64/ftello64 fix)"
else
    echo "[SKIP] GCC $GCC_MAJOR.x - no compat shim needed"
fi

echo ""
echo "=== Installation complete ==="
echo "Run 'bash projects/mingw32/windows-build.sh' to build."
