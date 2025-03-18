#include "LatticeUtils.h"

namespace lattice {

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

std::string File::getResourceDir()
{
    return getResourceDirFromBundle();
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

std::string File::joinPath(const std::string &dirPath, const std::string &fileName)
{
    if (dirPath.empty())
        return fileName;
    else if (fileName.empty())
        return dirPath;
    else
    {
        char separator =
#if defined(_WIN32)
            '\\';
#else
            '/';
#endif
        if (dirPath.back() == separator || fileName.front() == separator)
            return dirPath + fileName;
        else
            return dirPath + separator + fileName;
    }
}

}
