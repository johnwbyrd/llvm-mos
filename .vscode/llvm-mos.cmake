# llvm-mos.cmake
# For priming the CMake cache before a build

set(LLVM_TARGETS_TO_BUILD X86 
    CACHE STRING "LLVM targets to build")
set(LLVM_OPTIMIZED_TABLEGEN OFF 
    CACHE BOOL "LLVM optimized tablegen")
set(LLVM_EXPERIMENTAL_TARGETS_TO_BUILD MOS 
    CACHE STRING "LLVM experimental targets to build")
set(LLVM_ENABLE_PROJECTS clang;clang-tools-extra;lld;lldb
    CACHE STRING "LLVM enable projects")
set(LIBXML2_LIBRARIES "IGNORE" 
    CACHE STRING "LibXML2 libraries")
set(LLVM_INSTALL_TOOLCHAIN_ONLY ON 
    CACHE BOOL "LLVM install toolchain only")
set(CMAKE_BUILD_TYPE Release 
    CACHE STRING "CMake build type")
	
# disable lldb testing until the lldb tests stabilize
set(LLDB_INCLUDE_TESTS OFF 
    CACHE BOOL "Include lldb tests")
	
# sccache or ccache is used on every platform to speed up builds
find_program(LLVM_MOS_CCACHE 
    NAMES sccache ccache
    DOC "Path to compiler caching program"
	REQUIRED)
set(CMAKE_C_COMPILER_LAUNCHER ${LLVM_MOS_CCACHE})
set(CMAKE_CXX_COMPILER_LAUNCHER ${LLVM_MOS_CCACHE})

# Ship the release with these tools
set(LLVM_MOS_TOOLCHAIN_TOOLS
    bugpoint
    dsymutil
    llc
    llvm-addr2line
    llvm-ar
    llvm-as
    llvm-cat
    llvm-cov
    llvm-diff
    llvm-dwarfdump
    llvm-profdata
    llvm-lib
    llvm-link
    llvm-mc
    llvm-mca
    llvm-mt
    llvm-nm
    llvm-objcopy
    llvm-objdump
    llvm-profdata
    llvm-ranlib
    llvm-readelf
    llvm-readobj
    llvm-size
    llvm-strings
    llvm-strip
    opt
)
set(LLVM_TOOLCHAIN_TOOLS ${LLVM_MOS_TOOLCHAIN_TOOLS} 
    CACHE STRING "LLVM toolchain tools")
