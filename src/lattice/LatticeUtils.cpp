/*
    MIT License

    Copyright (c) 2025 Rory Walsh

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/


#include "LatticeUtils.h"

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

#if defined(__linux__)
#include <pwd.h>
#include <unistd.h>
#endif

namespace lattice {

void setTimeout(std::function<void()> callback, int delayMilliseconds) {
    std::thread([callback, delayMilliseconds]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMilliseconds));
        callback(); // Execute the callback after the delay
    }).detach(); // Detach the thread to run independently
}

//==================================================================================
// Logging methods
//==================================================================================

Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::setLogFile(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(fileMutex);

    if (logFile.is_open())
    {
        logFile.close();
    }

    logFile.open(filePath, std::ios::out | std::ios::app);

    if (!logFile.is_open())
    {
        throw std::runtime_error("Failed to open log file: " + filePath);
    }
}

void Logger::closeLogFile()
{
    std::lock_guard<std::mutex> lock(fileMutex);

    if (logFile.is_open())
    {
        logFile.close();
    }
}

void Logger::logMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(fileMutex);

    // Log to console
    std::cout << message << std::endl;

    // Log to file if open
    if (logFile.is_open())
    {
        logFile << message << std::endl;
    }

    // Log to Visual Studio debug console
    logToDebug(message + "\n");
}

//========================================================================
// File utility class
//========================================================================
std::string File::getBinaryFileAndPath()
{
#if defined(_WIN32)
    return getWindowsBinaryPath();
#elif defined(__APPLE__)
    return getMacBinaryPath();
#elif defined(__linux__)
    return getLinuxBinaryPath();
#else
    return "";
#endif
}

bool File::exists(const std::string &filePath)
{
    return std::filesystem::exists(filePath);
}

bool File::directoryExists(const std::string &dirPath)
{
#if defined(_WIN32)
    return std::filesystem::exists(std::filesystem::path(dirPath)) &&
           std::filesystem::is_directory(std::filesystem::path(dirPath));
#else
    struct stat info;
    if (stat(dirPath.c_str(), &info) != 0)
        return false;
    return (info.st_mode & S_IFDIR);
#endif
}

std::string File::getParentDirectory(const std::string &currentFile)
{
    std::filesystem::path path(currentFile);
    if (path.has_parent_path())
    {
        return path.parent_path().string();
    }
    return currentFile; // If no parent, return the original path
}

std::string File::getMimeType(const std::string& path) 
{
    // Convert to lowercase for case-insensitive comparison
    auto lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Text and Documents
    if (lowerPath.ends_with(".html") || lowerPath.ends_with(".htm")) return "text/html";
    if (lowerPath.ends_with(".css"))  return "text/css";
    if (lowerPath.ends_with(".csv"))  return "text/csv";
    if (lowerPath.ends_with(".txt"))  return "text/plain";
    if (lowerPath.ends_with(".xml"))  return "text/xml";
    if (lowerPath.ends_with(".vtt"))  return "text/vtt";

    // JavaScript and JSON
    if (lowerPath.ends_with(".js"))    return "application/javascript";
    if (lowerPath.ends_with(".json"))  return "application/json";
    if (lowerPath.ends_with(".jsonld")) return "application/ld+json";
    if (lowerPath.ends_with(".wasm"))  return "application/wasm";

    // Images
    if (lowerPath.ends_with(".png"))  return "image/png";
    if (lowerPath.ends_with(".jpg") || lowerPath.ends_with(".jpeg")) return "image/jpeg";
    if (lowerPath.ends_with(".gif"))  return "image/gif";
    if (lowerPath.ends_with(".webp")) return "image/webp";
    if (lowerPath.ends_with(".svg"))  return "image/svg+xml";
    if (lowerPath.ends_with(".ico"))  return "image/x-icon";
    if (lowerPath.ends_with(".bmp"))  return "image/bmp";
    if (lowerPath.ends_with(".tiff")) return "image/tiff";

    // Fonts
    if (lowerPath.ends_with(".woff"))  return "font/woff";
    if (lowerPath.ends_with(".woff2")) return "font/woff2";
    if (lowerPath.ends_with(".ttf"))   return "font/ttf";
    if (lowerPath.ends_with(".otf"))   return "font/otf";

    // Audio
    if (lowerPath.ends_with(".mp3"))  return "audio/mpeg";
    if (lowerPath.ends_with(".wav"))  return "audio/wav";
    if (lowerPath.ends_with(".ogg"))  return "audio/ogg";
    if (lowerPath.ends_with(".flac")) return "audio/flac";
    if (lowerPath.ends_with(".aac"))  return "audio/aac";
    if (lowerPath.ends_with(".weba")) return "audio/webm";

    // Video
    if (lowerPath.ends_with(".mp4"))  return "video/mp4";
    if (lowerPath.ends_with(".webm")) return "video/webm";
    if (lowerPath.ends_with(".ogv"))  return "video/ogg";
    if (lowerPath.ends_with(".avi"))  return "video/x-msvideo";
    if (lowerPath.ends_with(".mov"))  return "video/quicktime";

    // Archives
    if (lowerPath.ends_with(".zip"))  return "application/zip";
    if (lowerPath.ends_with(".tar"))  return "application/x-tar";
    if (lowerPath.ends_with(".gz"))   return "application/gzip";
    if (lowerPath.ends_with(".7z"))   return "application/x-7z-compressed";
    if (lowerPath.ends_with(".rar"))  return "application/vnd.rar";

    // Documents
    if (lowerPath.ends_with(".pdf"))  return "application/pdf";
    if (lowerPath.ends_with(".doc"))  return "application/msword";
    if (lowerPath.ends_with(".docx")) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (lowerPath.ends_with(".xls"))  return "application/vnd.ms-excel";
    if (lowerPath.ends_with(".xlsx")) return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (lowerPath.ends_with(".ppt"))  return "application/vnd.ms-powerpoint";
    if (lowerPath.ends_with(".pptx")) return "application/vnd.openxmlformats-officedocument.presentationml.presentation";

    // WebAssembly and Binary
    if (lowerPath.ends_with(".wasm")) return "application/wasm";
    if (lowerPath.ends_with(".bin"))  return "application/octet-stream";

    // Fallbacks
    if (lowerPath.ends_with(".php"))  return "application/x-httpd-php";
    if (lowerPath.ends_with(".sh"))   return "application/x-sh";
    if (lowerPath.ends_with(".exe"))  return "application/x-msdownload";

    // Default for unknown types
    return "application/octet-stream";
}

std::string File::withExtension(const std::string &filePath, const std::string &newExtension)
{
    std::filesystem::path path(filePath);

    // Ensure the extension starts with a '.'
    std::string adjustedExtension = newExtension;
    if (!newExtension.empty() && newExtension[0] != '.')
    {
        adjustedExtension = "." + newExtension;
    }

    // Replace the extension
    path.replace_extension(adjustedExtension);

    return path.string();
}

std::string File::getResourceDirFromBundle()
{
    if(File::isWindowsStandalone())
        return File::joinPath(File::getParentDirectory(File::getBinaryFileAndPath()), "/Resources");

    const auto bundleParentDirPath = File::getParentDirectory(File::getParentDirectory(File::getBinaryFileAndPath()));

    return File::joinPath(File::getParentDirectory(bundleParentDirPath), "/Contents/Resources");

}

std::string File::loadJSFile(const std::string &filePath)
{
    std::ifstream file(filePath);
    std::string jsContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return jsContent;
}


std::string File::getBinaryFileName()
{
    std::string binaryPath = getBinaryFileAndPath();
    size_t pos = binaryPath.find_last_of("/\\");

    if (pos != std::string::npos)
        return binaryPath.substr(pos + 1);
    else
        return binaryPath;
}

std::string File::formatPath(const std::string &path)
{
    std::string sanitizedPath = path;

    // Remove trailing backslashes
    while (!sanitizedPath.empty() && sanitizedPath.back() == '\\')
    {
        sanitizedPath.pop_back();
    }

    // Replace backslashes with forward slashes
    for (char &c : sanitizedPath)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    return sanitizedPath;
}

std::string File::getFileAsString(const std::string& filePath) 
{
    std::ifstream file(filePath);
    if (!file) {
        throw std::runtime_error("Could not open file: " + filePath);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<uint8_t> File::getFileAsBinary(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("Failed to open file: " + path);

    std::streamsize size = in.tellg();
    if (size < 0)
        return {}; // empty file or error

    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(buffer.data()), size))
        throw std::runtime_error("Failed to read file: " + path);

    return buffer;
}

std::vector<std::string> File::getFilesOfType(
    const std::string &dirPath,
    const std::string &fileTypes)
{
    namespace fs = std::filesystem;

    std::vector<std::string> result;

    // Resolve the absolute path
    fs::path searchPath = lattice::File::formatPath(dirPath);

    if (searchPath.is_relative())
    {
        fs::path baseDir =
            fs::path(lattice::File::getBinaryFileAndPath()).parent_path();
        searchPath = baseDir / searchPath;
    }

    searchPath = fs::canonical(searchPath);

    // Parse extensions
    std::unordered_set<std::string> extensions;
    std::stringstream ss(fileTypes);
    std::string ext;

    auto toLower = [](std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return s;
    };

    while (std::getline(ss, ext, ';'))
    {
        if (!ext.empty())
        {
            extensions.insert(toLower(ext));
        }
    }

    // Iterate and match extensions
    for (const auto &entry : fs::recursive_directory_iterator(searchPath))
    {
        if (!entry.is_regular_file())
            continue;

        std::string fileExt = toLower(entry.path().extension().string());

        if (extensions.contains(fileExt))
        {
            result.push_back(entry.path().string());
        }
    }

    // Sort numerically when possible
    std::sort(result.begin(), result.end(),
              [](const std::string &a, const std::string &b)
              {
                  fs::path pa(a), pb(b);

                  std::string sa = pa.stem().string();
                  std::string sb = pb.stem().string();

                  auto toInt = [](const std::string &s) -> std::optional<int>
                  {
                      try
                      {
                          size_t idx;
                          int val = std::stoi(s, &idx);
                          return (idx == s.size()) ? std::optional<int>(val)
                                                   : std::nullopt;
                      }
                      catch (...)
                      {
                          return std::nullopt;
                      }
                  };

                  auto ia = toInt(sa);
                  auto ib = toInt(sb);

                  if (ia && ib)
                      return *ia < *ib;

                  return sa < sb;
              });

    return result;
}

std::string getSpecialLocation(const std::string &locationType)
{
    if (locationType == "USER_HOME_DIRECTORY") {
#if defined(_WIN32)
        const char* homeDrive = std::getenv("HOMEDRIVE");
        const char* homePath = std::getenv("HOMEPATH");
        if (homeDrive && homePath) {
            return std::string(homeDrive) + homePath;
        }
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) : "";
#elif defined(__linux__)
        const char* home = std::getenv("HOME");
        if (home) return std::string(home);
        struct passwd* pw = getpwuid(getuid());
        return pw ? std::string(pw->pw_dir) : "";
#endif
    }
    else if (locationType == "USER_DOCUMENTS_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Documents" : "";
#elif defined(__linux__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) : ""; // Linux doesn't have a standard Documents folder
#endif
    }
    else if (locationType == "USER_DESKTOP_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Desktop" : "";
#elif defined(__linux__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Desktop" : "";
#endif
    }
    else if (locationType == "USER_MUSIC_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYMUSIC, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Music" : "";
#elif defined(__linux__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Music" : "";
#endif
    }
    else if (locationType == "USER_MOVIES_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYVIDEO, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Movies" : "";
#elif defined(__linux__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Videos" : "";
#endif
    }
    else if (locationType == "USER_PICTURES_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYPICTURES, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Pictures" : "";
#elif defined(__linux__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Pictures" : "";
#endif
    }
    else if (locationType == "USER_APPLICATION_DATA_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Library/Application Support" : "";
#elif defined(__linux__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/.config" : "";
#endif
    }
    else if (locationType == "COMMON_APPLICATION_DATA_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        return "/Library/Application Support";
#elif defined(__linux__)
        return "/opt";
#endif
    }
    else if (locationType == "COMMON_DOCUMENTS_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_DOCUMENTS, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        return "/Users/Shared";
#elif defined(__linux__)
        return "/usr/share"; // Linux doesn't have a standard shared documents folder
#endif
    }
    else if (locationType == "WINDOWS_SYSTEM_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (GetSystemDirectoryA(path, MAX_PATH) > 0) {
            return std::string(path);
        }
        return "";
#else
        return ""; // Not applicable on non-Windows platforms
#endif
    }
    else if (locationType == "GLOBAL_APPLICATIONS_DIRECTORY") {
#if defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, 0, path))) {
            return std::string(path);
        }
        return "";
#elif defined(__APPLE__)
        return "/Applications";
#elif defined(__linux__)
        return "/usr";
#endif
    }

    return ""; // Unknown location type
}

}
