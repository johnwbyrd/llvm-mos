/*
 * ZBC Semihosting ANSI Backend - Internal Header
 *
 * Shared types and functions used by both secure and insecure backends.
 * This is an internal header - not part of the public API.
 */

#ifndef ZBC_ANSI_INTERNAL_H
#define ZBC_ANSI_INTERNAL_H

#include <stdio.h>

#define ZBC_HOST
#include "llvm/Emulator/Semihost/zbc_semihost.h"

#ifdef __cplusplus
namespace llvm {
namespace zbc {
extern "C" {
#endif

/*========================================================================
 * Common FD State
 *
 * Both secure and insecure backends embed this structure to manage
 * file descriptors. This eliminates code duplication.
 *========================================================================*/

typedef struct zbc_ansi_fd_state_s {
  void *files[ZBC_ANSI_MAX_FILES];
  zbc_ansi_fd_node_t fd_pool[ZBC_ANSI_MAX_FILES];
  zbc_ansi_fd_node_t *free_fd_list;
  int next_fd;
} zbc_ansi_fd_state_t;

/*========================================================================
 * FD Management Functions
 *========================================================================*/

/*
 * Initialize FD state.
 */
void zbc_ansi_fd_init(zbc_ansi_fd_state_t *fd);

/*
 * Allocate a file descriptor for a FILE pointer.
 * Returns the FD (>= ZBC_ANSI_FIRST_FD) on success, -1 on failure.
 */
int zbc_ansi_fd_alloc(zbc_ansi_fd_state_t *fd, FILE *fp);

/*
 * Free a file descriptor, returning it to the free list.
 */
void zbc_ansi_fd_free(zbc_ansi_fd_state_t *fd, int fd_num);

/*
 * Get the FILE pointer for a file descriptor.
 * Returns NULL if invalid. Handles stdin/stdout/stderr (0/1/2).
 */
FILE *zbc_ansi_fd_get(zbc_ansi_fd_state_t *fd, int fd_num);

/*
 * Close all open files and reset state.
 */
void zbc_ansi_fd_cleanup(zbc_ansi_fd_state_t *fd);

/*========================================================================
 * Mode String Conversion
 *========================================================================*/

/*
 * Convert ARM semihosting open mode to fopen mode string.
 * Returns NULL for invalid mode.
 */
const char *zbc_ansi_mode_string(int mode);

/*
 * Check if open mode is a write mode (modes 4+ involve writing).
 */
int zbc_ansi_mode_is_write(int mode);

/*========================================================================
 * Console I/O
 *========================================================================*/

void zbc_ansi_writec(char c);
void zbc_ansi_write0(const char *str);
int zbc_ansi_readc(void);

/*========================================================================
 * Status Functions
 *========================================================================*/

int zbc_ansi_iserror(int status);
int zbc_ansi_istty(int fd_num);

/*========================================================================
 * Time Functions
 *========================================================================*/

int zbc_ansi_time(void);
int zbc_ansi_tickfreq(void);

/*========================================================================
 * Stubs
 *========================================================================*/

int zbc_ansi_get_cmdline(char *buf, size_t buf_size);
int zbc_ansi_heapinfo(uintptr_t *heap_base, uintptr_t *heap_limit,
                      uintptr_t *stack_base, uintptr_t *stack_limit);

#ifdef __cplusplus
} /* extern "C" */
} /* namespace zbc */
} /* namespace llvm */
#endif

#endif /* ZBC_ANSI_INTERNAL_H */
