//===-- llvm/Emulator/Semihost/RiffCodec.h - RIFF Codec ---------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// RIFF encoding and decoding for ZBC semihosting protocol.
/// All operations are parameterized by PlatformConfig for platform independence.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_RIFFCODEC_H
#define LLVM_EMULATOR_SEMIHOST_RIFFCODEC_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Emulator/Semihost/Protocol.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Error.h"
#include <cstdint>

namespace llvm {
namespace emu {
namespace semihost {

//===----------------------------------------------------------------------===//
// PlatformConfig - Runtime platform parameters
//===----------------------------------------------------------------------===//

/// Platform configuration - provided by the platform, NOT the semihosting layer.
/// These values are REQUIRED at construction time. NO DEFAULTS.
///
/// NOTE: These values should be sourced from existing infrastructure (MC layer,
/// LLDB target info) to maintain DRY. The System class is responsible for
/// providing the correct values for the target being emulated.
struct PlatformConfig {
  uint8_t IntSize;        ///< Platform-defined integer size (1, 2, 4, or 8)
  uint8_t PtrSize;        ///< Platform-defined pointer size (1, 2, 4, or 8)
  llvm::endianness Endian; ///< Platform-defined byte order

  /// No default constructor - forces caller to provide all values.
  PlatformConfig(uint8_t Int, uint8_t Ptr, llvm::endianness E)
      : IntSize(Int), PtrSize(Ptr), Endian(E) {}

  /// Validate configuration values.
  bool isValid() const {
    return (IntSize == 1 || IntSize == 2 || IntSize == 4 || IntSize == 8) &&
           (PtrSize == 1 || PtrSize == 2 || PtrSize == 4 || PtrSize == 8);
  }
};

//===----------------------------------------------------------------------===//
// Parsed RIFF Request
//===----------------------------------------------------------------------===//

/// Maximum number of PARM/DATA chunks in a parsed request.
constexpr size_t MaxParms = 8;
constexpr size_t MaxData = 4;

/// Parsed RIFF SEMI request structure.
/// Parse once, then access fields directly.
struct ParsedRequest {
  /// Guest configuration (from CNFG chunk).
  PlatformConfig Config;
  bool HasCnfg = false;

  /// Request: CALL chunk info.
  Opcode Op = Opcode::Open;
  bool HasCall = false;

  /// Request: parameters from PARM sub-chunks.
  SmallVector<intmax_t, MaxParms> Parms;

  /// Request/Response: data from DATA sub-chunks.
  struct DataChunk {
    ArrayRef<uint8_t> Data;
  };
  SmallVector<DataChunk, MaxData> DataChunks;

  /// Response chunk offsets for host-side writing.
  size_t RetnPayloadOffset = 0;
  size_t RetnPayloadCapacity = 0;
  size_t ErroPayloadOffset = 0;
  size_t ErroPayloadCapacity = 0;

  ParsedRequest(PlatformConfig Cfg) : Config(Cfg) {}

  /// Get a DATA chunk as a StringRef.
  /// @param Index Index of the DATA chunk (0-based).
  /// @return StringRef to the data, or empty if index out of bounds.
  StringRef getDataAsString(size_t Index) const {
    if (Index >= DataChunks.size())
      return StringRef();
    return StringRef(reinterpret_cast<const char *>(DataChunks[Index].Data.data()),
                     DataChunks[Index].Data.size());
  }
};

//===----------------------------------------------------------------------===//
// RIFF Codec Functions
//===----------------------------------------------------------------------===//

/// Read an integer of the given size with the specified endianness.
/// @param Buf Source buffer (must have at least Size bytes).
/// @param Size Integer size in bytes (1, 2, 4, or 8).
/// @param Endian Byte order.
/// @return The decoded signed integer value.
intmax_t readInt(const uint8_t *Buf, uint8_t Size, llvm::endianness Endian);

/// Read an unsigned integer of the given size with the specified endianness.
uintmax_t readUInt(const uint8_t *Buf, uint8_t Size, llvm::endianness Endian);

/// Write an integer of the given size with the specified endianness.
/// @param Buf Destination buffer (must have at least Size bytes).
/// @param Value Value to write.
/// @param Size Integer size in bytes (1, 2, 4, or 8).
/// @param Endian Byte order.
void writeInt(uint8_t *Buf, intmax_t Value, uint8_t Size,
              llvm::endianness Endian);

/// Write an unsigned integer of the given size with the specified endianness.
void writeUInt(uint8_t *Buf, uintmax_t Value, uint8_t Size,
               llvm::endianness Endian);

/// Parse a RIFF SEMI request buffer.
/// @param Buf The RIFF buffer to parse.
/// @param BufSize Size of the buffer in bytes.
/// @param DefaultConfig Default platform config (used if no CNFG chunk).
/// @return ParsedRequest on success, Error on parse failure.
Expected<ParsedRequest> parseRequest(const uint8_t *Buf, size_t BufSize,
                                     PlatformConfig DefaultConfig);

/// Write a RETN (return) response to the pre-allocated RETN chunk.
/// @param Buf The full RIFF buffer.
/// @param Parsed The parsed request (contains RETN offset/capacity).
/// @param Result The return value.
/// @param ErrNo The errno value (0 for success).
/// @param OutData Optional output data to include in a DATA sub-chunk.
/// @return Error if buffer too small.
Error writeReturn(uint8_t *Buf, const ParsedRequest &Parsed, intmax_t Result,
                  int ErrNo, ArrayRef<uint8_t> OutData = {});

/// Write an ERRO (error) response to the pre-allocated ERRO chunk.
/// @param Buf The full RIFF buffer.
/// @param Parsed The parsed request (contains ERRO offset/capacity).
/// @param ErrorCode The protocol error code.
/// @return Error if buffer too small.
Error writeError(uint8_t *Buf, const ParsedRequest &Parsed, uint16_t ErrorCode);

//===----------------------------------------------------------------------===//
// RIFF Builder - for constructing responses
//===----------------------------------------------------------------------===//

/// Helper class for building RIFF responses.
class RiffBuilder {
public:
  explicit RiffBuilder(MutableArrayRef<uint8_t> Buf) : Buf(Buf), Offset(0) {}

  /// Begin a RIFF container with the given form type.
  /// Returns the offset of the size field for later patching.
  Expected<size_t> beginContainer(uint32_t FormType);

  /// Begin a chunk with the given FourCC.
  /// Returns the offset of the size field for later patching.
  Expected<size_t> beginChunk(uint32_t FourCC);

  /// End a chunk, patching its size field.
  void endChunk(size_t SizeOffset);

  /// End the container, patching its size field.
  void endContainer(size_t SizeOffset);

  /// Write raw bytes.
  Error writeBytes(ArrayRef<uint8_t> Data);

  /// Write a single byte.
  Error writeByte(uint8_t Value);

  /// Write a 32-bit little-endian value.
  Error writeU32LE(uint32_t Value);

  /// Write a platform-sized integer.
  Error writeInt(intmax_t Value, const PlatformConfig &Config);

  /// Add padding for word alignment.
  void pad();

  /// Get the current offset.
  size_t getOffset() const { return Offset; }

private:
  MutableArrayRef<uint8_t> Buf;
  size_t Offset;
};

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_RIFFCODEC_H
