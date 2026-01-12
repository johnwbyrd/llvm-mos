/**
 * @file zbc_backend.h
 * @brief ZBC Semihosting Backend Interface
 *
 * Backend vtable definition and factory declarations. Backends provide
 * the actual implementation of semihosting operations (file I/O, console,
 * time). The host library dispatches requests to a backend vtable.
 *
 * Vendored from https://github.com/johnwbyrd/semihost
 * Modified to add llvm::zbc namespace when compiled as C++.
 */

#ifndef ZBC_BACKEND_H
#define ZBC_BACKEND_H

#include "zbc_protocol.h"

#ifdef __cplusplus
namespace llvm {
namespace zbc {
extern "C" {
#endif

/*========================================================================
 * Backend Vtable
 *========================================================================*/

/**
 * Backend vtable defining semihosting operations.
 *
 * Implement the functions you need, set unused operations to NULL.
 * The host library returns an error to the guest for any NULL operation.
 *
 * Return value conventions:
 * - open: file descriptor (>=0) on success, -1 on error
 * - close, seek, remove, rename: 0 on success, -1 on error
 * - read, write: bytes NOT transferred (0 = complete), -1 on error
 * - flen: file length on success, -1 on error
 * - clock: centiseconds since start, -1 on error
 * - time: seconds since epoch, -1 on error
 * - tmpnam: 0 on success (fills buf), -1 on error
 */
typedef struct zbc_backend_s {
    /** Open a file. Path is NOT null-terminated; use path_len. Mode is SH_OPEN_*. */
    int (*open)(void *ctx, const char *path, size_t path_len, int mode);
    /** Close a file descriptor. */
    int (*close)(void *ctx, int fd);
    /** Read up to count bytes. Returns bytes NOT read (0 = all read). */
    int (*read)(void *ctx, int fd, void *buf, size_t count);
    /** Write count bytes. Returns bytes NOT written (0 = all written). */
    int (*write)(void *ctx, int fd, const void *buf, size_t count);
    /** Seek to absolute position. */
    int (*seek)(void *ctx, int fd, int pos);
    /** Return file length. */
    intmax_t (*flen)(void *ctx, int fd);
    /** Delete a file. Path is NOT null-terminated. */
    int (*remove)(void *ctx, const char *path, size_t path_len);
    /** Rename a file. Paths are NOT null-terminated. */
    int (*rename)(void *ctx, const char *old_path, size_t old_len,
                  const char *new_path, size_t new_len);
    /** Generate temporary filename. Write to buf, return 0 on success. */
    int (*tmpnam)(void *ctx, char *buf, size_t buf_size, int id);

    /** Write a single character to console. */
    void (*writec)(void *ctx, char c);
    /** Write a null-terminated string to console. */
    void (*write0)(void *ctx, const char *str);
    /** Read a character from console (blocking). Return char or -1. */
    int (*readc)(void *ctx);

    /** Check if status indicates error. Return 1 if error, 0 otherwise. */
    int (*iserror)(void *ctx, int status);
    /** Check if fd is a TTY. Return 1 if TTY, 0 otherwise. */
    int (*istty)(void *ctx, int fd);
    /** Return centiseconds since program start. */
    int (*clock)(void *ctx);
    /** Return seconds since Unix epoch. */
    int (*time)(void *ctx);
    /** Return 64-bit tick count in *lo and *hi. */
    int (*elapsed)(void *ctx, unsigned int *lo, unsigned int *hi);
    /** Return ticks per second. */
    int (*tickfreq)(void *ctx);

    /** Execute a shell command. Return exit code. */
    int (*do_system)(void *ctx, const char *cmd, size_t cmd_len);
    /** Get command line. Write to buf, return 0 on success. */
    int (*get_cmdline)(void *ctx, char *buf, size_t buf_size);
    /** Get heap/stack info. Fill all four values, return 0 on success. */
    int (*heapinfo)(void *ctx, uintptr_t *heap_base, uintptr_t *heap_limit,
                    uintptr_t *stack_base, uintptr_t *stack_limit);
    /** Guest is exiting. Handle as appropriate (stop emulation, etc.). */
    void (*do_exit)(void *ctx, unsigned int reason, unsigned int subcode);
    /** Return last errno value. */
    int (*get_errno)(void *ctx);
    /** Configure periodic timer. rate_hz=0 disables. Returns 0 on success. */
    int (*timer_config)(void *ctx, unsigned int rate_hz);
} zbc_backend_t;

/*========================================================================
 * Built-in Backends
 *========================================================================*/

/**
 * Get the secure (sandboxed) ANSI backend.
 *
 * The secure backend restricts file access to a sandbox directory.
 * Guest code cannot escape the sandbox or access arbitrary host files.
 * Use with zbc_ansi_state_t from zbc_backend_ansi.h.
 *
 * @return Pointer to backend vtable
 * @see zbc_ansi_init
 */
const zbc_backend_t *zbc_backend_ansi(void);

/**
 * Get the insecure (unrestricted) ANSI backend.
 *
 * The insecure backend provides unrestricted access to the host
 * filesystem. Guest code can read, write, and delete any file the
 * host process can access. Use only for trusted code.
 * Use with zbc_ansi_insecure_state_t from zbc_backend_ansi.h.
 *
 * @return Pointer to backend vtable
 * @see zbc_ansi_insecure_init
 */
const zbc_backend_t *zbc_backend_ansi_insecure(void);

/**
 * Get the dummy (no-op) backend.
 *
 * All operations succeed with no side effects. Useful for testing
 * the host processing logic without actual I/O.
 * No state required -- pass NULL as backend_ctx.
 *
 * @return Pointer to backend vtable
 */
const zbc_backend_t *zbc_backend_dummy(void);

/**
 * Get the console-only ANSI backend.
 *
 * This backend supports console I/O (writec, write0, readc) and
 * read/write on stdin/stdout/stderr. File operations return errors.
 * Use with zbc_ansi_console_state_t from zbc_backend_ansi.h.
 *
 * @return Pointer to backend vtable
 * @see zbc_ansi_console_init
 */
const zbc_backend_t *zbc_backend_console(void);

#ifdef __cplusplus
} /* extern "C" */
} /* namespace zbc */
} /* namespace llvm */
#endif

#endif /* ZBC_BACKEND_H */
