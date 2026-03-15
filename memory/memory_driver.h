// memory/memory_driver.h
#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include "imemory.h"
#include "shared.h"
#include "driver_manager.h"
#include "memory_utils.h"

class MemoryDriver : public IMemory {
public:
    MemoryDriver() = default;

    ~MemoryDriver() override {
        close();
    }

    bool attach(const wchar_t* process_name) override {
        close();

        // Step 1: System checks and setup
        auto result = DriverManager::setup();
        switch (result) {
        case DriverManager::NEED_ADMIN:
            printf("[-] Kernel driver requires administrator privileges.\n");
            return false;

        case DriverManager::NEED_BIOS:
            printf("[-] Disable Secure Boot in BIOS first.\n");
            return false;

        case DriverManager::NEED_REBOOT:
            printf("[-] Reboot required. Run again after restart.\n");
            return false;

        case DriverManager::DRIVER_FILE_MISSING:
            printf("[-] MemReader.sys not found.\n");
            return false;

        case DriverManager::SETUP_FAILED:
            printf("[-] Driver setup failed.\n");
            return false;

        case DriverManager::READY:
            break;
        }

        // Step 2: Load the driver
        if (!DriverManager::load_driver()) {
            printf("[-] Failed to load driver.\n");
            return false;
        }

        // Mark that we loaded the driver (for cleanup on exit)
        driver_loaded_by_us = true;

        // Step 3: Open handle to the driver device
        h_driver = CreateFileW(
            DRIVER_USER_PATH,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr
        );

        if (h_driver == INVALID_HANDLE_VALUE) {
            printf("[-] Failed to open driver device: %lu\n", GetLastError());
            return false;
        }

        // Step 4: Find target process
        pid = find_process(process_name);
        if (!pid) {
            printf("[-] Process '%ls' not found.\n", process_name);
            return false;
        }

        // Step 5: Get client.dll base via driver
        client_base = query_module_base(L"client.dll");
        if (!client_base) {
            printf("[-] Failed to find client.dll in target process.\n");
            return false;
        }

        return true;
    }

    void close() override {
        if (h_driver != INVALID_HANDLE_VALUE) {
            CloseHandle(h_driver);
            h_driver = INVALID_HANDLE_VALUE;
        }

        if (driver_loaded_by_us) {
            DriverManager::full_cleanup();
            driver_loaded_by_us = false;
        }

        pid = 0;
        client_base = 0;
    }

    bool read_raw(uintptr_t address, void* buffer, size_t size) const override {
        if (h_driver == INVALID_HANDLE_VALUE || !pid || !buffer || size == 0) {
            return false;
        }

        const size_t MAX_CHUNK = 0x10000; // 64KB per IOCTL
        BYTE* dst = static_cast<BYTE*>(buffer);
        size_t remaining = size;
        uintptr_t current_addr = address;

        while (remaining > 0) {
            size_t chunk = (remaining > MAX_CHUNK) ? MAX_CHUNK : remaining;

            READ_MEMORY_REQUEST request{};
            request.target_pid = pid;
            request.source_address = static_cast<ULONG64>(current_addr);
            request.read_size = static_cast<ULONG>(chunk);
            request.padding = 0;

            DWORD returned = 0;
            BOOL ok = DeviceIoControl(
                h_driver,
                IOCTL_READ_MEMORY,
                &request, sizeof(request),
                dst, static_cast<DWORD>(chunk),
                &returned,
                nullptr
            );

            if (!ok || returned != static_cast<DWORD>(chunk)) {
                return false;
            }

            dst += chunk;
            current_addr += chunk;
            remaining -= chunk;
        }

        return true;
    }

    uintptr_t get_client_base() const override { return client_base; }

    uintptr_t get_module_base(const wchar_t* name) const {
        return query_module_base(name);
    }

    DWORD get_pid() const override { return pid; }

private:
    HANDLE    h_driver = INVALID_HANDLE_VALUE;
    DWORD     pid = 0;
    uintptr_t client_base = 0;
    bool      driver_loaded_by_us = false;

    uintptr_t query_module_base(const wchar_t* module_name) const {
        if (h_driver == INVALID_HANDLE_VALUE || !pid) return 0;

        MODULE_BASE_REQUEST request{};
        request.target_pid = pid;
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
};
