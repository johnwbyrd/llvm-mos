/*
 * ZBC Semihosting ANSI C Backend - Console Only
 *
 * Implements a minimal backend that provides:
 * - Console I/O (writec, write0, readc, read/write on stdin/stdout/stderr)
 * - Exit handling
 * - Time/clock functions
 *
 * File operations (open, etc.) are rejected with an error.
 * This backend is used when no sandbox directory is specified.
 */

#include "zbc_ansi_internal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*========================================================================
 * File Operations (all return error)
 *========================================================================*/

static int console_open(void *ctx, const char *path, size_t path_len, int mode) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    (void)path;
    (void)path_len;
    (void)mode;
    if (state) {
        state->last_errno = EACCES;
    }
    return -1; /* File operations not supported in console-only mode */
}

static int console_close(void *ctx, int fd) {
    (void)ctx;
    /* Allow "closing" stdin/stdout/stderr (no-op) */
    if (fd >= 0 && fd <= 2) {
        return 0;
    }
    return -1;
}

static int console_read(void *ctx, int fd, void *buf, size_t count) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    FILE *fp;
    size_t nread;

    /* Only allow reading from stdin */
    if (fd != 0) {
        if (state) {
            state->last_errno = EBADF;
        }
        return -1;
    }

    fp = stdin;
    nread = fread(buf, 1, count, fp);
    if (nread < count && ferror(fp)) {
        if (state) {
            state->last_errno = errno;
        }
        return -1;
    }

    /* Return bytes NOT read */
    return (int)(count - nread);
}

static int console_write(void *ctx, int fd, const void *buf, size_t count) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    FILE *fp;
    size_t nwritten;

    /* Only allow writing to stdout/stderr */
    if (fd == 1) {
        fp = stdout;
    } else if (fd == 2) {
        fp = stderr;
    } else {
        if (state) {
            state->last_errno = EBADF;
        }
        return -1;
    }

    nwritten = fwrite(buf, 1, count, fp);
    if (nwritten < count) {
        if (state) {
            state->last_errno = errno;
        }
    }

    fflush(fp);

    /* Return bytes NOT written */
    return (int)(count - nwritten);
}

static int console_seek(void *ctx, int fd, int pos) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    (void)fd;
    (void)pos;
    if (state) {
        state->last_errno = ESPIPE; /* Illegal seek */
    }
    return -1;
}

static intmax_t console_flen(void *ctx, int fd) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    (void)fd;
    if (state) {
        state->last_errno = ESPIPE;
    }
    return -1;
}

static int console_remove(void *ctx, const char *path, size_t path_len) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    (void)path;
    (void)path_len;
    if (state) {
        state->last_errno = EACCES;
    }
    return -1;
}

static int console_rename(void *ctx, const char *old_path, size_t old_len,
                          const char *new_path, size_t new_len) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    (void)old_path;
    (void)old_len;
    (void)new_path;
    (void)new_len;
    if (state) {
        state->last_errno = EACCES;
    }
    return -1;
}

static int console_tmpnam(void *ctx, char *buf, size_t buf_size, int id) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    (void)buf;
    (void)buf_size;
    (void)id;
    if (state) {
        state->last_errno = EACCES;
    }
    return -1;
}

/*========================================================================
 * Console I/O
 *========================================================================*/

static void console_writec(void *ctx, char c) {
    (void)ctx;
    zbc_ansi_writec(c);
}

static void console_write0(void *ctx, const char *str) {
    (void)ctx;
    zbc_ansi_write0(str);
}

static int console_readc(void *ctx) {
    (void)ctx;
    return zbc_ansi_readc();
}

/*========================================================================
 * Status Functions
 *========================================================================*/

static int console_iserror(void *ctx, int status) {
    (void)ctx;
    return zbc_ansi_iserror(status);
}

static int console_istty(void *ctx, int fd) {
    (void)ctx;
    return zbc_ansi_istty(fd);
}

/*========================================================================
 * Time Functions
 *========================================================================*/

static int console_clock(void *ctx) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    clock_t now;
    uint64_t elapsed;

    if (!state || !state->initialized) {
        return -1;
    }

    now = clock();
    elapsed = (uint64_t)(now - (clock_t)state->start_clock);

    /* Convert to centiseconds */
    return (int)((elapsed * 100) / CLOCKS_PER_SEC);
}

static int console_time(void *ctx) {
    (void)ctx;
    return zbc_ansi_time();
}

static int console_elapsed(void *ctx, unsigned int *lo, unsigned int *hi) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
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

static int console_tickfreq(void *ctx) {
    (void)ctx;
    return zbc_ansi_tickfreq();
}

/*========================================================================
 * System Functions
 *========================================================================*/

static int console_do_system(void *ctx, const char *cmd, size_t cmd_len) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    (void)cmd;
    (void)cmd_len;
    if (state) {
        state->last_errno = EACCES;
    }
    return -1; /* system() not supported in console-only mode */
}

static int console_get_cmdline(void *ctx, char *buf, size_t buf_size) {
    (void)ctx;
    return zbc_ansi_get_cmdline(buf, buf_size);
}

static int console_heapinfo(void *ctx, uintptr_t *heap_base,
                            uintptr_t *heap_limit, uintptr_t *stack_base,
                            uintptr_t *stack_limit) {
    (void)ctx;
    return zbc_ansi_heapinfo(heap_base, heap_limit, stack_base, stack_limit);
}

static void console_do_exit(void *ctx, unsigned int reason,
                            unsigned int subcode) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;

    if (!state) {
        return;
    }

    /* Call the exit callback if set */
    if (state->on_exit) {
        state->on_exit(state->callback_ctx, reason, subcode);
    }
}

static int console_get_errno(void *ctx) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;
    if (!state) {
        return 0;
    }
    return state->last_errno;
}

static int console_timer_config(void *ctx, unsigned int rate_hz) {
    zbc_ansi_console_state_t *state = (zbc_ansi_console_state_t *)ctx;

    if (!state) {
        return 0;
    }

    /* Call the timer config callback if set */
    if (state->on_timer_config) {
        state->on_timer_config(state->timer_callback_ctx, rate_hz);
    }

    return 0;
}

/*========================================================================
 * Vtable and Public API
 *========================================================================*/

static const zbc_backend_t ansi_console_backend = {
    console_open,       console_close,      console_read,
    console_write,      console_seek,       console_flen,
    console_remove,     console_rename,     console_tmpnam,
    console_writec,     console_write0,     console_readc,
    console_iserror,    console_istty,      console_clock,
    console_time,       console_elapsed,    console_tickfreq,
    console_do_system,  console_get_cmdline, console_heapinfo,
    console_do_exit,    console_get_errno,  console_timer_config};

const zbc_backend_t *zbc_backend_console(void) { return &ansi_console_backend; }

void zbc_ansi_console_init(zbc_ansi_console_state_t *state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->start_clock = (uint64_t)clock();
    state->initialized = 1;
}

void zbc_ansi_console_set_exit_callback(
    zbc_ansi_console_state_t *state,
    void (*on_exit)(void *ctx, unsigned int reason, unsigned int subcode),
    void *ctx) {
    if (!state) {
        return;
    }
    state->on_exit = on_exit;
    state->callback_ctx = ctx;
}

void zbc_ansi_console_set_timer_callback(
    zbc_ansi_console_state_t *state,
    void (*on_timer_config)(void *ctx, unsigned int rate_hz),
    void *ctx) {
    if (!state) {
        return;
    }
    state->on_timer_config = on_timer_config;
    state->timer_callback_ctx = ctx;
}

void zbc_ansi_console_cleanup(zbc_ansi_console_state_t *state) {
    if (!state) {
        return;
    }
    state->initialized = 0;
}
