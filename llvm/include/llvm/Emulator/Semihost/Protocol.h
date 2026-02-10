//===-- llvm/Emulator/Semihost/Protocol.h - Wire Protocol ------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Wire protocol constants for ZBC semihosting: opcodes, RIFF FourCC codes,
/// register definitions, and error codes.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_PROTOCOL_H
#define LLVM_EMULATOR_SEMIHOST_PROTOCOL_H

#include <cstdint>

namespace llvm {
namespace emu {
namespace semihost {

//===----------------------------------------------------------------------===//
// ARM Semihosting Opcodes
//===----------------------------------------------------------------------===//

enum class Opcode : uint8_t {
  Open = 0x01,
  Close = 0x02,
  WriteC = 0x03,
  Write0 = 0x04,
  Write = 0x05,
  Read = 0x06,
  ReadC = 0x07,
  IsError = 0x08,
  IsTTY = 0x09,
  Seek = 0x0A,
  FLen = 0x0C,
  TmpNam = 0x0D,
  Remove = 0x0E,
  Rename = 0x0F,
  Clock = 0x10,
  Time = 0x11,
  System = 0x12,
  Errno = 0x13,
  GetCmdLine = 0x15,
  HeapInfo = 0x16,
  Exit = 0x18,
  ExitExtended = 0x20,
  Elapsed = 0x30,
  TickFreq = 0x31,
  TimerConfig = 0x32,
};

//===----------------------------------------------------------------------===//
// Open Mode Flags (ARM semihosting compatible)
//===----------------------------------------------------------------------===//

enum class OpenMode : uint8_t {
  R = 0,        // "r"
  RB = 1,       // "rb"
  RPlus = 2,    // "r+"
  RPlusB = 3,   // "r+b"
  W = 4,        // "w"
  WB = 5,       // "wb"
  WPlus = 6,    // "w+"
  WPlusB = 7,   // "w+b"
  A = 8,        // "a"
  AB = 9,       // "ab"
  APlus = 10,   // "a+"
  APlusB = 11,  // "a+b"
};

/// Convert OpenMode to fopen mode string.
inline const char *openModeToString(OpenMode Mode) {
  switch (Mode) {
  case OpenMode::R:      return "r";
  case OpenMode::RB:     return "rb";
  case OpenMode::RPlus:  return "r+";
  case OpenMode::RPlusB: return "r+b";
  case OpenMode::W:      return "w";
  case OpenMode::WB:     return "wb";
  case OpenMode::WPlus:  return "w+";
  case OpenMode::WPlusB: return "w+b";
  case OpenMode::A:      return "a";
  case OpenMode::AB:     return "ab";
  case OpenMode::APlus:  return "a+";
  case OpenMode::APlusB: return "a+b";
  }
  return nullptr;
}

/// Check if the open mode implies writing.
inline bool openModeIsWrite(OpenMode Mode) {
  switch (Mode) {
  case OpenMode::R:
  case OpenMode::RB:
    return false;
  default:
    return true;
  }
}

//===----------------------------------------------------------------------===//
// RIFF FourCC Codes
//===----------------------------------------------------------------------===//

/// Construct a FourCC code from 4 characters (little-endian).
constexpr uint32_t makeFourCC(char A, char B, char C, char D) {
  return static_cast<uint32_t>(A) | (static_cast<uint32_t>(B) << 8) |
         (static_cast<uint32_t>(C) << 16) | (static_cast<uint32_t>(D) << 24);
}

namespace FourCC {
constexpr uint32_t RIFF = makeFourCC('R', 'I', 'F', 'F');
constexpr uint32_t SEMI = makeFourCC('S', 'E', 'M', 'I');
constexpr uint32_t CNFG = makeFourCC('C', 'N', 'F', 'G');
constexpr uint32_t CALL = makeFourCC('C', 'A', 'L', 'L');
constexpr uint32_t PARM = makeFourCC('P', 'A', 'R', 'M');
constexpr uint32_t DATA = makeFourCC('D', 'A', 'T', 'A');
constexpr uint32_t RETN = makeFourCC('R', 'E', 'T', 'N');
constexpr uint32_t ERRO = makeFourCC('E', 'R', 'R', 'O');
} // namespace FourCC

//===----------------------------------------------------------------------===//
// Device Register Offsets
//===----------------------------------------------------------------------===//

namespace DeviceReg {
constexpr uint64_t Signature = 0x00;   // 8 bytes, R - ASCII "SEMIHOST"
constexpr uint64_t RiffPtr = 0x08;     // 8 bytes, RW - pointer to RIFF buffer
constexpr uint64_t Reserved = 0x10;    // 8 bytes, reserved
constexpr uint64_t Doorbell = 0x18;    // 1 byte, W - write to trigger request
constexpr uint64_t Status = 0x19;      // 1 byte, RW - interrupt pending
constexpr uint64_t Size = 0x20;        // Total register space: 32 bytes
} // namespace DeviceReg

//===----------------------------------------------------------------------===//
// Status Register Values
//===----------------------------------------------------------------------===//

namespace Status {
constexpr uint8_t None = 0;   // No interrupt pending
constexpr uint8_t Timer = 1;  // Timer tick occurred
} // namespace Status

//===----------------------------------------------------------------------===//
// Signature
//===----------------------------------------------------------------------===//

constexpr char SignatureStr[] = "SEMIHOST";
constexpr size_t SignatureSize = 8;

//===----------------------------------------------------------------------===//
// PARM/DATA Chunk Type Codes
//===----------------------------------------------------------------------===//

namespace ParmType {
constexpr uint8_t Int = 0x01;
constexpr uint8_t Ptr = 0x02;
} // namespace ParmType

namespace DataType {
constexpr uint8_t Binary = 0x01;
constexpr uint8_t String = 0x02;
} // namespace DataType

//===----------------------------------------------------------------------===//
// Wire Format Structures
//===----------------------------------------------------------------------===//

#pragma pack(push, 1)

/// Generic RIFF chunk header: id(4) + size(4).
struct ChunkHeader {
  uint32_t Id;    ///< FourCC, little-endian
  uint32_t Size;  ///< Payload size in bytes (not including this header)
};

/// RIFF container header: "RIFF"(4) + size(4) + form_type(4).
struct RiffHeader {
  uint32_t RiffId;    ///< Must be FourCC::RIFF
  uint32_t Size;      ///< Size of everything after this field
  uint32_t FormType;  ///< e.g., FourCC::SEMI
};

/// CNFG chunk payload.
struct CnfgPayload {
  uint8_t IntSize;     ///< Guest integer size (1, 2, 4, or 8)
  uint8_t PtrSize;     ///< Guest pointer size (1, 2, 4, or 8)
  uint8_t Endianness;  ///< 0=little, 1=big
  uint8_t Reserved;
};

/// CALL chunk header (before sub-chunks).
struct CallHeader {
  uint8_t OpCode;      ///< SH_SYS_* opcode
  uint8_t Reserved[3];
};

/// PARM chunk header (value bytes follow).
struct ParmHeader {
  uint8_t Type;        ///< ParmType::Int or ParmType::Ptr
  uint8_t Reserved[3];
  // Followed by IntSize or PtrSize bytes of value data
};

/// DATA chunk header (payload bytes follow).
struct DataHeader {
  uint8_t Type;        ///< DataType::Binary or DataType::String
  uint8_t Reserved[3];
  // Followed by variable-length payload
};

/// ERRO chunk payload.
struct ErroPayload {
  uint16_t ErrorCode;  ///< Protocol error code, little-endian
  uint8_t Reserved[2];
  // Optional error message may follow
};

#pragma pack(pop)

/// Round size up to word boundary (RIFF requires even-byte alignment).
constexpr size_t padSize(size_t Size) { return (Size + 1) & ~size_t(1); }

/// RETN errno field is always 32-bit little-endian.
constexpr size_t RetnErrnoSize = 4;

//===----------------------------------------------------------------------===//
// Protocol Error Codes (in ERRO chunk)
//===----------------------------------------------------------------------===//

namespace ProtoError {
constexpr uint16_t InvalidChunk = 0x01;
constexpr uint16_t MalformedRiff = 0x02;
constexpr uint16_t MissingCnfg = 0x03;
constexpr uint16_t UnsupportedOp = 0x04;
constexpr uint16_t InvalidParams = 0x05;
} // namespace ProtoError

/// Endianness values for CNFG chunk.
namespace Endian {
constexpr uint8_t Little = 0;
constexpr uint8_t Big = 1;
} // namespace Endian

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_PROTOCOL_H
