//===-- ABISysV_mos.h ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_ABI_MOS_ABISYSV_MOS_H
#define LLDB_SOURCE_PLUGINS_ABI_MOS_ABISYSV_MOS_H

#include "lldb/Target/ABI.h"
#include "lldb/lldb-private.h"

class ABISysV_mos : public lldb_private::RegInfoBasedABI {
public:
  ~ABISysV_mos() override = default;

  size_t GetRedZoneSize() const override;

  bool PrepareTrivialCall(lldb_private::Thread &thread, lldb::addr_t sp,
                          lldb::addr_t functionAddress,
                          lldb::addr_t returnAddress,
                          llvm::ArrayRef<lldb::addr_t> args) const override;

  bool GetArgumentValues(lldb_private::Thread &thread,
                         lldb_private::ValueList &values) const override;

  lldb_private::Status
  SetReturnValueObject(lldb::StackFrameSP &frame_sp,
                       lldb::ValueObjectSP &new_value) override;

  lldb::ValueObjectSP
  GetReturnValueObjectImpl(lldb_private::Thread &thread,
                           lldb_private::CompilerType &type) const override;

  lldb::UnwindPlanSP CreateFunctionEntryUnwindPlan() override;

  lldb::UnwindPlanSP CreateDefaultUnwindPlan() override;

  bool RegisterIsVolatile(const lldb_private::RegisterInfo *reg_info) override;

  bool CallFrameAddressIsValid(lldb::addr_t cfa) override {
    // 6502 addresses are 16-bit, no specific alignment requirements
    return cfa <= 0xFFFF;
  }

  bool CodeAddressIsValid(lldb::addr_t pc) override {
    // 6502 addresses are 16-bit
    return pc <= 0xFFFF;
  }

  const lldb_private::RegisterInfo *
  GetRegisterInfoArray(uint32_t &count) override;

  // Override to add imaginary registers dynamically
  void AugmentRegisterInfo(
      std::vector<lldb_private::DynamicRegisterInfo::Register> &regs) override;

  uint64_t GetStackFrameSize() override {
    return 256;
  } // 6502 stack is 256 bytes

  //------------------------------------------------------------------
  // Static Functions
  //------------------------------------------------------------------

  static void Initialize();

  static void Terminate();

  static lldb::ABISP CreateInstance(lldb::ProcessSP process_sp,
                                    const lldb_private::ArchSpec &arch);

  static llvm::StringRef GetPluginNameStatic() { return "sysv-mos"; }

  // PluginInterface protocol
  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

protected:
  void CreateRegisterMapIfNeeded();

  lldb::ValueObjectSP
  GetReturnValueObjectSimple(lldb_private::Thread &thread,
                             lldb_private::CompilerType &ast_type) const;

private:
  using lldb_private::RegInfoBasedABI::RegInfoBasedABI;

  // MOS-specific functionality for imaginary registers
  struct ImaginaryRegisterConfig {
    bool has_imaginary_regs = false;
    uint32_t max_rc_register =
        0; // Highest RC register found (e.g., 31 for RC31)
    uint32_t max_rs_register =
        0; // Highest RS register found (e.g., 15 for RS15)
    uint32_t frame_register_dwarf = LLDB_INVALID_REGNUM; // RS15 if present
  };

  ImaginaryRegisterConfig DetectImaginaryRegisters();
  void AddImaginaryRegistersToList(
      std::vector<lldb_private::DynamicRegisterInfo::Register> &regs,
      const ImaginaryRegisterConfig &config);
};

#endif // LLDB_SOURCE_PLUGINS_ABI_MOS_ABISYSV_MOS_H