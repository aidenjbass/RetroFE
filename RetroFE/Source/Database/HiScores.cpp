#include "HiScores.h"
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
