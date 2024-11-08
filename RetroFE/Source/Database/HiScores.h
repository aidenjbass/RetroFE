/* This file is part of RetroFE.
*
* RetroFE is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* RetroFE is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <unordered_map>
#include <string>
#include <vector>

struct HighScoreTable {
    std::vector<std::string> columns; // Column names
    std::vector<std::vector<std::string>> rows; // Each row is a vector of cell values
};

class HiScores {
public:
    static HiScores& getInstance();

    // Initialize by loading all high scores
    void loadHighScores(const std::string& zipPath, const std::string& overridePath);

    // Retrieve high score table for a game
    std::string getHighScore(const std::string& gameName);

    const HighScoreTable* getHighScoreTable(const std::string& gameName) const;


private:
    HiScores() = default;

    std::unordered_map<std::string, HighScoreTable> scoresCache_;

    void loadFromZip(const std::string& zipPath);
    void loadFromFile(const std::string& gameName, const std::string& filePath, std::vector<char>& buffer);
};
