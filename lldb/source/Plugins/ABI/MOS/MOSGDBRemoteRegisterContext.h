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
      bool read_all_registers_at_once, bool write_all_registers_at_once,
      const std::unordered_map<std::string, lldb::addr_t> &imaginary_register_map);

  bool ReadRegister(const lldb_private::RegisterInfo *reg_info,
                    lldb_private::RegisterValue &value) override;
  bool WriteRegister(const lldb_private::RegisterInfo *reg_info,
                     const lldb_private::RegisterValue &value) override;

private:
  // Reference to the ABI's imaginary register map (single source of truth for rc* addresses)
  const std::unordered_map<std::string, lldb::addr_t> &imaginary_register_map_;
  bool IsImaginaryRegister(const lldb_private::RegisterInfo *reg_info) const;
  bool IsProcessReady() const;
};