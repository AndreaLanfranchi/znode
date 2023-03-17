/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#ifdef __linux__
#include <unistd.h>

#include <cstdio>
#include <limits>

#include <sys/mman.h>
#endif

#ifdef __APPLE__
#include <limits.h>
#include <unistd.h>

#include <mach/mach_init.h>
#include <mach/task.h>
#include <sys/mman.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
// clang-format off
#include <windows.h>
#include <Psapi.h>
// clang-format on
#endif

#include <zen/core/common/memory.hpp>

namespace zen {
// Inspired by:
// https://stackoverflow.com/questions/372484/how-do-i-programmatically-check-memory-use-in-a-fairly-portable-way-c-c
size_t get_mem_usage(bool resident) {
    size_t ret{0};
#if defined(__linux__)
    // getrusage doesn't work well on Linux. Try grabbing info directly from the /proc pseudo-filesystem.
    // Reading from /proc/self/statm gives info on your own process, as one line of numbers that are:
    // virtual mem program size, resident set size, shared pages, text/code, data/stack, library, dirty pages.
    // The mem sizes should all be multiplied by the page size.
    size_t vm_size = 0, rm_size = 0;
    FILE* file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long vm = 0, rm = 0;
        if (fscanf(file, "%lu %lu", &vm, &rm) == 2) {  // the first 2 num: vm size, resident set size
            vm_size = vm * static_cast<size_t>(getpagesize());
            rm_size = rm * static_cast<size_t>(getpagesize());
        }
        fclose(file);
    }
    ret = resident ? rm_size : vm_size;

#elif defined(__APPLE__)
    // Inspired by: http://miknight.blogspot.com/2005/11/resident-set-size-in-mac-os-x.html
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    task_info(current_task(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&t_info), &t_info_count);
    ret = (resident ? t_info.resident_size : t_info.virtual_size);

#elif defined(_WIN32) || defined(_WIN64)
    static HANDLE phandle{GetCurrentProcess()};
    PROCESS_MEMORY_COUNTERS_EX counters;
    if (K32GetProcessMemoryInfo(phandle, (PROCESS_MEMORY_COUNTERS*)&counters, sizeof(counters))) {
        ret = resident ? counters.WorkingSetSize : counters.PagefileUsage;
    }
#else
    // Unsupported platform
    (void)resident;  // disable unused-parameter warning
#endif
    return ret;
}

size_t get_system_page_size() {
    size_t ret{0};
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    ret = systemInfo.dwPageSize;
#elif defined(PAGESIZE)  // defined in limits.h
    ret = PAGESIZE;
#else                    // assume POSIX OS
    ret = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
    return ret;
}

void memory_cleanse(void* ptr, size_t size) {
#if defined(WIN32)
    SecureZeroMemory(ptr, size);
#else
    std::memset(ptr, static_cast<int>('\0'), size);
#endif
}

bool MemoryPageLocker::lock(const void* addr, size_t len) {
#ifdef WIN32
    return VirtualLock(const_cast<void*>(addr), len) != 0;
#else
    return mlock(addr, len) == 0;
#endif
}

bool MemoryPageLocker::unlock(const void* addr, size_t len) {
#ifdef WIN32
    return VirtualUnlock(const_cast<void*>(addr), len) != 0;
#else
    return munlock(addr, len) == 0;
#endif
}

LockedPagesManager* LockedPagesManager::instance_ = nullptr;
std::once_flag LockedPagesManager::create_instance_once_{};

}  // namespace zen
