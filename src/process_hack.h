#pragma once

#include <cstdint>
#include <vector>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace guillemot {
class ProcessHack {
public:
    ProcessHack() = default;
    ~ProcessHack();
    bool init();
    bool apply_override();
    bool remove_override();
    bool ready() const { return ready_; }
    std::string status_string() const;

private:
#ifdef _WIN32
    DWORD find_parent_pid();
    bool open_target_process(DWORD pid);
    bool find_module_base();
    bool map_sections();
    uint32_t rva_to_offset(uint32_t rva) const;
    uint32_t offset_to_rva(uint32_t offset) const;
    bool scan_for_target();

    struct Section {
        uint32_t vaddr;
        uint32_t vsize;
        uint32_t raddr;
        uint32_t rsize;
        std::string name;
    };

    HANDLE process_ = nullptr;
    uintptr_t module_base_ = 0;
    uintptr_t vtable_san_ptr_addr_ = 0;
    uint64_t orig_san_func_ = 0;
    uint64_t lan_func_ = 0;

    std::vector<Section> sections_;
#endif

    bool ready_ = false;
    bool overridden_ = false;
};

}
