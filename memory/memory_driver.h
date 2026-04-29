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
    explicit MemoryDriver(bool use_kdmapper = true)
        : m_use_kdmapper(use_kdmapper) {}

    ~MemoryDriver() override {
        close();
    }

    bool attach(const wchar_t* process_name) override {
        close();

        // Step 1: System checks and setup
        if (m_use_kdmapper) {
            auto result = DriverManager::setup_kdmapper();
            switch (result) {
                case DriverManager::NEED_ADMIN:
                    printf("[-] kdmapper requires administrator privileges.\n");
                    return false;
                case DriverManager::NEED_REBOOT:
                    printf("[-] Reboot required. Run again after restart.\n");
                    return false;
                case DriverManager::DRIVER_FILE_MISSING:
                    printf("[-] MemReader.sys or kdmapper.exe not found.\n");
                    return false;
                case DriverManager::SETUP_FAILED:
                    printf("[-] kdmapper launch failed.\n");
                    return false;
                case DriverManager::READY:
                    break;
                default: ;
            }
        } else {
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
        }

        // Step 3: Open handle to the driver device
        h_driver = CreateFileW(
            m_use_kdmapper ? KDMP_USER_PATH : DRIVER_USER_PATH,
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

        // Step 5: Get modules base adr via driver
        m_modules.client = query_module_base(L"client.dll", &m_modules.client_size);
        if (!m_modules.client) {
            printf("[-] Failed to find client.dll in target process.\n");
            return false;
        }
        m_modules.engine2 = query_module_base(L"engine2.dll", &m_modules.engine2_size);
        if (!m_modules.engine2) {
            printf("[-] Failed to find engine2.dll in target process.\n");
            return false;
        }
        m_modules.schemasystem = query_module_base(L"schemasystem.dll", &m_modules.schemasystem_size);
        if (!m_modules.schemasystem) {
            printf("[-] Failed to find schemasystem.dll in target process.\n");
            return false;
        }
        m_modules.tier0 = query_module_base(L"tier0.dll", &m_modules.tier0_size);
        if (!m_modules.tier0) {
            printf("[-] Failed to find tier0.dll in target process.\n");
            return false;
        }
        m_modules.vphysics2 = query_module_base(L"vphysics2.dll", &m_modules.vphysics2_size);
        if (!m_modules.vphysics2) {
            printf("[-] Failed to find vphysics2.dll in target process.\n");
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
        m_modules = {};
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

    uintptr_t get_client_base() const override { return m_modules.client; }

    DWORD get_pid() const override { return pid; }

private:
    HANDLE    h_driver = INVALID_HANDLE_VALUE;
    DWORD     pid = 0;
    bool      driver_loaded_by_us = false;
    bool      m_use_kdmapper = false;

    uintptr_t query_module_base(const wchar_t* module_name, size_t* out_size) const {
        if (h_driver == INVALID_HANDLE_VALUE || !pid) return 0;

        MODULE_BASE_REQUEST request{};
        request.target_pid   = pid;
        request.base_address = 0;
        request.module_size  = 0;
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
            if (out_size) {
                *out_size = static_cast<size_t>(response.module_size);
            }
            return static_cast<uintptr_t>(response.base_address);
        }

        return 0;
    }
};
