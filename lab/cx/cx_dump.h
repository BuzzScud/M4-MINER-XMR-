/* cx_dump — newline-delimited JSON trace of a RandomX hash, for the lab page.
 * Off unless cx_dump_open() was called. Every event is one line: {"ev":"...", ...} */
#ifndef CX_DUMP_H
#define CX_DUMP_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void cx_dump_open(const char *path);          /* "-" = stdout */
void cx_dump_close(void);
int  cx_dump_on(void);
int  cx_dump_want_iter(unsigned ic);          /* which loop iterations to record */
int  cx_dump_want_item(void);                 /* first N dataset items */
void cx_dump_reset_items(void);               /* call at each job start */
void cx_dump_set_active(int on);              /* an "iter" event is open (dataset read may add to it) */
int  cx_dump_active(void);

void cx_dump_begin(const char *ev);           /* {"ev":"<ev>"          */
void cx_dump_end(void);                       /* }\n                   */
void cx_dump_str(const char *k, const char *v);
void cx_dump_int(const char *k, long long v);
void cx_dump_u64(const char *k, uint64_t v);  /* as hex string         */
void cx_dump_hex(const char *k, const void *p, size_t n);
void cx_dump_u64s(const char *k, const uint64_t *v, size_t n);
void cx_dump_vec128s(const char *k, const void *v, size_t nvec);   /* nvec x 16 bytes -> [[lo,hi],...] hex */
void cx_dump_counters(void);
#ifdef __cplusplus
}
#endif
#endif
