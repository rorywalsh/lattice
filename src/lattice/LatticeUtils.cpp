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

std::vector<std::string> File::getFilesOfType(const std::string &dirPath, const std::string &fileTypes)
{
    std::vector<std::string> result;

    // Resolve the absolute path based on the current CSD file location
    std::filesystem::path searchPath = lattice::File::formatPath(dirPath);

    if (searchPath.is_relative())
    {
        std::filesystem::path dirPath = std::filesystem::path(lattice::File::getBinaryFileAndPath()).parent_path();
        searchPath = dirPath / searchPath;
    }

    // Normalize the path to remove any redundant elements
    searchPath = std::filesystem::canonical(searchPath);

    // Split the fileTypes string into individual patterns
    std::vector<std::string> patterns;
    std::stringstream ss(fileTypes);
    std::string pattern;

    while (std::getline(ss, pattern, ';'))
    {
        patterns.push_back(pattern);
    }

    // Iterate over the directory and match the patterns
    for (const auto &entry : std::filesystem::recursive_directory_iterator(searchPath))
    {
        if (entry.is_regular_file())
        {
            std::string filePath = entry.path().string();
            for (const auto &p : patterns)
            {
                if (std::filesystem::path(filePath).filename().string().find(p.substr(1)) != std::string::npos)
                {
                    result.push_back(filePath);
                    break;
                }
            }
        }
    }

    std::sort(result.begin(), result.end(),
              [](const std::string &a, const std::string &b)
              {
                  // Extract filenames without extensions
                  std::string fileNameA = std::filesystem::path(a).filename().stem().string();
                  std::string fileNameB = std::filesystem::path(b).filename().stem().string();

                  // Convert filenames to integers if possible
                  auto convertToInt = [](const std::string &s) -> int
                  {
                      try
                      {
                          return std::stoi(s);
                      }
                      catch (...)
                      {
                          return 0; // Return 0 if conversion fails
                      }
                  };

                  int numA = convertToInt(fileNameA);
                  int numB = convertToInt(fileNameB);

                  // Compare numeric parts if both filenames are numeric, otherwise use lexicographical comparison
                  if (numA != 0 && numB != 0)
                  {
                      return numA < numB;
                  }
                  else
                  {
                      return fileNameA < fileNameB;
                  }
              });

    return result;
}

}
