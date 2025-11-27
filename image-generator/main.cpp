#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <vector>
#include <thread>
namespace fs = std::filesystem;

std::string readBinary(const std::string& file)
// read binary file content
{
    std::ifstream ifs(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
}
std::string addDatePrefix(const std::string& filename) 
// add date prefix to filename to prevent overwrite
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H%M%S_") << filename;
    return oss.str();
}

fs::path makeYearMonthFolder(const fs::path& baseDir) 
// make year/month folder structure for better filing
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream year, month;
    year << std::put_time(&tm, "%Y");
    month << std::put_time(&tm, "%m");

    fs::path folder = baseDir / year.str() / month.str();

    if (!fs::exists(folder)) {
        fs::create_directories(folder);
    }
    return folder;
}

std::string saveToBackup(const std::string& filename, const std::string& data) 
// save file to backup directory
{
    fs::path backupBaseDir = "../images/backup";
    fs::path backupDatedFolder = makeYearMonthFolder(backupBaseDir);
    

    std::string prefixedFilename = addDatePrefix(filename);

    fs::path outputPath = backupDatedFolder / prefixedFilename;
    
    std::ofstream ofs(outputPath, std::ios::binary);
    ofs.write(data.data(), data.size());
    ofs.close();

    std::cout << "Saved backup file: " << outputPath << std::endl;
    return outputPath;
}

std::vector<fs::path> get_all_files(const fs::path& directory_path) 
// get all files in a directory
{
    std::vector<fs::path> file_paths;
    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        std::cerr << "Error: Directory not found or inaccessible: " << directory_path << std::endl;
        return file_paths;
    }
    
    // Iterate over all entries in the directory
    for (const auto& entry : fs::directory_iterator(directory_path)) {
        if (entry.is_regular_file()) {
            file_paths.push_back(entry.path());
        }
    }
    return file_paths;
}

bool isFileStable(const fs::path& p) {
// check if file size is stable (not changing) over short period
    std::uintmax_t size1 = fs::file_size(p);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::uintmax_t size2 = fs::file_size(p);
    return size1 == size2 && size1 > 0;
}

int main() {
    const char* host_env = getenv("RABBITMQ_HOST");
    std::string host = host_env ? host_env : "localhost";
    AmqpClient::Channel::OpenOpts opts;
    int port = 5672;
    std::string user = "guest";
    std::string pass = "guest";
    opts.host = host;
    opts.port = port;
    opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth(user, pass);
    auto channel = AmqpClient::Channel::Open(opts);
    while (true)
    {
        std::string folderpath = "/images/in";
        std::vector<fs::path> files_to_process = get_all_files(folderpath);
        for (const auto& file_path : files_to_process) {
            std::string fullFilePath = file_path.string();

            if (!isFileStable(fullFilePath)) {
                continue;
            }

            std::string filename = file_path.filename().string();
            std::string binaryData = readBinary(fullFilePath);

            std::string backupPath = saveToBackup(filename, binaryData);
            
            AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create(binaryData);
            AmqpClient::Table headers;
            headers["filename"] = AmqpClient::TableValue(filename);
            headers["backupPath"] = AmqpClient::TableValue(backupPath);
            msg->HeaderTable(headers);

            // Set metadata
            msg->ContentType("application/octet-stream");
            msg->DeliveryMode(AmqpClient::BasicMessage::dm_persistent);
            try {
                channel->BasicPublish("", "file_queue", msg);

                std::cout << "Published: " << filename << " size=" << binaryData.size() << std::endl;

                // 4. Delete file after successful publish
                fs::remove(fullFilePath);
                std::cout << "Deleted: " << filename << std::endl;

            } catch (const std::exception &ex) {
                std::cerr << "Error publishing " << filename 
                        << ": " << ex.what() << std::endl;
            }

        }
    }
    
    
    

    return 0;
}
