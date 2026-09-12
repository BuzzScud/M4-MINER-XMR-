#include "cx_dump.h"
#include "cx64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One JSON object per line. Events may nest (a dataset_item begins while an
 * iter is open): inner events are written to a memory buffer and flushed as
 * their own lines after the outer event closes. */
static FILE *out = NULL;
static int items_dumped = 0;
static int active = 0;

#define MAXDEPTH 4
static FILE  *cur[MAXDEPTH];
static char  *membuf[MAXDEPTH];
static size_t memlen[MAXDEPTH];
static int    first_field[MAXDEPTH];
static int    depth = 0;
static char  *pending[64]; static int npending = 0;

static FILE *W(void) { return depth ? cur[depth - 1] : out; }

void cx_dump_open(const char *path) { out = (!path || !strcmp(path, "-")) ? stdout : fopen(path, "w"); }
void cx_dump_close(void) { if (out && out != stdout) fclose(out); out = NULL; }
int  cx_dump_on(void) { return out != NULL; }
int  cx_dump_want_iter(unsigned ic) { return out && (ic < 3 || ic == 2047); }
int  cx_dump_want_item(void) { if (!out || items_dumped >= 2) return 0; items_dumped++; return 1; }
void cx_dump_reset_items(void) { items_dumped = 0; }
void cx_dump_set_active(int on) { active = on; }
int  cx_dump_active(void) { return out && active; }

static void sep(void) { if (!first_field[depth - 1]) fputc(',', W()); first_field[depth - 1] = 0; }

void cx_dump_begin(const char *ev) {
    if (!out || depth >= MAXDEPTH) return;
    if (depth == 0) cur[0] = out;
    else { membuf[depth] = NULL; memlen[depth] = 0; cur[depth] = open_memstream(&membuf[depth], &memlen[depth]); }
    first_field[depth] = 1;
    depth++;
    fprintf(W(), "{\"ev\":\"%s\"", ev);
    first_field[depth - 1] = 0;
    cx_dump_int("ops", (long long)cx_total_calls());
}
void cx_dump_end(void) {
    if (!out || depth == 0) return;
    fputs("}\n", W());
    if (depth > 1) {                       /* inner: stash the finished line */
        fclose(cur[depth - 1]);
        if (npending < 64) pending[npending++] = membuf[depth - 1]; else free(membuf[depth - 1]);
        depth--;
        return;
    }
    depth--;
    fflush(out);
    for (int i = 0; i < npending; i++) { fputs(pending[i], out); free(pending[i]); }
    npending = 0;
    fflush(out);
}
void cx_dump_str(const char *k, const char *v) {
    if (!out || !depth) return; sep(); fprintf(W(), "\"%s\":\"", k);
    for (; *v; v++) { if (*v == '"' || *v == '\\') fputc('\\', W()); if ((unsigned char)*v < 32) fprintf(W(), "\\u%04x", *v); else fputc(*v, W()); }
    fputc('"', W());
}
void cx_dump_int(const char *k, long long v) { if (!out || !depth) return; sep(); fprintf(W(), "\"%s\":%lld", k, v); }
void cx_dump_u64(const char *k, uint64_t v) { if (!out || !depth) return; sep(); fprintf(W(), "\"%s\":\"%016llx\"", k, (unsigned long long)v); }
void cx_dump_hex(const char *k, const void *p, size_t n) {
    if (!out || !depth) return; sep(); fprintf(W(), "\"%s\":\"", k);
    for (size_t i = 0; i < n; i++) fprintf(W(), "%02x", ((const unsigned char *)p)[i]);
    fputc('"', W());
}
void cx_dump_u64s(const char *k, const uint64_t *v, size_t n) {
    if (!out || !depth) return; sep(); fprintf(W(), "\"%s\":[", k);
    for (size_t i = 0; i < n; i++) fprintf(W(), "%s\"%016llx\"", i ? "," : "", (unsigned long long)v[i]);
    fputc(']', W());
}
void cx_dump_vec128s(const char *k, const void *v, size_t nvec) {
    if (!out || !depth) return; sep(); fprintf(W(), "\"%s\":[", k);
    for (size_t i = 0; i < nvec; i++) {
        uint64_t lo, hi; memcpy(&lo, (const char *)v + 16 * i, 8); memcpy(&hi, (const char *)v + 16 * i + 8, 8);
        fprintf(W(), "%s[\"%016llx\",\"%016llx\"]", i ? "," : "", (unsigned long long)lo, (unsigned long long)hi);
    }
    fputc(']', W());
}
void cx_dump_counters(void) {
    if (!out) return;
    size_t n; const cx_counter_t *c = cx_counters(&n);
    cx_dump_begin("counters");
    cx_dump_str("mode", cx_get_mode() == CX_OFF ? "off" : cx_get_mode() == CX_CHECK ? "check" : "strict");
    sep(); fputs("\"by_op\":[", W());
    int first = 1;
    for (size_t i = 0; i < n; i++) if (c[i].calls || c[i].native) {
        fprintf(W(), "%s{\"name\":\"%s\",\"calls\":%llu,\"mismatches\":%llu,\"native\":%llu}", first ? "" : ",", c[i].name,
                (unsigned long long)c[i].calls, (unsigned long long)c[i].mismatches, (unsigned long long)c[i].native); first = 0;
    }
    fputc(']', W());
    cx_dump_int("total_calls", (long long)cx_total_calls());
    cx_dump_int("total_mismatches", (long long)cx_total_mismatches());
    cx_dump_int("total_native", (long long)cx_total_native());
    cx_dump_int("argon2_blocks_checked", (long long)cx_argon_blocks_checked());
    cx_dump_int("argon2_blocks_total", (long long)cx_argon_blocks_total());
    cx_dump_end();
}
