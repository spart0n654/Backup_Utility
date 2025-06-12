#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <unordered_map>

// For easier use of filesystem library
namespace fs = std::filesystem;

// --- Configuration ---
const fs::path sourceDir = "E:/Misc";
const fs::path backupDir = "D:/Backup/misc";
const fs::path deletedDir = "D:/Backup/deleted";
const int checkIntervalHours = 1;
// -------------------

// Function to get a map of file paths to their last write times
std::unordered_map<std::string, fs::file_time_type> getFileStates(const fs::path& directory) {
    std::unordered_map<std::string, fs::file_time_type> fileStates;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (fs::is_regular_file(entry.path())) {
            std::string relativePath = fs::relative(entry.path(), sourceDir).string();
            fileStates[relativePath] = fs::last_write_time(entry);
        }
    }
    return fileStates;
}

void runBackup() {
    std::cout << "Starting backup check..." << std::endl;

    // Ensure backup and deleted directories exist
    fs::create_directories(backupDir);
    fs::create_directories(deletedDir);

    auto sourceFiles = getFileStates(sourceDir);
    auto backupFiles = getFileStates(backupDir);

    // 1. Check for new or modified files
    for (const auto& [relativePath, sourceWriteTime] : sourceFiles) {
        fs::path sourceFilePath = sourceDir / relativePath;
        fs::path backupFilePath = backupDir / relativePath;
        bool needsCopy = false;

        if (backupFiles.find(relativePath) == backupFiles.end()) {
            // New file
            needsCopy = true;
            std::cout << "New file detected: " << relativePath << std::endl;
        }
        else {
            // Existing file, check for modification
            if (sourceWriteTime > backupFiles.at(relativePath)) {
                needsCopy = true;
                std::cout << "File modified: " << relativePath << std::endl;
            }
        }

        if (needsCopy) {
            // Ensure the directory exists in the backup
            fs::create_directories(backupFilePath.parent_path());
            fs::copy_file(sourceFilePath, backupFilePath, fs::copy_options::overwrite_existing);
            std::cout << "Copied " << relativePath << " to backup." << std::endl;
        }
    }

    // 2. Check for deleted files
    for (const auto& [relativePath, backupWriteTime] : backupFiles) {
        if (sourceFiles.find(relativePath) == sourceFiles.end()) {
            // File was deleted from source
            fs::path backupFilePath = backupDir / relativePath;
            fs::path destination = deletedDir / (relativePath + ".deleted");

            // Ensure the directory exists in the deleted folder
            fs::create_directories(destination.parent_path());
            fs::rename(backupFilePath, destination);
            std::cout << "File deleted from source, moved to deleted folder: " << relativePath << std::endl;
        }
    }

    // 3. Purging of old files has been removed.
    // Files in the 'deletedDir' will be kept indefinitely.

    std::cout << "Backup check finished." << std::endl;
}

int main() {
    while (true) {
        runBackup();
        std::cout << "Next check in " << checkIntervalHours << " hour(s)." << std::endl;
        std::this_thread::sleep_for(std::chrono::hours(checkIntervalHours));
    }
    return 0;
}