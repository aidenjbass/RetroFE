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

#include "ReloadableScrollingText.h"
#include "../ViewInfo.h"
#include "../../Database/Configuration.h"
#include "../../Database/GlobalOpts.h"
#include "../../Database/HiScores.h"
#include "../../Utility/Log.h"
#include "../../Utility/Utils.h"
#include "../../SDL.h"
#include "../Font.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <algorithm>

ReloadableScrollingText::ReloadableScrollingText(Configuration& config, bool systemMode, bool layoutMode, bool menuMode, 
    std::string type, std::string textFormat, std::string singlePrefix, std::string singlePostfix, 
    std::string pluralPrefix, std::string pluralPostfix, std::string alignment, 
    Page& p, int displayOffset, Font* font, std::string direction, float scrollingSpeed, 
    float startPosition, float startTime, float endTime, std::string location, 
    float baseColumnPadding, float baseRowPadding)
    : Component(p)
    , config_(config)
    , systemMode_(systemMode)
    , layoutMode_(layoutMode)
    , fontInst_(font)
    , type_(type)
    , textFormat_(textFormat)
    , singlePrefix_(singlePrefix)
    , singlePostfix_(singlePostfix)
    , pluralPrefix_(pluralPrefix)
    , pluralPostfix_(pluralPostfix)
    , alignment_(alignment)
    , direction_(direction)
    , location_(location)
    , scrollingSpeed_(scrollingSpeed)
    , startPosition_(startPosition)
    , currentPosition_(-startPosition)
    , startTime_(startTime)
    , waitStartTime_(startTime)
    , endTime_(endTime)
    , waitEndTime_(0.0f)
    , baseColumnPadding_(baseColumnPadding)
    , baseRowPadding_(baseRowPadding)
    , currentCollection_("")
    , displayOffset_(displayOffset)
    , needsUpdate_(true)
    , textWidth_(0.0f)
    , textHeight_(0.0f)
    , lastScale_(0.0f)
    , lastImageMaxWidth_(0.0f)
    , lastImageMaxHeight_(0.0f)
    , lastWriteTime_(std::filesystem::file_time_type::min())
    , intermediateTexture_(nullptr) // Initialize to nullptr
{
}



ReloadableScrollingText::~ReloadableScrollingText() {
    if (intermediateTexture_) {
        SDL_DestroyTexture(intermediateTexture_);
        intermediateTexture_ = nullptr;
    }
}

bool ReloadableScrollingText::loadFileText(const std::string& filePath) {
    std::string absolutePath = Utils::combinePath(Configuration::absolutePath, filePath);
    std::filesystem::path file(absolutePath);
    std::filesystem::file_time_type currentWriteTime;

    // Lambda to round the file time to the nearest second
    auto roundToNearestSecond = [](std::filesystem::file_time_type ftt) {
        return std::chrono::time_point_cast<std::chrono::seconds>(ftt);
        };

    try {
        currentWriteTime = std::filesystem::last_write_time(file);
        currentWriteTime = roundToNearestSecond(currentWriteTime);  // Round to nearest second
    }
    catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR("ReloadableScrollingText", "Failed to retrieve file modification time: " + std::string(e.what()));
        return false;  // Return false if there is an error
    }

    // Check if the file has been modified since the last read
    if (currentWriteTime == lastWriteTime_ && !text_.empty()) {
        // No change in file, skip update
        return false;  // File has not changed
    }

    // Store the current modification time
    lastWriteTime_ = currentWriteTime;

    // Reload the text from the file
    std::ifstream fileStream(absolutePath);
    if (!fileStream.is_open()) {
        LOG_ERROR("ReloadableScrollingText", "Failed to open file: " + absolutePath);
        return false;  // Return false if the file could not be opened
    }

    std::string line;
    text_.clear();  // Clear previous content

    while (std::getline(fileStream, line)) {
        if (direction_ == "horizontal" && !text_.empty()) {
            line = " " + line;  // Add space between lines for horizontal scrolling
        }

        // Apply text formatting (uppercase/lowercase)
        if (textFormat_ == "uppercase" || textFormat_ == "lowercase") {
            // Only perform transformations, don't modify text_
            if (textFormat_ == "uppercase") {
                std::transform(line.begin(), line.end(), line.begin(), ::toupper);
            }
            else {
                std::transform(line.begin(), line.end(), line.begin(), ::tolower);
            }
        }

        text_.push_back(line);  // Add the line to the scrolling text vector
    }

    fileStream.close();

    needsUpdate_ = true;

    return true;  // File was modified, return true
}


bool ReloadableScrollingText::update(float dt) {
    if (waitEndTime_ > 0) {
        waitEndTime_ -= dt;
    } else if (waitStartTime_ > 0) {
        waitStartTime_ -= dt;
    } else {
        if (type_ == "hiscores" && highScoreTable_ && !highScoreTable_->tables.empty()) {
            // Ensure currentTableIndex_ is within bounds
            if (currentTableIndex_ >= highScoreTable_->tables.size()) {
                currentTableIndex_ = 0;
            }

            const HighScoreTable& table = highScoreTable_->tables[currentTableIndex_];

            // Consistent padding calculations as in draw()
            Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
            float scale = baseViewInfo.FontSize / static_cast<float>(font->getHeight());
            float drawableHeight = static_cast<float>(font->getAscent() + font->getDescent());
            int rowPadding = static_cast<int>(baseRowPadding_ * drawableHeight * scale);
            int totalTableHeight = static_cast<int>((drawableHeight * scale + rowPadding) * (table.rows.size() + 1));

            bool needsScrolling = (totalTableHeight > baseViewInfo.Height);

            if (needsScrolling) {
                currentPosition_ += scrollingSpeed_ * dt;
            } else {
                currentPosition_ = 0.0f;
            }

            if (highScoreTable_->tables.size() > 1) {
                if (needsScrolling) {
                    currentTableDisplayTime_ = totalTableHeight / scrollingSpeed_;
                } else {
                    currentTableDisplayTime_ = displayTime_;
                }

                tableDisplayTimer_ += dt;
                if (tableDisplayTimer_ >= currentTableDisplayTime_) {
                    tableDisplayTimer_ = 0.0f;
                    currentPosition_ = 0.0f;
                    currentTableIndex_ = (currentTableIndex_ + 1) % highScoreTable_->tables.size();
                }
            }
        } else {
            // Non-hiscore or empty highScoreTable_: use default scrolling behavior
            if (direction_ == "horizontal") {
                currentPosition_ += scrollingSpeed_ * dt;
                if (startPosition_ == 0.0f && textWidth_ <= baseViewInfo.Width) {
                    currentPosition_ = 0.0f;
                }
            } else if (direction_ == "vertical") {
                currentPosition_ += scrollingSpeed_ * dt;
            }
        }
    }

    // Reload text if necessary
    if (type_ == "file") {
        reloadTexture();
    } else if (newItemSelected || (newScrollItemSelected && getMenuScrollReload())) {
        currentTableIndex_ = 0;
        tableDisplayTimer_ = 0.0f; // Reset timer for the new game's table
        currentPosition_ = 0.0f;   // Reset scroll position
        reloadTexture();
        newItemSelected = false;
    }

    return Component::update(dt);
}

void ReloadableScrollingText::allocateGraphicsMemory( )
{
    Component::allocateGraphicsMemory( );
    reloadTexture( );
}


void ReloadableScrollingText::freeGraphicsMemory( )
{
    Component::freeGraphicsMemory( );
    text_.clear( );
}


void ReloadableScrollingText::deInitializeFonts( )
{
    fontInst_->deInitialize( );
}


void ReloadableScrollingText::initializeFonts( )
{
    fontInst_->initialize( );
}


void ReloadableScrollingText::reloadTexture(bool resetScroll) {
    // If the type is "file", check if the file has changed
    if (type_ == "file" && !location_.empty()) {
        bool fileChanged = loadFileText(location_);  // Load text and check if file changed

        // Reset the scroll only if the file has changed
        if (fileChanged) {
            resetScroll = true;
        }
        else {
            resetScroll = false;
        }
    }

    if (resetScroll) {
        // Reset scroll position
        if (direction_ == "horizontal") {
            currentPosition_ = -startPosition_;
        }
        else if (direction_ == "vertical") {
            currentPosition_ = -startPosition_;
        }

        // Reset start and end times to ensure proper wait timing after reload
        waitStartTime_ = startTime_;
        waitEndTime_ = 0.0f;  // Reset to zero when scroll restarts
    }

    text_.clear();

    // Load the appropriate text content
    if (type_ == "file" && !location_.empty()) {
        loadFileText(location_);  // Load the text from the file, but don't reset scroll
        return;  // Since it's file-based, just return after loading the text
    }

    Item *selectedItem = page.getSelectedItem( displayOffset_ );
    if (!selectedItem) {
        return;
    }

    config_.getProperty( "currentCollection", currentCollection_ );

    // build clone list
    std::vector<std::string> names;

    names.push_back( selectedItem->name );
    names.push_back( selectedItem->fullTitle );

    if (selectedItem->cloneof.length( ) > 0) {
        names.push_back( selectedItem->cloneof );
    }

    // Check for corresponding .txt files
    for (unsigned int n = 0; n < names.size( ) && text_.empty( ); ++n) {

        std::string basename = names[n];

        Utils::replaceSlashesWithUnderscores( basename );

        if (systemMode_) {

            // check the master collection for the system artifact 
            loadText( collectionName, type_, type_, "", true );

            // check collection for the system artifact
            if (text_.empty( )) {
              loadText( selectedItem->collectionInfo->name, type_, type_, "", true );
            }

        }
        else {
            // are we looking at a leaf or a submenu
            if (selectedItem->leaf) // item is a leaf
            {

              // check the master collection for the artifact 
              loadText( collectionName, type_, basename, "", false );

              // check the collection for the artifact
              if (text_.empty( )) {
                loadText( selectedItem->collectionInfo->name, type_, basename, "", false );
              }
            }
            else // item is a submenu
            {

                // check the master collection for the artifact
                loadText( collectionName, type_, basename, "", false );

                // check the collection for the artifact
                if (text_.empty( )) {
                loadText( selectedItem->collectionInfo->name, type_, basename, "", false );
                }

                // check the submenu collection for the system artifact
                if (text_.empty( )) {
                loadText( selectedItem->name, type_, type_, "", true );
                }
            }
        }
    }

    // Check for text in the roms directory
    if ( text_.empty( ))
        loadText( selectedItem->filepath, type_, type_, selectedItem->filepath, false );

    // Check for supported fields if text is still empty
    if (text_.empty( )) {
        std::stringstream ss;
        std::string text = "";
        
        if (type_ == "hiscores") {
            Item* selectedItem = page.getSelectedItem(displayOffset_);
            if (selectedItem) {
                // Get high score table for the selected game
                highScoreTable_ = HiScores::getInstance().getHighScoreTable(selectedItem->name);

                // If no high score table, show a default message
                //if (!highScoreTable_) {
                //    text_.push_back("No high scores available.");
                //}
            }
        }
        
        if (type_ == "numberButtons") {
            text = selectedItem->numberButtons;
        }
        else if (type_ == "numberPlayers") {
            text = selectedItem->numberPlayers;
        }
        else if (type_ == "ctrlType") {
            text = selectedItem->ctrlType;
        }
        else if (type_ == "numberJoyWays") {
            text = selectedItem->joyWays;
        }
        else if (type_ == "rating") {
            text = selectedItem->rating;
        }
        else if (type_ == "score") {
            text = selectedItem->score;
        }
        else if (type_ == "year") {
            if (selectedItem->leaf) // item is a leaf
              text = selectedItem->year;
            else // item is a collection
              (void)config_.getProperty("collections." + selectedItem->name + ".year", text );
        }
        else if (type_ == "title") {
            text = selectedItem->title;
        }
        else if(type_ == "developer") {
            text = selectedItem->developer;
            // Overwrite in case developer has not been specified
            if (text == "") {
                text = selectedItem->manufacturer;
            }
        }
        else if (type_ == "manufacturer") {
            if (selectedItem->leaf) // item is a leaf
              text = selectedItem->manufacturer;
            else // item is a collection
              (void)config_.getProperty("collections." + selectedItem->name + ".manufacturer", text );
        }
        else if (type_ == "genre") {
            if (selectedItem->leaf) // item is a leaf
              text = selectedItem->genre;
            else // item is a collection
              (void)config_.getProperty("collections." + selectedItem->name + ".genre", text );
        }
        else if (type_.rfind( "playlist", 0 ) == 0) {
            text = playlistName;
        }
        else if (type_ == "firstLetter") {
          text = selectedItem->fullTitle.at(0);
        }
        else if (type_ == "collectionName") {
            text = page.getCollectionName();
        }
        else if (type_ == "collectionSize") {
            if (page.getCollectionSize() == 0) {
                ss << singlePrefix_ << page.getCollectionSize() << pluralPostfix_;
            }
            else if (page.getCollectionSize() == 1) {
                ss << singlePrefix_ << page.getCollectionSize() << singlePostfix_;
            }
            else {
                ss << pluralPrefix_ << page.getCollectionSize() << pluralPostfix_;
            }
        }
        else if (type_ == "collectionIndex") {
            if (page.getSelectedIndex() == 0) {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << pluralPostfix_;
            }
            else if (page.getSelectedIndex() == 1) {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << singlePostfix_;
            }
            else {
                ss << pluralPrefix_ << (page.getSelectedIndex()+1) << pluralPostfix_;
            }
        }
        else if (type_ == "collectionIndexSize") {
            if (page.getSelectedIndex() == 0) {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << "/" << page.getCollectionSize() << pluralPostfix_;
            }
            else if (page.getSelectedIndex() == 1) {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << "/" << page.getCollectionSize() << singlePostfix_;
            }
            else {
                ss << pluralPrefix_ << (page.getSelectedIndex()+1) << "/" << page.getCollectionSize() << pluralPostfix_;
            }
        }
        else if (!selectedItem->leaf) // item is not a leaf
        {
            (void)config_.getProperty("collections." + selectedItem->name + "." + type_, text );
        }

        if (text == "0") {
            text = singlePrefix_ + text + pluralPostfix_;
        }
        else if (text == "1") {
            text = singlePrefix_ + text + singlePostfix_;
        }
        else if (text != "") {
            text = pluralPrefix_ + text + pluralPostfix_;
        }

        if (!text.empty()) {
            // Apply text formatting (uppercase/lowercase) safely
            if (textFormat_ == "uppercase" || textFormat_ == "lowercase") {
                if (textFormat_ == "uppercase") {
                    std::transform(text.begin(), text.end(), text.begin(), ::toupper);
                }
                else {
                    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
                }
            }
            ss << text;
            text_.push_back(ss.str());
        }
    }
    needsUpdate_ = true;
}


void ReloadableScrollingText::loadText( std::string collection, std::string type, std::string basename, std::string filepath, bool systemMode )
{

    std::string textPath = "";

    // check the system folder
    if (layoutMode_) {
        // check if collection's assets are in a different theme
        std::string layoutName;
        config_.getProperty("collections." + collection + ".layout", layoutName);
        if (layoutName == "") {
            config_.getProperty(OPTION_LAYOUT, layoutName);
        }
        textPath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", collection);
        if (systemMode)
            textPath = Utils::combinePath(textPath, "system_artwork");
        else
            textPath = Utils::combinePath(textPath, "medium_artwork", type);
    }
    else {
        config_.getMediaPropertyAbsolutePath( collection, type, systemMode, textPath );
    }
    if ( filepath != "" )
        textPath = filepath;

    textPath = Utils::combinePath( textPath, basename );

    textPath += ".txt";

    std::ifstream includeStream( textPath.c_str( ) );

    if (!includeStream.good( )) {
        return;
    }

    std::string line; 

    while(std::getline(includeStream, line)) {

        // In horizontal scrolling direction, add a space before every line except the first.
        if (direction_ == "horizontal" && !text_.empty( )) {
            line = " " + line;
        }

        // Reformat lines to uppercase or lowercase
        if (textFormat_ == "uppercase") {
            std::transform(line.begin(), line.end(), line.begin(), ::toupper);
        }
        if (textFormat_ == "lowercase") {
            std::transform(line.begin(), line.end(), line.begin(), ::tolower);
        }

        text_.push_back( line );

    }

    return;

}

void ReloadableScrollingText::draw() {
    Component::draw();

    // Early exit conditions
    if ((text_.empty() && !(type_ == "hiscores" && highScoreTable_ && !highScoreTable_->tables.empty())) ||
        waitEndTime_ > 0.0f ||
        baseViewInfo.Alpha <= 0.0f) {
        return;
    }

    // Retrieve font and texture
    Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
    SDL_Texture* texture = font ? font->getTexture() : nullptr;
    if (!texture) {
        std::cerr << "Error: Font texture is null." << std::endl;
        return;
    }

    // Retrieve renderer
    SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);
    if (!renderer) {
        std::cerr << "Error: Unable to retrieve SDL_Renderer." << std::endl;
        return;
    }

    // Calculate scaling and positioning
    float scale = baseViewInfo.FontSize / static_cast<float>(font->getHeight());
    float xOrigin = baseViewInfo.XRelativeToOrigin();
    float yOrigin = baseViewInfo.YRelativeToOrigin();
    float imageMaxWidth = (baseViewInfo.Width < baseViewInfo.MaxWidth && baseViewInfo.Width > 0) ? baseViewInfo.Width : baseViewInfo.MaxWidth;
    float imageMaxHeight = (baseViewInfo.Height < baseViewInfo.MaxHeight && baseViewInfo.Height > 0) ? baseViewInfo.Height : baseViewInfo.MaxHeight;
    float drawableHeight = static_cast<float>(font->getAscent() + font->getDescent());
    int paddingBetweenColumns = static_cast<int>(baseColumnPadding_ * drawableHeight * scale);
    int rowPadding = static_cast<int>(baseRowPadding_ * drawableHeight * scale);

    // Set clipping rectangle
    SDL_Rect clipRect = { static_cast<int>(xOrigin), static_cast<int>(yOrigin), static_cast<int>(imageMaxWidth), static_cast<int>(imageMaxHeight) };
    SDL_RenderSetClipRect(renderer, &clipRect);

    float scrollOffset = currentPosition_;

    if (type_ == "hiscores" && highScoreTable_ && !highScoreTable_->tables.empty()) {
        // ======== Start of "hiscores" Rendering with Intermediate Texture ========

        // Step 1: Save the current render target
        SDL_Texture* originalTarget = SDL_GetRenderTarget(renderer);
        if (!originalTarget) {
            std::cerr << "Error: Unable to get current render target." << std::endl;
            return;
        }

        // Create intermediate texture
        if (!createIntermediateTexture(renderer, static_cast<int>(imageMaxWidth), static_cast<int>(imageMaxHeight))) {
            LOG_ERROR("ReloadableScrollingText", "Failed to create intermediate texture in allocateGraphicsMemory.");
        }

        // Step 3: Set the intermediate texture as the render target
        SDL_SetRenderTarget(renderer, intermediateTexture_);

        // Step 4: Clear the intermediate texture (transparent background)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Assuming transparent background
        SDL_RenderClear(renderer);

        // Optional: Reapply the clipping rectangle on the intermediate texture
        SDL_RenderSetClipRect(renderer, &clipRect);

        // Retrieve the current high score table
        const HighScoreTable& table = highScoreTable_->tables[currentTableIndex_];

        // Calculate column widths
        std::vector<int> columnWidths(table.columns.size(), 0);
        for (size_t i = 0; i < table.columns.size(); ++i) {
            int headerWidth = static_cast<int>(font->getWidth(table.columns[i]) * scale);
            columnWidths[i] = std::max(columnWidths[i], headerWidth);

            for (const auto& row : table.rows) {
                if (i < row.size()) {
                    int cellWidth = static_cast<int>(font->getWidth(row[i]) * scale);
                    columnWidths[i] = std::max(columnWidths[i], cellWidth);
                }
            }
        }

        // Calculate total table width
        float totalTableWidth = 0;
        for (size_t i = 0; i < columnWidths.size(); ++i) {
            totalTableWidth += columnWidths[i] + (i < columnWidths.size() - 1 ? paddingBetweenColumns : 0);
        }

        bool hasTitle = !table.id.empty();
        float adjustedYOrigin = yOrigin;

        // Draw the title if present
        if (hasTitle) {
            const std::string& title = table.id;
            float titleX = xOrigin + (totalTableWidth - static_cast<float>(font->getWidth(title)) * scale) / 2.0f;
            float titleY = adjustedYOrigin;

            for (char c : title) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph)) {
                    SDL_Rect srcRect = glyph.rect;
                    SDL_FRect destRect = { 
                        titleX, 
                        titleY, 
                        glyph.rect.w * scale, 
                        glyph.rect.h * scale
                    };
                    // Use standard SDL_RenderCopy to draw onto the intermediate texture
                    SDL_RenderCopyF(renderer, texture, &srcRect, &destRect);
                    titleX += static_cast<float>(glyph.advance) * scale;
                }
            }
            adjustedYOrigin += drawableHeight * scale + rowPadding;
        }

        // Draw the headers
        float headerY = adjustedYOrigin;
        int xPos = static_cast<int>(xOrigin);
        for (size_t col = 0; col < table.columns.size(); ++col) {
            if (col >= columnWidths.size()) continue;

            const std::string& header = table.columns[col];
            int xAligned = xPos + (columnWidths[col] - static_cast<int>(font->getWidth(header) * scale)) / 2;
            float charX = static_cast<float>(xAligned);
            float charY = headerY;

            for (char c : header) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph)) {
                    SDL_Rect srcRect = glyph.rect;
                    SDL_FRect destRect = { 
                        charX, 
                        charY, 
                        glyph.rect.w * scale, 
                        glyph.rect.h * scale 
                    };
                    SDL_RenderCopyF(renderer, texture, &srcRect, &destRect);
                    charX += glyph.advance * scale;
                }
            }
            xPos += columnWidths[col] + paddingBetweenColumns;
        }

        adjustedYOrigin += drawableHeight * scale + rowPadding;

        // Calculate the starting position for the scrolling area precisely below the header row
        SDL_Rect scrollClipRect = {
            static_cast<int>(xOrigin),
            static_cast<int>(headerY + drawableHeight * scale + rowPadding),  // Start directly below the header row
            static_cast<int>(imageMaxWidth),
            static_cast<int>(std::max(imageMaxHeight - (headerY + drawableHeight * scale + rowPadding - yOrigin), 0.0f))
        };
        SDL_RenderSetClipRect(renderer, &scrollClipRect);

        float currentY = adjustedYOrigin;
        float adjustedScrollY = currentY - scrollOffset;
        for (const auto& row : table.rows) {
            xPos = static_cast<int>(xOrigin);

            bool skipRow = (adjustedScrollY + drawableHeight * scale < yOrigin || adjustedScrollY > yOrigin + imageMaxHeight);

            if (!skipRow) {
                float currentRowX = static_cast<float>(xPos);
                float currentRowY = adjustedScrollY;
                for (size_t col = 0; col < table.columns.size(); ++col) {
                    if (col >= row.size()) continue;

                    std::string cell = row[col];
                    int xAligned = xPos + (columnWidths[col] - static_cast<int>(font->getWidth(cell) * scale)) / 2;
                    float charX = static_cast<float>(xAligned);
                    float charY = currentRowY;

                    for (char c : cell) {
                        Font::GlyphInfo glyph;
                        if (font->getRect(c, glyph)) {
                            SDL_Rect srcRect = glyph.rect;
                            SDL_FRect destRect = { 
                                charX, 
                                charY, 
                                glyph.rect.w * scale, 
                                glyph.rect.h * scale 
                            };
                            SDL_RenderCopyF(renderer, texture, &srcRect, &destRect);
                            charX += glyph.advance * scale;
                        }
                    }
                    xPos += columnWidths[col] + paddingBetweenColumns;
                }
            }
            adjustedScrollY += drawableHeight * scale + rowPadding;
        }

        // Reset clipping rectangle after drawing
        SDL_RenderSetClipRect(renderer, nullptr);

        // Step 5: Restore the original render target
        SDL_SetRenderTarget(renderer, originalTarget);

        // Step 6: Define the destination rectangle where the intermediate texture should be drawn
        SDL_FRect destRect = {
            xOrigin,
            yOrigin,
            imageMaxWidth,
            imageMaxHeight
        };

        // Step 7: Render the intermediate texture to the original render target using SDL::renderCopy

        SDL::renderCopyF(intermediateTexture_, baseViewInfo.Alpha, nullptr, &destRect, baseViewInfo,
            page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
            page.getLayoutHeightByMonitor(baseViewInfo.Monitor));

        // ======== End of "hiscores" Rendering with Intermediate Texture ========
    }
    else {
        // ======== Non-"hiscores" Rendering: Original Scrolling Behavior ========

        float scrollPosition = currentPosition_;

        if (direction_ == "horizontal") {
            // Adjust for negative scroll position
            if (scrollPosition < 0.0f) {
                scrollPosition = -scrollPosition;
            }

            // Do not scroll if the text fits within the available width
            if (textWidth_ <= imageMaxWidth && startPosition_ == 0.0f) {
                currentPosition_ = 0.0f;
                waitStartTime_ = 0.0f;
                waitEndTime_ = 0.0f;
            }

            for (const auto& glyph : cachedGlyphs_) {
                SDL_Rect destRect = { 
                    static_cast<int>(xOrigin + glyph.destRect.x - scrollPosition), 
                    static_cast<int>(yOrigin + glyph.destRect.y), 
                    static_cast<int>(glyph.destRect.w), 
                    static_cast<int>(glyph.destRect.h) 
                };

                // Skip glyphs outside the visible area
                if ((destRect.x + destRect.w) <= xOrigin || destRect.x >= (xOrigin + imageMaxWidth)) {
                    continue;
                }

                // Adjust destRect and srcRect for clipping at edges
                SDL_Rect srcRect = glyph.sourceRect;

                // Left clipping
                if (destRect.x < xOrigin) {
                    int clipAmount = static_cast<int>(xOrigin - destRect.x);
                    destRect.x = static_cast<int>(xOrigin);
                    destRect.w -= clipAmount;
                    srcRect.x += static_cast<int>(clipAmount / scale);
                    srcRect.w -= static_cast<int>(clipAmount / scale);
                }

                // Right clipping
                if ((destRect.x + destRect.w) > (xOrigin + imageMaxWidth)) {
                    int clipAmount = static_cast<int>((destRect.x + destRect.w) - (xOrigin + imageMaxWidth));
                    destRect.w -= clipAmount;
                    srcRect.w -= static_cast<int>(clipAmount / scale);
                }

                if (destRect.w <= 0) {
                    continue;
                }

                SDL::renderCopy(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
                    page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
                    page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
            }

            // Update scrolling position
            if (currentPosition_ > textWidth_) {
                waitStartTime_ = startTime_;
                waitEndTime_ = endTime_;
                currentPosition_ = -startPosition_;
            }
        }
        else if (direction_ == "vertical") {
            // Adjust for negative scroll position
            if (scrollPosition < 0.0f) {
                scrollPosition = -scrollPosition;
            }

            // Do not scroll if the text fits within the available height
            if (textHeight_ <= imageMaxHeight && startPosition_ == 0.0f) {
                currentPosition_ = 0.0f;
                waitStartTime_ = 0.0f;
                waitEndTime_ = 0.0f;
            }

            for (const auto& glyph : cachedGlyphs_) {
                SDL_Rect destRect = { 
                    static_cast<int>(xOrigin + glyph.destRect.x), 
                    static_cast<int>(yOrigin + glyph.destRect.y - scrollPosition), 
                    glyph.destRect.w, 
                    glyph.destRect.h 
                };

                // Skip glyphs outside the visible area
                if ((destRect.y + destRect.h) <= yOrigin || destRect.y >= (yOrigin + imageMaxHeight)) {
                    continue;
                }

                // Adjust destRect and srcRect for clipping at edges
                SDL_Rect srcRect = glyph.sourceRect;

                // Top clipping
                if (destRect.y < yOrigin) {
                    int clipAmount = static_cast<int>(yOrigin - destRect.y);
                    destRect.y = static_cast<int>(yOrigin);
                    destRect.h -= clipAmount;
                    srcRect.y += static_cast<int>(clipAmount / scale);
                    srcRect.h -= static_cast<int>(clipAmount / scale);
                }

                // Bottom clipping
                if ((destRect.y + destRect.h) > (yOrigin + imageMaxHeight)) {
                    int clipAmount = static_cast<int>((destRect.y + destRect.h) - (yOrigin + imageMaxHeight));
                    destRect.h -= clipAmount;
                    srcRect.h -= static_cast<int>(clipAmount / scale);
                }

                if (destRect.h <= 0) {
                    continue;
                }

                SDL::renderCopy(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
                    page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
                    page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
            }

            // Update scrolling position
            if (currentPosition_ > textHeight_) {
                waitStartTime_ = startTime_;
                waitEndTime_ = endTime_;
                currentPosition_ = -startPosition_;
            }
        }
    }
}

void ReloadableScrollingText::updateGlyphCache() {
    cachedGlyphs_.clear();
    textWidth_ = 0.0f;
    textHeight_ = 0.0f;

    Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
    if (!font) {
        return;
    }

    float scale = baseViewInfo.FontSize / font->getHeight();
    float imageMaxWidth = (baseViewInfo.Width < baseViewInfo.MaxWidth && baseViewInfo.Width > 0)
        ? baseViewInfo.Width : baseViewInfo.MaxWidth;
    float imageMaxHeight = (baseViewInfo.Height < baseViewInfo.MaxHeight && baseViewInfo.Height > 0)
        ? baseViewInfo.Height : baseViewInfo.MaxHeight;

    // Update last known values
    lastScale_ = scale;
    lastImageMaxWidth_ = imageMaxWidth;
    lastImageMaxHeight_ = imageMaxHeight;

    // Position trackers
    float xPos = 0.0f;
    float yPos = 0.0f;

    if (direction_ == "horizontal") {
        for (const auto& line : text_) {
            for (char c : line) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph) && glyph.rect.h > 0) {
                    CachedGlyph cachedGlyph;
                    cachedGlyph.sourceRect = glyph.rect;
                    cachedGlyph.destRect.x = static_cast<int>(xPos);
                    cachedGlyph.destRect.y = 0;  // Adjusted later
                    cachedGlyph.destRect.w = static_cast<int>(glyph.rect.w * scale);
                    cachedGlyph.destRect.h = static_cast<int>(glyph.rect.h * scale);
                    cachedGlyph.advance = glyph.advance * scale;

                    cachedGlyphs_.push_back(cachedGlyph);

                    xPos += glyph.advance * scale;
                }
            }
        }
        textWidth_ = xPos;
    }
    else if (direction_ == "vertical") {
        // For vertical scrolling, we need to handle word wrapping based on imageMaxWidth
        std::vector<std::string> wrappedLines;

        // First, wrap the text to fit within imageMaxWidth
        for (const auto& line : text_) {
            std::istringstream iss(line);
            std::string word;
            std::string currentLine;
            float currentLineWidth = 0.0f;

            while (iss >> word) {
                // Calculate the width of the word
                float wordWidth = 0.0f;
                for (char c : word) {
                    Font::GlyphInfo glyph;
                    if (font->getRect(c, glyph)) {
                        wordWidth += glyph.advance * scale;
                    }
                }
                // Add space width if this is not the first word on the line
                if (!currentLine.empty()) {
                    Font::GlyphInfo spaceGlyph;
                    if (font->getRect(' ', spaceGlyph)) {
                        wordWidth += spaceGlyph.advance * scale;
                    }
                }
                // Check if the word fits on the current line
                if (currentLineWidth + wordWidth > imageMaxWidth && !currentLine.empty()) {
                    // Line is full, add it to wrappedLines
                    wrappedLines.push_back(currentLine);
                    currentLine = word;
                    currentLineWidth = wordWidth;
                }
                else {
                    // Add the word to the current line
                    if (!currentLine.empty()) {
                        currentLine += ' ';
                    }
                    currentLine += word;
                    currentLineWidth += wordWidth;
                }
            }
            // Add any remaining text in currentLine
            if (!currentLine.empty()) {
                wrappedLines.push_back(currentLine);
            }
        }

        // Now, cache the glyphs for the wrapped lines
        for (const auto& line : wrappedLines) {
            xPos = 0.0f;

            // Calculate line width for alignment
            float lineWidth = 0.0f;
            for (char c : line) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph)) {
                    lineWidth += glyph.advance * scale;
                }
            }

            // Adjust xPos based on alignment
            if (alignment_ == "right") {
                xPos = imageMaxWidth - lineWidth;
            }
            else if (alignment_ == "centered") {
                xPos = (imageMaxWidth - lineWidth) / 2.0f;
            }

            for (char c : line) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph) && glyph.rect.h > 0) {
                    CachedGlyph cachedGlyph;
                    cachedGlyph.sourceRect = glyph.rect;
                    cachedGlyph.destRect.x = static_cast<int>(xPos);
                    cachedGlyph.destRect.y = static_cast<int>(yPos);
                    cachedGlyph.destRect.w = static_cast<int>(glyph.rect.w * scale);
                    cachedGlyph.destRect.h = static_cast<int>(glyph.rect.h * scale);
                    cachedGlyph.advance = glyph.advance * scale;

                    cachedGlyphs_.push_back(cachedGlyph);

                    xPos += glyph.advance * scale;
                }
            }

            yPos += font->getHeight() * scale;
        }
        textHeight_ = yPos;
    }

    needsUpdate_ = false;
}

bool ReloadableScrollingText::createIntermediateTexture(SDL_Renderer* renderer, int width, int height) {
    // Destroy existing texture if it exists
    if (intermediateTexture_) {
        SDL_DestroyTexture(intermediateTexture_);
        intermediateTexture_ = nullptr;
    }

    // Create the intermediate texture with alpha support
    intermediateTexture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!intermediateTexture_) {
        LOG_ERROR("ReloadableScrollingText", "Failed to create intermediate texture: " + std::string(SDL_GetError()));
        return false;
    }

    // Set the blend mode to allow transparency
    if (SDL_SetTextureBlendMode(intermediateTexture_, SDL_BLENDMODE_BLEND) != 0) {
        LOG_ERROR("ReloadableScrollingText", "Failed to set blend mode for intermediate texture: " + std::string(SDL_GetError()));
        SDL_DestroyTexture(intermediateTexture_);
        intermediateTexture_ = nullptr;
        return false;
    }

    return true;
}