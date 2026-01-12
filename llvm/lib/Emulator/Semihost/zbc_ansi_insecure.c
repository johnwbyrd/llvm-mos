/*
 * ZBC Semihosting ANSI C Backend - Insecure (Unrestricted)
 *
 * WARNING: This backend provides unrestricted filesystem access!
 * Guest code can read/write/delete any file the host process can access.
 * Only use for trusted guest code or debugging.
 */

#include "zbc_ansi_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*========================================================================
 * FD State Helpers
 *
 * The insecure state embeds FD fields directly (for historical reasons).
 * These helpers bridge to the common FD functions.
 *========================================================================*/

static int insecure_alloc_fd(zbc_ansi_insecure_state_t *state, FILE *fp) {
  int fd_num;
  int idx;
  zbc_ansi_fd_node_t *node;

  if (state->free_fd_list != NULL) {
    node = state->free_fd_list;
    fd_num = node->fd;
    state->free_fd_list = node->next;
    node->fd = 0;
    node->next = NULL;
  } else {
    fd_num = state->next_fd++;
  }

  idx = fd_num - ZBC_ANSI_FIRST_FD;
  if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
    return -1;
  }

  state->files[idx] = fp;
  return fd_num;
}

static void insecure_free_fd(zbc_ansi_insecure_state_t *state, int fd_num) {
  int idx;
  int pool_idx;

  idx = fd_num - ZBC_ANSI_FIRST_FD;
  if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
    return;
  }

  state->files[idx] = NULL;

  for (pool_idx = 0; pool_idx < ZBC_ANSI_MAX_FILES; pool_idx++) {
    if (state->fd_pool[pool_idx].fd == 0 ||
        state->fd_pool[pool_idx].fd == fd_num) {
      state->fd_pool[pool_idx].fd = fd_num;
      state->fd_pool[pool_idx].next = state->free_fd_list;
      state->free_fd_list = &state->fd_pool[pool_idx];
      break;
    }
  }
}

static FILE *insecure_get_file(zbc_ansi_insecure_state_t *state, int fd_num) {
  int idx;

  if (fd_num == 0)
    return stdin;
  if (fd_num == 1)
    return stdout;
  if (fd_num == 2)
    return stderr;

  idx = fd_num - ZBC_ANSI_FIRST_FD;
  if (idx < 0 || idx >= ZBC_ANSI_MAX_FILES) {
    return NULL;
  }

  return (FILE *)state->files[idx];
}

/*========================================================================
 * Backend Implementation
 *========================================================================*/

static int ansi_insecure_open(void *ctx, const char *path, size_t path_len,
                              int mode) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  const char *mode_str;
  FILE *fp;
  int fd;

  if (!state || !state->initialized) {
    return -1;
  }

  mode_str = zbc_ansi_mode_string(mode);
  if (mode_str == NULL) {
    state->last_errno = EINVAL;
    return -1;
  }

  /* Path may not be null-terminated - use state buffer */
  if (path_len >= sizeof(state->path_buf)) {
    state->last_errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(state->path_buf, path, path_len);
  state->path_buf[path_len] = '\0';

  fp = fopen(state->path_buf, mode_str);
  if (fp == NULL) {
    state->last_errno = errno;
    return -1;
  }

  fd = insecure_alloc_fd(state, fp);
  if (fd < 0) {
    fclose(fp);
    state->last_errno = EMFILE;
    return -1;
  }

  return fd;
}

static int ansi_insecure_close(void *ctx, int fd) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  FILE *fp;

  if (!state || !state->initialized) {
    return -1;
  }

  if (fd < ZBC_ANSI_FIRST_FD) {
    return 0;
  }

  fp = insecure_get_file(state, fd);
  if (fp == NULL) {
    state->last_errno = EBADF;
    return -1;
  }

  if (fclose(fp) != 0) {
    state->last_errno = errno;
    return -1;
  }

  insecure_free_fd(state, fd);
  return 0;
}

static int ansi_insecure_read(void *ctx, int fd, void *buf, size_t count) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  FILE *fp;
  size_t nread;

  if (!state || !state->initialized) {
    return -1;
  }

  fp = insecure_get_file(state, fd);
  if (fp == NULL) {
    state->last_errno = EBADF;
    return -1;
  }

  nread = fread(buf, 1, count, fp);
  if (nread < count && ferror(fp)) {
    state->last_errno = errno;
    return -1;
  }

  return (int)(count - nread);
}

static int ansi_insecure_write(void *ctx, int fd, const void *buf,
                               size_t count) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  FILE *fp;
  size_t nwritten;

  if (!state || !state->initialized) {
    return -1;
  }

  fp = insecure_get_file(state, fd);
  if (fp == NULL) {
    state->last_errno = EBADF;
    return -1;
  }

  nwritten = fwrite(buf, 1, count, fp);
  if (nwritten < count) {
    state->last_errno = errno;
  }

  if (fd == 1 || fd == 2) {
    fflush(fp);
  }

  return (int)(count - nwritten);
}

static int ansi_insecure_seek(void *ctx, int fd, int pos) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  FILE *fp;

  if (!state || !state->initialized) {
    return -1;
  }

  fp = insecure_get_file(state, fd);
  if (fp == NULL) {
    state->last_errno = EBADF;
    return -1;
  }

  if (fseek(fp, pos, SEEK_SET) != 0) {
    state->last_errno = errno;
    return -1;
  }

  return 0;
}

static intmax_t ansi_insecure_flen(void *ctx, int fd) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  FILE *fp;
  long cur_pos;
  long end_pos;

  if (!state || !state->initialized) {
    return -1;
  }

  fp = insecure_get_file(state, fd);
  if (fp == NULL) {
    state->last_errno = EBADF;
    return -1;
  }

  cur_pos = ftell(fp);
  if (cur_pos < 0) {
    state->last_errno = errno;
    return -1;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    state->last_errno = errno;
    return -1;
  }

  end_pos = ftell(fp);
  if (end_pos < 0) {
    state->last_errno = errno;
    fseek(fp, cur_pos, SEEK_SET);
    return -1;
  }

  if (fseek(fp, cur_pos, SEEK_SET) != 0) {
    state->last_errno = errno;
    return -1;
  }

  return (intmax_t)end_pos;
}

static int ansi_insecure_remove_file(void *ctx, const char *path,
                                     size_t path_len) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;

  if (!state || !state->initialized) {
    return -1;
  }

  if (path_len >= sizeof(state->path_buf)) {
    state->last_errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(state->path_buf, path, path_len);
  state->path_buf[path_len] = '\0';

  if (remove(state->path_buf) != 0) {
    state->last_errno = errno;
    return -1;
  }

  return 0;
}

static int ansi_insecure_rename_file(void *ctx, const char *old_path,
                                     size_t old_len, const char *new_path,
                                     size_t new_len) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  char old_copy[ZBC_ANSI_PATH_BUF_MAX];

  if (!state || !state->initialized) {
    return -1;
  }

  if (old_len >= sizeof(old_copy) || new_len >= sizeof(state->path_buf)) {
    state->last_errno = ENAMETOOLONG;
    return -1;
  }

  memcpy(old_copy, old_path, old_len);
  old_copy[old_len] = '\0';

  memcpy(state->path_buf, new_path, new_len);
  state->path_buf[new_len] = '\0';

  if (rename(old_copy, state->path_buf) != 0) {
    state->last_errno = errno;
    return -1;
  }

  return 0;
}

static int ansi_insecure_tmpnam_func(void *ctx, char *buf, size_t buf_size,
                                     int id) {
  (void)ctx;

  if (buf_size < 12) {
    return -1;
  }

  sprintf(buf, "tmp%03d.tmp", id % 1000);
  return 0;
}

static void ansi_insecure_writec(void *ctx, char c) {
  (void)ctx;
  zbc_ansi_writec(c);
}

static void ansi_insecure_write0(void *ctx, const char *str) {
  (void)ctx;
  zbc_ansi_write0(str);
}

static int ansi_insecure_readc(void *ctx) {
  (void)ctx;
  return zbc_ansi_readc();
}

static int ansi_insecure_iserror(void *ctx, int status) {
  (void)ctx;
  return zbc_ansi_iserror(status);
}

static int ansi_insecure_istty(void *ctx, int fd) {
  (void)ctx;
  return zbc_ansi_istty(fd);
}

static int ansi_insecure_clock_func(void *ctx) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  clock_t now;
  uint64_t elapsed;

  if (!state || !state->initialized) {
    return -1;
  }

  now = clock();
  elapsed = (uint64_t)(now - (clock_t)state->start_clock);

  return (int)((elapsed * 100) / CLOCKS_PER_SEC);
}

static int ansi_insecure_time_func(void *ctx) {
  (void)ctx;
  return zbc_ansi_time();
}

static int ansi_insecure_elapsed(void *ctx, unsigned int *lo,
                                 unsigned int *hi) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  clock_t now;
  uint64_t elapsed;

  if (!state || !state->initialized) {
    return -1;
  }

  now = clock();
  elapsed = (uint64_t)(now - (clock_t)state->start_clock);

  *lo = (unsigned int)(elapsed & 0xFFFFFFFF);
  *hi = (unsigned int)(elapsed >> 32);

  return 0;
}

static int ansi_insecure_tickfreq(void *ctx) {
  (void)ctx;
  return zbc_ansi_tickfreq();
}

static int ansi_insecure_do_system(void *ctx, const char *cmd, size_t cmd_len) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;

  if (!state || !state->initialized) {
    return -1;
  }

  if (cmd_len >= sizeof(state->path_buf)) {
    state->last_errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(state->path_buf, cmd, cmd_len);
  state->path_buf[cmd_len] = '\0';

  return system(state->path_buf);
}

static int ansi_insecure_get_cmdline(void *ctx, char *buf, size_t buf_size) {
  (void)ctx;
  return zbc_ansi_get_cmdline(buf, buf_size);
}

static int ansi_insecure_heapinfo(void *ctx, uintptr_t *heap_base,
                                  uintptr_t *heap_limit, uintptr_t *stack_base,
                                  uintptr_t *stack_limit) {
  (void)ctx;
  return zbc_ansi_heapinfo(heap_base, heap_limit, stack_base, stack_limit);
}

static void ansi_insecure_do_exit(void *ctx, unsigned int reason,
                                  unsigned int subcode) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  (void)subcode;

  if (state) {
    zbc_ansi_insecure_cleanup(state);
  }

  exit((int)(reason & 0xFF));
}

static int ansi_insecure_get_errno(void *ctx) {
  zbc_ansi_insecure_state_t *state = (zbc_ansi_insecure_state_t *)ctx;
  if (!state) {
    return 0;
  }
  return state->last_errno;
}

static int ansi_insecure_timer_config(void *ctx, unsigned int rate_hz) {
  (void)ctx;
  (void)rate_hz;
  /* Insecure backend has no callback mechanism; return success */
  return 0;
}

/*========================================================================
 * Vtable and Public API
 *========================================================================*/

static const zbc_backend_t ansi_insecure_backend = {
    ansi_insecure_open,        ansi_insecure_close,
    ansi_insecure_read,        ansi_insecure_write,
    ansi_insecure_seek,        ansi_insecure_flen,
    ansi_insecure_remove_file, ansi_insecure_rename_file,
    ansi_insecure_tmpnam_func, ansi_insecure_writec,
    ansi_insecure_write0,      ansi_insecure_readc,
    ansi_insecure_iserror,     ansi_insecure_istty,
    ansi_insecure_clock_func,  ansi_insecure_time_func,
    ansi_insecure_elapsed,     ansi_insecure_tickfreq,
    ansi_insecure_do_system,   ansi_insecure_get_cmdline,
    ansi_insecure_heapinfo,    ansi_insecure_do_exit,
    ansi_insecure_get_errno,   ansi_insecure_timer_config};

const zbc_backend_t *zbc_backend_ansi_insecure(void) {
  return &ansi_insecure_backend;
}

void zbc_ansi_insecure_init(zbc_ansi_insecure_state_t *state) {
  int i;

  if (!state) {
    return;
  }

  memset(state, 0, sizeof(*state));

  state->next_fd = ZBC_ANSI_FIRST_FD;
  state->free_fd_list = NULL;
  for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
    state->files[i] = NULL;
    state->fd_pool[i].fd = 0;
    state->fd_pool[i].next = NULL;
  }

  state->start_clock = (uint64_t)clock();
  state->initialized = 1;
}

void zbc_ansi_insecure_cleanup(zbc_ansi_insecure_state_t *state) {
  int i;

  if (!state || !state->initialized) {
    return;
  }

  for (i = 0; i < ZBC_ANSI_MAX_FILES; i++) {
    if (state->files[i] != NULL) {
      fclose((FILE *)state->files[i]);
      state->files[i] = NULL;
    }
  }

  state->initialized = 0;
}
