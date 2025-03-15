#pragma once

#define lattAssert(exp, msg) assert(((void)msg, exp))

#include <algorithm> // for std::sort
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include <mutex>

#if defined(_WIN32)
#include <windows.h>
#include <Shlobj.h>
#include <wrl.h>
#include <wil/com.h>
#include "WebView2.h"
#include <winrt/Windows.System.h>
#include <DispatcherQueue.h>
#include <winrt/base.h> // For winrt::com_ptr

#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <dlfcn.h>

#elif defined(__linux__)
#include <dlfcn.h>
#include <pwd.h>
#include <sys/stat.h>
#endif

namespace lattice {

class File
{
  public:
    template <typename T>
    struct Soundfile
    {
        std::vector<T> audioData;
        int numChannels;
        int numSamples;
        Soundfile(std::vector<T> data = {}, int numChans = 0, int numSamps = 0)
            : audioData(data), numSamples(numSamps), numChannels(numChans)
        {
        }
    };

    // Returns a new file path with the specified file extension
    static std::string withExtension(const std::string &filePath, const std::string &newExtension);

    // Retrieves the name of the binary file
    static std::string getBinaryFileName();

    // Gets a list of files of a specific type in a directory
    static std::vector<std::string> getFilesOfType(const std::string &dirPath, const std::string &fileTypes);

    // Extracts properties from a given JavaScript content
    static nlohmann::json extractPropsFromJS(const std::string &jsContent);

    // Gets the full path of the .csd file
    static std::string getCsdPath(const std::string file = "");

    // Gets the .csd file name without its extension
    static std::string getCsdWithoutExtension();

    // Joins a directory path and a file name into a single path
    static std::string joinPath(const std::string &dirPath, const std::string &fileName);

    // Retrieves the path to the current binary
    static std::string getBinaryFileAndPath();

    // Checks if a file/folder exists at the given path
    static bool exists(const std::string &filePath);

    // Returns the parent directory of the given folder/file
    static std::string getParentDirectory(const std::string &currentFile);

    // Checks if a directory exists at the given path
    static bool directoryExists(const std::string &dirPath);

    // Retrieves the directory for resources
    static std::string getResourceDir();

    // Loads the content of a JavaScript file as a string
    static std::string loadJSFile(const std::string &filePath);

    // Reads the entire content of a file into a string
    static std::string getFileAsString(std::string csdFile = "");

    // Retrieves the number of input channels (nchnls_i) from the .csd file
    static int getNumberOfInputChannels(const std::string &csdFile);

    // Retrieves the number of output channels (nchnls) from the .csd file
    static int getNumberOfOutputChannels(const std::string &csdFile);

    // Formats a file path to a consistent style
    static std::string formatPath(const std::string &path);

    // Retrieves the path to the settings file
    static std::string getSettingsFile();

    // Retrieves a specific property from the settings file by section and key
    static std::string getSettingsProperty(const std::string &section, const std::string &key);

  private:
    // Retrieves the path to the directory that contains the resources
    static std::string getResourceDirFromBundle()
    {
        const auto bundleParentDirPath = getParentDirectory(getParentDirectory(getBinaryFileAndPath()));
        return joinPath(getParentDirectory(bundleParentDirPath), "/Contents/Resources");
    }


#if defined(_WIN32)
    static std::string getWindowsBinaryPath()
    {
        char dllPath[MAX_PATH] = {0};
        HMODULE hModule = NULL;

        // Get the handle to the module containing this function
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&getWindowsBinaryPath), &hModule))
        {
            GetModuleFileNameA(hModule, dllPath, sizeof(dllPath));
        }
        else
        {
            // Handle the error
            std::cerr << "Error retrieving module handle: " << GetLastError() << std::endl;
        }

        return std::string(dllPath);
    }

#elif defined(__APPLE__)
    static std::string getMacBinaryPath()
    {
        Dl_info info;
        if (dladdr((void *)"getMacBinaryPath", &info))
        {
            return std::string(info.dli_fname);
        }
        return "";
    }

#elif defined(__linux__)
    static std::string getSharedLibraryPath()
    {
        Dl_info dl_info;
        if (dladdr(reinterpret_cast<void *>(&getSharedLibraryPath), &dl_info) != 0)
        {
            return std::string(dl_info.dli_fname);
        }
        return {};
    }

    static std::string getLinuxBinaryPath() { return getSharedLibraryPath(); }

    static std::string getLinuxHomeDir()
    {
        const char *homeDir = getenv("HOME");
        if (homeDir)
            return std::string(homeDir);
        else
        {
            struct passwd *pw = getpwuid(getuid());
            if (pw)
                return std::string(pw->pw_dir);
            else
                return "";
        }
    }
#endif
};

}
