#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <sstream>

// Use the filesystem namespace for brevity
namespace fs = std::filesystem;

// --- Globals for Graceful Shutdown ---
std::atomic<bool> keepRunning{ true };

void signalHandler(int) {
    keepRunning = false;
}

// --- Function Prototypes ---

// Prints a message to the console with a timestamp.
void logMessage(const std::string& message);

// Main backup logic function.
void runBackup(const fs::path& sourceDir, const fs::path& backupDir, const fs::path& deletedDir);

// --- Main Program ---

int main() {
    // Define the paths for your source, backup, and deleted directories.
    // Make sure to use forward slashes or double backslashes for paths.
    fs::path sourcePath = "E:/Misc";
    fs::path backupPath = "D:/Backup/misc";
    fs::path deletedPath = "D:/Backup/deleted";

    // Register signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);

    // --- Initial Setup ---
    // Create the backup and deleted directories if they don't already exist.
    try {
        if (!fs::exists(backupPath)) {
            fs::create_directories(backupPath);
            logMessage("Created backup directory: " + backupPath.string());
        }
        if (!fs::exists(deletedPath)) {
            fs::create_directories(deletedPath);
            logMessage("Created deleted items directory: " + deletedPath.string());
        }
    }
    catch (const fs::filesystem_error& e) {
        logMessage("Error creating directories: " + std::string(e.what()));
        return 1; // Exit if we can't create directories
    }

    // --- Main Loop ---
    // This loop runs indefinitely, triggering the backup process every hour.
    while (keepRunning) {
        logMessage("Starting file check...");
        runBackup(sourcePath, backupPath, deletedPath);
        logMessage("Check complete. Waiting for the next trigger in 1 hour...");

        // Wait for one hour before the next cycle, checking for shutdown signal.
        for (int i = 0; i < 3600 && keepRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    logMessage("Backup utility terminated by user.");
    return 0;
}

// --- Function Definitions ---

/**
 * @brief Prints a message to the console with a timestamp.
 * @param message The message to display.
 */
void logMessage(const std::string& message) {
    // Get the current time for logging purposes
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &currentTime);
#else
    localtime_r(&currentTime, &tm);
#endif
    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << message;
    std::cout << oss.str() << std::endl;
}

/**
 * @brief Runs the backup process by comparing files between source and backup directories.
 * @param sourceDir The directory to back up from.
 * @param backupDir The directory to back up to.
 * @param deletedDir The directory to move old backups to.
 */
void runBackup(const fs::path& sourceDir, const fs::path& backupDir, const fs::path& deletedDir) {
    try {
        // Iterate over each item in the source directory recursively.
        for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
            if (!fs::is_regular_file(entry.path())) continue;

            // Get the relative path of the file within the source directory.
            fs::path relPath = fs::relative(entry.path(), sourceDir);

            // Construct the corresponding paths in the backup and deleted directories.
            fs::path backupFile = backupDir / relPath;
            fs::path deletedFile = deletedDir / relPath;

            // Ensure parent directories exist
            fs::create_directories(backupFile.parent_path());
            fs::create_directories(deletedFile.parent_path());

            if (!fs::exists(backupFile)) {
                // --- Case 1: File is new ---
                // The file does not exist in the backup, so copy it.
                fs::copy_file(entry.path(), backupFile, fs::copy_options::overwrite_existing);
                logMessage("Copied new file: " + relPath.string());
            }
            else {
                // --- Case 2: File already exists ---
                // Check if the file sizes are different.
                if (fs::file_size(entry.path()) != fs::file_size(backupFile)) {
                    // The file has been modified.
                    // Append timestamp to avoid overwriting in deletedDir
                    auto now = std::chrono::system_clock::now();
                    std::time_t t = std::chrono::system_clock::to_time_t(now);
                    std::ostringstream ts;
                    std::tm tm;
#ifdef _WIN32
                    localtime_s(&tm, &t);
#else
                    std::tm* tmPtr = std::localtime(&t);
                    tm = *tmPtr;
#endif
                    ts << std::put_time(&tm, "_%Y%m%d%H%M%S");
                    fs::path deletedFileWithTimestamp = deletedFile;
                    deletedFileWithTimestamp += ts.str();

                    // Move the old backup to the deleted folder with a timestamp.
                    fs::rename(backupFile, deletedFileWithTimestamp);
                    logMessage("Moved outdated backup: " + backupFile.string() + " -> " + deletedFileWithTimestamp.string());

                    // Copy the new file to the backup directory.
                    fs::copy_file(entry.path(), backupFile, fs::copy_options::overwrite_existing);
                    logMessage("Copied updated file: " + relPath.string());
                }
                // If sizes are the same, do nothing.
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        logMessage("An error occurred during backup: " + std::string(e.what()));
    }
}