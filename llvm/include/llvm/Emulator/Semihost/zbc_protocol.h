/**
 * @file zbc_protocol.h
 * @brief ZBC Semihosting Protocol Definitions
 *
 * Wire protocol constants: opcodes, RIFF FourCC codes, register definitions,
 * error codes, and byte manipulation helpers.
 *
 * Vendored from https://github.com/johnwbyrd/semihost
 * Modified to add llvm::zbc namespace when compiled as C++.
 */

#ifndef ZBC_PROTOCOL_H
#define ZBC_PROTOCOL_H

#ifdef __cplusplus
namespace llvm {
namespace zbc {
extern "C" {
#endif

/*========================================================================
 * Standard includes - C90 compatible
 *========================================================================*/

#include <stddef.h> /* for size_t */

#ifdef ZBC_NO_STDINT
/* Manual fallback for systems without <stdint.h> */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
#ifdef _MSC_VER
typedef unsigned __int64 uint64_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uintmax_t;
typedef signed __int64 intmax_t;
#else
typedef unsigned long long uint64_t;
typedef signed long long int64_t;
typedef unsigned long long uintmax_t;
typedef signed long long intmax_t;
#endif
#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || \
    defined(__LP64__)
typedef uint64_t uintptr_t;
#else
typedef uint32_t uintptr_t;
#endif
#else
#include <stdint.h>
#endif

/*========================================================================
 * ARM Semihosting opcodes
 *========================================================================*/

#define SH_SYS_OPEN           0x01
#define SH_SYS_CLOSE          0x02
#define SH_SYS_WRITEC         0x03
#define SH_SYS_WRITE0         0x04
#define SH_SYS_WRITE          0x05
#define SH_SYS_READ           0x06
#define SH_SYS_READC          0x07
#define SH_SYS_ISERROR        0x08
#define SH_SYS_ISTTY          0x09
#define SH_SYS_SEEK           0x0A
#define SH_SYS_FLEN           0x0C
#define SH_SYS_TMPNAM         0x0D
#define SH_SYS_REMOVE         0x0E
#define SH_SYS_RENAME         0x0F
#define SH_SYS_CLOCK          0x10
#define SH_SYS_TIME           0x11
#define SH_SYS_SYSTEM         0x12
#define SH_SYS_ERRNO          0x13
#define SH_SYS_GET_CMDLINE    0x15
#define SH_SYS_HEAPINFO       0x16
#define SH_SYS_EXIT           0x18
#define SH_SYS_EXIT_EXTENDED  0x20
#define SH_SYS_ELAPSED        0x30
#define SH_SYS_TICKFREQ       0x31
#define SH_SYS_TIMER_CONFIG   0x32  /* Configure periodic timer interrupt */

/*========================================================================
 * Open mode flags (ARM semihosting compatible)
 *========================================================================*/

#define SH_OPEN_R         0   /* "r" */
#define SH_OPEN_RB        1   /* "rb" */
#define SH_OPEN_R_PLUS    2   /* "r+" */
#define SH_OPEN_R_PLUS_B  3   /* "r+b" */
#define SH_OPEN_W         4   /* "w" */
#define SH_OPEN_WB        5   /* "wb" */
#define SH_OPEN_W_PLUS    6   /* "w+" */
#define SH_OPEN_W_PLUS_B  7   /* "w+b" */
#define SH_OPEN_A         8   /* "a" */
#define SH_OPEN_AB        9   /* "ab" */
#define SH_OPEN_A_PLUS    10  /* "a+" */
#define SH_OPEN_A_PLUS_B  11  /* "a+b" */

/*========================================================================
 * RIFF FourCC codes
 *========================================================================*/

/*
 * MAKEFOURCC - construct a FourCC code from 4 characters.
 * Result is little-endian: first char at lowest address.
 * e.g., MAKEFOURCC('R','I','F','F') == 0x46464952
 */
#define ZBC_MAKEFOURCC(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
     ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define ZBC_ID_RIFF  ZBC_MAKEFOURCC('R','I','F','F')
#define ZBC_ID_SEMI  ZBC_MAKEFOURCC('S','E','M','I')
#define ZBC_ID_CNFG  ZBC_MAKEFOURCC('C','N','F','G')
#define ZBC_ID_CALL  ZBC_MAKEFOURCC('C','A','L','L')
#define ZBC_ID_PARM  ZBC_MAKEFOURCC('P','A','R','M')
#define ZBC_ID_DATA  ZBC_MAKEFOURCC('D','A','T','A')
#define ZBC_ID_RETN  ZBC_MAKEFOURCC('R','E','T','N')
#define ZBC_ID_ERRO  ZBC_MAKEFOURCC('E','R','R','O')

/*========================================================================
 * Endianness values for CNFG chunk
 *========================================================================*/

#define ZBC_ENDIAN_LITTLE  0
#define ZBC_ENDIAN_BIG     1

/*========================================================================
 * PARM/DATA chunk type codes
 *========================================================================*/

#define ZBC_PARM_TYPE_INT     0x01
#define ZBC_PARM_TYPE_PTR     0x02
#define ZBC_DATA_TYPE_BINARY  0x01
#define ZBC_DATA_TYPE_STRING  0x02

/*========================================================================
 * Device register offsets
 *========================================================================*/

#define ZBC_REG_SIGNATURE   0x00  /* 8 bytes, R - ASCII "SEMIHOST" */
#define ZBC_REG_RIFF_PTR    0x08  /* 16 bytes, RW - pointer to RIFF buffer */
#define ZBC_REG_DOORBELL    0x18  /* 1 byte, W - write to trigger request */
#define ZBC_REG_STATUS      0x19  /* 1 byte, RW - interrupt pending (write 0 to clear) */
#define ZBC_REG_SIZE        0x20  /* Total register space: 32 bytes */

/*========================================================================
 * STATUS register values (at offset 0x19)
 *
 * The STATUS register indicates pending interrupt sources:
 *   0 = No interrupt pending
 *   1 = Timer tick occurred (from SYS_TIMER_CONFIG)
 *   2+ = Reserved for future interrupt sources
 *
 * Write 0 to STATUS to acknowledge the interrupt and deassert IRQ.
 *========================================================================*/

#define ZBC_STATUS_NONE   0  /* No interrupt pending */
#define ZBC_STATUS_TIMER  1  /* Timer tick occurred */

/*========================================================================
 * Signature bytes
 *========================================================================*/

#define ZBC_SIGNATURE_SIZE   8
#define ZBC_SIGNATURE_STR    "SEMIHOST"
#define ZBC_SIGNATURE_BYTE0  0x53  /* 'S' */
#define ZBC_SIGNATURE_BYTE1  0x45  /* 'E' */
#define ZBC_SIGNATURE_BYTE2  0x4D  /* 'M' */
#define ZBC_SIGNATURE_BYTE3  0x49  /* 'I' */
#define ZBC_SIGNATURE_BYTE4  0x48  /* 'H' */
#define ZBC_SIGNATURE_BYTE5  0x4F  /* 'O' */
#define ZBC_SIGNATURE_BYTE6  0x53  /* 'S' */
#define ZBC_SIGNATURE_BYTE7  0x54  /* 'T' */

/*========================================================================
 * Library error codes
 *
 * All errors are negative, allowing functions to return positive values
 * for success (e.g., byte counts) and negative for errors.
 * Check: if (rc < 0) { handle error }
 *========================================================================*/

#define ZBC_OK                     0
#define ZBC_ERR_NULL_ARG          (-1)   /* NULL pointer passed */
#define ZBC_ERR_HEADER_OVERFLOW   (-2)   /* Chunk header extends past container */
#define ZBC_ERR_DATA_OVERFLOW     (-3)   /* Chunk data extends past container */
#define ZBC_ERR_BAD_RIFF_MAGIC    (-4)   /* Not a RIFF container */
#define ZBC_ERR_BAD_FORM_TYPE     (-5)   /* Wrong form type (expected SEMI) */
#define ZBC_ERR_RIFF_OVERFLOW     (-6)   /* RIFF size exceeds buffer */
#define ZBC_ERR_NOT_FOUND         (-7)   /* Chunk with requested ID not found */
#define ZBC_ERR_BUFFER_FULL       (-8)   /* Not enough space to write chunk */
#define ZBC_ERR_UNKNOWN_OPCODE    (-9)   /* Opcode not in table */
#define ZBC_ERR_NOT_INITIALIZED   (-10)  /* State not initialized */
#define ZBC_ERR_DEVICE_ERROR      (-11)  /* Device communication error */
#define ZBC_ERR_TIMEOUT           (-12)  /* Operation timed out */
#define ZBC_ERR_INVALID_ARG       (-13)  /* Invalid argument */
#define ZBC_ERR_PARSE_ERROR       (-14)  /* Malformed RIFF data */

/*========================================================================
 * Protocol error codes (in ERRO chunk)
 *========================================================================*/

#define ZBC_PROTO_ERR_INVALID_CHUNK   0x01
#define ZBC_PROTO_ERR_MALFORMED_RIFF  0x02
#define ZBC_PROTO_ERR_MISSING_CNFG    0x03
#define ZBC_PROTO_ERR_UNSUPPORTED_OP  0x04
#define ZBC_PROTO_ERR_INVALID_PARAMS  0x05

/*========================================================================
 * RIFF chunk structures
 *
 * These structs represent the wire format. Use them for both reading
 * (overlay onto buffer) and writing (fill in fields directly).
 *
 * Note: We use [1] instead of [] for C90 compatibility. The [1] is a
 * placeholder - actual data may be larger. Access via pointer arithmetic
 * on the data field.
 *========================================================================*/

/**
 * Generic RIFF chunk: id(4) + size(4) + data[size].
 *
 * All chunk access goes through this struct - no magic offsets.
 */
typedef struct {
    uint32_t id;      /**< FourCC, little-endian */
    uint32_t size;    /**< Payload size in bytes (not including this header) */
    uint8_t  data[1]; /**< Chunk payload (variable length, [1] for C90) */
} zbc_chunk_t;

/**
 * RIFF container: "RIFF"(4) + size(4) + form_type(4) + chunks...
 */
typedef struct {
    uint32_t riff_id;    /**< Must be ZBC_ID_RIFF */
    uint32_t size;       /**< Size of everything after this field */
    uint32_t form_type;  /**< e.g., ZBC_ID_SEMI */
    uint8_t  data[1];    /**< Container chunks (variable length, [1] for C90) */
} zbc_riff_t;

/*========================================================================
 * Chunk payload structures
 *
 * Each chunk type has a payload struct. Access fields by name, not offset.
 *========================================================================*/

/** CNFG chunk payload */
typedef struct {
    uint8_t int_size;     /**< Guest integer size (1-4) */
    uint8_t ptr_size;     /**< Guest pointer size (1-8) */
    uint8_t endianness;   /**< 0=little, 1=big */
    uint8_t reserved;     /**< Reserved for future use */
} zbc_cnfg_payload_t;

/** CALL chunk header (before sub-chunks) */
typedef struct {
    uint8_t opcode;       /**< SH_SYS_* opcode */
    uint8_t reserved[3];  /**< Reserved for future use */
} zbc_call_header_t;

/** PARM chunk payload */
typedef struct {
    uint8_t type;         /**< ZBC_PARM_TYPE_INT or ZBC_PARM_TYPE_PTR */
    uint8_t reserved[3];  /**< Reserved for future use */
    uint8_t value[1];     /**< int_size or ptr_size bytes, native endian ([1] for C90) */
} zbc_parm_payload_t;

/** DATA chunk payload */
typedef struct {
    uint8_t type;         /**< ZBC_DATA_TYPE_BINARY or ZBC_DATA_TYPE_STRING */
    uint8_t reserved[3];  /**< Reserved for future use */
    uint8_t payload[1];   /**< Variable-length data ([1] for C90) */
} zbc_data_payload_t;

/** ERRO chunk payload */
typedef struct {
    uint16_t error_code;  /**< Protocol error code, little-endian */
    uint8_t reserved[2];  /**< Reserved for future use */
    /* Optional error message follows */
} zbc_erro_payload_t;

/*
 * RETN chunk payload:
 *   result[int_size] - native endian return value
 *   errno[4]         - little-endian errno
 *   optional DATA sub-chunk
 *
 * Note: result size varies by guest int_size, so we access via byte array.
 */
typedef struct {
    uint8_t data[1];  /* result[int_size] + errno[4] + optional sub-chunks ([1] for C90) */
} zbc_retn_payload_t;

/*========================================================================
 * Wire format size constants
 *
 * C90 requires [1] instead of [] for flexible arrays, which adds padding.
 * These constants give the actual wire format sizes for offset calculations.
 *========================================================================*/

/* RIFF header: "RIFF"(4) + size(4) + form_type(4) = 12 bytes */
#define ZBC_RIFF_HDR_SIZE    12

/* Chunk header: id(4) + size(4) = 8 bytes */
#define ZBC_CHUNK_HDR_SIZE   8

/* Round size up to word boundary (RIFF requires even-byte alignment) */
#define ZBC_PAD_SIZE(size) (((size) + 1U) & ~(size_t)1U)

/* Total bytes for a chunk on wire: header + padded payload */
#define ZBC_CHUNK_WIRE_SIZE(chunk) \
    (ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE((chunk)->size))

/*========================================================================
 * Payload wire sizes (without struct padding)
 *========================================================================*/

/* CNFG payload: int_size(1) + ptr_size(1) + endianness(1) + reserved(1) = 4 bytes */
#define ZBC_CNFG_PAYLOAD_SIZE    4

/* CALL header: opcode(1) + reserved(3) = 4 bytes */
#define ZBC_CALL_HDR_PAYLOAD_SIZE  4

/* PARM header: type(1) + reserved(3) = 4 bytes (value follows) */
#define ZBC_PARM_HDR_SIZE    4

/* DATA header: type(1) + reserved(3) = 4 bytes (payload follows) */
#define ZBC_DATA_HDR_SIZE    4

/* ERRO payload: error_code(2) + reserved(2) = 4 bytes */
#define ZBC_ERRO_PAYLOAD_SIZE    4

/* RETN errno field is always 32-bit little-endian (spec line 667) */
#define ZBC_RETN_ERRNO_SIZE      4

/* Recommended ERRO pre-allocation size (error code + optional message) */
#define ZBC_ERRO_PREALLOC_SIZE   64

/*========================================================================
 * Legacy defines (kept for compatibility)
 *========================================================================*/

#define ZBC_HDR_SIZE         ZBC_RIFF_HDR_SIZE
#define ZBC_CNFG_DATA_SIZE   ZBC_CNFG_PAYLOAD_SIZE
#define ZBC_CNFG_TOTAL_SIZE  (ZBC_CHUNK_HDR_SIZE + ZBC_CNFG_PAYLOAD_SIZE)
#define ZBC_CALL_HDR_SIZE    (ZBC_CHUNK_HDR_SIZE + ZBC_CALL_HDR_PAYLOAD_SIZE)
#define ZBC_RETN_HDR_SIZE    ZBC_CHUNK_HDR_SIZE

/*========================================================================
 * Helper macros for little-endian byte manipulation
 *========================================================================*/

#define ZBC_WRITE_U32_LE(buf, val) \
    do { \
        unsigned char *_p = (unsigned char *)(buf); \
        uint32_t _v = (uint32_t)(val); \
        _p[0] = (unsigned char)(_v & 0xFFU); \
        _p[1] = (unsigned char)((_v >> 8) & 0xFFU); \
        _p[2] = (unsigned char)((_v >> 16) & 0xFFU); \
        _p[3] = (unsigned char)((_v >> 24) & 0xFFU); \
    } while (0)

#define ZBC_READ_U32_LE(buf) \
    ((uint32_t)(((const unsigned char *)(buf))[0]) | \
     ((uint32_t)(((const unsigned char *)(buf))[1]) << 8) | \
     ((uint32_t)(((const unsigned char *)(buf))[2]) << 16) | \
     ((uint32_t)(((const unsigned char *)(buf))[3]) << 24))

#define ZBC_WRITE_U16_LE(buf, val) \
    do { \
        unsigned char *_p = (unsigned char *)(buf); \
        uint16_t _v = (uint16_t)(val); \
        _p[0] = (unsigned char)(_v & 0xFFU); \
        _p[1] = (unsigned char)((_v >> 8) & 0xFFU); \
    } while (0)

#define ZBC_READ_U16_LE(buf) \
    ((uint16_t)(((const unsigned char *)(buf))[0]) | \
     ((uint16_t)(((const unsigned char *)(buf))[1]) << 8))

#define ZBC_WRITE_FOURCC(buf, c0, c1, c2, c3) \
    do { \
        unsigned char *_p = (unsigned char *)(buf); \
        _p[0] = (unsigned char)(c0); \
        _p[1] = (unsigned char)(c1); \
        _p[2] = (unsigned char)(c2); \
        _p[3] = (unsigned char)(c3); \
    } while (0)

/*========================================================================
 * Memory helpers (no libc dependency for bare-metal targets)
 *========================================================================*/

#define ZBC_MEMCPY(dst, src, n) \
    do { \
        unsigned char *_d = (unsigned char *)(dst); \
        const unsigned char *_s = (const unsigned char *)(src); \
        size_t _n = (n); \
        while (_n-- > 0) *_d++ = *_s++; \
    } while (0)

#define ZBC_MEMSET(dst, val, n) \
    do { \
        unsigned char *_d = (unsigned char *)(dst); \
        unsigned char _v = (unsigned char)(val); \
        size_t _n = (n); \
        while (_n-- > 0) *_d++ = _v; \
    } while (0)

/*========================================================================
 * Logging (opt-in)
 *
 * By default, logging is disabled (zero overhead). To enable:
 *
 * Option A: Define your own log function before including headers:
 *   #define ZBC_LOG(level, fmt, ...) my_log(level, fmt, ##__VA_ARGS__)
 *   #include "zbc_semihost.h"
 *
 * Option B: Use the built-in printf-based logger (requires libc):
 *   #define ZBC_LOG_ENABLE 1
 *   #define ZBC_LOG_LEVEL ZBC_LOG_WARN
 *   #include "zbc_semihost.h"
 *
 * Log levels:
 *   ZBC_LOG_ERROR (1) - Unrecoverable failures, protocol violations
 *   ZBC_LOG_WARN  (2) - Timeouts, retries, validation rejections
 *   ZBC_LOG_INFO  (3) - Syscall dispatch, configuration
 *   ZBC_LOG_DEBUG (4) - Chunk parsing, buffer operations
 *========================================================================*/

/* Log levels */
#define ZBC_LOG_LVL_NONE  0
#define ZBC_LOG_LVL_ERROR 1
#define ZBC_LOG_LVL_WARN  2
#define ZBC_LOG_LVL_INFO  3
#define ZBC_LOG_LVL_DEBUG 4

/* Default log level if not specified */
#ifndef ZBC_LOG_LEVEL
#define ZBC_LOG_LEVEL ZBC_LOG_LVL_WARN
#endif

/* User can define ZBC_LOG() directly for custom logging */
#ifndef ZBC_LOG

#if defined(ZBC_LOG_ENABLE) && ZBC_LOG_ENABLE
  /* Built-in printf-based logger */
  #include <stdio.h>
  #define ZBC_LOG(level, fmt, ...) \
      do { \
          if ((level) <= ZBC_LOG_LEVEL) { \
              fprintf(stderr, "[ZBC:%d] " fmt "\n", (level), ##__VA_ARGS__); \
          } \
      } while (0)
#else
  /* Logging disabled - zero overhead */
  #define ZBC_LOG(level, fmt, ...) ((void)0)
#endif

#endif /* ZBC_LOG */

/* Convenience macros - use these for messages with format arguments */
#define ZBC_LOG_ERROR(fmt, ...)  ZBC_LOG(ZBC_LOG_LVL_ERROR, fmt, ##__VA_ARGS__)
#define ZBC_LOG_WARN(fmt, ...)   ZBC_LOG(ZBC_LOG_LVL_WARN, fmt, ##__VA_ARGS__)
#define ZBC_LOG_INFO(fmt, ...)   ZBC_LOG(ZBC_LOG_LVL_INFO, fmt, ##__VA_ARGS__)
#define ZBC_LOG_DEBUG(fmt, ...)  ZBC_LOG(ZBC_LOG_LVL_DEBUG, fmt, ##__VA_ARGS__)

/*
 * String-only variants (_S suffix) - use these for plain string messages
 * without format arguments. Avoids C90 "empty macro arguments" warning
 * from -Wpedantic when calling ZBC_LOG_ERROR("message") with no varargs.
 */
#define ZBC_LOG_ERROR_S(msg)  ZBC_LOG(ZBC_LOG_LVL_ERROR, "%s", msg)
#define ZBC_LOG_WARN_S(msg)   ZBC_LOG(ZBC_LOG_LVL_WARN, "%s", msg)
#define ZBC_LOG_INFO_S(msg)   ZBC_LOG(ZBC_LOG_LVL_INFO, "%s", msg)
#define ZBC_LOG_DEBUG_S(msg)  ZBC_LOG(ZBC_LOG_LVL_DEBUG, "%s", msg)

/*========================================================================
 * Alignment requirements
 *
 * On platforms with 4+ byte pointers, buffers are typically naturally
 * aligned, so we use byte-safe access to avoid undefined behavior from
 * struct overlay at potentially misaligned RIFF chunk boundaries.
 *
 * On smaller platforms (16-bit), we use direct overlay for size/speed
 * since the buffer may only be 2-byte aligned anyway.
 *
 * Set ZBC_REQUIRE_ALIGNED_ACCESS=1 to force byte-safe access path.
 * Set ZBC_REQUIRE_ALIGNED_ACCESS=0 to force direct overlay path.
 *========================================================================*/

#ifndef ZBC_REQUIRE_ALIGNED_ACCESS
  #if (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ >= 4) || \
      defined(__LP64__) || defined(_LP64) || defined(__x86_64__) || \
      defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64) || \
      defined(__i386__) || defined(_M_IX86) || defined(__arm__)
    /* 32-bit or 64-bit platform: use byte-safe access */
    #define ZBC_REQUIRE_ALIGNED_ACCESS 1
  #else
    /* 16-bit or unknown: use direct overlay */
    #define ZBC_REQUIRE_ALIGNED_ACCESS 0
  #endif
#endif

/*
 * ZBC_CHUNK_WRITE_HDR - write chunk header (id + size) to wire buffer
 *
 * On alignment-sensitive platforms, copies from aligned local struct.
 * On x86/ARM7+, casts directly (faster but technically UB).
 *
 * Parameters:
 *   wire_ptr   - destination in wire buffer (uint8_t *)
 *   id_val     - FourCC chunk ID (uint32_t)
 *   size_val   - chunk payload size (uint32_t)
 */
#if ZBC_REQUIRE_ALIGNED_ACCESS

#define ZBC_CHUNK_WRITE_HDR(wire_ptr, id_val, size_val) \
    do { \
        ZBC_WRITE_U32_LE((wire_ptr), (id_val)); \
        ZBC_WRITE_U32_LE((wire_ptr) + 4, (size_val)); \
    } while (0)

#else /* !ZBC_REQUIRE_ALIGNED_ACCESS */

#define ZBC_CHUNK_WRITE_HDR(wire_ptr, id_val, size_val) \
    do { \
        zbc_chunk_t *_c = (zbc_chunk_t *)(wire_ptr); \
        _c->id = (id_val); \
        _c->size = (size_val); \
    } while (0)

#endif /* ZBC_REQUIRE_ALIGNED_ACCESS */

/*
 * ZBC_RIFF_WRITE_HDR - write RIFF container header to wire buffer
 *
 * Parameters:
 *   wire_ptr   - destination in wire buffer (uint8_t *)
 *   size_val   - RIFF size field (uint32_t)
 *   form_val   - form type FourCC (uint32_t)
 */
#if ZBC_REQUIRE_ALIGNED_ACCESS

#define ZBC_RIFF_WRITE_HDR(wire_ptr, size_val, form_val) \
    do { \
        ZBC_WRITE_U32_LE((wire_ptr), ZBC_ID_RIFF); \
        ZBC_WRITE_U32_LE((wire_ptr) + 4, (size_val)); \
        ZBC_WRITE_U32_LE((wire_ptr) + 8, (form_val)); \
    } while (0)

#else /* !ZBC_REQUIRE_ALIGNED_ACCESS */

#define ZBC_RIFF_WRITE_HDR(wire_ptr, size_val, form_val) \
    do { \
        zbc_riff_t *_r = (zbc_riff_t *)(wire_ptr); \
        _r->riff_id = ZBC_ID_RIFF; \
        _r->size = (size_val); \
        _r->form_type = (form_val); \
    } while (0)

#endif /* ZBC_REQUIRE_ALIGNED_ACCESS */

/*
 * ZBC_PATCH_U32 - patch a uint32_t value in wire buffer
 *
 * Used to fix up size fields after writing chunk contents.
 */
#if ZBC_REQUIRE_ALIGNED_ACCESS

#define ZBC_PATCH_U32(wire_ptr, val) \
    ZBC_WRITE_U32_LE((wire_ptr), (val))

#else /* !ZBC_REQUIRE_ALIGNED_ACCESS */

#define ZBC_PATCH_U32(wire_ptr, val) \
    do { *(uint32_t *)(wire_ptr) = (val); } while (0)

#endif /* ZBC_REQUIRE_ALIGNED_ACCESS */

/*========================================================================
 * Opcode table types
 *========================================================================*/

/* Chunk types for request building */
#define ZBC_CHUNK_NONE       0  /* Unused slot */
#define ZBC_CHUNK_PARM_INT   1  /* PARM chunk with signed int */
#define ZBC_CHUNK_PARM_UINT  2  /* PARM chunk with unsigned int */
#define ZBC_CHUNK_DATA_PTR   3  /* DATA chunk: ptr from slot, len from len_slot */
#define ZBC_CHUNK_DATA_STR   4  /* DATA chunk: null-terminated string */
#define ZBC_CHUNK_DATA_BYTE  5  /* DATA chunk: single byte from *(uint8_t*)slot */

/* Response types */
#define ZBC_RESP_INT       0  /* Returns integer in result */
#define ZBC_RESP_DATA      1  /* Returns DATA chunk, copy to dest_slot */
#define ZBC_RESP_HEAPINFO  2  /* Returns 4 pointer values */
#define ZBC_RESP_ELAPSED   3  /* Returns 8-byte tick count */

/* Single parameter/data chunk descriptor */
typedef struct {
    uint8_t type;      /* ZBC_CHUNK_* */
    uint8_t slot;      /* args[] index for value or pointer */
    uint8_t len_slot;  /* args[] index for length (DATA_PTR only) */
} zbc_chunk_desc_t;

/* Opcode table entry */
typedef struct {
    uint8_t opcode;              /* SH_SYS_* */
    uint8_t arg_count;           /* Number of args[] slots used */
    zbc_chunk_desc_t params[4];  /* Request chunks to emit */
    uint8_t resp_type;           /* ZBC_RESP_* */
    uint8_t resp_dest;           /* args[] index for response data */
    uint8_t resp_len_slot;       /* args[] index for max length */
} zbc_opcode_entry_t;

/**
 * Look up an opcode table entry by opcode number.
 *
 * @param opcode  The SH_SYS_* opcode to look up
 * @return Pointer to opcode entry, or NULL if not found
 */
const zbc_opcode_entry_t *zbc_opcode_lookup(int opcode);

/**
 * Get the number of entries in the opcode table.
 *
 * @return Number of defined opcodes
 */
int zbc_opcode_count(void);

/*========================================================================
 * RIFF helper functions (shared by client and host)
 *========================================================================*/

/**
 * Calculate string length without libc dependency.
 *
 * @param s  Null-terminated string
 * @return Length of string (not including null terminator)
 */
size_t zbc_strlen(const char *s);

/**
 * Write an unsigned integer in specified endianness.
 *
 * @param buf         Destination buffer
 * @param value       Value to write
 * @param size        Size in bytes (1-4)
 * @param endianness  ZBC_ENDIAN_LITTLE or ZBC_ENDIAN_BIG
 */
void zbc_write_native_uint(uint8_t *buf, uintptr_t value, int size,
                           int endianness);

/**
 * Read a signed integer in specified endianness.
 *
 * @param buf         Source buffer
 * @param size        Size in bytes (1-8)
 * @param endianness  ZBC_ENDIAN_LITTLE or ZBC_ENDIAN_BIG
 * @return Signed integer value
 */
intptr_t zbc_read_native_int(const uint8_t *buf, int size, int endianness);

/**
 * Read an unsigned integer in specified endianness.
 *
 * @param buf         Source buffer
 * @param size        Size in bytes (1-8)
 * @param endianness  ZBC_ENDIAN_LITTLE or ZBC_ENDIAN_BIG
 * @return Unsigned integer value
 */
uintptr_t zbc_read_native_uint(const uint8_t *buf, int size, int endianness);

/**
 * Begin writing a new RIFF chunk.
 *
 * Writes the FourCC ID and reserves space for the size field.
 * Returns pointer to the size field so caller can patch it later.
 *
 * @param buf       Buffer to write to
 * @param capacity  Total buffer capacity
 * @param offset    Current write offset (updated on return)
 * @param fourcc    FourCC chunk ID
 * @return Pointer to size field for later patching, or NULL if no space
 */
uint8_t *zbc_riff_begin_chunk(uint8_t *buf, size_t capacity, size_t *offset,
                              uint32_t fourcc);

/**
 * Patch the size field of a chunk after writing its data.
 *
 * @param size_ptr   Pointer returned by zbc_riff_begin_chunk()
 * @param data_size  Actual size of chunk data
 */
void zbc_riff_patch_size(uint8_t *size_ptr, size_t data_size);

/**
 * Write raw bytes to a RIFF buffer.
 *
 * @param buf       Buffer to write to
 * @param capacity  Total buffer capacity
 * @param offset    Current write offset (updated on return)
 * @param data      Data to write
 * @param size      Number of bytes to write
 * @return ZBC_OK on success, ZBC_ERR_BUFFER_FULL if no space
 */
int zbc_riff_write_bytes(uint8_t *buf, size_t capacity, size_t *offset,
                         const void *data, size_t size);

/**
 * Add padding byte if needed for RIFF word alignment.
 *
 * RIFF requires chunks to be word-aligned (2-byte boundary).
 *
 * @param buf       Buffer to write to
 * @param capacity  Total buffer capacity
 * @param offset    Current write offset (updated on return)
 */
void zbc_riff_pad(uint8_t *buf, size_t capacity, size_t *offset);

/**
 * Read a RIFF chunk header.
 *
 * @param buf       Buffer to read from
 * @param capacity  Total buffer capacity
 * @param offset    Offset to chunk header
 * @param[out] fourcc  Receives FourCC chunk ID
 * @param[out] size    Receives chunk data size
 * @return ZBC_OK on success, ZBC_ERR_HEADER_OVERFLOW if not enough data
 */
int zbc_riff_read_header(const uint8_t *buf, size_t capacity, size_t offset,
                         uint32_t *fourcc, uint32_t *size);

/**
 * Skip past a chunk to the next sibling.
 *
 * @param buf       Buffer containing chunk
 * @param capacity  Total buffer capacity
 * @param offset    Offset to current chunk header
 * @return Offset to next chunk, or capacity if at end
 */
size_t zbc_riff_skip_chunk(const uint8_t *buf, size_t capacity, size_t offset);

/**
 * Begin writing a RIFF container.
 *
 * Writes "RIFF", reserves space for size, and writes form type.
 *
 * @param buf        Buffer to write to
 * @param capacity   Total buffer capacity
 * @param offset     Current write offset (updated on return)
 * @param form_type  Form type FourCC (e.g., ZBC_ID_SEMI)
 * @return Pointer to size field for later patching, or NULL if no space
 */
uint8_t *zbc_riff_begin_container(uint8_t *buf, size_t capacity, size_t *offset,
                                  uint32_t form_type);

/**
 * Validate a RIFF container header.
 *
 * @param buf                Buffer containing RIFF data
 * @param capacity           Total buffer capacity
 * @param expected_form_type Expected form type (e.g., ZBC_ID_SEMI)
 * @return ZBC_OK on success, ZBC_ERR_HEADER_OVERFLOW if buffer too small,
 *         ZBC_ERR_BAD_RIFF_MAGIC if not "RIFF", ZBC_ERR_BAD_FORM_TYPE if wrong form
 */
int zbc_riff_validate_container(const uint8_t *buf, size_t capacity,
                                uint32_t expected_form_type);

/*========================================================================
 * New chunk-based API (struct-based, no magic offsets)
 *========================================================================*/

/**
 * Validate that a chunk fits within container bounds.
 *
 * @param chunk          Pointer to chunk to validate
 * @param container_end  First byte PAST the valid container region
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_HEADER_OVERFLOW, or
 *         ZBC_ERR_DATA_OVERFLOW
 */
int zbc_chunk_validate(const zbc_chunk_t *chunk, const uint8_t *container_end);

/**
 * Get pointer to next sibling chunk.
 *
 * Caller MUST validate the returned chunk before accessing it.
 *
 * @param[out] out  Receives pointer to next chunk
 * @param chunk     Current chunk
 * @return ZBC_OK or ZBC_ERR_NULL_ARG
 */
int zbc_chunk_next(zbc_chunk_t **out, const zbc_chunk_t *chunk);

/**
 * Get pointer to first sub-chunk within a container chunk.
 *
 * @param[out] out     Receives pointer to first sub-chunk
 * @param container    Parent chunk (e.g., CALL) that contains sub-chunks
 * @param header_size  Bytes to skip before sub-chunks
 *                     (e.g., sizeof(zbc_call_header_t))
 * @return ZBC_OK or ZBC_ERR_NULL_ARG
 */
int zbc_chunk_first_sub(zbc_chunk_t **out, const zbc_chunk_t *container,
                        size_t header_size);

/**
 * Get container end pointer (for validating sub-chunks).
 *
 * @param[out] out  Receives pointer to first byte past chunk data
 * @param chunk     The container chunk
 * @return ZBC_OK or ZBC_ERR_NULL_ARG
 */
int zbc_chunk_end(const uint8_t **out, const zbc_chunk_t *chunk);

/**
 * Find chunk by ID within container bounds.
 *
 * Validates each chunk while searching.
 *
 * @param[out] out  Receives pointer to found chunk
 * @param start     Start of search region (first chunk)
 * @param end       First byte PAST the search region
 * @param id        FourCC to find
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_NOT_FOUND,
 *         ZBC_ERR_HEADER_OVERFLOW, or ZBC_ERR_DATA_OVERFLOW
 */
int zbc_chunk_find(zbc_chunk_t **out, const uint8_t *start, const uint8_t *end,
                   uint32_t id);

/**
 * Validate RIFF container.
 *
 * @param riff           Pointer to RIFF container
 * @param buf_size       Total buffer size
 * @param expected_form  Expected form type (e.g., ZBC_ID_SEMI)
 * @return ZBC_OK, ZBC_ERR_NULL_ARG, ZBC_ERR_BAD_RIFF_MAGIC,
 *         ZBC_ERR_BAD_FORM_TYPE, or ZBC_ERR_RIFF_OVERFLOW
 */
int zbc_riff_validate(const zbc_riff_t *riff, size_t buf_size,
                      uint32_t expected_form);

/**
 * Get end pointer for RIFF container.
 *
 * @param[out] out  Receives pointer to first byte past RIFF data
 * @param riff      The RIFF container
 * @return ZBC_OK or ZBC_ERR_NULL_ARG
 */
int zbc_riff_end(const uint8_t **out, const zbc_riff_t *riff);

/*========================================================================
 * Parsed RIFF structure
 *
 * Parse once, then access fields. No state machine, no interleaved
 * validation. Just pointers into the original buffer.
 *========================================================================*/

#define ZBC_MAX_PARMS 8   /**< Maximum PARM chunks in parsed structure */
#define ZBC_MAX_DATA  4   /**< Maximum DATA chunks in parsed structure */

/**
 * Parsed RIFF SEMI structure.
 *
 * Parse once with zbc_riff_parse(), then access fields directly.
 * Pointers reference the original buffer (no copies).
 */
typedef struct {
    /* Guest configuration (from CNFG chunk) */
    uint8_t int_size;     /**< Guest integer size (1-4) */
    uint8_t ptr_size;     /**< Guest pointer size (1-8) */
    uint8_t endianness;   /**< Guest endianness (ZBC_ENDIAN_*) */
    uint8_t has_cnfg;     /**< 1 if CNFG chunk was present */

    /* Request: CALL chunk info */
    uint8_t opcode;       /**< SH_SYS_* opcode from CALL chunk */
    uint8_t has_call;     /**< 1 if CALL chunk was present */

    /* Request: parameters from PARM sub-chunks */
    int parm_count;       /**< Number of PARM values parsed */
    intptr_t parms[ZBC_MAX_PARMS];  /**< Decoded parameter values */

    /* Request/Response: data from DATA sub-chunks */
    int data_count;       /**< Number of DATA chunks parsed */
    struct {
        const uint8_t *ptr;  /**< Pointer to data payload */
        size_t size;         /**< Size of data payload */
    } data[ZBC_MAX_DATA];    /**< DATA chunk references */

    /* Response: RETN chunk info */
    intptr_t result;      /**< Return value from RETN chunk */
    int host_errno;       /**< Errno value from RETN chunk */
    uint8_t has_retn;     /**< 1 if RETN chunk was present */

    /* Response: ERRO chunk info */
    uint16_t proto_error; /**< Protocol error code from ERRO chunk */
    uint8_t has_erro;     /**< 1 if ERRO chunk was present */

    /*
     * Host-side: offsets to pre-allocated response chunks.
     *
     * These are byte offsets from the start of the RIFF buffer to the
     * chunk payload (after the chunk header). The host writes response
     * data directly to these locations within the pre-allocated space.
     */
    size_t retn_payload_offset;   /**< Offset to RETN chunk payload */
    size_t retn_payload_capacity; /**< Size field from RETN chunk header */
    size_t erro_payload_offset;   /**< Offset to ERRO chunk payload */
    size_t erro_payload_capacity; /**< Size field from ERRO chunk header */
} zbc_parsed_t;

/**
 * Parse a RIFF SEMI request buffer into a zbc_parsed_t structure.
 *
 * This is the host-side entry point for parsing client requests.
 * It walks all chunks (CNFG/CALL/PARM/DATA), extracts relevant fields,
 * and populates the parsed structure. After this call, the caller can
 * simply check fields like:
 *
 *     if (parsed.has_call) { dispatch parsed.opcode; }
 *
 * Note: Clients should not call this function. Client response parsing
 * is done internally by zbc_parse_response() which only extracts RETN/ERRO.
 *
 * @param[out] out  Receives parsed structure
 * @param buf       RIFF buffer to parse
 * @param buf_size  Size of buffer
 * @param int_size  Guest int size (for decoding PARM/RETN values)
 * @param endian    Guest endianness (ZBC_ENDIAN_*)
 * @return ZBC_OK on success, error code on parse failure
 */
int zbc_riff_parse_request(zbc_parsed_t *out, const uint8_t *buf, size_t buf_size,
                           int int_size, int endian);

#ifdef __cplusplus
} /* extern "C" */
} /* namespace zbc */
} /* namespace llvm */
#endif

#endif /* ZBC_PROTOCOL_H */
