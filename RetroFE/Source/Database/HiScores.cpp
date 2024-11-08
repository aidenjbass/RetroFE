#include <Windows.h>
#include "HiScores.h"
#include "../Utility/Utils.h"
#include "minizip/unzip.h"
#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>

// Get the singleton instance
HiScores& HiScores::getInstance() {
    static HiScores instance;
    return instance;
}

// Load all high scores, first from ZIP, then overriding with external XMLs
void HiScores::loadHighScores(const std::string& zipPath, const std::string& overridePath) {
    // Load defaults from the ZIP file
    loadFromZip(zipPath);

    // Check if the override directory exists
    if (std::filesystem::exists(overridePath) && std::filesystem::is_directory(overridePath)) {
        // Load override XML files from the directory
        for (const auto& file : std::filesystem::directory_iterator(overridePath)) {
            if (file.path().extension() == ".xml") {
                std::string gameName = file.path().stem().string();
                std::ifstream fileStream(file.path(), std::ios::binary);
                std::vector<char> buffer((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
                buffer.push_back('\0');
                loadFromFile(gameName, file.path().string(), buffer);
            }
        }
    } else {
        std::cerr << "Override directory does not exist or is not accessible: " << overridePath << std::endl;
    }
}


// Load high scores from XML files within the ZIP archive
void HiScores::loadFromZip(const std::string& zipPath) {
    unzFile zipFile = unzOpen(zipPath.c_str());
    if (zipFile == nullptr) {
        std::cerr << "Failed to open ZIP file: " << zipPath << std::endl;
        return;
    }

    if (unzGoToFirstFile(zipFile) == UNZ_OK) {
        do {
            unz_file_info fileInfo;
            char fileName[256];
            unzGetCurrentFileInfo(zipFile, &fileInfo, fileName, sizeof(fileName), nullptr, 0, nullptr, 0);

            if (std::string(fileName).find(".xml") != std::string::npos) {
                unzOpenCurrentFile(zipFile);

                // Read file content into buffer and ensure null-termination
                std::vector<char> buffer(fileInfo.uncompressed_size + 1);
                int bytesRead = unzReadCurrentFile(zipFile, buffer.data(), fileInfo.uncompressed_size);

                if (bytesRead < 0) {
                    std::cerr << "Error reading file: " << fileName << std::endl;
                    unzCloseCurrentFile(zipFile);
                    continue;
                }

                buffer[bytesRead] = '\0';  // Null-terminate

                std::string gameName = std::filesystem::path(fileName).stem().string();

                try {
                    loadFromFile(gameName, fileName, buffer);  // Parse and load the XML data
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing XML file " << fileName << ": " << e.what() << std::endl;
                }

                unzCloseCurrentFile(zipFile);
            }
        } while (unzGoToNextFile(zipFile) == UNZ_OK);
    }


    unzClose(zipFile);
}

// Parse a single XML file for high score data with dynamic columns
void HiScores::loadFromFile(const std::string& gameName, const std::string& filePath, std::vector<char>& buffer) {
    rapidxml::xml_document<> doc;

    // Attempt to parse the XML buffer
    try {
        doc.parse<0>(buffer.data());
    } catch (const rapidxml::parse_error& e) {
        std::cerr << "Parse error in file " << filePath << ": " << e.what() << std::endl;
        return;  // Exit if parsing fails
    }

    // Ensure the root node exists
    rapidxml::xml_node<>* rootNode = doc.first_node("hi2txt");
    if (!rootNode) {
        std::cerr << "Root node <hi2txt> not found in file " << filePath << std::endl;
        return;
    }

    rapidxml::xml_node<>* tableNode = rootNode->first_node("table");
    if (!tableNode) {
        std::cerr << "Table node <table> not found in file " << filePath << std::endl;
        return;
    }

    HighScoreTable highScoreTable;

    // Parse columns
    for (rapidxml::xml_node<>* colNode = tableNode->first_node("col"); colNode; colNode = colNode->next_sibling("col")) {
        highScoreTable.columns.push_back(colNode->value());
    }

    // Parse rows
    for (rapidxml::xml_node<>* row = tableNode->first_node("row"); row; row = row->next_sibling("row")) {
        std::vector<std::string> rowData;

        for (rapidxml::xml_node<>* cell = row->first_node("cell"); cell; cell = cell->next_sibling("cell")) {
            rowData.push_back(cell->value());
        }

        highScoreTable.rows.push_back(rowData);
    }

    scoresCache_[gameName] = highScoreTable;
}

// Retrieve a pointer to the high score table for a specific game
const HighScoreTable* HiScores::getHighScoreTable(const std::string& gameName) const {
    auto it = scoresCache_.find(gameName);
    if (it != scoresCache_.end()) {
        return &it->second;  // Return pointer to the high score table
    }
    return nullptr;  // Return null if game not found
}

// Check if a .hi file exists for the given game
bool HiScores::hasHiFile(const std::string& gameName) const {
    std::string hiFilePath = Utils::combinePath(hiFilesDirectory_, gameName + ".hi");
    return std::filesystem::exists(hiFilePath);
}

// Run hi2txt to process the .hi file, generate XML output, save to scores directory, and update cache
bool HiScores::runHi2Txt(const std::string& gameName) {
    // Set up paths
    std::string hi2txtPath = Utils::combinePath(Configuration::absolutePath, "hi2txt", "hi2txt.exe");
    std::string hiFilePath = Utils::combinePath(hiFilesDirectory_, gameName + ".hi");

    // Create the command string
    std::string command = "\"" + hi2txtPath + "\" -r -xml \"" + hiFilePath + "\"";

    // Initialize structures for the process
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&processInfo, sizeof(processInfo));

    // Redirect output to capture it into a buffer
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &saAttr, 0)) {
        std::cerr << "Failed to create pipe." << std::endl;
        return false;
    }
    startupInfo.hStdOutput = hWrite;
    startupInfo.hStdError = hWrite;

    // Start the process with CREATE_NO_WINDOW to prevent CMD from appearing
    if (!CreateProcessA(
        nullptr,
        const_cast<char*>(command.c_str()),  // Command line
        nullptr, nullptr, TRUE,              // Inherit handles
        CREATE_NO_WINDOW,                    // No window
        nullptr, nullptr, &startupInfo, &processInfo)) {
        std::cerr << "Failed to launch hi2txt for game " << gameName << std::endl;
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    // Close the write handle and read from the pipe
    CloseHandle(hWrite);
    std::vector<char> buffer;
    char tempBuffer[128];
    DWORD bytesRead;
    while (ReadFile(hRead, tempBuffer, sizeof(tempBuffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        tempBuffer[bytesRead] = '\0';
        buffer.insert(buffer.end(), tempBuffer, tempBuffer + bytesRead);
    }
    CloseHandle(hRead);

    // Wait for the process to complete
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    // Null-terminate and process the buffer
    buffer.push_back('\0');
    std::string xmlFilePath = Utils::combinePath(scoresDirectory_, gameName + ".xml");

    // Save XML to the scores directory
    std::ofstream outFile(xmlFilePath);
    if (!outFile) {
        std::cerr << "Error: Could not create XML file " << xmlFilePath << std::endl;
        return false;
    }
    outFile.write(buffer.data(), buffer.size() - 1);  // Exclude null terminator
    outFile.close();

    // Parse and load into cache
    try {
        loadFromFile(gameName, xmlFilePath, buffer);
    } catch (const rapidxml::parse_error& e) {
        std::cerr << "Parsing failed for game " << gameName << ": " << e.what() << std::endl;
        return false;
    }

    std::cout << "Scores updated for " << gameName << " and saved to " << xmlFilePath << std::endl;
    return true;
}


// Helper function to load the XML file content into a buffer
bool HiScores::loadFileToBuffer(const std::string& filePath, std::vector<char>& buffer) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        return false;
    }

    // Get the file size and resize the buffer accordingly
    file.seekg(0, std::ios::end);
    buffer.resize(file.tellg());
    file.seekg(0, std::ios::beg);

    // Read file content into buffer and close the file
    file.read(buffer.data(), buffer.size());
    buffer.push_back('\0'); // Null-terminate for rapidxml parsing
    file.close();

    return true;
}