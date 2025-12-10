#pragma once
#include <Windows.h>
#include <string>

namespace process {
    DWORD get_process_id_by_name(const std::wstring& process_name);
}
