#!/bin/bash
# windows-build.sh - Build eepp on Windows with MinGW + Ninja
# Usage: bash projects/mingw32/windows-build.sh [options]
#   --debug           Build debug configuration
#   --target=NAME     Build specific target (default: ecode)
#   --all             Build all release targets
#   --jobs=N          Parallel jobs (default: nproc)
#   --clean           Clean build artifacts before building
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/make/windows"

CONFIG="release"
TARGET="ecode"
BUILD_ALL=0
JOBS="$(nproc)"
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        --debug)       CONFIG="debug" ;;
        --target=*)    TARGET="${arg#*=}" ;;
        --all)         BUILD_ALL=1 ;;
        --jobs=*)      JOBS="${arg#*=}" ;;
        --clean)       CLEAN=1 ;;
        --help|-h)
            sed -n '2,8p' "$0" | sed 's/^# //'
            exit 0
            ;;
        *)
            echo "Unknown option: $arg (use --help)"
            exit 1
            ;;
    esac
done

echo "=== eepp Windows Build ==="
echo "  Config:  $CONFIG"
echo "  Target:  $([ "$BUILD_ALL" -eq 1 ] && echo "all" || echo "$TARGET")"
echo "  Jobs:    $JOBS"
echo ""

# -------------------------------------------------------
# Verify tools
# -------------------------------------------------------
for tool in gcc g++ premake5 ninja; do
    if ! command -v "$tool" &> /dev/null; then
        echo "ERROR: $tool not found. Run 'bash projects/mingw32/windows-install.sh' first."
        exit 1
    fi
done

# -------------------------------------------------------
# Verify submodules
# -------------------------------------------------------
for dir in premake/premake-cmake premake/premake-ninja; do
    if [ -z "$(ls -A "$PROJECT_ROOT/$dir" 2>/dev/null)" ]; then
        echo "ERROR: Submodule $dir is empty. Run 'bash projects/mingw32/windows-install.sh' first."
        exit 1
    fi
done

# -------------------------------------------------------
# Clean if requested
# -------------------------------------------------------
if [ "$CLEAN" -eq 1 ]; then
    echo "--- Cleaning ---"
    rm -rf "$PROJECT_ROOT/obj/windows" "$PROJECT_ROOT/libs/windows" "$BUILD_DIR"/*.ninja
    rm -f "$PROJECT_ROOT/bin/"*.exe
    echo "[OK] Cleaned"
    echo ""
fi

# -------------------------------------------------------
# Generate Ninja build files
# -------------------------------------------------------
echo "--- Generating build files (premake5 + ninja) ---"
cd "$PROJECT_ROOT"

premake5 --os=windows --cc=gcc --windows-mingw-build --with-static-eepp ninja 2>&1 \
    | grep -v "^Das System" || true

echo "[OK] Ninja files generated in make/windows/"
echo ""

# -------------------------------------------------------
# Patch Ninja files for MinGW 15.x compatibility
# -------------------------------------------------------
GCC_MAJOR="$(gcc -dumpversion | cut -d. -f1)"
COMPAT_OBJ="$PROJECT_ROOT/mingw15_compat.o"

if [ "$GCC_MAJOR" -ge 15 ]; then
    echo "--- Patching for MinGW $GCC_MAJOR.x compatibility ---"

    # Build compat object if missing
    if [ ! -f "$COMPAT_OBJ" ]; then
        echo "Building mingw15_compat.o..."
        cat > "$PROJECT_ROOT/mingw15_compat.c" << 'COMPAT'
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
        gcc -c -m64 -O2 -o "$COMPAT_OBJ" "$PROJECT_ROOT/mingw15_compat.c" 2>/dev/null
    fi

    # Use relative path from make/windows/ to project root
    COMPAT_REL="../../mingw15_compat.o"
    PATCH_COUNT=0
    for ninja_file in "$BUILD_DIR"/*_${CONFIG}_x86_64.ninja; do
        [ -f "$ninja_file" ] || continue
        if grep -q "command = g++ -o" "$ninja_file" && ! grep -q "mingw15_compat" "$ninja_file"; then
            # Add compat .o after $in and add -lbcrypt if missing
            sed -i "s|command = g++ -o \$out \$in|command = g++ -o \$out \$in $COMPAT_REL|" "$ninja_file"
            if ! grep -q "\-lbcrypt" "$ninja_file"; then
                # Add -lbcrypt before -Wl,-Bstatic (static linking section)
                sed -i "s|-Wl,-Bstatic|-lbcrypt -Wl,-Bstatic|" "$ninja_file"
            fi
            PATCH_COUNT=$((PATCH_COUNT + 1))
        fi
    done

    echo "[OK] Patched $PATCH_COUNT ninja file(s)"
    echo ""
fi

# -------------------------------------------------------
# Build
# -------------------------------------------------------
echo "--- Building ---"
cd "$BUILD_DIR"

if [ "$BUILD_ALL" -eq 1 ]; then
    NINJA_TARGET="$CONFIG"
else
    NINJA_TARGET="${TARGET}_${CONFIG}_x86_64"
fi

echo "ninja -j$JOBS $NINJA_TARGET"
echo ""

ninja -j"$JOBS" "$NINJA_TARGET"

echo ""
echo "=== Build complete ==="

# Show results
if [ "$BUILD_ALL" -eq 1 ]; then
    echo "Libraries: $(ls "$PROJECT_ROOT/libs/windows/x86_64/"*.lib 2>/dev/null | wc -l) libs in libs/windows/x86_64/"
    echo "Executables:"
    ls -lh "$PROJECT_ROOT/bin/"*.exe 2>/dev/null | awk '{print "  " $NF " (" $5 ")"}'
else
    EXE="$PROJECT_ROOT/bin/${TARGET}.exe"
    if [ -f "$EXE" ]; then
        SIZE=$(ls -lh "$EXE" | awk '{print $5}')
        echo "Output: bin/${TARGET}.exe ($SIZE)"
    else
        LIB="$PROJECT_ROOT/libs/windows/x86_64/${TARGET}.lib"
        if [ -f "$LIB" ]; then
            SIZE=$(ls -lh "$LIB" | awk '{print $5}')
            echo "Output: libs/windows/x86_64/${TARGET}.lib ($SIZE)"
        fi
    fi
fi
