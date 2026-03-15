#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>

#include "memory_utils.h"

// ─── NT Structures ───

typedef struct _OBJECT_ATTRIBUTES_NT {
    ULONG  Length;
    HANDLE RootDirectory;
    PVOID  ObjectName;
    ULONG  Attributes;
    PVOID  SecurityDescriptor;
    PVOID  SecurityQualityOfService;
} OBJECT_ATTRIBUTES_NT;

typedef struct _CLIENT_ID_NT {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID_NT;

// ─── Hardened Syscall Invoker ───
//
// Fixes:
//   1. No RWX memory — use RX after writing (W^X policy)
//   2. Stubs mimic real ntdll layout to avoid signature detection
//   3. Stubs are placed inside ntdll's memory range (indirect syscall)
//      OR we use the "trampoline" technique

class SyscallInvoker {
public:
    ~SyscallInvoker() {
        // Wipe stubs before freeing
        if (stub_mem_) {
            DWORD old;
            VirtualProtect(stub_mem_, 4096, PAGE_READWRITE, &old);
            SecureZeroMemory(stub_mem_, 4096);
            VirtualFree(stub_mem_, 0, MEM_RELEASE);
            stub_mem_ = nullptr;
        }
    }

    bool init() {
        // Allocate as RW first (not RWX!)
        stub_mem_ = (BYTE*)VirtualAlloc(
            nullptr, 4096,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE          // ← NOT execute, just write
        );
        if (!stub_mem_) return false;

        memset(stub_mem_, 0x90, 4096); // NOP fill, not int3

        // Find the 'syscall; ret' gadget inside real ntdll
        // This makes the actual syscall instruction execute
        // from ntdll's memory — fixing the RIP anomaly
        syscall_ret_gadget_ = find_syscall_ret_gadget();
        if (!syscall_ret_gadget_) {
            printf("[-] Could not find syscall;ret gadget in ntdll\n");
            // Fallback to direct syscall (less safe)
            use_indirect_ = false;
        } else {
            printf("[+] syscall;ret gadget: %p\n", syscall_ret_gadget_);
            use_indirect_ = true;
        }

        build_stub(IDX_OPEN,  0);
        build_stub(IDX_READ,  0);
        build_stub(IDX_CLOSE, 0);

        // Flip to RX — no more writing allowed
        DWORD old_protect;
        VirtualProtect(stub_mem_, 4096, PAGE_EXECUTE_READ, &old_protect);

        return true;
    }

    void set_ssn(int index, DWORD ssn) {
        // Temporarily make writable to patch SSN
        DWORD old;
        VirtualProtect(stub_mem_, 4096, PAGE_READWRITE, &old);

        BYTE* stub = stub_mem_ + index * STRIDE;
        memcpy(stub + 4, &ssn, sizeof(DWORD));

        // Rebuild the stub with correct SSN
        build_stub(index, ssn);

        // Back to RX
        VirtualProtect(stub_mem_, 4096, PAGE_EXECUTE_READ, &old);
        FlushInstructionCache(GetCurrentProcess(), stub, STRIDE);
    }

    void* get_stub(int index) const {
        if (!stub_mem_) return nullptr;
        return stub_mem_ + index * STRIDE;
    }

    bool is_ready() const { return stub_mem_ != nullptr; }

    static constexpr int IDX_OPEN  = 0;
    static constexpr int IDX_READ  = 1;
    static constexpr int IDX_CLOSE = 2;

private:
    static constexpr int STRIDE = 32;
    BYTE* stub_mem_ = nullptr;
    void* syscall_ret_gadget_ = nullptr;
    bool  use_indirect_ = false;

    // Find 'syscall; ret' (0F 05 C3) inside ntdll.dll's .text section
    // When we jump here, the CPU's RIP is inside ntdll — looks legitimate
    static void* find_syscall_ret_gadget() {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return nullptr;

        BYTE* base = (BYTE*)ntdll;
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        auto* sec = IMAGE_FIRST_SECTION(nt);

        // Find .text section
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                BYTE* start = base + sec[i].VirtualAddress;
                DWORD size  = sec[i].Misc.VirtualSize;

                // Scan for 0F 05 C3 (syscall; ret)
                for (DWORD j = 0; j < size - 2; j++) {
                    if (start[j]     == 0x0F &&
                        start[j + 1] == 0x05 &&
                        start[j + 2] == 0xC3) {
                        return start + j;
                    }
                }
            }
        }
        return nullptr;
    }

    void build_stub(int index, DWORD ssn) {
        BYTE* p = stub_mem_ + index * STRIDE;

        if (use_indirect_ && syscall_ret_gadget_) {
            // INDIRECT SYSCALL:
            //   mov r10, rcx            ; 3 bytes
            //   mov eax, <SSN>          ; 5 bytes
            //   jmp <ntdll_gadget>      ; 6 bytes (FF 25 + 8-byte addr)
            //                           ; total: 14 bytes + 8 addr = 22
            //
            // The 'jmp' lands on ntdll's 'syscall; ret'
            // So the actual syscall instruction executes with
            // RIP pointing inside ntdll — passes RIP checks

            int off = 0;

            // mov r10, rcx
            p[off++] = 0x4C;
            p[off++] = 0x8B;
            p[off++] = 0xD1;

            // mov eax, imm32
            p[off++] = 0xB8;
            memcpy(p + off, &ssn, 4);
            off += 4;

            // jmp qword ptr [rip+0]  →  FF 25 00 00 00 00 [8-byte addr]
            p[off++] = 0xFF;
            p[off++] = 0x25;
            p[off++] = 0x00;
            p[off++] = 0x00;
            p[off++] = 0x00;
            p[off++] = 0x00;

            // 8-byte absolute address of the gadget
            uintptr_t gadget_addr = (uintptr_t)syscall_ret_gadget_;
            memcpy(p + off, &gadget_addr, 8);
            off += 8;

            // Fill rest with NOPs
            while (off < STRIDE) p[off++] = 0x90;

        } else {
            // DIRECT SYSCALL (fallback):
            //   mov r10, rcx
            //   mov eax, <SSN>
            //   syscall
            //   ret
            p[0]  = 0x4C; p[1] = 0x8B; p[2] = 0xD1;
            p[3]  = 0xB8;
            memcpy(p + 4, &ssn, 4);
            p[8]  = 0x0F; p[9] = 0x05;
            p[10] = 0xC3;
            for (int i = 11; i < STRIDE; i++) p[i] = 0x90;
        }
    }
};

// ─── SSN Resolver (reads clean ntdll from disk) ───

class SyscallResolver {
public:
    DWORD ssn_open  = 0;
    DWORD ssn_read  = 0;
    DWORD ssn_close = 0;

    bool resolve() {
        // Map ntdll from disk with SEC_IMAGE for clean copy
        wchar_t path[MAX_PATH];
        GetSystemDirectoryW(path, MAX_PATH);
        wcscat_s(path, L"\\ntdll.dll");

        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            // Fallback to in-memory
            return resolve_in_memory();
        }

        HANDLE mapping = CreateFileMappingW(
            file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr
        );
        CloseHandle(file);

        if (!mapping) return resolve_in_memory();

        BYTE* base = (BYTE*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(mapping);

        if (!base) return resolve_in_memory();

        ssn_open  = extract_ssn_from_mapped(base, "NtOpenProcess");
        ssn_read  = extract_ssn_from_mapped(base, "NtReadVirtualMemory");
        ssn_close = extract_ssn_from_mapped(base, "NtClose");

        UnmapViewOfFile(base);

        if (!ssn_open || !ssn_read || !ssn_close) {
            return resolve_in_memory();
        }

        print_results();
        return true;
    }

private:
    bool resolve_in_memory() {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return false;

        if (!ssn_open)  ssn_open  = extract_ssn_getproc(ntdll, "NtOpenProcess");
        if (!ssn_read)  ssn_read  = extract_ssn_getproc(ntdll, "NtReadVirtualMemory");
        if (!ssn_close) ssn_close = extract_ssn_getproc(ntdll, "NtClose");

        print_results();
        return ssn_open && ssn_read && ssn_close;
    }

    void print_results() {
        printf("[%c] NtOpenProcess:       0x%04X\n", ssn_open  ? '+' : '-', ssn_open);
        printf("[%c] NtReadVirtualMemory: 0x%04X\n", ssn_read  ? '+' : '-', ssn_read);
        printf("[%c] NtClose:             0x%04X\n", ssn_close ? '+' : '-', ssn_close);
    }

    static DWORD extract_ssn_getproc(HMODULE mod, const char* name) {
        BYTE* func = (BYTE*)GetProcAddress(mod, name);
        if (!func) return 0;
        return parse_stub(func);
    }

    static DWORD extract_ssn_from_mapped(BYTE* base, const char* func_name) {
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress) return 0;

        auto* exp = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
        auto* names = (DWORD*)(base + exp->AddressOfNames);
        auto* funcs = (DWORD*)(base + exp->AddressOfFunctions);
        auto* ords  = (WORD*)(base + exp->AddressOfNameOrdinals);

        for (DWORD i = 0; i < exp->NumberOfNames; i++) {
            if (strcmp((const char*)(base + names[i]), func_name) == 0) {
                BYTE* func = base + funcs[ords[i]];
                return parse_stub(func);
            }
        }
        return 0;
    }

    static DWORD parse_stub(BYTE* func) {
        // Standard: 4C 8B D1 B8 [SSN]
        if (func[0] == 0x4C && func[1] == 0x8B &&
            func[2] == 0xD1 && func[3] == 0xB8) {
            return *(DWORD*)(func + 4);
        }
        // Scan fallback
        for (int i = 0; i < 32; i++) {
            if (func[i] == 0xB8) {
                DWORD val = *(DWORD*)(func + i + 1);
                if (val > 0 && val < 0x1000) return val;
            }
        }
        return 0;
    }
};

// ─── Hardened Memory Class ───

class MemorySyscall : public IMemory {
public:
    MemorySyscall() = default;
    ~MemorySyscall() override { close(); }

    MemorySyscall(const MemorySyscall&) = delete;
    MemorySyscall& operator=(const MemorySyscall&) = delete;

    bool attach(const wchar_t* process_name) override {
        if (!initialized_) {
            if (!do_init()) return false;
            initialized_ = true;
        }

        pid_ = find_process(process_name);
        if (!pid_) {
            printf("[-] Process not found\n");
            return false;
        }

        CLIENT_ID_NT cid{};
        cid.UniqueProcess = (HANDLE)(uintptr_t)pid_;
        OBJECT_ATTRIBUTES_NT oa{};
        oa.Length = sizeof(oa);

        HANDLE handle = nullptr;
        using fn_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK,
                                       OBJECT_ATTRIBUTES_NT*, CLIENT_ID_NT*);
        auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_OPEN);
        if (!fn) return false;

        NTSTATUS status = fn(&handle, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                             &oa, &cid);
        if (status != 0 || !handle) {
            printf("[-] NtOpenProcess: 0x%08lX\n", status);
            return false;
        }

        process_ = handle;
        client_base_ = get_module_base(pid_, L"client.dll");
        return client_base_ != 0;
    }

    void close() override {
        if (process_ && invoker_.is_ready()) {
            using fn_t = NTSTATUS(NTAPI*)(HANDLE);
            auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_CLOSE);
            if (fn) fn(process_);
            process_ = nullptr;
        }
        pid_ = 0;
        client_base_ = 0;
    }

    template <typename T>
    T read(uintptr_t address) const {
        T value{};
        if (!process_ || !invoker_.is_ready()) return value;
        SIZE_T br = 0;
        using fn_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
        auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_READ);
        if (fn) fn(process_, (PVOID)address, &value, sizeof(T), &br);
        return value;
    }

    bool read_raw(uintptr_t address, void* buffer, size_t size) const override {
        if (!process_ || !invoker_.is_ready()) return false;
        SIZE_T br = 0;
        using fn_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
        auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_READ);
        if (!fn) return false;
        NTSTATUS s = fn(process_, (PVOID)address, buffer, size, &br);
        return s == 0 && br == size;
    }

    uintptr_t get_client_base() const override { return client_base_; }
    DWORD get_pid() const override { return pid_; }

private:
    HANDLE    process_     = nullptr;
    DWORD     pid_         = 0;
    uintptr_t client_base_ = 0;
    bool      initialized_ = false;

    SyscallResolver resolver_;
    SyscallInvoker  invoker_;

    bool do_init() {
        if (!resolver_.resolve()) return false;
        if (!invoker_.init()) return false;

        invoker_.set_ssn(SyscallInvoker::IDX_OPEN,  resolver_.ssn_open);
        invoker_.set_ssn(SyscallInvoker::IDX_READ,  resolver_.ssn_read);
        invoker_.set_ssn(SyscallInvoker::IDX_CLOSE, resolver_.ssn_close);

        return invoker_.is_ready();
    }

    static uintptr_t get_module_base(DWORD p, const wchar_t* mod) {
        uintptr_t base = 0;
        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, p);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (Module32FirstW(snap, &me)) {
            do {
                if (!_wcsicmp(me.szModule, mod)) {
                    base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return base;
    }
};