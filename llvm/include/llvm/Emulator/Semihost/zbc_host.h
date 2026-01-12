/**
 * @file zbc_host.h
 * @brief ZBC Semihosting Host API
 *
 * Host-side API for processing semihosting requests. Use this when
 * implementing a semihosting device in an emulator or virtual machine.
 *
 * Vendored from https://github.com/johnwbyrd/semihost
 * Modified to add llvm::zbc namespace when compiled as C++.
 */

#ifndef ZBC_HOST_H
#define ZBC_HOST_H

#include "zbc_protocol.h"

#ifdef __cplusplus
namespace llvm {
namespace zbc {
extern "C" {
#endif

/*========================================================================
 * Memory Operations
 *========================================================================*/

/**
 * Memory access callbacks for reading/writing guest memory.
 *
 * You must implement all four callbacks. The ctx parameter is passed
 * through from zbc_host_init().
 */
typedef struct {
    uint8_t (*read_u8)(uintptr_t addr, void *ctx);     /**< Read single byte */
    void (*write_u8)(uintptr_t addr, uint8_t val, void *ctx);  /**< Write single byte */
    void (*read_block)(void *dest, uintptr_t addr, size_t size, void *ctx);  /**< Read block */
    void (*write_block)(uintptr_t addr, const void *src, size_t size, void *ctx);  /**< Write block */
} zbc_host_mem_ops_t;

/*========================================================================
 * Host Types
 *========================================================================*/

/** Forward declaration for backend */
struct zbc_backend_s;

/**
 * Host state structure.
 *
 * Initialize with zbc_host_init() before use.
 */
typedef struct {
    zbc_host_mem_ops_t mem_ops;        /**< Memory operation callbacks */
    void *mem_ctx;                      /**< Context for memory callbacks */
    const struct zbc_backend_s *backend;  /**< Backend vtable */
    void *backend_ctx;                  /**< Backend-specific state */
    uint8_t *work_buf;                  /**< Working buffer for RIFF parsing */
    size_t work_buf_size;               /**< Size of working buffer */
    uint8_t guest_int_size;             /**< Guest integer size (from CNFG) */
    uint8_t guest_ptr_size;             /**< Guest pointer size (from CNFG) */
    uint8_t guest_endianness;           /**< Guest endianness (from CNFG) */
    uint8_t cnfg_received;              /**< 1 if CNFG has been received */
} zbc_host_state_t;

/*========================================================================
 * Host API
 *========================================================================*/

/**
 * Initialize host state.
 *
 * @param state         Host state structure to initialize
 * @param mem_ops       Memory operation callbacks
 * @param mem_ctx       Context passed to memory callbacks
 * @param backend       Backend vtable (from zbc_backend_*() factory)
 * @param backend_ctx   Backend-specific state
 * @param work_buf      Working buffer for RIFF parsing
 * @param work_buf_size Size of working buffer (recommended: 1024 bytes)
 */
void zbc_host_init(zbc_host_state_t *state,
                   const zbc_host_mem_ops_t *mem_ops, void *mem_ctx,
                   const struct zbc_backend_s *backend, void *backend_ctx,
                   uint8_t *work_buf, size_t work_buf_size);

/**
 * Process a semihosting request.
 *
 * Call this when the guest writes to DOORBELL. The function:
 * 1. Reads the RIFF request from guest memory
 * 2. Parses the CNFG chunk (first request only)
 * 3. Parses the CALL chunk and sub-chunks
 * 4. Dispatches to the appropriate backend function
 * 5. Writes the RETN (or ERRO) chunk back to guest memory
 *
 * After this returns, set STATUS bit 0 (RESPONSE_READY) in your
 * device register emulation.
 *
 * @param state     Initialized host state
 * @param riff_addr Guest address of RIFF buffer
 * @return ZBC_OK on success, error code on failure
 */
int zbc_host_process(zbc_host_state_t *state, uintptr_t riff_addr);

/**
 * Read an integer from guest-endian data.
 *
 * Converts from guest endianness to host integer.
 *
 * @param state Host state (for guest endianness)
 * @param data  Pointer to integer data
 * @param size  Size of integer in bytes
 * @return Integer value
 */
intptr_t zbc_host_read_guest_int(const zbc_host_state_t *state,
                                 const uint8_t *data, size_t size);

/**
 * Write an integer in guest-endian format.
 *
 * Converts from host integer to guest endianness.
 *
 * @param state Host state (for guest endianness)
 * @param data  Destination buffer
 * @param value Integer value to write
 * @param size  Size of integer in bytes
 */
void zbc_host_write_guest_int(const zbc_host_state_t *state,
                              uint8_t *data, uintptr_t value, size_t size);

#ifdef __cplusplus
} /* extern "C" */
} /* namespace zbc */
} /* namespace llvm */
#endif

#endif /* ZBC_HOST_H */
