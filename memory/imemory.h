#pragma once

class IMemory {
public:
    virtual ~IMemory() = default;

    virtual bool attach(const wchar_t* process_name) = 0;
    virtual void close() = 0;

    virtual bool      read_raw(uintptr_t address, void* buffer, size_t size) const = 0;
    virtual uintptr_t get_client_base() const = 0;
    virtual DWORD     get_pid()         const = 0;

    template <typename T>
    T read(uintptr_t address) const {
        T value{};
        read_raw(address, &value, sizeof(T));
        return value;
    }
};

inline std::unique_ptr<IMemory> g_memory;