#pragma once

#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <urlmon.h>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include "console_colors.hpp"
#include "logger.hpp"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "urlmon.lib")

namespace DownloadHelper {

    // Check if file exists
    inline bool FileExists(const std::string& path) {
        DWORD attrib = GetFileAttributesA(path.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    // Get current executable directory
    inline std::string GetExecutableDirectory() {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        std::string path(buffer);
        size_t pos = path.find_last_of("\\/");
        return path.substr(0, pos);
    }

    // Download file from URL to specified path
    inline bool DownloadFile(const std::string& url, const std::string& outputPath) {
        ConsoleColor::PrintInfo("Downloading from: " + url);
        ConsoleColor::PrintInfo("Saving to: " + outputPath);

        HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), outputPath.c_str(), 0, nullptr);
        
        if (SUCCEEDED(hr)) {
            // ensure file exists and non-zero size
            std::ifstream in(outputPath, std::ios::binary | std::ios::ate);
            if (!in.is_open()) return false;
            auto size = in.tellg();
            in.close();
            if (size <= 0) {
                ConsoleColor::PrintError("Downloaded file is empty");
                DeleteFileA(outputPath.c_str());
                return false;
            }

            ConsoleColor::PrintSuccess("Download completed successfully");
            return true;
        }
        else {
            ConsoleColor::PrintError("Download failed with HRESULT: 0x" + std::to_string(hr));
            return false;
        }
    }

    // Extract ZIP file using Windows Shell API (native support, no 7-Zip needed)
    inline bool ExtractZIP(const std::string& zipPath, const std::string& destFolder) {
        ConsoleColor::PrintInfo("Extracting: " + zipPath);
        ConsoleColor::PrintInfo("Destination: " + destFolder);

        // Convert paths to wide strings for COM
        int zipPathLen = MultiByteToWideChar(CP_ACP, 0, zipPath.c_str(), -1, nullptr, 0);
        int destPathLen = MultiByteToWideChar(CP_ACP, 0, destFolder.c_str(), -1, nullptr, 0);
        
        wchar_t* wZipPath = new wchar_t[zipPathLen];
        wchar_t* wDestPath = new wchar_t[destPathLen];
        
        MultiByteToWideChar(CP_ACP, 0, zipPath.c_str(), -1, wZipPath, zipPathLen);
        MultiByteToWideChar(CP_ACP, 0, destFolder.c_str(), -1, wDestPath, destPathLen);

        // Initialize COM
        HRESULT hr = CoInitialize(nullptr);
        bool comInitialized = SUCCEEDED(hr);

        IShellDispatch* pISD = nullptr;
        hr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void**)&pISD);
        
        if (FAILED(hr) || !pISD) {
            ConsoleColor::PrintError("Failed to create Shell instance");
            delete[] wZipPath;
            delete[] wDestPath;
            if (comInitialized) CoUninitialize();
            return false;
        }

        // Get ZIP file folder object
        VARIANT vZipFile;
        VariantInit(&vZipFile);
        vZipFile.vt = VT_BSTR;
        vZipFile.bstrVal = SysAllocString(wZipPath);

        Folder* pZipFolder = nullptr;
        hr = pISD->NameSpace(vZipFile, &pZipFolder);
        VariantClear(&vZipFile);

        if (FAILED(hr) || !pZipFolder) {
            ConsoleColor::PrintError("Failed to open ZIP file");
            pISD->Release();
            delete[] wZipPath;
            delete[] wDestPath;
            if (comInitialized) CoUninitialize();
            return false;
        }

        // Get destination folder object
        VARIANT vDestFolder;
        VariantInit(&vDestFolder);
        vDestFolder.vt = VT_BSTR;
        vDestFolder.bstrVal = SysAllocString(wDestPath);

        Folder* pDestFolder = nullptr;
        hr = pISD->NameSpace(vDestFolder, &pDestFolder);
        VariantClear(&vDestFolder);

        if (FAILED(hr) || !pDestFolder) {
            ConsoleColor::PrintError("Failed to access destination folder");
            pZipFolder->Release();
            pISD->Release();
            delete[] wZipPath;
            delete[] wDestPath;
            if (comInitialized) CoUninitialize();
            return false;
        }

        // Get items from ZIP
        FolderItems* pItems = nullptr;
        hr = pZipFolder->Items(&pItems);
        
        if (FAILED(hr) || !pItems) {
            ConsoleColor::PrintError("Failed to get ZIP contents");
            pDestFolder->Release();
            pZipFolder->Release();
            pISD->Release();
            delete[] wZipPath;
            delete[] wDestPath;
            if (comInitialized) CoUninitialize();
            return false;
        }

        // Extract all items with no UI
        VARIANT vItems;
        VariantInit(&vItems);
        vItems.vt = VT_DISPATCH;
        vItems.pdispVal = pItems;

        VARIANT vOptions;
        VariantInit(&vOptions);
        vOptions.vt = VT_I4;
        vOptions.lVal = 0x14; // FOF_NO_UI | FOF_NOCONFIRMATION

        ConsoleColor::PrintInfo("Extracting files, please wait...");
        hr = pDestFolder->CopyHere(vItems, vOptions);

        // Cleanup
        VariantClear(&vItems);
        VariantClear(&vOptions);
        pItems->Release();
        pDestFolder->Release();
        pZipFolder->Release();
        pISD->Release();
        
        delete[] wZipPath;
        delete[] wDestPath;
        
        if (comInitialized) CoUninitialize();

        if (SUCCEEDED(hr)) {
            ConsoleColor::PrintSuccess("Extraction completed successfully");
            return true;
        }
        else {
            ConsoleColor::PrintError("Extraction failed with HRESULT: 0x" + std::to_string(hr));
            return false;
        }
    }

    // Download and extract driver package
    inline bool DownloadAndExtractDriverPackage(const std::string& url) {
        std::string exeDir = GetExecutableDirectory();
        std::string zipPath = exeDir + "\\Hiroyoshi.zip";
        
        ConsoleColor::PrintInfo("Starting driver package download...");
        Logger::LogInfo("Starting driver package download...");
        Logger::Log("[*] URL: " + url);
        Logger::Log("[*] Destination: " + zipPath);
        
        // Download the ZIP file
        if (!DownloadFile(url, zipPath)) {
            Logger::LogError("Download failed");
            return false;
        }

        // Verify download
        if (!FileExists(zipPath)) {
            ConsoleColor::PrintError("Downloaded file not found!");
            Logger::LogError("Downloaded file not found!");
            return false;
        }

        ConsoleColor::PrintSuccess("File downloaded successfully");
        Logger::LogSuccess("File downloaded successfully");

        // Extract using Windows native ZIP support
        if (!ExtractZIP(zipPath, exeDir)) {
            DeleteFileA(zipPath.c_str());
            Logger::LogError("Extraction failed");
            return false;
        }

        // Wait for filesystem to finish writing files
        ConsoleColor::PrintInfo("Waiting for extraction to complete...");
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        // The ZIP extracts to a "Hiroyoshi" subfolder
        std::string hiroyoshiFolder = exeDir + "\\Hiroyoshi";
        
        // Verify the Hiroyoshi folder was created
        if (!FileExists(hiroyoshiFolder + "\\Hiroyoshi.sys")) {
            ConsoleColor::PrintError("Extraction created no Hiroyoshi folder or files are missing");
            ConsoleColor::PrintWarning("Expected structure: Hiroyoshi\\Hiroyoshi.sys, Hiroyoshi\\kdu.exe, etc.");
            DeleteFileA(zipPath.c_str());
            return false;
        }

        // Move files from Hiroyoshi subfolder to root directory
        ConsoleColor::PrintInfo("Moving driver files to root directory...");
        
        std::vector<std::string> requiredFiles = {
            "Hiroyoshi.sys",
            "kdu.exe",
            "Taigei64.dll",
            "drv64.dll"
        };

        bool allFilesMoved = true;
        for (const auto& file : requiredFiles) {
            std::string sourcePath = hiroyoshiFolder + "\\" + file;
            std::string destPath = exeDir + "\\" + file;
            
            // Check if source file exists
            if (!FileExists(sourcePath)) {
                ConsoleColor::PrintError("Missing file in archive: " + file);
                allFilesMoved = false;
                continue;
            }
            
            // Move file to root directory
            if (!MoveFileExA(sourcePath.c_str(), destPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                // If move fails, try copy + delete
                if (CopyFileA(sourcePath.c_str(), destPath.c_str(), FALSE)) {
                    DeleteFileA(sourcePath.c_str());
                    ConsoleColor::PrintSuccess("Moved: " + file);
                } else {
                    ConsoleColor::PrintError("Failed to move: " + file);
                    allFilesMoved = false;
                }
            } else {
                ConsoleColor::PrintSuccess("Moved: " + file);
            }
        }

        // Clean up: Delete the Hiroyoshi folder and ZIP file
        for (const auto& file : requiredFiles) {
            DeleteFileA((hiroyoshiFolder + "\\" + file).c_str());
        }
        RemoveDirectoryA(hiroyoshiFolder.c_str());
        DeleteFileA(zipPath.c_str());

        if (!allFilesMoved) {
            ConsoleColor::PrintError("Some files could not be moved");
            return false;
        }

        // Verify all files are now in root directory
        ConsoleColor::PrintInfo("Verifying extracted files...");
        for (const auto& file : requiredFiles) {
            std::string fullPath = exeDir + "\\" + file;
            if (!FileExists(fullPath)) {
                ConsoleColor::PrintError("Verification failed - missing: " + file);
                return false;
            }
            else {
                ConsoleColor::PrintSuccess("Verified: " + file);
            }
        }

        ConsoleColor::PrintSuccess("All driver files extracted and verified successfully");
        return true;
    }
}
