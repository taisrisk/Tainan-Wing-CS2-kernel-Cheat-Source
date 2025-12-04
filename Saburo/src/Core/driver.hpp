#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>

namespace driver {
	namespace codes {
		constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_ANY_ACCESS);
		constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_ANY_ACCESS);
		constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_ANY_ACCESS);
	}

	struct Request {
		HANDLE pid;          // Process ID
		PVOID buffer;        // Buffer for read/write
		PVOID target;        // Target address
		SIZE_T size;         // Size to read/write
		SIZE_T return_size;  // Returned size
	};

	class DriverHandle {
	private:
		HANDLE handle;
		std::string device_name;

	public:
		// Default constructor uses hardcoded name for backward compatibility
		DriverHandle();
		
		// Constructor with custom device name (for version control)
		explicit DriverHandle(const std::string& deviceName);
		
		~DriverHandle();

		bool is_valid() const;
		bool attach_to_process(HANDLE process_id);
		bool read_memory(PVOID target_address, PVOID buffer, SIZE_T size);
		bool write_memory(PVOID target_address, PVOID buffer, SIZE_T size);

		// Template read for any type - THIS IS THE KEY METHOD
		template<typename T>
		T read(std::uintptr_t address) {
			T value = {};
			if (!read_memory(reinterpret_cast<PVOID>(address), &value, sizeof(T))) {
				return {}; // Return zero-initialized value on failure
			}
			return value;
		}

		// Specialized read for void* (useful for pointers)
		std::uintptr_t read_ptr(std::uintptr_t address) {
			return read<std::uintptr_t>(address);
		}

		void write(std::uintptr_t address, const void* buffer, SIZE_T size);

		std::uintptr_t get_module_base(DWORD pid, const wchar_t* module_name);
		
		// Get the device name currently in use
		std::string get_device_name() const { return device_name; }
	};
}