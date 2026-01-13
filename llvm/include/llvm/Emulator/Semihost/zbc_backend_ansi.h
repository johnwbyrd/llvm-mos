/**
 * @file zbc_backend_ansi.h
 * @brief ZBC Semihosting ANSI Backend Types
 *
 * State structures and initialization functions for the ANSI C backends.
 *
 * Vendored from https://github.com/johnwbyrd/semihost
 * Modified to add llvm::zbc namespace when compiled as C++.
 */

#ifndef ZBC_BACKEND_ANSI_H
#define ZBC_BACKEND_ANSI_H

#include "zbc_protocol.h"

#ifdef __cplusplus
namespace llvm {
namespace zbc {
extern "C" {
#endif

/*========================================================================
 * Capacity limits
 *========================================================================*/

#define ZBC_ANSI_MAX_FILES       64    /**< Maximum simultaneously open files */
#define ZBC_ANSI_MAX_PATH_RULES  16    /**< Maximum additional path rules */
#define ZBC_ANSI_SANDBOX_DIR_MAX 512   /**< Maximum sandbox directory path length */
#define ZBC_ANSI_PATH_BUF_MAX    1024  /**< Maximum resolved path length */
#define ZBC_ANSI_FIRST_FD        3     /**< First FD to allocate (0-2 are stdio) */

/*========================================================================
 * Configuration flags
 *========================================================================*/

#define ZBC_ANSI_FLAG_ALLOW_SYSTEM  0x0001  /**< Enable system() calls */
#define ZBC_ANSI_FLAG_ALLOW_EXIT    0x0002  /**< Allow exit() to terminate host */
#define ZBC_ANSI_FLAG_READ_ONLY     0x0004  /**< Block all write operations */

/*========================================================================
 * Violation types for callback
 *========================================================================*/

#define ZBC_ANSI_VIOL_PATH_BLOCKED    1  /**< Path outside sandbox */
#define ZBC_ANSI_VIOL_PATH_TRAVERSAL  2  /**< Path contains .. escape */
#define ZBC_ANSI_VIOL_SYSTEM_BLOCKED  3  /**< system() call blocked */
#define ZBC_ANSI_VIOL_EXIT_BLOCKED    4  /**< exit() call blocked */
#define ZBC_ANSI_VIOL_WRITE_BLOCKED   5  /**< Write op in read-only mode */
#define ZBC_ANSI_VIOL_REMOVE_BLOCKED  6  /**< remove() blocked */
#define ZBC_ANSI_VIOL_RENAME_BLOCKED  7  /**< rename() blocked */

/*========================================================================
 * Common Types
 *========================================================================*/

/** File descriptor tracking node (for free list) */
typedef struct zbc_ansi_fd_node_s {
    int fd;                             /**< File descriptor value */
    struct zbc_ansi_fd_node_s *next;    /**< Next node in free list */
} zbc_ansi_fd_node_t;

/** Path rule for additional allowed directories */
typedef struct zbc_ansi_path_rule_s {
    const char *prefix;        /**< Path prefix to allow */
    size_t prefix_len;         /**< Length of prefix */
    int allow_write;           /**< 0 = read-only, 1 = read-write */
} zbc_ansi_path_rule_t;

/**
 * Custom policy vtable (optional, for OS-specific sandboxing).
 */
typedef struct zbc_ansi_policy_s {
    /**
     * Validate and resolve a path.
     *
     * Called before any file operation.
     *
     * @param ctx           Policy context (from zbc_ansi_set_policy)
     * @param path          Original path from guest
     * @param path_len      Length of path
     * @param for_write     1 if operation will modify file, 0 for read-only
     * @param resolved      Buffer to write resolved path
     * @param resolved_size Size of resolved buffer
     * @param[out] resolved_len Length of resolved path
     * @return 0 on success (path allowed), non-zero to deny (EACCES)
     */
    int (*validate_path)(void *ctx, const char *path, size_t path_len,
                         int for_write, char *resolved, size_t resolved_size,
                         size_t *resolved_len);

    /**
     * Validate a system() command.
     *
     * Only called if ZBC_ANSI_FLAG_ALLOW_SYSTEM is set.
     *
     * @param ctx     Policy context
     * @param cmd     Command string
     * @param cmd_len Length of command
     * @return 0 to allow execution, non-zero to deny
     */
    int (*validate_system)(void *ctx, const char *cmd, size_t cmd_len);

    /**
     * Handle exit() request.
     *
     * Only called if ZBC_ANSI_FLAG_ALLOW_EXIT is NOT set.
     *
     * @param ctx     Policy context
     * @param reason  Exit reason code
     * @param subcode Exit subcode
     * @return 0 to allow exit, non-zero to block
     */
    int (*handle_exit)(void *ctx, unsigned int reason, unsigned int subcode);
} zbc_ansi_policy_t;

/*========================================================================
 * Secure Backend State
 *========================================================================*/

/**
 * Secure ANSI backend state (caller-allocated).
 *
 * Initialize with zbc_ansi_init() before use.
 */
typedef struct zbc_ansi_state_s {
    /*--- Sandbox configuration ---*/
    char sandbox_dir[ZBC_ANSI_SANDBOX_DIR_MAX];  /**< Primary sandbox directory */
    size_t sandbox_dir_len;                       /**< Length of sandbox_dir */
    unsigned int flags;                           /**< ZBC_ANSI_FLAG_* */

    /*--- Additional path rules ---*/
    zbc_ansi_path_rule_t path_rules[ZBC_ANSI_MAX_PATH_RULES];  /**< Path rules */
    int path_rule_count;                          /**< Number of path rules */

    /*--- Custom policy (optional) ---*/
    const zbc_ansi_policy_t *policy;   /**< NULL = use built-in policy */
    void *policy_ctx;                  /**< Context for policy callbacks */

    /*--- Callbacks ---*/
    void (*on_violation)(void *ctx, int type, const char *detail);  /**< Violation callback */
    void (*on_exit)(void *ctx, unsigned int reason, unsigned int subcode);  /**< Exit callback */
    void (*on_timer_config)(void *ctx, unsigned int rate_hz);  /**< Timer config callback */
    void *callback_ctx;                /**< Context for callbacks */

    /*--- Internal: file descriptor table (void* to avoid stdio.h dep) ---*/
    void *files[ZBC_ANSI_MAX_FILES];
    zbc_ansi_fd_node_t fd_pool[ZBC_ANSI_MAX_FILES];
    zbc_ansi_fd_node_t *free_fd_list;
    int next_fd;

    /*--- Internal: other state ---*/
    int last_errno;
    /* Clock accumulator using 64-bit time */
    uint64_t start_clock;              /**< Stored as uint64_t to avoid time.h */
    char path_buf[ZBC_ANSI_PATH_BUF_MAX];
    int initialized;
} zbc_ansi_state_t;

/*========================================================================
 * Secure Backend API
 *========================================================================*/

/**
 * Initialize secure ANSI backend.
 *
 * The sandbox_dir is copied into state, so the original can be freed.
 *
 * @param state       Caller-allocated state structure
 * @param sandbox_dir Directory to sandbox file operations to (required).
 *                    All file paths must start with this prefix.
 *                    Should end with '/' (will be added if missing).
 */
void zbc_ansi_init(zbc_ansi_state_t *state, const char *sandbox_dir);

/**
 * Add an additional allowed path.
 *
 * The prefix string must remain valid for the lifetime of state.
 *
 * @param state       Initialized state
 * @param prefix      Path prefix to allow (e.g., "/usr/lib/")
 * @param allow_write 0 for read-only, 1 for read-write
 * @return 0 on success, -1 if path_rules array is full
 */
int zbc_ansi_add_path(zbc_ansi_state_t *state, const char *prefix,
                      int allow_write);

/**
 * Set custom security policy.
 *
 * @param state  Initialized state
 * @param policy Policy vtable (NULL to use built-in)
 * @param ctx    Context passed to policy callbacks
 */
void zbc_ansi_set_policy(zbc_ansi_state_t *state,
                         const zbc_ansi_policy_t *policy, void *ctx);

/**
 * Set callbacks for security events.
 *
 * @param state           Initialized state
 * @param on_violation    Called when operation is blocked (may be NULL)
 * @param on_exit         Called when exit() is intercepted (may be NULL)
 * @param on_timer_config Called when timer is configured (may be NULL)
 * @param ctx             Context passed to callbacks
 */
void zbc_ansi_set_callbacks(zbc_ansi_state_t *state,
                            void (*on_violation)(void *ctx, int type,
                                                 const char *detail),
                            void (*on_exit)(void *ctx, unsigned int reason,
                                            unsigned int subcode),
                            void (*on_timer_config)(void *ctx,
                                                    unsigned int rate_hz),
                            void *ctx);

/**
 * Clean up secure ANSI backend state.
 *
 * Closes all open files. State can be reused after calling zbc_ansi_init().
 *
 * @param state Initialized state
 */
void zbc_ansi_cleanup(zbc_ansi_state_t *state);

/*========================================================================
 * Insecure Backend State
 *========================================================================*/

/**
 * Insecure ANSI backend state (caller-allocated).
 *
 * Initialize with zbc_ansi_insecure_init() before use.
 */
typedef struct zbc_ansi_insecure_state_s {
    /*--- Internal: file descriptor table (void* to avoid stdio.h dep) ---*/
    void *files[ZBC_ANSI_MAX_FILES];
    zbc_ansi_fd_node_t fd_pool[ZBC_ANSI_MAX_FILES];
    zbc_ansi_fd_node_t *free_fd_list;
    int next_fd;

    /*--- Internal: other state ---*/
    int last_errno;
    /* Clock accumulator using 64-bit time */
    uint64_t start_clock;              /**< Stored as uint64_t to avoid time.h */
    char path_buf[ZBC_ANSI_PATH_BUF_MAX];  /**< For null-terminating paths */
    int initialized;
} zbc_ansi_insecure_state_t;

/*========================================================================
 * Insecure Backend API
 *========================================================================*/

/**
 * Initialize insecure ANSI backend.
 *
 * @warning This provides unrestricted filesystem access!
 * Guest code can read/write/delete any file the host process can access.
 * Only use for trusted guest code or debugging.
 *
 * @param state Caller-allocated state structure
 */
void zbc_ansi_insecure_init(zbc_ansi_insecure_state_t *state);

/**
 * Clean up insecure ANSI backend state.
 *
 * Closes all open files. State can be reused after calling
 * zbc_ansi_insecure_init().
 *
 * @param state Initialized state
 */
void zbc_ansi_insecure_cleanup(zbc_ansi_insecure_state_t *state);

/*========================================================================
 * Console-Only Backend State
 *========================================================================*/

/**
 * Console-only ANSI backend state (caller-allocated).
 *
 * Initialize with zbc_ansi_console_init() before use.
 * This backend only supports console I/O (stdin/stdout/stderr) and exit.
 * File operations are rejected with EACCES.
 */
typedef struct zbc_ansi_console_state_s {
    /* Exit callback */
    void (*on_exit)(void *ctx, unsigned int reason, unsigned int subcode);
    void *callback_ctx;

    /* Timer config callback */
    void (*on_timer_config)(void *ctx, unsigned int rate_hz);
    void *timer_callback_ctx;

    /* Other state */
    int last_errno;
    uint64_t start_clock;
    int initialized;
} zbc_ansi_console_state_t;

/*========================================================================
 * Console-Only Backend API
 *========================================================================*/

/**
 * Initialize console-only ANSI backend.
 *
 * @param state Caller-allocated state structure
 */
void zbc_ansi_console_init(zbc_ansi_console_state_t *state);

/**
 * Set exit callback for console-only backend.
 *
 * @param state   Initialized state
 * @param on_exit Called when guest requests exit
 * @param ctx     Context passed to callback
 */
void zbc_ansi_console_set_exit_callback(
    zbc_ansi_console_state_t *state,
    void (*on_exit)(void *ctx, unsigned int reason, unsigned int subcode),
    void *ctx);

/**
 * Set timer config callback for console-only backend.
 *
 * @param state           Initialized state
 * @param on_timer_config Called when guest configures timer
 * @param ctx             Context passed to callback
 */
void zbc_ansi_console_set_timer_callback(
    zbc_ansi_console_state_t *state,
    void (*on_timer_config)(void *ctx, unsigned int rate_hz),
    void *ctx);

/**
 * Clean up console-only ANSI backend state.
 *
 * @param state Initialized state
 */
void zbc_ansi_console_cleanup(zbc_ansi_console_state_t *state);

#ifdef __cplusplus
} /* extern "C" */
} /* namespace zbc */
} /* namespace llvm */
#endif

#endif /* ZBC_BACKEND_ANSI_H */
