/*
 * LLDB MOS Plugin Logging
 *
 * Usage:
 *   #include "LLDBMOSLog.h"
 *   LLDB_MOS_LOG_REG("message: value={0}", value);
 *   LLDB_MOS_LOG_SYM("symbol: {0}", symbol);
 *   LLDB_MOS_LOG_ABI("ABI fallback");
 *
 * Enable logging in LLDB (must specify at least one category):
 *   (lldb) log enable target-mos reg
 *   (lldb) log enable target-mos sym
 *   (lldb) log enable target-mos abi
 *   (lldb) log enable target-mos reg sym abi   # Enable all categories
 *
 * List available categories:
 *   (lldb) log list target-mos
 *
 * Disable logging:
 *   (lldb) log disable target-mos
 *
 * Categories:
 *   reg  - Register operations
 *   sym  - Symbol operations
 *   abi  - ABI/fallback logic
 */

#include "LLDBMOSLog.h"
#include <cstddef> // for std::size if available

using namespace lldb_private;

namespace {
static const Log::Category g_lldb_mos_log_categories[] = {
    {"reg", "Register operations", LLDB_MOS_LOG_REG},
    {"sym", "Symbol operations", LLDB_MOS_LOG_SYM},
    {"abi", "ABI/fallback logic", LLDB_MOS_LOG_ABI},
};
constexpr size_t kNumCategories = sizeof(g_lldb_mos_log_categories) / sizeof(g_lldb_mos_log_categories[0]);
}

static Log::Channel g_lldb_mos_log_channel(
    llvm::ArrayRef<Log::Category>(g_lldb_mos_log_categories, kNumCategories),
    LLDB_MOS_LOG_NONE);

Log::Channel &lldb_private::GetLLDBMOSLogChannel() {
    return g_lldb_mos_log_channel;
}

namespace lldb_private {
void RegisterLLDBMOSLogChannel() {
    Log::Register("target-mos", g_lldb_mos_log_channel);
}
} 