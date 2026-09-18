/*
 * Regression test for a heap buffer overflow in raw_to_header().
 *
 * The ustar format allows the `name` and `linkname` header fields to use
 * all 100 of their bytes with no trailing NUL. raw_to_header() used to
 * copy those fields into mtar_header_t with plain strcpy(), which needs
 * room for a terminator that isn't there, so it kept copying into
 * whatever memory followed the field until it happened to hit a zero
 * byte. With `linkname` being the last member of mtar_header_t, this
 * reliably wrote at least one byte past the end of the struct.
 *
 * This test builds such a header by hand (mirroring the internal raw
 * header layout, since there is no public API that can produce one) and
 * feeds it to mtar_read_header() through an in-memory stream. Build and
 * run it under ASan to see the original bug:
 *
 *   cc -fsanitize=address,undefined -g -Isrc test/test_name_overflow.c \
 *      src/microtar.c -o test_name_overflow && ./test_name_overflow
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "microtar.h"

typedef struct {
  char name[100];
  char mode[8];
  char owner[8];
  char group[8];
  char size[12];
  char mtime[12];
  char checksum[8];
  char type;
  char linkname[100];
  char _padding[255];
} raw_header_t;

typedef struct {
  unsigned char *data;
  unsigned len;
  unsigned pos;
} membuf_t;

static int mem_read(mtar_t *tar, void *data, unsigned size) {
  membuf_t *m = (membuf_t*) tar->stream;
  if (m->pos + size > m->len) {
    fprintf(stderr, "test bug: read past end of fixture\n");
    exit(2);
  }
  memcpy(data, m->data + m->pos, size);
  m->pos += size;
  return 0;
}

static int mem_seek(mtar_t *tar, unsigned pos) {
  ((membuf_t*) tar->stream)->pos = pos;
  return 0;
}

static int mem_close(mtar_t *tar) {
  (void) tar;
  return 0;
}

static unsigned raw_checksum(const raw_header_t *rh) {
  unsigned i, res = 256;
  const unsigned char *p = (const unsigned char*) rh;
  for (i = 0; i < offsetof(raw_header_t, checksum); i++) res += p[i];
  for (i = offsetof(raw_header_t, type); i < sizeof(*rh); i++) res += p[i];
  return res;
}

/* Fills in mode/owner/group/size/mtime/type/checksum with well-formed,
 * NUL-terminated values, leaving name/linkname for the caller to set. */
static void build_valid_skeleton(raw_header_t *rh) {
  memset(rh, 0, sizeof(*rh));
  snprintf(rh->mode, sizeof(rh->mode), "%o", 0644);
  snprintf(rh->owner, sizeof(rh->owner), "%o", 0);
  snprintf(rh->group, sizeof(rh->group), "%o", 0);
  snprintf(rh->size, sizeof(rh->size), "%o", 0);
  snprintf(rh->mtime, sizeof(rh->mtime), "%o", 0);
  rh->type = MTAR_TREG;
}

static void finish_checksum(raw_header_t *rh) {
  unsigned chk = raw_checksum(rh);
  snprintf(rh->checksum, sizeof(rh->checksum), "%06o", chk);
  rh->checksum[7] = ' ';
}

static int read_via_mtar(raw_header_t *rh, mtar_header_t *out) {
  membuf_t m = { (unsigned char*) rh, sizeof(*rh), 0 };
  mtar_t tar;
  memset(&tar, 0, sizeof(tar));
  tar.read = mem_read;
  tar.seek = mem_seek;
  tar.close = mem_close;
  tar.stream = &m;
  return mtar_read_header(&tar, out);
}

int main(void) {
  int failures = 0;

  /* Case 1: name fills all 100 bytes with no NUL. Must be rejected, not
   * crash. The destination is heap allocated at its exact size so ASan's
   * redzone sits right where the old strcpy() would run off the end. */
  {
    raw_header_t rh;
    build_valid_skeleton(&rh);
    memset(rh.name, 'A', sizeof(rh.name));
    memset(rh.linkname, 'C', sizeof(rh.linkname) - 1); /* leave room for NUL */
    rh.linkname[sizeof(rh.linkname) - 1] = '\0';
    finish_checksum(&rh);

    mtar_header_t *h = malloc(sizeof(mtar_header_t));
    int err = read_via_mtar(&rh, h);
    if (err == MTAR_ESUCCESS) {
      fprintf(stderr, "FAIL: unterminated name was accepted instead of rejected\n");
      failures++;
    } else {
      printf("OK: unterminated name rejected (err=%d)\n", err);
    }
    free(h);
  }

  /* Case 2: linkname fills all 100 bytes with no NUL. */
  {
    raw_header_t rh;
    build_valid_skeleton(&rh);
    strcpy(rh.name, "short-name.txt");
    memset(rh.linkname, 'C', sizeof(rh.linkname));
    finish_checksum(&rh);

    mtar_header_t *h = malloc(sizeof(mtar_header_t));
    int err = read_via_mtar(&rh, h);
    if (err == MTAR_ESUCCESS) {
      fprintf(stderr, "FAIL: unterminated linkname was accepted instead of rejected\n");
      failures++;
    } else {
      printf("OK: unterminated linkname rejected (err=%d)\n", err);
    }
    free(h);
  }

  /* Case 3: sanity check that ordinary, correctly terminated headers are
   * still read successfully, so the fix doesn't reject valid archives. */
  {
    raw_header_t rh;
    build_valid_skeleton(&rh);
    strcpy(rh.name, "ordinary.txt");
    strcpy(rh.linkname, "");
    finish_checksum(&rh);

    mtar_header_t h;
    int err = read_via_mtar(&rh, &h);
    if (err != MTAR_ESUCCESS || strcmp(h.name, "ordinary.txt") != 0) {
      fprintf(stderr, "FAIL: ordinary header was not read correctly (err=%d, name=%s)\n",
              err, err == MTAR_ESUCCESS ? h.name : "?");
      failures++;
    } else {
      printf("OK: ordinary header still reads correctly\n");
    }
  }

  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("all checks passed\n");
  return 0;
}
