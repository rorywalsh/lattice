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

// Function to handle debug output in Visual Studio
inline void logToDebug(const std::string& message)
{
#ifdef _WIN32
    OutputDebugStringA(message.c_str());
#endif
}

//==========================================================
class Logger
{
public:
    static Logger& getInstance();
    void setLogFile(const std::string& filePath);
    void closeLogFile();
    void logMessage(const std::string& message);

private:
    Logger() = default;
    ~Logger() { closeLogFile(); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream logFile;
    std::mutex fileMutex;
};

// Stream class to allow chaining with << operator
class LogStream
{
public:
    LogStream(const char* logLevel, bool includeContext) : logLevel(logLevel), includeContext(includeContext) {}

    // Move constructor and move assignment operator
    LogStream(LogStream&& other) noexcept
        : logLevel(other.logLevel), includeContext(other.includeContext), message(std::move(other.message)),
        file(other.file), line(other.line), function(other.function)
    {
    }

    LogStream& operator=(LogStream&& other) noexcept
    {
        if (this != &other)
        {
            logLevel = other.logLevel;
            includeContext = other.includeContext;
            message = std::move(other.message);
            file = other.file;
            line = other.line;
            function = other.function;
        }
        return *this;
    }

    // Destructor to log the message
    ~LogStream()
    {
        try
        {
            std::ostringstream oss;
            oss << "Lattice " << logLevel << ": " << message.str();

            if (includeContext)
            {
                oss << " " << std::filesystem::path(file).filename().string()
                    << " (" << line << ") " << function
                    << " [Thread ID: " << std::this_thread::get_id() << "]";
            }

            Logger::getInstance().logMessage(oss.str());
        }
        catch (const std::exception& e)
        {
            std::cerr << "LogStream destructor error: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "LogStream destructor encountered an unknown exception!" << std::endl;
        }
    }


    template <typename T>
    LogStream& operator<<(const T& value)
    {
        message << value;
        return *this;
    }

    void setContext(const char* file, int line, const char* function)
    {
        this->file = file;
        this->line = line;
        this->function = function;
    }

private:
    const char* logLevel;
    bool includeContext;
    std::ostringstream message;
    const char* file = nullptr;
    int line = 0;
    const char* function = nullptr;

    // Disable copy operations
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;
};

// Stream-like logger functions for different log levels
inline LogStream LogInfo()
{
    return LogStream("INFO", false); // Info doesn't include file/line info
}

inline LogStream LogVerbose(const char* file, int line, const char* function)
{
    LogStream log("DEBUG", true);
    log.setContext(file, line, function);
    return log;
}

inline LogStream LogWarning(const char* file, int line, const char* function)
{
    LogStream log("WARNING", true);
    log.setContext(file, line, function);
    return log;
}

inline LogStream LogError(const char* file, int line, const char* function)
{
    LogStream log("ERROR", true);
    log.setContext(file, line, function);
    return log;
}

// To use the logging functions like std::ostream:
#define logInfo LogInfo()
#define logDebug LogVerbose(__FILE__, __LINE__, __FUNCTION__)
#define logWarning LogWarning(__FILE__, __LINE__, __FUNCTION__)
#define logError LogError(__FILE__, __LINE__, __FUNCTION__)

//==========================================================
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

    static bool isWindowsStandalone()
    {
        std::string path = File::getBinaryFileAndPath();
        return path.size() >= 4 && path.rfind(".exe") == (path.size() - 4);
    }


  private:
    // Retrieves the path to the directory that contains the resources
      static std::string getResourceDirFromBundle();


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
