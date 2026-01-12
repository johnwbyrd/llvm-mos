/*
 * ZBC Semihosting RIFF Helpers
 *
 * Shared functions for reading and writing RIFF chunks.
 * Used by both client and host libraries.
 */

/*
 * This file is compiled into both client and host libraries.
 * ZBC_CLIENT or ZBC_HOST is defined via CMakeLists.txt.
 */
#define ZBC_HOST
#include "llvm/Emulator/Semihost/zbc_semihost.h"

/*========================================================================
 * String length helper (no libc dependency)
 *========================================================================*/

size_t zbc_strlen(const char *s) {
  size_t len = 0;
  if (s) {
    while (*s++) {
      len++;
    }
  }
  return len;
}

/*========================================================================
 * Native endianness read/write helpers
 *========================================================================*/

/*
 * Write an unsigned integer in native endianness.
 * Used for PARM/DATA values that are in guest's native format.
 */
void zbc_write_native_uint(uint8_t *buf, uintptr_t value, int size,
                           int endianness) {
  int i;

  if (endianness == ZBC_ENDIAN_LITTLE) {
    for (i = 0; i < size; i++) {
      buf[i] = (uint8_t)(value & 0xFFU);
      value >>= 8;
    }
  } else {
    /* Big endian */
    for (i = size - 1; i >= 0; i--) {
      buf[i] = (uint8_t)(value & 0xFFU);
      value >>= 8;
    }
  }
}

/*
 * Read a signed integer in native endianness with sign extension.
 */
intptr_t zbc_read_native_int(const uint8_t *buf, int size, int endianness) {
  uintptr_t value = 0;
  int i;
  uintptr_t sign_bit;
  uintptr_t sign_extend;

  if (endianness == ZBC_ENDIAN_LITTLE) {
    for (i = size - 1; i >= 0; i--) {
      value = (value << 8) | buf[i];
    }
  } else {
    /* Big endian */
    for (i = 0; i < size; i++) {
      value = (value << 8) | buf[i];
    }
  }

  /* Sign extend if necessary */
  if (size < (int)sizeof(intptr_t)) {
    sign_bit = (uintptr_t)1U << (size * 8 - 1);
    if (value & sign_bit) {
      sign_extend = ~(((uintptr_t)1U << (size * 8)) - 1);
      value |= sign_extend;
    }
  }

  return (intptr_t)value;
}

/*
 * Read an unsigned integer in native endianness.
 */
uintptr_t zbc_read_native_uint(const uint8_t *buf, int size, int endianness) {
  uintptr_t value = 0;
  int i;

  if (endianness == ZBC_ENDIAN_LITTLE) {
    for (i = size - 1; i >= 0; i--) {
      value = (value << 8) | buf[i];
    }
  } else {
    /* Big endian */
    for (i = 0; i < size; i++) {
      value = (value << 8) | buf[i];
    }
  }

  return value;
}

/*========================================================================
 * RIFF chunk writing helpers
 *========================================================================*/

/*
 * Write a chunk header (FourCC + size placeholder).
 * Returns pointer to size field for later patching.
 */
uint8_t *zbc_riff_begin_chunk(uint8_t *buf, size_t capacity, size_t *offset,
                              uint32_t fourcc) {
  uint8_t *size_ptr;

  if (*offset + ZBC_CHUNK_HDR_SIZE > capacity) {
    return (uint8_t *)0; /* Buffer overflow */
  }

  ZBC_WRITE_U32_LE(buf + *offset, fourcc);
  size_ptr = buf + *offset + 4;
  ZBC_WRITE_U32_LE(size_ptr, 0); /* Placeholder */
  *offset += ZBC_CHUNK_HDR_SIZE;

  return size_ptr;
}

/*
 * Patch a chunk size field.
 * data_size is the size of chunk data (not including header).
 */
void zbc_riff_patch_size(uint8_t *size_ptr, size_t data_size) {
  ZBC_WRITE_U32_LE(size_ptr, (uint32_t)data_size);
}

/*
 * Write bytes to buffer with bounds checking.
 * Returns ZBC_OK on success, ZBC_ERR_BUFFER_FULL on overflow.
 */
int zbc_riff_write_bytes(uint8_t *buf, size_t capacity, size_t *offset,
                         const void *data, size_t size) {
  size_t i;
  const uint8_t *src;

  if (*offset + size > capacity) {
    return ZBC_ERR_BUFFER_FULL;
  }

  src = (const uint8_t *)data;
  for (i = 0; i < size; i++) {
    buf[*offset + i] = src[i];
  }
  *offset += size;

  return ZBC_OK;
}

/*
 * Write padding byte if needed (RIFF chunks are word-aligned).
 */
void zbc_riff_pad(uint8_t *buf, size_t capacity, size_t *offset) {
  if ((*offset & 1) && *offset < capacity) {
    buf[*offset] = 0;
    (*offset)++;
  }
}

/*========================================================================
 * RIFF chunk reading helpers
 *========================================================================*/

/*
 * Read a chunk header.
 * Returns ZBC_OK on success, fills fourcc and size.
 * Returns ZBC_ERR_HEADER_OVERFLOW if not enough data.
 */
int zbc_riff_read_header(const uint8_t *buf, size_t capacity, size_t offset,
                         uint32_t *fourcc, uint32_t *size) {
  if (offset + ZBC_CHUNK_HDR_SIZE > capacity) {
    return ZBC_ERR_HEADER_OVERFLOW;
  }

  *fourcc = ZBC_READ_U32_LE(buf + offset);
  *size = ZBC_READ_U32_LE(buf + offset + 4);

  return ZBC_OK;
}

/*
 * Skip over a chunk (header + data + padding).
 * Returns new offset, or 0 on error.
 */
size_t zbc_riff_skip_chunk(const uint8_t *buf, size_t capacity, size_t offset) {
  uint32_t fourcc, size;

  if (zbc_riff_read_header(buf, capacity, offset, &fourcc, &size) != ZBC_OK) {
    return 0;
  }

  offset += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(size);

  if (offset > capacity) {
    return 0;
  }

  return offset;
}

/*========================================================================
 * RIFF container helpers
 *========================================================================*/

/*
 * Write RIFF container header.
 * Returns pointer to size field for later patching.
 */
uint8_t *zbc_riff_begin_container(uint8_t *buf, size_t capacity, size_t *offset,
                                  uint32_t form_type) {
  uint8_t *size_ptr;

  if (capacity < ZBC_HDR_SIZE) {
    return (uint8_t *)0;
  }

  ZBC_WRITE_U32_LE(buf, ZBC_ID_RIFF);
  size_ptr = buf + 4;
  ZBC_WRITE_U32_LE(size_ptr, 0); /* Placeholder */
  ZBC_WRITE_U32_LE(buf + 8, form_type);
  *offset = ZBC_HDR_SIZE;

  return size_ptr;
}

/*
 * Validate RIFF container header.
 * Returns ZBC_OK on success, or ZBC_ERR_HEADER_OVERFLOW/ZBC_ERR_BAD_RIFF_MAGIC/
 * ZBC_ERR_BAD_FORM_TYPE on error.
 */
int zbc_riff_validate_container(const uint8_t *buf, size_t capacity,
                                uint32_t expected_form_type) {
  uint32_t id, form_type;

  if (capacity < ZBC_HDR_SIZE) {
    return ZBC_ERR_HEADER_OVERFLOW;
  }

  id = ZBC_READ_U32_LE(buf);
  if (id != ZBC_ID_RIFF) {
    return ZBC_ERR_BAD_RIFF_MAGIC;
  }

  form_type = ZBC_READ_U32_LE(buf + 8);
  if (form_type != expected_form_type) {
    return ZBC_ERR_BAD_FORM_TYPE;
  }

  return ZBC_OK;
}

/*========================================================================
 * New chunk-based API (struct-based, no magic offsets)
 *========================================================================*/

int zbc_chunk_validate(const zbc_chunk_t *chunk, const uint8_t *container_end) {
  const uint8_t *chunk_start;
  const uint8_t *chunk_data_end;

  if (!chunk || !container_end) {
    return ZBC_ERR_NULL_ARG;
  }

  chunk_start = (const uint8_t *)chunk;

  /* Check header fits (8 bytes on wire, not sizeof which includes padding) */
  if (chunk_start + ZBC_CHUNK_HDR_SIZE > container_end) {
    return ZBC_ERR_HEADER_OVERFLOW;
  }

  /* Check data fits (including padding) */
  chunk_data_end = chunk_start + ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(chunk->size);
  if (chunk_data_end > container_end) {
    return ZBC_ERR_DATA_OVERFLOW;
  }

  return ZBC_OK;
}

int zbc_chunk_next(zbc_chunk_t **out, const zbc_chunk_t *chunk) {
  const uint8_t *next_ptr;

  if (!out || !chunk) {
    return ZBC_ERR_NULL_ARG;
  }

  /* Next chunk starts after this chunk's header + padded data */
  next_ptr =
      (const uint8_t *)chunk + ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(chunk->size);
  *out = (zbc_chunk_t *)next_ptr;

  return ZBC_OK;
}

int zbc_chunk_first_sub(zbc_chunk_t **out, const zbc_chunk_t *container,
                        size_t header_size) {
  if (!out || !container) {
    return ZBC_ERR_NULL_ARG;
  }

  /* Sub-chunks start after the container's header payload */
  *out = (zbc_chunk_t *)(container->data + header_size);

  return ZBC_OK;
}

int zbc_chunk_end(const uint8_t **out, const zbc_chunk_t *chunk) {
  if (!out || !chunk) {
    return ZBC_ERR_NULL_ARG;
  }

  /* End is first byte past the chunk's data (not including padding) */
  *out = chunk->data + chunk->size;

  return ZBC_OK;
}

int zbc_chunk_find(zbc_chunk_t **out, const uint8_t *start, const uint8_t *end,
                   uint32_t id) {
  zbc_chunk_t *chunk;
  int err;

  if (!out || !start || !end) {
    return ZBC_ERR_NULL_ARG;
  }

  chunk = (zbc_chunk_t *)start;

  while ((const uint8_t *)chunk < end) {
    err = zbc_chunk_validate(chunk, end);
    if (err != ZBC_OK) {
      return err;
    }

    if (chunk->id == id) {
      *out = chunk;
      return ZBC_OK;
    }

    err = zbc_chunk_next(&chunk, chunk);
    if (err != ZBC_OK) {
      return err;
    }
  }

  return ZBC_ERR_NOT_FOUND;
}

int zbc_riff_validate(const zbc_riff_t *riff, size_t buf_size,
                      uint32_t expected_form) {
  size_t riff_total_size;

  if (!riff) {
    return ZBC_ERR_NULL_ARG;
  }

  /* Check we can read the header (12 bytes on wire) */
  if (buf_size < ZBC_RIFF_HDR_SIZE) {
    ZBC_LOG_ERROR("RIFF header overflow: buf_size=%u < %u", (unsigned)buf_size,
                  (unsigned)ZBC_RIFF_HDR_SIZE);
    return ZBC_ERR_HEADER_OVERFLOW;
  }

  /* Check magic */
  if (riff->riff_id != ZBC_ID_RIFF) {
    ZBC_LOG_ERROR("bad RIFF magic: 0x%08x", (unsigned)riff->riff_id);
    return ZBC_ERR_BAD_RIFF_MAGIC;
  }

  /* Check form type */
  if (riff->form_type != expected_form) {
    ZBC_LOG_ERROR("bad form type: 0x%08x (expected 0x%08x)",
                  (unsigned)riff->form_type, (unsigned)expected_form);
    return ZBC_ERR_BAD_FORM_TYPE;
  }

  /* Check size fits in buffer: size field counts bytes after itself */
  /* Total = 4 (riff_id) + 4 (size field) + size */
  riff_total_size = 4 + 4 + riff->size;
  if (riff_total_size > buf_size) {
    ZBC_LOG_ERROR("RIFF overflow: size=%u exceeds buf_size=%u",
                  (unsigned)riff->size, (unsigned)buf_size);
    return ZBC_ERR_RIFF_OVERFLOW;
  }

  return ZBC_OK;
}

int zbc_riff_end(const uint8_t **out, const zbc_riff_t *riff) {
  if (!out || !riff) {
    return ZBC_ERR_NULL_ARG;
  }

  /* RIFF size counts everything after the size field itself.
   * So end = start + 4 (riff_id) + 4 (size) + size
   */
  *out = (const uint8_t *)riff + 4 + 4 + riff->size;

  return ZBC_OK;
}

/*========================================================================
 * Unified RIFF parser
 *
 * Parse once, extract everything. No state machine.
 *========================================================================*/

/*
 * Parse sub-chunks within a container (CALL or RETN).
 * Extracts PARM and DATA chunks into the parsed structure.
 */
static void parse_subchunks(const uint8_t *start, const uint8_t *end,
                            int int_size, int ptr_size, int endian,
                            zbc_parsed_t *out) {
  const uint8_t *pos = start;
  uint32_t id, size;
  const uint8_t *data;
  int value_size;

  while (pos + ZBC_CHUNK_HDR_SIZE <= end) {
    id = ZBC_READ_U32_LE(pos);
    size = ZBC_READ_U32_LE(pos + 4);
    data = pos + ZBC_CHUNK_HDR_SIZE;

    /* Bounds check */
    if (data + size > end) {
      break;
    }

    if (id == ZBC_ID_PARM && out->parm_count < ZBC_MAX_PARMS) {
      /* PARM: type(1) + reserved(3) + value(int_size or ptr_size) */
      if (size >= ZBC_PARM_HDR_SIZE) {
        uint8_t parm_type = data[0];
        value_size = (parm_type == ZBC_PARM_TYPE_PTR) ? ptr_size : int_size;

        if (size >= ZBC_PARM_HDR_SIZE + (size_t)value_size) {
          out->parms[out->parm_count] =
              zbc_read_native_int(data + ZBC_PARM_HDR_SIZE, value_size, endian);
          out->parm_count++;
        }
      }
    } else if (id == ZBC_ID_DATA && out->data_count < ZBC_MAX_DATA) {
      /* DATA: type(1) + reserved(3) + payload */
      if (size >= ZBC_DATA_HDR_SIZE) {
        out->data[out->data_count].ptr = data + ZBC_DATA_HDR_SIZE;
        out->data[out->data_count].size = size - ZBC_DATA_HDR_SIZE;
        out->data_count++;
      }
    }

    /* Advance to next chunk (with padding) */
    pos += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(size);
  }
}

/*
 * Parse a RIFF SEMI request buffer into a zbc_parsed_t structure.
 * Used by the host to parse client requests (CNFG/CALL/PARM/DATA chunks).
 */
int zbc_riff_parse_request(zbc_parsed_t *out, const uint8_t *buf,
                           size_t buf_size, int int_size, int endian) {
  const zbc_riff_t *riff;
  const uint8_t *riff_end_ptr;
  const uint8_t *pos;
  uint32_t id, size;
  const uint8_t *chunk_data;
  int ptr_size;
  int rc;

  if (!out || !buf) {
    return ZBC_ERR_NULL_ARG;
  }

  /* Zero the output structure */
  {
    uint8_t *p = (uint8_t *)out;
    size_t i;
    for (i = 0; i < sizeof(*out); i++) {
      p[i] = 0;
    }
  }

  /* Default to provided int_size for ptr_size until we see CNFG */
  ptr_size = int_size;

  /* Validate RIFF container */
  riff = (const zbc_riff_t *)buf;
  rc = zbc_riff_validate(riff, buf_size, ZBC_ID_SEMI);
  if (rc != ZBC_OK) {
    return rc;
  }

  rc = zbc_riff_end(&riff_end_ptr, riff);
  if (rc != ZBC_OK) {
    return rc;
  }

  /* Walk all top-level chunks */
  pos = buf + ZBC_RIFF_HDR_SIZE;

  while (pos + ZBC_CHUNK_HDR_SIZE <= riff_end_ptr) {
    id = ZBC_READ_U32_LE(pos);
    size = ZBC_READ_U32_LE(pos + 4);
    chunk_data = pos + ZBC_CHUNK_HDR_SIZE;

    /* Bounds check */
    if (chunk_data + size > riff_end_ptr) {
      ZBC_LOG_ERROR("chunk data overflow: size=%u exceeds container",
                    (unsigned)size);
      return ZBC_ERR_DATA_OVERFLOW;
    }

    if (id == ZBC_ID_CNFG) {
      /* CNFG: int_size(1) + ptr_size(1) + endian(1) + reserved(1) */
      if (size >= ZBC_CNFG_PAYLOAD_SIZE) {
        out->int_size = chunk_data[0];
        out->ptr_size = chunk_data[1];
        out->endianness = chunk_data[2];
        out->has_cnfg = 1;
        /* Validate sizes - must be 1, 2, 4, or 8 */
        if ((out->int_size != 1 && out->int_size != 2 && out->int_size != 4 &&
             out->int_size != 8) ||
            (out->ptr_size != 1 && out->ptr_size != 2 && out->ptr_size != 4 &&
             out->ptr_size != 8)) {
          ZBC_LOG_ERROR("CNFG: invalid int_size=%u or ptr_size=%u",
                        (unsigned)out->int_size, (unsigned)out->ptr_size);
          return ZBC_ERR_INVALID_ARG;
        }
#ifdef ZBC_HOST
        /* Host-side: reject sizes larger than native can handle */
        if (out->int_size > sizeof(uintptr_t) ||
            out->ptr_size > sizeof(uintptr_t)) {
          ZBC_LOG_ERROR("CNFG: sizes exceed host capacity (int=%u, ptr=%u)",
                        (unsigned)out->int_size, (unsigned)out->ptr_size);
          return ZBC_ERR_INVALID_ARG;
        }
#endif
        /* Update for sub-chunk parsing */
        int_size = out->int_size;
        ptr_size = out->ptr_size;
        endian = out->endianness;
      }
    } else if (id == ZBC_ID_CALL) {
      /* CALL: opcode(1) + reserved(3) + sub-chunks */
      if (size >= ZBC_CALL_HDR_PAYLOAD_SIZE) {
        out->opcode = chunk_data[0];
        out->has_call = 1;
        /* Parse sub-chunks within CALL */
        parse_subchunks(chunk_data + ZBC_CALL_HDR_PAYLOAD_SIZE,
                        chunk_data + size, int_size, ptr_size, endian, out);
      }
    } else if (id == ZBC_ID_RETN) {
      /* RETN: result(int_size) + errno(ZBC_RETN_ERRNO_SIZE) + optional
       * sub-chunks */
      out->has_retn = 1;
      /* Record offset and capacity for host-side writing */
      out->retn_payload_offset = (size_t)(chunk_data - buf);
      out->retn_payload_capacity = size;
      /* Parse contents if present (for client-side reading) */
      if (size >= (size_t)int_size + ZBC_RETN_ERRNO_SIZE) {
        out->result = zbc_read_native_int(chunk_data, int_size, endian);
        out->host_errno = (int)ZBC_READ_U32_LE(chunk_data + int_size);
        /* Parse sub-chunks within RETN (for DATA) */
        parse_subchunks(chunk_data + int_size + ZBC_RETN_ERRNO_SIZE,
                        chunk_data + size, int_size, ptr_size, endian, out);
      }
    } else if (id == ZBC_ID_ERRO) {
      /* ERRO: error_code(2) + reserved(2) */
      out->has_erro = 1;
      /* Record offset and capacity for host-side writing */
      out->erro_payload_offset = (size_t)(chunk_data - buf);
      out->erro_payload_capacity = size;
      /* Parse contents if present (for client-side reading) */
      if (size >= ZBC_ERRO_PAYLOAD_SIZE) {
        out->proto_error = ZBC_READ_U16_LE(chunk_data);
      }
    }
    /* Skip unknown chunks silently */

    /* Advance to next chunk (with padding) */
    pos += ZBC_CHUNK_HDR_SIZE + ZBC_PAD_SIZE(size);
  }

  return ZBC_OK;
}
