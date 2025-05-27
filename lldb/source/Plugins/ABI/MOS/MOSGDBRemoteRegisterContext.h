#pragma once

#include "Plugins/Process/gdb-remote/GDBRemoteRegisterContext.h"
#include <string>
#include <unordered_map>

class MOSGDBRemoteRegisterContext
    : public lldb_private::process_gdb_remote::GDBRemoteRegisterContext {
public:
  MOSGDBRemoteRegisterContext(
      lldb_private::process_gdb_remote::ThreadGDBRemote &thread,
      uint32_t concrete_frame_idx,
      lldb_private::process_gdb_remote::GDBRemoteDynamicRegisterInfoSP
          reg_info_sp,
      bool read_all_registers_at_once, bool write_all_registers_at_once);

  bool ReadRegister(const lldb_private::RegisterInfo *reg_info,
                    lldb_private::RegisterValue &value) override;
  bool WriteRegister(const lldb_private::RegisterInfo *reg_info,
                     const lldb_private::RegisterValue &value) override;

private:
  // Map from DWARF regnum to zero page address for imaginary registers
  std::unordered_map<uint32_t, lldb::addr_t> m_imaginary_reg_addr;
  bool m_fallback_to_hardware_sp =
      false; // True if no imaginary registers found
  bool IsImaginaryRegister(const lldb_private::RegisterInfo *reg_info) const;
  void InitializeImaginaryRegisterMap();
  bool IsProcessReady() const;
};