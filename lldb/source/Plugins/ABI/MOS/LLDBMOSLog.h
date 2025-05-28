#pragma once

#include "lldb/Utility/Log.h"
#include <cstdint>

// Define log categories as an enum for type safety
enum LLDBMOSLogCategory : uint64_t {
    LLDB_MOS_LOG_NONE = 0,
    LLDB_MOS_LOG_REG  = 1ull << 0,
    LLDB_MOS_LOG_SYM  = 1ull << 1,
    LLDB_MOS_LOG_ABI  = 1ull << 2,
    LLDB_MOS_LOG_ALL  = 0xFFFFFFFFFFFFFFFFull
};

// Central macro: only logs if enabled for the category
#define LLDB_MOS_LOG(CAT, ...) \
    do { \
        if (lldb_private::Log *log = lldb_private::GetLLDBMOSLogChannel().GetLog(CAT)) { \
            LLDB_LOG(log, __VA_ARGS__); \
        } \
    } while (0)

// Helper macros for common categories
#define LLDB_MOS_LOG_REG(...) LLDB_MOS_LOG(LLDB_MOS_LOG_REG, __VA_ARGS__)
#define LLDB_MOS_LOG_SYM(...) LLDB_MOS_LOG(LLDB_MOS_LOG_SYM, __VA_ARGS__)
#define LLDB_MOS_LOG_ABI(...) LLDB_MOS_LOG(LLDB_MOS_LOG_ABI, __VA_ARGS__)

// Channel getter (implement in .cpp)
namespace lldb_private {
    lldb_private::Log::Channel &GetLLDBMOSLogChannel();
    void RegisterLLDBMOSLogChannel();
} 