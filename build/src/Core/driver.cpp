#include "driver.hpp"

#include <tlhelp32.h>
#include <iostream>

namespace driver {

	DriverHandle::DriverHandle() : handle(INVALID_HANDLE_VALUE), device_name("airway_radio") {
		std::wstring device_path = L"\\\\.\\airway_radio";
		
		handle = CreateFileW(
			device_path.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (handle != INVALID_HANDLE_VALUE) {
			std::cout << "[+] Connected to driver: " << device_name << "\n";
		}
		else {
			std::cout << "[-] Failed to connect to driver: " << device_name << "\n";
		}
	}

	DriverHandle::DriverHandle(const std::string& deviceName) : handle(INVALID_HANDLE_VALUE), device_name(deviceName) {
		// Convert device name to wide string path
		std::string device_path_str = "\\\\.\\" + deviceName;
		int wideLen = MultiByteToWideChar(CP_ACP, 0, device_path_str.c_str(), -1, nullptr, 0);
		wchar_t* wDevicePath = new wchar_t[wideLen];
		MultiByteToWideChar(CP_ACP, 0, device_path_str.c_str(), -1, wDevicePath, wideLen);
		
		handle = CreateFileW(
			wDevicePath,
			GENERIC_READ | GENERIC_WRITE,
			0,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		delete[] wDevicePath;

		if (handle != INVALID_HANDLE_VALUE) {
			std::cout << "[+] Connected to driver: " << device_name << "\n";
		}
		else {
			std::cout << "[-] Failed to connect to driver: " << device_name << "\n";
		}
	}

	DriverHandle::~DriverHandle() {
		if (handle != INVALID_HANDLE_VALUE) {
			CloseHandle(handle);
		}
	}

	bool DriverHandle::is_valid() const {
		return handle != INVALID_HANDLE_VALUE;
	}

	bool DriverHandle::attach_to_process(HANDLE process_id) {
		if (!is_valid()) {
			std::cout << "[-] Driver handle is invalid\n";
			return false;
		}

		Request req = {};
		req.pid = process_id; // Pass HANDLE directly - no casting needed

		DWORD bytes_returned = 0;
		BOOL result = DeviceIoControl(
			handle,
			codes::attach,
			&req,
			sizeof(req),
			&req,
			sizeof(req),
			&bytes_returned,
			nullptr
		);

		if (result) {
			std::cout << "[+] Attached to process: " << reinterpret_cast<uint64_t>(process_id) << "\n";
		}
		else {
			std::cout << "[-] Attach failed. Error: " << GetLastError() << "\n";
		}

		return result;
	}

	bool DriverHandle::read_memory(PVOID target_address, PVOID buffer, SIZE_T size) {
		if (!is_valid()) {
			std::cout << "[-] Driver handle is invalid\n";
			return false;
		}

		if (target_address == nullptr || buffer == nullptr || size == 0) {
			std::cout << "[-] Invalid parameters for read_memory\n";
			return false;
		}

		Request req = {};
		req.target = target_address;
		req.buffer = buffer;
		req.size = size;
		req.return_size = 0;

		DWORD bytes_returned = 0;
		BOOL result = DeviceIoControl(
			handle,
			codes::read,
			&req,
			sizeof(req),
			&req,
			sizeof(req),
			&bytes_returned,
			nullptr
		);

		if (!result) {
			// Uncomment for debugging:
			// std::cout << "[-] Read failed at 0x" << std::hex << reinterpret_cast<uint64_t>(target_address) 
			//           << " Size: " << std::dec << size << " Error: " << GetLastError() << "\n";
		}

		return result;
	}

	bool DriverHandle::write_memory(PVOID target_address, PVOID buffer, SIZE_T size) {
		if (!is_valid()) {
			std::cout << "[-] Driver handle is invalid\n";
			return false;
		}

		if (target_address == nullptr || buffer == nullptr || size == 0) {
			std::cout << "[-] Invalid parameters for write_memory\n";
			return false;
		}

		Request req = {};
		req.target = target_address;
		req.buffer = buffer;
		req.size = size;
		req.return_size = 0;

		DWORD bytes_returned = 0;
		BOOL result = DeviceIoControl(
			handle,
			codes::write,
			&req,
			sizeof(req),
			&req,
			sizeof(req),
			&bytes_returned,
			nullptr
		);

		if (!result) {
			std::cout << "[-] Write failed at 0x" << std::hex << reinterpret_cast<uint64_t>(target_address)
				<< " Size: " << std::dec << size << " Error: " << GetLastError() << "\n";
		}

		return result;
	}

	void DriverHandle::write(std::uintptr_t address, const void* buffer, SIZE_T size) {
		write_memory(reinterpret_cast<PVOID>(address), const_cast<void*>(buffer), size);
	}

	std::uintptr_t DriverHandle::get_module_base(DWORD pid, const wchar_t* module_name) {
		std::uintptr_t module_base = 0;

		HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
		if (snap_shot == INVALID_HANDLE_VALUE) {
			std::cout << "[-] Failed to create module snapshot\n";
			return module_base;
		}

		MODULEENTRY32W entry = {};
		entry.dwSize = sizeof(MODULEENTRY32W);

		if (Module32FirstW(snap_shot, &entry)) {
			do {
				if (_wcsicmp(module_name, entry.szModule) == 0) {
					module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
					std::cout << "[+] Found module " << std::string(module_name, module_name + wcslen(module_name))
						<< " at 0x" << std::hex << module_base << std::dec << "\n";
					break;
				}
			} while (Module32NextW(snap_shot, &entry));
		}
		else {
			std::cout << "[-] Module32FirstW failed\n";
		}

		CloseHandle(snap_shot);
		return module_base;
	}

}