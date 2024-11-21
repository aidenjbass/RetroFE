#pragma once
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
#include "Component.h"
#include "../../Collection/Item.h"
#include "../../Database/HiScores.h"
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <filesystem>

class ReloadableHiscores : public Component
{
public:
    ReloadableHiscores(Configuration& config, std::string textFormat, Page& p, int displayOffset, Font* font,
        float scrollingSpeed, float startPosition, float startTime, float endTime, float baseColumnPadding, float baseRowPadding, size_t maxRows);
    virtual ~ReloadableHiscores( );
    bool     update(float dt);
    void     draw( );
    void     allocateGraphicsMemory( );
    void     freeGraphicsMemory( );
    void     deInitializeFonts();
    void     initializeFonts();


private:
    void reloadTexture(bool resetScroll = true);
    void cacheColumnWidths(Font* font, float scale, const HighScoreTable& table, float paddingBetweenColumns);
    bool createIntermediateTexture(SDL_Renderer* renderer, int width, int height);
    Configuration           &config_;
    Font                    *fontInst_;
    std::string              textFormat_;
    std::string              location_; 
    float                    scrollingSpeed_;
    float                    startPosition_;
    float                    currentPosition_;
    float                    startTime_;
    float                    waitStartTime_;
    float                    endTime_;
    float                    waitEndTime_;
    float                    baseColumnPadding_;
    float                    baseRowPadding_;
    int                      displayOffset_;
    size_t                   maxRows_;
    bool needsRedraw_;
    size_t cachedTableIndex_ = std::numeric_limits<size_t>::max(); // Invalid table index
    std::vector<float> cachedColumnWidths_;
    float cachedTotalTableWidth_ = 0.0f;
    Item* lastSelectedItem_ = nullptr;  // Track the previously selected item
    const HighScoreData* highScoreTable_ = nullptr;
    size_t currentTableIndex_ = 0;           // Tracks the current table being displayed (for multi-table support)
    float tableDisplayTimer_ = 0.0f;      // Timer to manage the display time for each table
    float currentTableDisplayTime_ = 0.0f; // Calculated display time for the current table, based on scrolling needs
    bool switchTable_ = false;            // Flag indicating when to switch to the next table
    float displayTime_ = 5.0f;            // Default display time for non-scrolling tables (adjustable as needed)
    SDL_Texture* intermediateTexture_;
};
