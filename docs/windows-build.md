# Windows Build Guide (MinGW, ohne MSVC)

Anleitung zum Bauen von eepp unter Windows mit MinGW-w64 (GCC), ohne Visual Studio / MSVC.

## Voraussetzungen

| Tool | Getestet mit | Installation (Scoop) |
|------|-------------|---------------------|
| MinGW-w64 (GCC/G++) | 15.2.0 | `scoop install mingw` |
| premake5 | 5.0.0-beta8 | `scoop install premake` |
| ninja | 1.13.2 | `scoop install ninja` |
| Git | - | `scoop install git` |

Alle Tools muessen im PATH sein. Geprueft mit:

```bash
gcc --version
premake5 --version
ninja --version
```

## Schnellstart

```bash
# 1. Abhaengigkeiten installieren (einmalig)
bash projects/mingw32/windows-install.sh

# 2. Bauen
bash projects/mingw32/windows-build.sh
```

Das Ergebnis liegt in `bin/ecode.exe`.

## Bekannte Probleme und Loesungen

### 1. Git Submodules nicht initialisiert

**Symptom:**
```
Error: module 'premake.premake-cmake.cmake' not found
```

**Ursache:** Die Verzeichnisse `premake/premake-cmake/`, `premake/premake-ninja/`, `src/thirdparty/SOIL2/` und `src/thirdparty/efsw/` sind leer.

**Loesung:**
```bash
git submodule update --init --recursive
```

### 2. premake5: Invalid toolset 'mingw'

**Symptom:**
```
Error: [string "gmake/gmake.lua"]:116: Invalid toolset 'mingw'
```

**Ursache:** Die Option `--cc=mingw` ist in premake5 (ab beta8) kein gueltiges Toolset fuer den `gmake`-Generator. Das war ein premake4-Feature.

**Loesung:** `--cc=gcc` verwenden oder `--cc` weglassen. Die Option `--windows-mingw-build` reicht fuer die SDL2-Abhaengigkeiten:
```bash
premake5 --os=windows --cc=gcc --windows-mingw-build ninja
```

### 3. ar.exe: too many sections / Kommandozeile zu lang

**Symptom:**
```
ar.exe: error: ../../obj/windows/x86_64/rele: no such file or directory
```
Der Pfad wird abgeschnitten (z.B. `rele` statt `release/freetype-static/...`).

**Ursache:** Windows hat ein Kommandozeilenlimit von ~8191 Zeichen. Bei Bibliotheken mit vielen Object-Files (z.B. freetype mit 180 .o-Dateien) wird die `ar`-Kommandozeile zu lang. Der `gmake`-Generator hat keine Response-File-Unterstuetzung.

**Loesung:** Ninja statt gmake verwenden. Ninja nutzt automatisch Response-Files fuer lange Kommandozeilen:
```bash
scoop install ninja
premake5 --os=windows --cc=gcc --windows-mingw-build ninja
```

### 4. MinGW 15.x: sec_api/wconio_s.h fehlt

**Symptom:**
```
fatal error: sec_api/wconio_s.h: No such file or directory
```

**Ursache:** Packaging-Bug in MinGW-Builds 15.2.0 - die Datei `sec_api/wconio_s.h` fehlt im Include-Verzeichnis, wird aber von `corecrt_wconio.h` eingebunden.

**Loesung:** Stub-Header erstellen. Das `windows-install.sh`-Script macht das automatisch, manuell:

```bash
MINGW_INCLUDE="$(dirname "$(which gcc)")/../x86_64-w64-mingw32/include"

cat > "$MINGW_INCLUDE/sec_api/wconio_s.h" << 'HEADER'
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
```

### 5. MinGW 15.x: undefined reference to __imp_fseeko64 / __imp_ftello64

**Symptom:**
```
undefined reference to `__imp_fseeko64'
undefined reference to `__imp_ftello64'
```

**Ursache:** In MinGW 15.x sind `fseeko64`/`ftello64` nur noch als statische Funktionen in `libmingwex.a` verfuegbar (Symbol-Typ `T`), aber der Code erwartet DLL-Import-Versionen (`__imp_`-Praefix). Die Header deklarieren die Funktionen mit `__declspec(dllimport)`, die Library liefert sie aber nicht mehr als Import.

**Loesung:** Eine Compat-Datei (`mingw15_compat.c`) erstellen, die die `__imp_`-Symbole als Funktionszeiger auf die statischen Versionen definiert:

```c
/* mingw15_compat.c */
#include <stdio.h>
int fseeko64(FILE *stream, long long offset, int whence);
long long ftello64(FILE *stream);
int (*__imp_fseeko64)(FILE*, long long, int) = fseeko64;
long long (*__imp_ftello64)(FILE*) = ftello64;
```

Kompilieren und beim Linken einbinden:
```bash
gcc -c -m64 -O2 -o mingw15_compat.o mingw15_compat.c
```

### 6. MinGW 15.x: undefined reference to __imp__setjmp

**Symptom:**
```
undefined reference to `__imp__setjmp'
```

**Ursache:** `msvcrt.dll` exportiert `__intrinsic_setjmp`, aber nicht `_setjmp`. Einige Bibliotheken (freetype, libpng) referenzieren jedoch `__imp__setjmp` als DLL-Import.

**Loesung:** Eine Wrapper-Funktion in `mingw15_compat.c` hinzufuegen, die `_setjmp` auf `__intrinsic_setjmp` delegiert:

```c
typedef long long SETJMP_FLOAT128[2];
int __intrinsic_setjmp(SETJMP_FLOAT128 *, void *);
int _setjmp(SETJMP_FLOAT128 *buf, void *frame) {
    return __intrinsic_setjmp(buf, frame);
}
void *__imp__setjmp = (void*)_setjmp;
```

### 7. undefined reference to BCryptGenRandom

**Symptom:**
```
undefined reference to `BCryptGenRandom'
```

**Ursache:** Die Bibliothek `-lbcrypt` fehlt in den Link-Flags einiger Targets. mbedtls nutzt `BCryptGenRandom` fuer die Entropy-Erzeugung unter Windows.

**Loesung:** `-lbcrypt` zu den Linker-Flags hinzufuegen. Das `windows-build.sh`-Script patcht die generierten Ninja-Dateien automatisch.

### 8. too many sections (COFF bigobj) im Debug-Build

**Symptom:**
```
as.exe: ../../obj/.../hb-subset.o: too many sections (41448)
Fatal error: can't write ... to section: 'file too big'
```

**Ursache:** Grosse C++-Dateien (z.B. harfbuzz `hb-subset.cc`) erzeugen im Debug-Modus mehr als 65535 COFF-Sections. Das Standard-COFF-Format hat dieses Limit.

**Loesung:** Das Flag `-Wa,-mbig-obj` aktiviert das BigObj-Format. premake5 setzt dieses Flag bereits fuer die statisch gelinkten Targets (`--with-static-eepp`). Fuer den Debug-Build trotzdem problematisch - empfohlen wird der Release-Build.

## Build-Konfigurationen

| Konfiguration | Beschreibung |
|--------------|-------------|
| `release` | Alle Release-Targets (empfohlen) |
| `ecode_release_x86_64` | Nur ecode Editor (Release) |
| `eepp-static_release_x86_64` | Nur eepp statische Bibliothek |

## Verzeichnisstruktur nach dem Build

```
bin/
  ecode.exe          # ecode Editor
  SDL2.dll           # SDL2 Runtime
  assets/            # ecode Assets
libs/windows/x86_64/
  eepp-static.lib    # eepp statische Bibliothek
  thirdparty/        # Drittanbieter-Bibliotheken
```
