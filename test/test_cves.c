/* Build: cc -std=c89 -I../src test_cves.c ../src/microtar.c -o test_cves */

#include <stdio.h>
#include <string.h>

#include "microtar.h"

static char buf[4096];
static unsigned buf_len = sizeof(buf);
static int failures;

static int mem_read(mtar_t *tar, void *data, unsigned size) {
  if (tar->pos + size > buf_len) {
    return MTAR_EREADFAIL;
  }
  memcpy(data, buf + tar->pos, size);
  return MTAR_ESUCCESS;
}

static int mem_seek(mtar_t *tar, unsigned pos) {
  (void) tar;
  return pos <= buf_len ? MTAR_ESUCCESS : MTAR_ESEEKFAIL;
}

static int mem_close(mtar_t *tar) {
  (void) tar;
  return MTAR_ESUCCESS;
}

static void open_mem(mtar_t *tar) {
  memset(tar, 0, sizeof(*tar));
  tar->read = mem_read;
  tar->seek = mem_seek;
  tar->close = mem_close;
}

/* Writes a ustar header at `off`. `name_len` bytes of `name` are copied, so a
 * length of 100 leaves the field without NUL. A NULL `checksum` is computed. */
static void put_header(unsigned off, const char *name, unsigned name_len,
                       const char *size, const char *checksum) {
  char *h = buf + off;
  unsigned i, sum = 256;
  memset(h, 0, 512);
  memcpy(h, name, name_len);
  strcpy(h + 100, "0000644");
  strcpy(h + 108, "0000000");
  memcpy(h + 124, size, strlen(size));
  strcpy(h + 136, "00000000000");
  h[156] = '0';
  for (i = 0; i < 512; i++) {
    if (i < 148 || i >= 156) {
      sum += (unsigned char) h[i];
    }
  }
  if (checksum) {
    memcpy(h + 148, checksum, 8);
  } else {
    sprintf(h + 148, "%06o", sum);
    h[155] = ' ';
  }
}

static void expect(const char *what, int got, int want) {
  if (got != want) {
    printf("FAIL %s: got %d, want %d\n", what, got, want);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

int main(void) {
  mtar_t tar;
  mtar_header_t h;
  char name[102];

  setvbuf(stdout, NULL, _IONBF, 0);

  /* CVE-2026-43623, CVE-2026-55738 */
  memset(name, 'a', 101);
  memset(buf, 0, sizeof(buf));
  put_header(0, name, 100, "00000000000", NULL);
  open_mem(&tar);
  expect("name of 100 characters without NUL", mtar_read_header(&tar, &h), MTAR_ESUCCESS);
  expect("  read as 100 characters", (int) strlen(h.name), 100);

  memset(buf, 0, sizeof(buf));
  put_header(0, "a", 1, "0000000012x", NULL);
  open_mem(&tar);
  expect("size with trailing byte", mtar_read_header(&tar, &h), MTAR_ESUCCESS);
  expect("  read as 012", (int) h.size, 10);

  memset(buf, 0, sizeof(buf));
  put_header(0, "a", 1, "777777777777", NULL);
  open_mem(&tar);
  expect("size that overflows", mtar_read_header(&tar, &h), MTAR_EBADHEADER);

  memset(buf, 0, sizeof(buf));
  put_header(0, "a", 1, "00000000000", "xxxxxxxx");
  open_mem(&tar);
  expect("checksum without digits", mtar_read_header(&tar, &h), MTAR_EBADCHKSUM);

  /* CVE-2026-54417 */
  memset(buf, 0, sizeof(buf));
  put_header(0, "a", 1, "37777777000", NULL);
  open_mem(&tar);
  expect("wrapping size", mtar_find(&tar, "b", &h), MTAR_EBADHEADER);

  memset(buf, 0, sizeof(buf));
  put_header(0, "a", 1, "00000000000", NULL);
  put_header(512, "b", 1, "00000000000", NULL);
  open_mem(&tar);
  expect("valid archive", mtar_find(&tar, "b", &h), MTAR_ESUCCESS);

  /* CVE-2026-71267, #31, #32 */
  expect("open for writing", mtar_open(&tar, "test_cves.tar", "w"), MTAR_ESUCCESS);
  name[101] = '\0';
  expect("  file name of 101 characters", mtar_write_file_header(&tar, name, 0), MTAR_EFAILURE);
  expect("  dir name of 101 characters", mtar_write_dir_header(&tar, name), MTAR_EFAILURE);
  name[100] = '\0';
  expect("  file name of 100 characters", mtar_write_file_header(&tar, name, 0), MTAR_ESUCCESS);
  memset(&h, 0, sizeof(h));
  memset(h.name, 'a', sizeof(h.name));
  expect("  header with unterminated name", mtar_write_header(&tar, &h), MTAR_EFAILURE);
  memset(&h, 0, sizeof(h));
  h.mode = 010000000;
  expect("  mode above 07777777", mtar_write_header(&tar, &h), MTAR_EFAILURE);
  mtar_close(&tar);
  remove("test_cves.tar");

  return failures ? 1 : 0;
}
