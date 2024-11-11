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
{
}



ReloadableScrollingText::~ReloadableScrollingText( ) = default;

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

    if ((text_.empty() && !(type_ == "hiscores" && highScoreTable_ && !highScoreTable_->tables.empty())) ||
        waitEndTime_ > 0.0f ||
        baseViewInfo.Alpha <= 0.0f) {
        return;
    }

    Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
    SDL_Texture* texture = font ? font->getTexture() : nullptr;
    if (!texture) {
        std::cerr << "Error: Font texture is null." << std::endl;
        return;
    }

    SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);
    if (!renderer) {
        std::cerr << "Error: Unable to retrieve SDL_Renderer." << std::endl;
        return;
    }

    float scale = baseViewInfo.FontSize / static_cast<float>(font->getHeight());
    float xOrigin = baseViewInfo.XRelativeToOrigin();
    float yOrigin = baseViewInfo.YRelativeToOrigin();
    float imageMaxWidth = (baseViewInfo.Width < baseViewInfo.MaxWidth && baseViewInfo.Width > 0) ? baseViewInfo.Width : baseViewInfo.MaxWidth;
    float imageMaxHeight = (baseViewInfo.Height < baseViewInfo.MaxHeight && baseViewInfo.Height > 0) ? baseViewInfo.Height : baseViewInfo.MaxHeight;
    float drawableHeight = static_cast<float>(font->getAscent() + font->getDescent());
    int paddingBetweenColumns = static_cast<int>(baseColumnPadding_ * drawableHeight * scale);
    int rowPadding = static_cast<int>(baseRowPadding_ * drawableHeight * scale);

    SDL_Rect clipRect = { static_cast<int>(xOrigin), static_cast<int>(yOrigin), static_cast<int>(imageMaxWidth), static_cast<int>(imageMaxHeight) };
    SDL_RenderSetClipRect(renderer, &clipRect);

    float scrollOffset = currentPosition_;

    if (type_ == "hiscores" && highScoreTable_ && !highScoreTable_->tables.empty()) {
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

        int totalTableWidth = 0;
        for (size_t i = 0; i < columnWidths.size(); ++i) {
            totalTableWidth += columnWidths[i] + (i < columnWidths.size() - 1 ? paddingBetweenColumns : 0);
        }

        bool hasTitle = !table.id.empty();
        float adjustedYOrigin = yOrigin;

        if (hasTitle) {
            const std::string& title = table.id;
            float titleX = xOrigin + (totalTableWidth - font->getWidth(title) * scale) / 2.0f;
            float titleY = adjustedYOrigin;

            for (char c : title) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph)) {
                    SDL_Rect srcRect = glyph.rect;
                    SDL_Rect destRect = { 
                        static_cast<int>(titleX), 
                        static_cast<int>(titleY), 
                        static_cast<int>(glyph.rect.w * scale), 
                        static_cast<int>(glyph.rect.h * scale) 
                    };
                    SDL::renderCopy(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
                        page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
                        page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
                    titleX += glyph.advance * scale;
                }
            }
            adjustedYOrigin += drawableHeight * scale + rowPadding;
        }

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
                    SDL_Rect destRect = { 
                        static_cast<int>(charX), 
                        static_cast<int>(charY), 
                        static_cast<int>(glyph.rect.w * scale), 
                        static_cast<int>(glyph.rect.h * scale) 
                    };
                    SDL::renderCopy(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
                        page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
                        page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
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
                            SDL_Rect destRect = { 
                                static_cast<int>(charX), 
                                static_cast<int>(charY), 
                                static_cast<int>(glyph.rect.w * scale), 
                                static_cast<int>(glyph.rect.h * scale) 
                            };
                            SDL::renderCopy(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
                                page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
                                page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
                            charX += glyph.advance * scale;
                        }
                    }
                    xPos += columnWidths[col] + paddingBetweenColumns;
                }
            }
            adjustedScrollY += drawableHeight * scale + rowPadding;
        }
        SDL_RenderSetClipRect(renderer, nullptr);
    }
    else {
        // **Normal Scrolling Text Rendering**

        // Determine if scrolling is necessary
        bool needsScrolling = false;
        float totalTextHeight = 0.0f;
        float totalTextWidth = 0.0f;

        if (direction_ == "horizontal") {
            for (const auto& line : text_) {
                totalTextWidth += font->getWidth(line) * scale;
            }
            needsScrolling = (totalTextWidth > imageMaxWidth);
        }
        else if (direction_ == "vertical") {
            for (const auto& line : text_) {
                totalTextHeight += font->getHeight() * scale;
            }
            needsScrolling = (totalTextHeight > imageMaxHeight);
        }

        // If scrolling is not needed, reset scroll position and render statically
        if (!needsScrolling) {
            currentPosition_ = 0.0f;
            waitStartTime_ = 0.0f;
            waitEndTime_ = 0.0f;
        }

        // Render each line of text
        float currentOffset = needsScrolling ? scrollOffset : 0.0f;
        float currentX = xOrigin;
        float currentY = yOrigin;

        for (const auto& line : text_) {
            SDL_Rect destRect;

            if (direction_ == "horizontal") {
                // Calculate X position with scrolling
                destRect.x = static_cast<int>(currentX - currentOffset);
                destRect.y = static_cast<int>(currentY);
                destRect.w = static_cast<int>(font->getWidth(line) * scale);
                destRect.h = static_cast<int>(font->getHeight() * scale);
            }
            else { // "vertical"
                // Calculate Y position with scrolling
                destRect.x = static_cast<int>(currentX);
                destRect.y = static_cast<int>(currentY + currentOffset);
                destRect.w = static_cast<int>(font->getWidth(line) * scale);
                destRect.h = static_cast<int>(font->getHeight() * scale);
            }

            // Clipping: Skip rendering if the entire line is outside the visible area
            bool isOutside = false;
            if (direction_ == "horizontal") {
                if (destRect.x + destRect.w < xOrigin || destRect.x > xOrigin + imageMaxWidth) {
                    isOutside = true;
                }
            }
            else { // "vertical"
                if (destRect.y + destRect.h < yOrigin || destRect.y > yOrigin + imageMaxHeight) {
                    isOutside = true;
                }
            }

            if (isOutside) {
                // Advance offset based on direction
                if (direction_ == "horizontal") {
                    currentX += font->getWidth(line) * scale;
                }
                else { // "vertical"
                    currentY += font->getHeight() * scale;
                }
                continue;
            }

            // Render each character in the line
            float charX = (direction_ == "horizontal") ? static_cast<float>(destRect.x) : currentX;
            float charY = (direction_ == "vertical") ? static_cast<float>(destRect.y) : currentY;
            for (char c : line) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph)) {
                    SDL_Rect srcRect = glyph.rect;
                    SDL_Rect renderedRect = {
                        static_cast<int>(charX),
                        static_cast<int>(charY),
                        static_cast<int>(glyph.rect.w * scale),
                        static_cast<int>(glyph.rect.h * scale)
                    };

                    // Render the glyph
                    SDL::renderCopy(texture, baseViewInfo.Alpha, &srcRect, &renderedRect, baseViewInfo,
                        page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
                        page.getLayoutHeightByMonitor(baseViewInfo.Monitor));

                    // Advance the X or Y position based on direction
                    if (direction_ == "horizontal") {
                        charX += glyph.advance * scale;
                    }
                    else { // "vertical"
                        // **Correction:** Advance X instead of Y to keep characters horizontally aligned
                        charX += glyph.advance * scale;
                    }
                }
            }

            // Advance the position based on direction
            if (direction_ == "horizontal") {
                currentX += font->getWidth(line) * scale;
            }
            else { // "vertical"
                currentY += font->getHeight() * scale;
            }
        }

        // **Reset Scrolling if the end is reached**
        if (needsScrolling) {
            if (direction_ == "horizontal") {
                if (currentPosition_ > totalTextWidth) {
                    currentPosition_ = -startPosition_;
                }
            }
            else if (direction_ == "vertical") {
                if (currentPosition_ > totalTextHeight) {
                    currentPosition_ = -startPosition_;
                }
            }
        }

        // Disable clipping after rendering
        SDL_RenderSetClipRect(renderer, nullptr);
    }
}

void ReloadableScrollingText::updateGlyphCache() {
    cachedGlyphs_.clear();
    textWidth_ = 0.0f;
    textHeight_ = 0.0f;

    Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
    if (!font) {
        std::cerr << "Error: Font not set." << std::endl;
        return;
    }

    float scale = baseViewInfo.FontSize / static_cast<float>(font->getHeight());

    float imageMaxWidth = (baseViewInfo.Width < baseViewInfo.MaxWidth && baseViewInfo.Width > 0)
        ? baseViewInfo.Width : baseViewInfo.MaxWidth;
    float imageMaxHeight = (baseViewInfo.Height < baseViewInfo.MaxHeight && baseViewInfo.Height > 0)
        ? baseViewInfo.Height : baseViewInfo.MaxHeight;

    lastScale_ = scale;
    lastImageMaxWidth_ = imageMaxWidth;
    lastImageMaxHeight_ = imageMaxHeight;

    float xPos = 0.0f;
    float yPos = 0.0f;

    if (type_ == "hiscores" && highScoreTable_ && !highScoreTable_->tables.empty()) {
        const HighScoreTable& table = highScoreTable_->tables.front();  // Access the first table only

        int paddingBetweenColumns = static_cast<int>(baseColumnPadding_ * font->getHeight() * scale);
        int rowPadding = static_cast<int>(baseRowPadding_ * font->getHeight() * scale);

        // 1. Calculate column widths
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

        // 2. Precompute positions for headers
        float currentY = yPos;
        float currentX = xPos;
        for (size_t col = 0; col < table.columns.size(); ++col) {
            const std::string& header = table.columns[col];
            float headerWidth = font->getWidth(header) * scale;
            float xAligned = currentX + (columnWidths[col] - headerWidth) / 2.0f;

            // Precompute glyphs for the header
            for (char c : header) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph)) {
                    CachedGlyph cachedGlyph;
                    cachedGlyph.sourceRect = glyph.rect;
                    cachedGlyph.destRect = SDL_FRect{ xAligned, currentY, glyph.rect.w * scale, glyph.rect.h * scale };
                    cachedGlyph.advance = glyph.advance * scale;

                    cachedGlyphs_.push_back(cachedGlyph);

                    xAligned += glyph.advance * scale;
                }
            }

            currentX += columnWidths[col] + paddingBetweenColumns;
        }

        // Move to the next row after header
        currentY += font->getHeight() * scale + rowPadding;

        // 3. Precompute positions for each row
        for (const auto& row : table.rows) {
            currentX = xPos;
            for (size_t col = 0; col < table.columns.size(); ++col) {
                std::string cell = (col < row.size()) ? row[col] : "";

                float cellWidth = font->getWidth(cell) * scale;
                float xAligned = currentX + (columnWidths[col] - cellWidth) / 2.0f;

                for (char c : cell) {
                    Font::GlyphInfo glyph;
                    if (font->getRect(c, glyph)) {
                        CachedGlyph cachedGlyph;
                        cachedGlyph.sourceRect = glyph.rect;
                        cachedGlyph.destRect = SDL_FRect{ xAligned, currentY, glyph.rect.w * scale, glyph.rect.h * scale };
                        cachedGlyph.advance = glyph.advance * scale;

                        cachedGlyphs_.push_back(cachedGlyph);

                        xAligned += glyph.advance * scale;
                    }
                }

                currentX += columnWidths[col] + paddingBetweenColumns;
            }

            currentY += font->getHeight() * scale + rowPadding;
        }

        textWidth_ = currentX;
        textHeight_ = currentY;
    }
    else {
        // **Normal Scrolling Text Rendering Caching**

        for (const auto& line : text_) {
            float charX = xPos;
            float charY = yPos;

            for (char c : line) {
                Font::GlyphInfo glyph;
                if (font->getRect(c, glyph)) {
                    CachedGlyph cachedGlyph;
                    cachedGlyph.sourceRect = glyph.rect;
                    cachedGlyph.destRect = SDL_FRect{ charX, charY, glyph.rect.w * scale, glyph.rect.h * scale };
                    cachedGlyph.advance = glyph.advance * scale;

                    cachedGlyphs_.push_back(cachedGlyph);

                    charX += glyph.advance * scale;
                }
            }

            yPos += font->getHeight() * scale;
        }

        textWidth_ = xPos;
        textHeight_ = yPos;
    }

    needsUpdate_ = false;
}
