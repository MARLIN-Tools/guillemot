#include "process_hack.h"

#include <sstream>
#include <algorithm>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace guillemot {

ProcessHack::~ProcessHack() {
    if (overridden_) remove_override();
#ifdef _WIN32
    if (process_) CloseHandle(process_);
#endif
}

std::string ProcessHack::status_string() const {
    std::ostringstream ss;
    if (!ready_) ss << "Boss memory linkage NOT ready";
    else ss << "Boss memory linkage active, override ptr=0x" << std::hex << vtable_san_ptr_addr_
            << ", overridden=" << (overridden_ ? "YES" : "no");
    return ss.str();
}

#ifdef _WIN32

static void log(const std::string& msg) {
    (void)msg;
}

bool ProcessHack::init() {
    ready_ = false;

    DWORD parent_pid = find_parent_pid();
    if (parent_pid == 0) { log("Could not find parent process"); return false; }

    if (!open_target_process(parent_pid)) { log("Could not open parent process"); return false; }

    if (!find_module_base()) { log("Could not find module base"); return false; }

    if (!map_sections()) { log("Could not map PE sections"); return false; }

    if (!scan_for_target()) { log("RTTI Pattern scan FAILED"); return false; }

    ready_ = true;
    log("Boss AI overrides initialized successfully!");
    return true;
}

DWORD ProcessHack::find_parent_pid() {
    DWORD my_pid = GetCurrentProcessId();
    DWORD parent_pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == my_pid) { parent_pid = pe.th32ParentProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return parent_pid;
}

bool ProcessHack::open_target_process(DWORD pid) {
    process_ = OpenProcess(
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    return process_ != nullptr;
}

bool ProcessHack::find_module_base() {
    HMODULE modules[1024]; DWORD needed = 0;
    if (!EnumProcessModules(process_, modules, sizeof(modules), &needed)) return false;
    module_base_ = reinterpret_cast<uintptr_t>(modules[0]);
    return true;
}

bool ProcessHack::map_sections() {
    IMAGE_DOS_HEADER dos{}; SIZE_T br = 0;
    if (!ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(module_base_), &dos, sizeof(dos), &br)) return false;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return false;

    IMAGE_NT_HEADERS64 nt{};
    uintptr_t nt_addr = module_base_ + dos.e_lfanew;
    if (!ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(nt_addr), &nt, sizeof(nt), &br)) return false;
    if (nt.Signature != IMAGE_NT_SIGNATURE) return false;

    sections_.clear();
    uintptr_t sec_addr = nt_addr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + nt.FileHeader.SizeOfOptionalHeader;
    for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER sec{};
        if (!ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(sec_addr + i * sizeof(sec)), &sec, sizeof(sec), &br)) continue;
        Section s;
        s.vaddr = sec.VirtualAddress;
        s.vsize = sec.Misc.VirtualSize;
        s.raddr = sec.PointerToRawData;
        s.rsize = sec.SizeOfRawData;
        char name[9] = {0};
        std::memcpy(name, sec.Name, 8);
        s.name = name;
        sections_.push_back(s);
    }
    return !sections_.empty();
}

uint32_t ProcessHack::rva_to_offset(uint32_t rva) const {
    for (const auto& s : sections_) {
        if (rva >= s.vaddr && rva < s.vaddr + s.vsize) {
            return s.raddr + (rva - s.vaddr);
        }
    }
    return 0;
}

uint32_t ProcessHack::offset_to_rva(uint32_t offset) const {
    for (const auto& s : sections_) {
        if (offset >= s.raddr && offset < s.raddr + s.rsize) {
            return s.vaddr + (offset - s.raddr);
        }
    }
    return 0;
}

bool ProcessHack::scan_for_target() {
    uint32_t image_size = 0;
    for (const auto& s : sections_) {
        image_size = (std::max)(image_size, s.raddr + s.rsize);
    }
    
    std::vector<uint8_t> data(image_size);
    SIZE_T br = 0;
    for (const auto& s : sections_) {
        ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(module_base_ + s.vaddr),
                          data.data() + s.raddr, s.rsize, &br);
    }

    const char* target_name = ".?AVStandardBoard@Chess@@";
    size_t name_len = std::strlen(target_name) + 1;
    
    auto it = std::search(data.begin(), data.end(), target_name, target_name + name_len);
    if (it == data.end()) {
        log("RTTI string for StandardBoard not found");
        return false;
    }
    
    uint32_t name_offset = static_cast<uint32_t>(std::distance(data.begin(), it));
    uint32_t type_desc_offset = name_offset - 16;
    uint32_t type_desc_rva = offset_to_rva(type_desc_offset);
    
    std::vector<uint32_t> col_offsets;
    uint32_t search_val = type_desc_rva;
    
    for (size_t i = 0; i + 4 <= data.size(); i += 4) {
        uint32_t val;
        std::memcpy(&val, data.data() + i, 4);
        if (val == search_val) {
            uint32_t col_start_offset = static_cast<uint32_t>(i - 12);
            if (col_start_offset >= data.size()) continue;
            
            uint32_t sig, pself;
            std::memcpy(&sig, data.data() + col_start_offset, 4);
            std::memcpy(&pself, data.data() + col_start_offset + 20, 4);
            
            if (sig == 1 && pself == offset_to_rva(col_start_offset)) {
                col_offsets.push_back(col_start_offset);
            }
        }
    }
    
    if (col_offsets.empty()) {
        log("COL not found");
        return false;
    }
    
    for (uint32_t col_offset : col_offsets) {
        uint32_t col_rva = offset_to_rva(col_offset);
        uint64_t col_addr = module_base_ + col_rva;
        
        for (size_t i = 0; i + 8 <= data.size(); i += 8) {
            uint64_t val;
            std::memcpy(&val, data.data() + i, 8);
            if (val == col_addr) {
                uint32_t vtable_offset = static_cast<uint32_t>(i + 8);
                uint32_t vtable_rva = offset_to_rva(vtable_offset);
                
                std::ostringstream vs;
                vs << "BINGO! Found StandardBoard vtable at RVA 0x" << std::hex << vtable_rva;
                log(vs.str());
                
                int lan_index = 25;
                int san_index = 26;
                uint32_t lan_ptr_offset = vtable_offset + (lan_index * 8);
                uint32_t san_ptr_offset = vtable_offset + (san_index * 8);
                
                std::memcpy(&lan_func_, data.data() + lan_ptr_offset, 8);
                std::memcpy(&orig_san_func_, data.data() + san_ptr_offset, 8);
                vtable_san_ptr_addr_ = module_base_ + vtable_rva + (san_index * 8);
                
                std::ostringstream fs;
                fs << "moveFromLanString impl: 0x" << std::hex << lan_func_ << " | ";
                fs << "moveFromSanString impl: 0x" << std::hex << orig_san_func_;
                log(fs.str());
                return true;
            }
        }
    }
    
    log("VTable pointer to COL not found");
    return false;
}

bool ProcessHack::apply_override() {
    if (!ready_ || overridden_) return overridden_;

    DWORD old_protect = 0;
    if (!VirtualProtectEx(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_),
                          8, PAGE_EXECUTE_READWRITE, &old_protect)) {
        log("VirtualProtectEx failed");
        return false;
    }

    SIZE_T bw = 0;
    if (!WriteProcessMemory(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_), 
                            &lan_func_, 8, &bw)) {
        log("Write failed");
        VirtualProtectEx(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_), 8, old_protect, &old_protect);
        return false;
    }
    FlushInstructionCache(process_, reinterpret_cast<LPCVOID>(vtable_san_ptr_addr_), 8);
    
    VirtualProtectEx(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_), 8, old_protect, &old_protect);
    
    overridden_ = true;
    log("Overrides applied successfully!");
    return true;
}

bool ProcessHack::remove_override() {
    if (!ready_ || !overridden_) return true;

    DWORD old_protect = 0;
    VirtualProtectEx(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_),
                     8, PAGE_EXECUTE_READWRITE, &old_protect);

    SIZE_T bw = 0;
    if (!WriteProcessMemory(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_),
                            &orig_san_func_, 8, &bw)) {
        log("Override removal failed");
        VirtualProtectEx(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_), 8, old_protect, &old_protect);
        return false;
    }

    FlushInstructionCache(process_, reinterpret_cast<LPCVOID>(vtable_san_ptr_addr_), 8);
    VirtualProtectEx(process_, reinterpret_cast<LPVOID>(vtable_san_ptr_addr_), 8, old_protect, &old_protect);
    
    overridden_ = false;
    return true;
}

#else
static void log(const std::string& msg) { (void)msg; }
bool ProcessHack::init() { log("Not on Windows"); return false; }
bool ProcessHack::apply_override() { return false; }
bool ProcessHack::remove_override() { return true; }
#endif

}
