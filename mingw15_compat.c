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
