#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <unordered_map>
#include <future>

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

    fs::create_directories(backupDir);
    fs::create_directories(deletedDir);

    // Calculate total data size in sourceDir
    size_t totalSize = 0;
    for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
        if (fs::is_regular_file(entry.path())) {
            totalSize += fs::file_size(entry.path());
        }
    }
    std::cout << "Scanning source files..." << std::endl;

    // Scan source files
    std::unordered_map<std::string, fs::file_time_type> sourceFiles;
    for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
        if (fs::is_regular_file(entry.path())) {
            std::string relativePath = fs::relative(entry.path(), sourceDir).string();
            sourceFiles[relativePath] = fs::last_write_time(entry);
        }
    }
    std::cout << "Scanning backup files..." << std::endl;
    auto backupFiles = getFileStates(backupDir);

    // 1. Check for new or modified files
    std::vector<std::future<void>> copyTasks;
    int filesToCopy = 0;
    for (const auto& [relativePath, sourceWriteTime] : sourceFiles) {
        fs::path sourceFilePath = sourceDir / relativePath;
        fs::path backupFilePath = backupDir / relativePath;
        bool needsCopy = false;
        bool moveOldBackup = false;

        if (backupFiles.find(relativePath) == backupFiles.end()) {
            needsCopy = true;
        } else {
            auto backupWriteTime = backupFiles.at(relativePath);
            auto sourceSize = fs::file_size(sourceFilePath);
            auto backupSize = fs::exists(backupFilePath) ? fs::file_size(backupFilePath) : 0;

            if (sourceWriteTime > backupWriteTime || sourceSize != backupSize) {
                needsCopy = true;
                moveOldBackup = true;
            }
        }

        if (needsCopy) {
            ++filesToCopy;
            std::cout << "Copying: " << relativePath << std::endl;
            copyTasks.push_back(std::async(std::launch::async, [=]() {
                if (moveOldBackup && fs::exists(backupFilePath)) {
                    fs::path deletedBackupPath = deletedDir / (relativePath + ".deleted");
                    fs::create_directories(deletedBackupPath.parent_path());
                    fs::rename(backupFilePath, deletedBackupPath);
                }
                fs::create_directories(backupFilePath.parent_path());
                fs::copy_file(sourceFilePath, backupFilePath, fs::copy_options::overwrite_existing);
            }));
        }
    }

    // Wait for all copy tasks to finish
    for (auto& task : copyTasks) {
        task.get();
    }
    if (filesToCopy == 0) {
        std::cout << "No new or modified files to copy." << std::endl;
    }

    // 2. Check for deleted files
    int filesDeleted = 0;
    for (const auto& [relativePath, backupWriteTime] : backupFiles) {
        if (sourceFiles.find(relativePath) == sourceFiles.end()) {
            fs::path backupFilePath = backupDir / relativePath;
            fs::path destination = deletedDir / (relativePath + ".deleted");
            fs::create_directories(destination.parent_path());
            fs::rename(backupFilePath, destination);
            std::cout << "File deleted from source, moved to deleted folder: " << relativePath << std::endl;
            ++filesDeleted;
        }
    }
    if (filesDeleted == 0) {
        std::cout << "No files deleted from source." << std::endl;
    }

    std::cout << "Backup check finished." << std::endl;
}

int main() {
    while (true) {
        runBackup();
        // Clear the console output before displaying the waiting message
        system("cls");
        std::cout << "Waiting for the next check in " << checkIntervalHours << " hour(s)..." << std::endl;
        std::this_thread::sleep_for(std::chrono::hours(checkIntervalHours));
    }
    return 0;
}