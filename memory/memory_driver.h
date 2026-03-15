#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>

// Shared definitions with the driver
#include "imemory.h"
#include "memory_utils.h"
#include "shared.h"

class MemoryDriver : public IMemory {
public:
    MemoryDriver() = default;
    ~MemoryDriver() { close(); }

    // No copying
    MemoryDriver(const MemoryDriver&) = delete;
    MemoryDriver& operator=(const MemoryDriver&) = delete;

    // Move support
    MemoryDriver(MemoryDriver&& other) noexcept
        : h_driver(other.h_driver)
        , pid(other.pid)
        , client_base(other.client_base)
    {
        other.h_driver    = INVALID_HANDLE_VALUE;
        other.pid         = 0;
        other.client_base = 0;
    }

    MemoryDriver& operator=(MemoryDriver&& other) noexcept {
        if (this != &other) {
            close();
            h_driver    = other.h_driver;
            pid         = other.pid;
            client_base = other.client_base;
            other.h_driver    = INVALID_HANDLE_VALUE;
            other.pid         = 0;
            other.client_base = 0;
        }
        return *this;
    }

    bool ping() const {
        if (h_driver == INVALID_HANDLE_VALUE) return false;

        PING_RESPONSE response{};
        DWORD returned = 0;

        BOOL ok = DeviceIoControl(
            h_driver,
            IOCTL_PING,
            nullptr, 0,                              // No input
            &response, sizeof(response),             // Output
            &returned,
            nullptr
        );

        return ok && returned == sizeof(PING_RESPONSE)
                  && response.magic == PING_MAGIC;
    }

    // ================================================================
    // Attach to target process
    // ================================================================

    bool attach(const wchar_t* process_name) override {
        // Connect to driver if not already connected
        if (h_driver == INVALID_HANDLE_VALUE) {
            if (!connect()) return false;
        }

        // Find PID (still done in usermode - no reason to do this in kernel)
        pid = find_process(process_name);
        if (!pid) return false;

        // Get client.dll base via kernel driver
        client_base = query_module_base(L"client.dll");
        return client_base != 0;
    }

    void close() override {
        if (h_driver != INVALID_HANDLE_VALUE) {
            CloseHandle(h_driver);
            h_driver = INVALID_HANDLE_VALUE;
        }
        pid = 0;
        client_base = 0;
    }

    // ================================================================
    // Memory reading
    // ================================================================

    // Read a typed value
    template <typename T>
    T read(uintptr_t address) const {
        T value{};
        read_raw(address, &value, sizeof(T));
        return value;
    }

    // Read raw bytes
    bool read_raw(uintptr_t address, void* buffer, size_t size) const override {
        if (h_driver == INVALID_HANDLE_VALUE || !pid || !buffer || size == 0) {
            return false;
        }

        // For large reads, chunk them (driver has 64KB limit)
        const size_t MAX_CHUNK = 0x10000;
        BYTE* dst = static_cast<BYTE*>(buffer);
        size_t remaining = size;
        uintptr_t current_addr = address;

        while (remaining > 0) {
            size_t chunk = (remaining > MAX_CHUNK) ? MAX_CHUNK : remaining;

            if (!read_chunk(current_addr, dst, chunk)) {
                return false;
            }

            dst          += chunk;
            current_addr += chunk;
            remaining    -= chunk;
        }

        return true;
    }

    // Read a chain of pointers (useful for multi-level offsets)
    uintptr_t read_chain(uintptr_t base, std::initializer_list<uintptr_t> offsets) const {
        uintptr_t addr = base;
        for (auto offset : offsets) {
            addr = read<uintptr_t>(addr);
            if (!addr) return 0;
            addr += offset;
        }
        return addr;
    }

    // Read a string
    std::string read_string(uintptr_t address, size_t max_len = 256) const {
        std::vector<char> buf(max_len + 1, 0);
        if (read_raw(address, buf.data(), max_len)) {
            return std::string(buf.data());  // stops at null terminator
        }
        return {};
    }

    // ================================================================
    // Utility
    // ================================================================

    uintptr_t get_client_base() const override { return client_base; }
    DWORD     get_pid()         const override { return pid; }

    // Query any module base at runtime
    uintptr_t query_module_base(const wchar_t* module_name) const {
        if (h_driver == INVALID_HANDLE_VALUE || !pid) return 0;

        MODULE_BASE_REQUEST request{};
        request.target_pid  = pid;
        request.base_address = 0;
        wcsncpy_s(request.module_name, 256, module_name, _TRUNCATE);

        MODULE_BASE_REQUEST response{};
        DWORD returned = 0;

        BOOL ok = DeviceIoControl(
            h_driver,
            IOCTL_GET_MODULE_BASE,
            &request, sizeof(request),
            &response, sizeof(response),
            &returned,
            nullptr
        );

        if (ok && returned == sizeof(MODULE_BASE_REQUEST)) {
            return static_cast<uintptr_t>(response.base_address);
        }

        return 0;
    }

private:
    HANDLE    h_driver    = INVALID_HANDLE_VALUE;
    DWORD     pid         = 0;
    uintptr_t client_base = 0;

    // Read a single chunk via IOCTL (up to 64KB)
    bool read_chunk(uintptr_t address, void* buffer, size_t size) const {
        READ_MEMORY_REQUEST request{};
        request.target_pid    = pid;
        request.source_address = static_cast<ULONG64>(address);
        request.read_size     = static_cast<ULONG>(size);
        request.padding       = 0;

        DWORD returned = 0;

        // DeviceIoControl with METHOD_BUFFERED:
        //   1. Copies &request into SystemBuffer (kernel)
        //   2. Driver reads the request, calls MmCopyVirtualMemory
        //   3. Driver writes game memory into SystemBuffer
        //   4. I/O manager copies SystemBuffer → buffer (our output)
        BOOL ok = DeviceIoControl(
            h_driver,
            IOCTL_READ_MEMORY,
            &request, sizeof(request),      // Input: the request struct
            buffer, static_cast<DWORD>(size), // Output: the read data
            &returned,
            nullptr
        );

        return ok && returned == static_cast<DWORD>(size);
    }

    // ================================================================
    // Connection to the driver
    // ================================================================

    bool connect() {
        if (h_driver != INVALID_HANDLE_VALUE) return true;

        // Open a handle to \\.\MemReader
        // This triggers IRP_MJ_CREATE in the driver
        h_driver = CreateFileW(
            DRIVER_USER_PATH,                       // L"\\\\.\\MemReader"
            GENERIC_READ | GENERIC_WRITE,           // Need both for IOCTL
            FILE_SHARE_READ | FILE_SHARE_WRITE,     // Allow sharing
            nullptr,                                // Default security
            OPEN_EXISTING,                          // Device must exist
            0,                                      // No special flags
            nullptr                                 // No template
        );

        if (h_driver == INVALID_HANDLE_VALUE) {
            return false;
        }

        // Verify the driver is actually ours with a ping
        if (!ping()) {
            CloseHandle(h_driver);
            h_driver = INVALID_HANDLE_VALUE;
            return false;
        }

        return true;
    }

};