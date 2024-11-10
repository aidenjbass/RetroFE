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

struct CachedGlyph {
    SDL_Rect sourceRect;  // Source rectangle on the font texture
    SDL_FRect destRect;    // Destination rectangle on the screen
    float advance;        // Advance value for the glyph
};


class ReloadableScrollingText : public Component
{
public:
    ReloadableScrollingText(Configuration& config, bool systemMode, bool layoutMode, 
        bool menuMode, std::string type, std::string textFormat, 
        std::string singlePrefix, std::string singlePostfix, std::string pluralPrefix, 
        std::string pluralPostfix, std::string alignment, Page& p, int displayOffset, 
        Font* font, std::string direction, float scrollingSpeed, float startPosition, 
        float startTime, float endTime, std::string location, float baseColumnPadding, float baseRowPadding);
    virtual ~ReloadableScrollingText( );
    bool     update(float dt);
    void     draw( );
    void renderText(const std::string& text, Font* font, SDL_Texture* texture, int x, int y, float scale, float alpha);
    void     allocateGraphicsMemory( );
    void     freeGraphicsMemory( );
    void     deInitializeFonts();
    void     initializeFonts();


private:
    bool loadFileText(const std::string& filePath);
    void reloadTexture(bool resetScroll = true);
    void loadText( std::string collection, std::string type, std::string basename, std::string filepath, bool systemMode );
    void renderScrollingTextHorizontal(Font* font, SDL_Texture* texture, float& currentX, float y, float scale, float maxWidth);
    void renderScrollingTextVertical(Font* font, SDL_Texture* texture, float x, float& currentY, float scale, float maxHeight);
    void updateGlyphCache();
    Configuration           &config_;
    bool                     systemMode_;
    bool                     layoutMode_;
    Font                    *fontInst_;
    std::string              type_;
    std::string              textFormat_;
    std::string              singlePrefix_;
    std::string              singlePostfix_;
    std::string              pluralPrefix_;
    std::string              pluralPostfix_;
    std::string              alignment_;
    std::vector<std::string> text_;
    std::string              direction_;
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
    std::string              currentCollection_;
    int                      displayOffset_;
    std::vector<CachedGlyph> cachedGlyphs_;
    bool needsUpdate_;
    float textWidth_;
    float textHeight_;
    float lastScale_;
    float lastImageMaxWidth_;
    float lastImageMaxHeight_;
    std::filesystem::file_time_type lastWriteTime_;
    const HighScoreTable* highScoreTable_ = nullptr;
    SDL_BlendMode maskBlendMode = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_COLOR, SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD
    );

};
