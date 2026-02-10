//===-- RiffCodec.cpp - RIFF Codec Implementation --------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost/RiffCodec.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Error.h"

using namespace llvm;
using namespace llvm::emu::semihost;
using namespace llvm::support;

//===----------------------------------------------------------------------===//
// Integer Read/Write with Runtime Size
//===----------------------------------------------------------------------===//

intmax_t readInt(const uint8_t *Buf, uint8_t Size,
                           llvm::endianness Endian) {
  uintmax_t Value = readUInt(Buf, Size, Endian);

  // Sign extend if high bit is set
  if (Size < sizeof(intmax_t)) {
    uintmax_t SignBit = uintmax_t(1) << (Size * 8 - 1);
    if (Value & SignBit) {
      uintmax_t SignExtend = ~((uintmax_t(1) << (Size * 8)) - 1);
      Value |= SignExtend;
    }
  }
  return static_cast<intmax_t>(Value);
}

uintmax_t readUInt(const uint8_t *Buf, uint8_t Size,
                             llvm::endianness Endian) {
  uintmax_t Value = 0;

  if (Endian == llvm::endianness::little) {
    for (int I = Size - 1; I >= 0; --I)
      Value = (Value << 8) | Buf[I];
  } else {
    for (int I = 0; I < Size; ++I)
      Value = (Value << 8) | Buf[I];
  }
  return Value;
}

void writeInt(uint8_t *Buf, intmax_t Value, uint8_t Size,
                        llvm::endianness Endian) {
  writeUInt(Buf, static_cast<uintmax_t>(Value), Size, Endian);
}

void writeUInt(uint8_t *Buf, uintmax_t Value, uint8_t Size,
                         llvm::endianness Endian) {
  if (Endian == llvm::endianness::little) {
    for (int I = 0; I < Size; ++I) {
      Buf[I] = static_cast<uint8_t>(Value & 0xFF);
      Value >>= 8;
    }
  } else {
    for (int I = Size - 1; I >= 0; --I) {
      Buf[I] = static_cast<uint8_t>(Value & 0xFF);
      Value >>= 8;
    }
  }
}

//===----------------------------------------------------------------------===//
// RIFF Parsing
//===----------------------------------------------------------------------===//

namespace {

/// Parse sub-chunks within a CALL or RETN chunk.
void parseSubchunks(const uint8_t *Start, const uint8_t *End,
                    const PlatformConfig &Config, ParsedRequest &Out) {
  const uint8_t *Pos = Start;

  while (Pos + sizeof(ChunkHeader) <= End) {
    auto *Hdr = reinterpret_cast<const ChunkHeader *>(Pos);
    uint32_t Id = endian::read32le(&Hdr->Id);
    uint32_t Size = endian::read32le(&Hdr->Size);
    const uint8_t *Data = Pos + sizeof(ChunkHeader);

    // Bounds check
    if (Data + Size > End)
      break;

    if (Id == FourCC::PARM && Out.Parms.size() < MaxParms) {
      if (Size >= sizeof(ParmHeader)) {
        auto *Parm = reinterpret_cast<const ParmHeader *>(Data);
        uint8_t ValueSize =
            (Parm->Type == ParmType::Ptr) ? Config.PtrSize : Config.IntSize;

        if (Size >= sizeof(ParmHeader) + ValueSize) {
          intmax_t Value = llvm::emu::semihost::readInt(
              Data + sizeof(ParmHeader), ValueSize, Config.Endian);
          Out.Parms.push_back(Value);
        }
      }
    } else if (Id == FourCC::DATA && Out.DataChunks.size() < MaxData) {
      if (Size >= sizeof(DataHeader)) {
        Out.DataChunks.push_back(
            {ArrayRef<uint8_t>(Data + sizeof(DataHeader),
                               Size - sizeof(DataHeader))});
      }
    }

    // Advance to next chunk (with padding)
    Pos += sizeof(ChunkHeader) + padSize(Size);
  }
}

} // namespace

Expected<ParsedRequest> parseRequest(const uint8_t *Buf,
                                               size_t BufSize,
                                               PlatformConfig DefaultConfig) {
  ParsedRequest Out(DefaultConfig);

  // Validate RIFF header
  if (BufSize < sizeof(RiffHeader))
    return createStringError(inconvertibleErrorCode(),
                             "Buffer too small for RIFF header");

  auto *Riff = reinterpret_cast<const RiffHeader *>(Buf);

  if (endian::read32le(&Riff->RiffId) != FourCC::RIFF)
    return createStringError(inconvertibleErrorCode(), "Bad RIFF magic");

  if (endian::read32le(&Riff->FormType) != FourCC::SEMI)
    return createStringError(inconvertibleErrorCode(),
                             "Wrong form type (expected SEMI)");

  uint32_t RiffSize = endian::read32le(&Riff->Size);
  size_t TotalSize = 8 + RiffSize; // riff_id(4) + size(4) + payload
  if (TotalSize > BufSize)
    return createStringError(inconvertibleErrorCode(),
                             "RIFF size exceeds buffer");

  const uint8_t *RiffEnd = Buf + TotalSize;
  const uint8_t *Pos = Buf + sizeof(RiffHeader);

  // Walk all top-level chunks
  while (Pos + sizeof(ChunkHeader) <= RiffEnd) {
    auto *Hdr = reinterpret_cast<const ChunkHeader *>(Pos);
    uint32_t Id = endian::read32le(&Hdr->Id);
    uint32_t Size = endian::read32le(&Hdr->Size);
    const uint8_t *Data = Pos + sizeof(ChunkHeader);

    // Bounds check
    if (Data + Size > RiffEnd)
      return createStringError(inconvertibleErrorCode(),
                               "Chunk data exceeds container");

    if (Id == FourCC::CNFG) {
      if (Size >= sizeof(CnfgPayload)) {
        auto *Cnfg = reinterpret_cast<const CnfgPayload *>(Data);
        Out.Config.IntSize = Cnfg->IntSize;
        Out.Config.PtrSize = Cnfg->PtrSize;
        Out.Config.Endian = (Cnfg->Endianness == Endian::Little)
                                ? llvm::endianness::little
                                : llvm::endianness::big;
        Out.HasCnfg = true;

        // Validate sizes
        if (!Out.Config.isValid())
          return createStringError(inconvertibleErrorCode(),
                                   "Invalid int_size or ptr_size in CNFG");
      }
    } else if (Id == FourCC::CALL) {
      if (Size >= sizeof(CallHeader)) {
        auto *Call = reinterpret_cast<const CallHeader *>(Data);
        Out.Op = static_cast<Opcode>(Call->OpCode);
        Out.HasCall = true;
        // Parse sub-chunks within CALL
        parseSubchunks(Data + sizeof(CallHeader), Data + Size, Out.Config, Out);
      }
    } else if (Id == FourCC::RETN) {
      Out.RetnPayloadOffset = Data - Buf;
      Out.RetnPayloadCapacity = Size;
    } else if (Id == FourCC::ERRO) {
      Out.ErroPayloadOffset = Data - Buf;
      Out.ErroPayloadCapacity = Size;
    }
    // Skip unknown chunks silently

    // Advance to next chunk (with padding)
    Pos += sizeof(ChunkHeader) + padSize(Size);
  }

  return Out;
}

//===----------------------------------------------------------------------===//
// Response Writing
//===----------------------------------------------------------------------===//

Error writeReturn(uint8_t *Buf, const ParsedRequest &Parsed,
                            intmax_t Result, int ErrNo,
                            ArrayRef<uint8_t> OutData) {
  if (Parsed.RetnPayloadOffset == 0)
    return createStringError(inconvertibleErrorCode(), "No RETN chunk");

  size_t Needed = Parsed.Config.IntSize + RetnErrnoSize;
  if (!OutData.empty())
    Needed += sizeof(ChunkHeader) + sizeof(DataHeader) + OutData.size();

  if (Parsed.RetnPayloadCapacity < Needed)
    return createStringError(inconvertibleErrorCode(),
                             "RETN chunk too small for response");

  uint8_t *Dst = Buf + Parsed.RetnPayloadOffset;

  // Write result value
  llvm::emu::semihost::writeInt(Dst, Result, Parsed.Config.IntSize,
                                 Parsed.Config.Endian);
  Dst += Parsed.Config.IntSize;

  // Write errno (always 32-bit LE)
  endian::write32le(Dst, static_cast<uint32_t>(ErrNo));
  Dst += RetnErrnoSize;

  // Write DATA sub-chunk if output data provided
  if (!OutData.empty()) {
    auto *DataHdr = reinterpret_cast<ChunkHeader *>(Dst);
    endian::write32le(&DataHdr->Id, FourCC::DATA);
    endian::write32le(&DataHdr->Size,
                      static_cast<uint32_t>(sizeof(DataHeader) + OutData.size()));
    Dst += sizeof(ChunkHeader);

    auto *DataPl = reinterpret_cast<DataHeader *>(Dst);
    DataPl->Type = DataType::Binary;
    DataPl->Reserved[0] = DataPl->Reserved[1] = DataPl->Reserved[2] = 0;
    Dst += sizeof(DataHeader);

    std::memcpy(Dst, OutData.data(), OutData.size());
  }

  return Error::success();
}

Error writeError(uint8_t *Buf, const ParsedRequest &Parsed,
                           uint16_t ErrorCode) {
  if (Parsed.ErroPayloadOffset == 0)
    return createStringError(inconvertibleErrorCode(), "No ERRO chunk");

  if (Parsed.ErroPayloadCapacity < sizeof(ErroPayload))
    return createStringError(inconvertibleErrorCode(),
                             "ERRO chunk too small for response");

  auto *Erro =
      reinterpret_cast<ErroPayload *>(Buf + Parsed.ErroPayloadOffset);
  endian::write16le(&Erro->ErrorCode, ErrorCode);
  Erro->Reserved[0] = Erro->Reserved[1] = 0;

  return Error::success();
}

//===----------------------------------------------------------------------===//
// RiffBuilder
//===----------------------------------------------------------------------===//

Expected<size_t> RiffBuilder::beginContainer(uint32_t FormType) {
  if (Offset + sizeof(RiffHeader) > Buf.size())
    return createStringError(inconvertibleErrorCode(),
                             "Buffer too small for RIFF header");

  auto *Hdr = reinterpret_cast<RiffHeader *>(Buf.data() + Offset);
  endian::write32le(&Hdr->RiffId, FourCC::RIFF);
  size_t SizeOffset = Offset + 4; // Offset of the size field
  endian::write32le(&Hdr->Size, 0); // Placeholder
  endian::write32le(&Hdr->FormType, FormType);
  Offset += sizeof(RiffHeader);

  return SizeOffset;
}

Expected<size_t> RiffBuilder::beginChunk(uint32_t FourCC) {
  if (Offset + sizeof(ChunkHeader) > Buf.size())
    return createStringError(inconvertibleErrorCode(),
                             "Buffer too small for chunk header");

  auto *Hdr = reinterpret_cast<ChunkHeader *>(Buf.data() + Offset);
  endian::write32le(&Hdr->Id, FourCC);
  size_t SizeOffset = Offset + 4; // Offset of the size field
  endian::write32le(&Hdr->Size, 0); // Placeholder
  Offset += sizeof(ChunkHeader);

  return SizeOffset;
}

void RiffBuilder::endChunk(size_t SizeOffset) {
  // Calculate chunk data size (current offset - end of chunk header)
  size_t DataStart = SizeOffset + 4;
  uint32_t DataSize = static_cast<uint32_t>(Offset - DataStart);
  endian::write32le(Buf.data() + SizeOffset, DataSize);
  pad();
}

void RiffBuilder::endContainer(size_t SizeOffset) {
  // RIFF size counts everything after the size field (form_type + chunks)
  size_t DataStart = SizeOffset + 4;
  uint32_t ContentSize = static_cast<uint32_t>(Offset - DataStart);
  endian::write32le(Buf.data() + SizeOffset, ContentSize);
}

Error RiffBuilder::writeBytes(ArrayRef<uint8_t> Data) {
  if (Offset + Data.size() > Buf.size())
    return createStringError(inconvertibleErrorCode(), "Buffer overflow");

  std::memcpy(Buf.data() + Offset, Data.data(), Data.size());
  Offset += Data.size();
  return Error::success();
}

Error RiffBuilder::writeByte(uint8_t Value) {
  if (Offset >= Buf.size())
    return createStringError(inconvertibleErrorCode(), "Buffer overflow");

  Buf[Offset++] = Value;
  return Error::success();
}

Error RiffBuilder::writeU32LE(uint32_t Value) {
  if (Offset + 4 > Buf.size())
    return createStringError(inconvertibleErrorCode(), "Buffer overflow");

  endian::write32le(Buf.data() + Offset, Value);
  Offset += 4;
  return Error::success();
}

Error RiffBuilder::writeInt(intmax_t Value, const PlatformConfig &Config) {
  if (Offset + Config.IntSize > Buf.size())
    return createStringError(inconvertibleErrorCode(), "Buffer overflow");

  llvm::emu::semihost::writeInt(Buf.data() + Offset, Value, Config.IntSize,
                                 Config.Endian);
  Offset += Config.IntSize;
  return Error::success();
}

void RiffBuilder::pad() {
  if ((Offset & 1) && Offset < Buf.size()) {
    Buf[Offset++] = 0;
  }
}
