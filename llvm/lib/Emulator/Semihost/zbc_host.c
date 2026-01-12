/*
 * ZBC Semihosting Host Library
 *
 * Parses RIFF requests, dispatches to backend, builds responses.
 */

#define ZBC_HOST
#include "llvm/Emulator/Semihost/zbc_semihost.h"

#ifndef ENOSYS
#define ENOSYS 38
#endif

/*========================================================================
 * Initialization
 *========================================================================*/

void zbc_host_init(zbc_host_state_t *state, const zbc_host_mem_ops_t *mem_ops,
                   void *mem_ctx, const zbc_backend_t *backend,
                   void *backend_ctx, uint8_t *work_buf, size_t work_buf_size) {
  if (!state) {
    return;
  }

  state->mem_ops = *mem_ops;
  state->mem_ctx = mem_ctx;
  state->backend = backend;
  state->backend_ctx = backend_ctx;
  state->work_buf = work_buf;
  state->work_buf_size = work_buf_size;
  state->guest_int_size = 0;
  state->guest_ptr_size = 0;
  state->guest_endianness = ZBC_ENDIAN_LITTLE;
  state->cnfg_received = 0;
}

/*========================================================================
 * Guest memory access
 *========================================================================*/

static void read_guest(zbc_host_state_t *state, void *dest, uintptr_t addr,
                       size_t size) {
  if (state->mem_ops.read_block) {
    state->mem_ops.read_block(dest, addr, size, state->mem_ctx);
  } else {
    uint8_t *d = (uint8_t *)dest;
    size_t i;
    for (i = 0; i < size; i++) {
      d[i] = state->mem_ops.read_u8(addr + i, state->mem_ctx);
    }
  }
}

static void write_guest(zbc_host_state_t *state, uintptr_t addr,
                        const void *src, size_t size) {
  if (state->mem_ops.write_block) {
    state->mem_ops.write_block(addr, src, size, state->mem_ctx);
  } else {
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < size; i++) {
      state->mem_ops.write_u8(addr + i, s[i], state->mem_ctx);
    }
  }
}

/*========================================================================
 * Value conversion (guest endianness)
 *========================================================================*/

intptr_t zbc_host_read_guest_int(const zbc_host_state_t *state,
                                 const uint8_t *data, size_t size) {
  return zbc_read_native_int(data, (int)size, state->guest_endianness);
}

void zbc_host_write_guest_int(const zbc_host_state_t *state, uint8_t *data,
                              uintptr_t value, size_t size) {
  zbc_write_native_uint(data, value, (int)size, state->guest_endianness);
}

/*========================================================================
 * Response building
 *
 * The client pre-allocates RETN and ERRO chunks. The host writes only
 * the payload contents within those pre-allocated bounds. The RIFF
 * structure is never modified by the host.
 *========================================================================*/

/*
 * Write ERRO payload to pre-allocated ERRO chunk.
 * Only writes the payload (error_code + reserved), not the chunk header.
 */
static void write_erro_payload(zbc_host_state_t *state, uintptr_t riff_addr,
                               const zbc_parsed_t *parsed, int error_code) {
  uint8_t buf[ZBC_ERRO_PAYLOAD_SIZE];

  if (!parsed->has_erro ||
      parsed->erro_payload_capacity < ZBC_ERRO_PAYLOAD_SIZE) {
    ZBC_LOG_ERROR_S("write_erro_payload: no pre-allocated ERRO chunk");
    return;
  }

  /* ERRO payload: error_code(2) + reserved(2) */
  ZBC_WRITE_U16_LE(buf, (uint16_t)error_code);
  buf[2] = 0;
  buf[3] = 0;

  write_guest(state, riff_addr + parsed->erro_payload_offset, buf,
              ZBC_ERRO_PAYLOAD_SIZE);
}

/*
 * Write RETN payload to pre-allocated RETN chunk.
 * Only writes the payload contents, not the chunk header.
 */
static void write_retn_payload(zbc_host_state_t *state, uintptr_t riff_addr,
                               const zbc_parsed_t *parsed, intptr_t result,
                               int err, const void *data, size_t data_size) {
  uint8_t buf[256];
  size_t pos;
  int int_size;
  size_t i;
  const uint8_t *src;

  if (!parsed->has_retn) {
    ZBC_LOG_ERROR_S("write_retn_payload: no pre-allocated RETN chunk");
    return;
  }

  int_size = state->guest_int_size;
  pos = 0;

  /* RETN payload: result[int_size] + errno[ZBC_RETN_ERRNO_SIZE] */
  zbc_host_write_guest_int(state, buf + pos, (uintptr_t)result, int_size);
  pos += (size_t)int_size;
  ZBC_WRITE_U32_LE(buf + pos, (uint32_t)err);
  pos += ZBC_RETN_ERRNO_SIZE;

  /* Add DATA sub-chunk if present */
  if (data && data_size > 0) {
    size_t data_payload_size = ZBC_DATA_HDR_SIZE + data_size;
    size_t padded_size = ZBC_PAD_SIZE(data_payload_size);

    /* DATA chunk header */
    ZBC_WRITE_U32_LE(buf + pos, ZBC_ID_DATA);
    pos += 4;
    ZBC_WRITE_U32_LE(buf + pos, (uint32_t)data_payload_size);
    pos += 4;

    /* DATA payload: type + reserved + data */
    buf[pos] = ZBC_DATA_TYPE_BINARY;
    buf[pos + 1] = 0;
    buf[pos + 2] = 0;
    buf[pos + 3] = 0;
    pos += ZBC_DATA_HDR_SIZE;

    src = (const uint8_t *)data;
    for (i = 0; i < data_size; i++) {
      buf[pos + i] = src[i];
    }
    pos += data_size;

    /* Pad to even boundary if needed */
    if (padded_size > data_payload_size) {
      buf[pos] = 0;
      pos++;
    }
  }

  /* Check capacity before writing */
  if (pos > parsed->retn_payload_capacity) {
    ZBC_LOG_ERROR("write_retn_payload: response %u exceeds capacity %u",
                  (unsigned)pos, (unsigned)parsed->retn_payload_capacity);
    return;
  }

  write_guest(state, riff_addr + parsed->retn_payload_offset, buf, pos);
}

/*
 * Write ERRO chunk for early errors (before parsing completes).
 * This is the fallback when we can't use the pre-allocated chunk.
 * It overwrites at the first chunk position - not ideal but necessary
 * for protocol errors that prevent parsing the RIFF structure.
 */
static void write_erro_early(zbc_host_state_t *state, uintptr_t addr,
                             int error_code) {
  uint8_t buf[ZBC_CHUNK_HDR_SIZE + ZBC_ERRO_PAYLOAD_SIZE];

  /* Build ERRO chunk: id + size + payload */
  ZBC_WRITE_U32_LE(buf, ZBC_ID_ERRO);
  ZBC_WRITE_U32_LE(buf + 4, ZBC_ERRO_PAYLOAD_SIZE);
  ZBC_WRITE_U16_LE(buf + 8, (uint16_t)error_code);
  buf[10] = 0;
  buf[11] = 0;

  /* Write at fixed offset - this is a fallback for parse failures */
  write_guest(state, addr + ZBC_RIFF_HDR_SIZE, buf, sizeof(buf));
}

/*========================================================================
 * Request parsing
 *
 * Uses the unified zbc_riff_parse() to extract all fields at once.
 *========================================================================*/

static int parse_request(zbc_host_state_t *state, uintptr_t riff_addr,
                         zbc_parsed_t *parsed) {
  uint8_t *buf = state->work_buf;
  size_t capacity = state->work_buf_size;
  size_t riff_total_size;
  uint32_t riff_size;
  int rc;

  /* Read RIFF header first to get size */
  read_guest(state, buf, riff_addr, ZBC_RIFF_HDR_SIZE);

  /* Check magic and get size */
  if (ZBC_READ_U32_LE(buf) != ZBC_ID_RIFF) {
    ZBC_LOG_ERROR_S("parse_request: bad RIFF magic");
    write_erro_early(state, riff_addr, ZBC_PROTO_ERR_MALFORMED_RIFF);
    return ZBC_ERR_PARSE_ERROR;
  }

  riff_size = ZBC_READ_U32_LE(buf + 4);
  riff_total_size = 4 + 4 + riff_size;

  if (riff_total_size > capacity) {
    ZBC_LOG_ERROR("parse_request: RIFF size=%u exceeds work_buf=%u",
                  (unsigned)riff_total_size, (unsigned)capacity);
    return ZBC_ERR_BUFFER_FULL;
  }

  /* Read entire RIFF structure */
  read_guest(state, buf, riff_addr, riff_total_size);

  /* Parse everything at once */
  rc = zbc_riff_parse_request(parsed, buf, riff_total_size,
                              state->guest_int_size, state->guest_endianness);
  if (rc != ZBC_OK) {
    ZBC_LOG_ERROR("parse_request: zbc_riff_parse failed (%d)", rc);
    write_erro_early(state, riff_addr, ZBC_PROTO_ERR_MALFORMED_RIFF);
    return ZBC_ERR_PARSE_ERROR;
  }

  /* Update state from CNFG if present */
  if (parsed->has_cnfg) {
    state->guest_int_size = parsed->int_size;
    state->guest_ptr_size = parsed->ptr_size;
    state->guest_endianness = parsed->endianness;
    state->cnfg_received = 1;
    ZBC_LOG_INFO("CNFG: int_size=%u ptr_size=%u endian=%u",
                 (unsigned)parsed->int_size, (unsigned)parsed->ptr_size,
                 (unsigned)parsed->endianness);
  }

  if (!state->cnfg_received) {
    ZBC_LOG_ERROR_S("parse_request: missing CNFG chunk");
    /* At this point parsing succeeded, so we can try the pre-allocated ERRO */
    if (parsed->has_erro) {
      write_erro_payload(state, riff_addr, parsed, ZBC_PROTO_ERR_MISSING_CNFG);
    } else {
      write_erro_early(state, riff_addr, ZBC_PROTO_ERR_MISSING_CNFG);
    }
    return ZBC_ERR_PARSE_ERROR;
  }

  if (!parsed->has_call) {
    ZBC_LOG_ERROR_S("parse_request: missing CALL chunk");
    if (parsed->has_erro) {
      write_erro_payload(state, riff_addr, parsed, ZBC_PROTO_ERR_INVALID_CHUNK);
    } else {
      write_erro_early(state, riff_addr, ZBC_PROTO_ERR_INVALID_CHUNK);
    }
    return ZBC_ERR_PARSE_ERROR;
  }

  return ZBC_OK;
}

/*========================================================================
 * Backend dispatch - table-driven
 *
 * Each opcode has a typed caller wrapper that:
 *   - Casts the function pointer to the correct signature
 *   - Extracts arguments from parsed structure
 *   - Returns result and optional data
 *
 * The dispatcher handles null-check and errno centrally.
 *========================================================================*/

/* Call result - returned by all caller wrappers */
typedef struct {
  intmax_t result;
  const void *data;
  size_t data_len;
} call_result_t;

/* Caller function signature */
typedef call_result_t (*caller_t)(void *fn, void *ctx, const zbc_parsed_t *p,
                                  uint8_t *buf, size_t buf_size);

/*------------------------------------------------------------------------
 * Typed caller wrappers - one per distinct backend signature
 *------------------------------------------------------------------------*/

/* Helper to convert void* to function pointer without pedantic warning */
typedef int (*fn_ctx_t)(void *);
typedef int (*fn_fd_t)(void *, int);
typedef intmax_t (*fn_fd_flen_t)(void *, int);
typedef int (*fn_fd_int_t)(void *, int, int);
typedef int (*fn_fd_buf_t)(void *, int, void *, size_t);
typedef int (*fn_fd_cbuf_t)(void *, int, const void *, size_t);
typedef int (*fn_path_t)(void *, const char *, size_t);
typedef int (*fn_path_mode_t)(void *, const char *, size_t, int);
typedef int (*fn_path_path_t)(void *, const char *, size_t, const char *, size_t);
typedef int (*fn_tmpnam_t)(void *, char *, size_t, int);
typedef void (*fn_writec_t)(void *, char);
typedef void (*fn_write0_t)(void *, const char *);
typedef int (*fn_uint_t)(void *, unsigned int);
typedef void (*fn_exit_t)(void *, unsigned int, unsigned int);
typedef int (*fn_elapsed_t)(void *, unsigned int *, unsigned int *);
typedef int (*fn_cmdline_t)(void *, char *, size_t);
typedef int (*fn_heapinfo_t)(void *, uintptr_t *, uintptr_t *, uintptr_t *,
                             uintptr_t *);

/* Union for void* to function pointer conversion (avoids pedantic warning) */
typedef union {
  void *ptr;
  fn_ctx_t ctx;
  fn_fd_t fd;
  fn_fd_flen_t fd_flen;
  fn_fd_int_t fd_int;
  fn_fd_buf_t fd_buf;
  fn_fd_cbuf_t fd_cbuf;
  fn_path_t path;
  fn_path_mode_t path_mode;
  fn_path_path_t path_path;
  fn_tmpnam_t tmpnam;
  fn_writec_t writec;
  fn_write0_t write0;
  fn_uint_t uint;
  fn_exit_t exit;
  fn_elapsed_t elapsed;
  fn_cmdline_t cmdline;
  fn_heapinfo_t heapinfo;
} fn_union_t;

/* int fn(void *ctx) */
static call_result_t call_ctx(void *fn, void *ctx, const zbc_parsed_t *p,
                              uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)p; (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.ctx(ctx);
  return r;
}

/* int fn(void *ctx, int fd) */
static call_result_t call_fd(void *fn, void *ctx, const zbc_parsed_t *p,
                             uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.fd(ctx, (int)p->parms[0]);
  return r;
}

/* intmax_t fn(void *ctx, int fd) - for flen */
static call_result_t call_fd_flen(void *fn, void *ctx, const zbc_parsed_t *p,
                                  uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.fd_flen(ctx, (int)p->parms[0]);
  return r;
}

/* int fn(void *ctx, int fd, int pos) */
static call_result_t call_fd_int(void *fn, void *ctx, const zbc_parsed_t *p,
                                 uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.fd_int(ctx, (int)p->parms[0], (int)p->parms[1]);
  return r;
}

/* int fn(void *ctx, int fd, void *buf, size_t count) - read */
static call_result_t call_fd_read(void *fn, void *ctx, const zbc_parsed_t *p,
                                  uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  size_t count = (size_t)p->parms[1];
  if (count > buf_size)
    count = buf_size;
  u.ptr = fn;
  r.result = u.fd_buf(ctx, (int)p->parms[0], buf, count);
  if (r.result >= 0) {
    r.data = buf;
    r.data_len = count - (size_t)r.result; /* bytes actually read */
  }
  return r;
}

/* int fn(void *ctx, int fd, const void *buf, size_t count) - write */
static call_result_t call_fd_write(void *fn, void *ctx, const zbc_parsed_t *p,
                                   uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.fd_cbuf(ctx, (int)p->parms[0], p->data[0].ptr, p->data[0].size);
  return r;
}

/* int fn(void *ctx, const char *path, size_t len) */
static call_result_t call_path(void *fn, void *ctx, const zbc_parsed_t *p,
                               uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.path(ctx, (const char *)p->data[0].ptr, p->data[0].size);
  return r;
}

/* int fn(void *ctx, const char *path, size_t len, int mode) - open */
static call_result_t call_path_mode(void *fn, void *ctx, const zbc_parsed_t *p,
                                    uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.path_mode(ctx, (const char *)p->data[0].ptr, p->data[0].size,
                         (int)p->parms[0]);
  return r;
}

/* int fn(void *ctx, const char *old, size_t ol, const char *new, size_t nl) */
static call_result_t call_path_path(void *fn, void *ctx, const zbc_parsed_t *p,
                                    uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.path_path(ctx, (const char *)p->data[0].ptr, p->data[0].size,
                         (const char *)p->data[1].ptr, p->data[1].size);
  return r;
}

/* int fn(void *ctx, char *buf, size_t size, int id) - tmpnam */
static call_result_t call_tmpnam(void *fn, void *ctx, const zbc_parsed_t *p,
                                 uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  size_t maxlen = (size_t)p->parms[1];
  if (maxlen > buf_size)
    maxlen = buf_size;
  u.ptr = fn;
  r.result = u.tmpnam(ctx, (char *)buf, maxlen, (int)p->parms[0]);
  if (r.result == 0) {
    r.data = buf;
    r.data_len = zbc_strlen((const char *)buf) + 1;
  }
  return r;
}

/* void fn(void *ctx, char c) - writec */
static call_result_t call_writec(void *fn, void *ctx, const zbc_parsed_t *p,
                                 uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  if (p->data_count > 0 && p->data[0].size > 0) {
    u.writec(ctx, (char)p->data[0].ptr[0]);
  }
  return r;
}

/* void fn(void *ctx, const char *str) - write0 */
static call_result_t call_write0(void *fn, void *ctx, const zbc_parsed_t *p,
                                 uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  if (p->data_count > 0) {
    u.write0(ctx, (const char *)p->data[0].ptr);
  }
  return r;
}

/* int fn(void *ctx, unsigned int val) - timer_config */
static call_result_t call_uint(void *fn, void *ctx, const zbc_parsed_t *p,
                               uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  r.result = u.uint(ctx, (unsigned int)p->parms[0]);
  return r;
}

/* void fn(void *ctx, unsigned reason, unsigned subcode) - exit */
static call_result_t call_exit(void *fn, void *ctx, const zbc_parsed_t *p,
                               uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  unsigned int reason = (unsigned int)p->parms[0];
  unsigned int subcode = (p->parm_count >= 2) ? (unsigned int)p->parms[1] : 0;
  (void)buf; (void)buf_size;
  u.ptr = fn;
  u.exit(ctx, reason, subcode);
  return r;
}

/* int fn(void *ctx, unsigned *lo, unsigned *hi) - elapsed */
static call_result_t call_elapsed(void *fn, void *ctx, const zbc_parsed_t *p,
                                  uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  unsigned int lo, hi;
  (void)p;
  if (buf_size < 8) {
    r.result = -1;
    return r;
  }
  u.ptr = fn;
  r.result = u.elapsed(ctx, &lo, &hi);
  if (r.result == 0) {
    ZBC_WRITE_U32_LE(buf, lo);
    ZBC_WRITE_U32_LE(buf + 4, hi);
    r.data = buf;
    r.data_len = 8;
  }
  return r;
}

/* No backend call - pure logic for ISERROR */
static call_result_t call_iserror(void *fn, void *ctx, const zbc_parsed_t *p,
                                  uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  (void)fn; (void)ctx; (void)buf; (void)buf_size;
  r.result = (p->parm_count >= 1 && p->parms[0] < 0) ? 1 : 0;
  return r;
}

/* int fn(void *ctx, char *buf, size_t size) - get_cmdline */
static call_result_t call_cmdline(void *fn, void *ctx, const zbc_parsed_t *p,
                                  uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  size_t maxlen = (size_t)p->parms[0];
  if (maxlen > buf_size)
    maxlen = buf_size;
  u.ptr = fn;
  r.result = u.cmdline(ctx, (char *)buf, maxlen);
  if (r.result == 0) {
    r.data = buf;
    r.data_len = zbc_strlen((const char *)buf) + 1;
  }
  return r;
}

/* int fn(void *ctx, uintptr_t*, uintptr_t*, uintptr_t*, uintptr_t*) - heapinfo */
static call_result_t call_heapinfo(void *fn, void *ctx, const zbc_parsed_t *p,
                                   uint8_t *buf, size_t buf_size) {
  call_result_t r = {0, NULL, 0};
  fn_union_t u;
  uintptr_t heap_base, heap_limit, stack_base, stack_limit;
  int ps = p->ptr_size;
  int endian = p->endianness;

  if (buf_size < (size_t)(4 * ps)) {
    r.result = -1;
    return r;
  }
  u.ptr = fn;
  r.result = u.heapinfo(ctx, &heap_base, &heap_limit, &stack_base, &stack_limit);
  if (r.result == 0) {
    zbc_write_native_uint(buf, heap_base, ps, endian);
    zbc_write_native_uint(buf + ps, heap_limit, ps, endian);
    zbc_write_native_uint(buf + 2 * ps, stack_base, ps, endian);
    zbc_write_native_uint(buf + 3 * ps, stack_limit, ps, endian);
    r.data = buf;
    r.data_len = (size_t)(4 * ps);
  }
  return r;
}

/*------------------------------------------------------------------------
 * Dispatch table
 *------------------------------------------------------------------------*/

typedef struct {
  uint8_t opcode;
  uint8_t fn_offset;   /* offsetof(zbc_backend_t, field) / sizeof(void*) */
  uint8_t wants_errno; /* 1 = get errno on error */
  caller_t caller;
} dispatch_entry_t;

#define OFF(field) (offsetof(zbc_backend_t, field) / sizeof(void *))

static const dispatch_entry_t dispatch_table[] = {
    /* File operations */
    {SH_SYS_OPEN, OFF(open), 1, call_path_mode},
    {SH_SYS_CLOSE, OFF(close), 1, call_fd},
    {SH_SYS_WRITE, OFF(write), 1, call_fd_write},
    {SH_SYS_READ, OFF(read), 1, call_fd_read},
    {SH_SYS_SEEK, OFF(seek), 1, call_fd_int},
    {SH_SYS_FLEN, OFF(flen), 1, call_fd_flen},
    {SH_SYS_ISTTY, OFF(istty), 0, call_fd},
    {SH_SYS_REMOVE, OFF(remove), 1, call_path},
    {SH_SYS_RENAME, OFF(rename), 1, call_path_path},
    {SH_SYS_TMPNAM, OFF(tmpnam), 1, call_tmpnam},

    /* Console */
    {SH_SYS_WRITEC, OFF(writec), 0, call_writec},
    {SH_SYS_WRITE0, OFF(write0), 0, call_write0},
    {SH_SYS_READC, OFF(readc), 0, call_ctx},

    /* System - no errno */
    {SH_SYS_ISERROR, 0, 0, call_iserror},
    {SH_SYS_CLOCK, OFF(clock), 0, call_ctx},
    {SH_SYS_TIME, OFF(time), 0, call_ctx},
    {SH_SYS_TICKFREQ, OFF(tickfreq), 0, call_ctx},
    {SH_SYS_ERRNO, OFF(get_errno), 0, call_ctx},
    {SH_SYS_SYSTEM, OFF(do_system), 0, call_path},
    {SH_SYS_GET_CMDLINE, OFF(get_cmdline), 0, call_cmdline},
    {SH_SYS_HEAPINFO, OFF(heapinfo), 0, call_heapinfo},
    {SH_SYS_EXIT, OFF(do_exit), 0, call_exit},
    {SH_SYS_EXIT_EXTENDED, OFF(do_exit), 0, call_exit},
    {SH_SYS_ELAPSED, OFF(elapsed), 0, call_elapsed},
    {SH_SYS_TIMER_CONFIG, OFF(timer_config), 1, call_uint},

    {0, 0, 0, NULL} /* end marker */
};

/*------------------------------------------------------------------------
 * Main dispatch function
 *------------------------------------------------------------------------*/

int zbc_host_process(zbc_host_state_t *state, uintptr_t riff_addr) {
  zbc_parsed_t parsed;
  const zbc_backend_t *be;
  void *ctx;
  const dispatch_entry_t *d;
  int rc;

  if (!state || !state->work_buf || !state->backend) {
    ZBC_LOG_ERROR_S("zbc_host_process: invalid arguments");
    return ZBC_ERR_INVALID_ARG;
  }

  rc = parse_request(state, riff_addr, &parsed);
  if (rc != ZBC_OK) {
    return rc;
  }

  be = state->backend;
  ctx = state->backend_ctx;

  /* Find dispatch entry */
  for (d = dispatch_table; d->caller != NULL; d++) {
    if (d->opcode == parsed.opcode) {
      void *fn = ((void **)be)[d->fn_offset];
      call_result_t r;
      int err = 0;

      /* Check for missing backend function (fn_offset 0 = no backend needed) */
      if (!fn && d->fn_offset != 0) {
        write_retn_payload(state, riff_addr, &parsed, -1, ENOSYS, NULL, 0);
        return ZBC_OK;
      }

      /* Call the typed wrapper */
      r = d->caller(fn, ctx, &parsed, state->work_buf + state->work_buf_size / 2,
                    state->work_buf_size / 2);

      /* Get errno if needed */
      if (d->wants_errno && r.result < 0 && be->get_errno) {
        err = be->get_errno(ctx);
      }

      /* Write response */
      write_retn_payload(state, riff_addr, &parsed, (intptr_t)r.result, err,
                         r.data, r.data_len);
      return ZBC_OK;
    }
  }

  /* Unknown opcode */
  ZBC_LOG_WARN("unknown opcode 0x%02x", (unsigned)parsed.opcode);
  write_erro_payload(state, riff_addr, &parsed, ZBC_PROTO_ERR_UNSUPPORTED_OP);
  return ZBC_OK;
}
