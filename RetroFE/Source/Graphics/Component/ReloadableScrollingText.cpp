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
#include "../../Utility/Log.h"
#include "../../Utility/Utils.h"
#include "../../SDL.h"
#include "../Font.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <algorithm>

ReloadableScrollingText::ReloadableScrollingText(Configuration& config, bool systemMode, bool layoutMode, bool menuMode, std::string type, std::string textFormat, std::string singlePrefix, std::string singlePostfix, std::string pluralPrefix, std::string pluralPostfix, std::string alignment, Page& p, int displayOffset, Font* font, std::string direction, float scrollingSpeed, float startPosition, float startTime, float endTime, std::string location)
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
    }
    else if (waitStartTime_ > 0) {
        waitStartTime_ -= dt;
    }
    else {
        if (direction_ == "horizontal") {
            currentPosition_ += scrollingSpeed_ * dt;
            if (startPosition_ == 0.0f && textWidth_ <= baseViewInfo.Width) {
                currentPosition_ = 0.0f;
            }
        }
        else if (direction_ == "vertical") {
            currentPosition_ += scrollingSpeed_ * dt;
        }
    }

    // If the type is "file", always reload the text
    if (type_ == "file") {
        reloadTexture();
    }
    // For non-file types, use the default behavior
    else if (newItemSelected || (newScrollItemSelected && getMenuScrollReload())) {
        reloadTexture();  // Reset scroll position as usual for non-file types
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

    if (text_.empty() || waitEndTime_ > 0.0f || baseViewInfo.Alpha <= 0.0f) {
        return;
    }

    Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
    SDL_Texture* texture = font ? font->getTexture() : nullptr;

    if (!texture) {
        return;
    }

    float scale = baseViewInfo.FontSize / font->getHeight();
    float imageMaxWidth = (baseViewInfo.Width < baseViewInfo.MaxWidth && baseViewInfo.Width > 0)
        ? baseViewInfo.Width : baseViewInfo.MaxWidth;
    float imageMaxHeight = (baseViewInfo.Height < baseViewInfo.MaxHeight && baseViewInfo.Height > 0)
        ? baseViewInfo.Height : baseViewInfo.MaxHeight;

    float xOrigin = baseViewInfo.XRelativeToOrigin();
    float yOrigin = baseViewInfo.YRelativeToOrigin();

    // Update glyph cache if needed
    if (needsUpdate_ || lastScale_ != scale || lastImageMaxWidth_ != imageMaxWidth || lastImageMaxHeight_ != imageMaxHeight) {
        updateGlyphCache();
    }

    SDL_FRect destRect;
    destRect.y = yOrigin;

    if (direction_ == "horizontal") {
        float scrollPosition = currentPosition_;

        // Adjust for negative scroll position
        if (scrollPosition < 0.0f) {
            destRect.x = xOrigin - scrollPosition;
        }
        else {
            destRect.x = xOrigin;
        }

        // Do not scroll if the text fits within the available width
        if (textWidth_ <= imageMaxWidth && startPosition_ == 0.0f) {
            currentPosition_ = 0.0f;
            waitStartTime_ = 0.0f;
            waitEndTime_ = 0.0f;
        }

        for (const auto& glyph : cachedGlyphs_) {
            // Calculate the glyph's position with scrolling
            destRect.x = xOrigin + glyph.destRect.x - scrollPosition;
            destRect.y = yOrigin + glyph.destRect.y;
            destRect.w = glyph.destRect.w;
            destRect.h = glyph.destRect.h;

            // Skip glyphs outside the visible area
            if ((destRect.x + destRect.w) <= xOrigin || destRect.x >= (xOrigin + imageMaxWidth)) {
                continue;
            }

            // Adjust destRect and srcRect for clipping at edges
            SDL_Rect srcRect = glyph.sourceRect;

            // Left clipping
            if (destRect.x < xOrigin) {
                float clipAmount = xOrigin - destRect.x;
                destRect.x = xOrigin;
                destRect.w -= clipAmount;
                srcRect.x += static_cast<int>(clipAmount / scale);
                srcRect.w -= static_cast<int>(clipAmount / scale);
            }

            // Right clipping
            if ((destRect.x + destRect.w) > (xOrigin + imageMaxWidth)) {
                float clipAmount = (destRect.x + destRect.w) - (xOrigin + imageMaxWidth);
                destRect.w -= clipAmount;
                srcRect.w -= static_cast<int>(clipAmount / scale);
            }

            if (destRect.w <= 0) {
                continue;
            }

            SDL::renderCopyF(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
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
        float scrollPosition = currentPosition_;

        // Adjust for negative scroll position
        if (scrollPosition < 0.0f) {
            destRect.y = yOrigin - scrollPosition;
        }
        else {
            destRect.y = yOrigin;
        }

        // Do not scroll if the text fits within the available height
        if (textHeight_ <= imageMaxHeight && startPosition_ == 0.0f) {
            currentPosition_ = 0.0f;
            waitStartTime_ = 0.0f;
            waitEndTime_ = 0.0f;
        }

        for (const auto& glyph : cachedGlyphs_) {
            // Calculate the glyph's position with scrolling
            destRect.x = xOrigin + glyph.destRect.x;
            destRect.y = yOrigin + glyph.destRect.y - scrollPosition;
            destRect.w = glyph.destRect.w;
            destRect.h = glyph.destRect.h;

            // Skip glyphs outside the visible area
            if ((destRect.y + destRect.h) <= yOrigin || destRect.y >= (yOrigin + imageMaxHeight)) {
                continue;
            }

            // Adjust destRect and srcRect for clipping at edges
            SDL_Rect srcRect = glyph.sourceRect;

            // Top clipping
            if (destRect.y < yOrigin) {
                float clipAmount = yOrigin - destRect.y;
                destRect.y = yOrigin;
                destRect.h -= clipAmount;
                srcRect.y += static_cast<int>(clipAmount / scale);
                srcRect.h -= static_cast<int>(clipAmount / scale);
            }

            // Bottom clipping
            if ((destRect.y + destRect.h) > (yOrigin + imageMaxHeight)) {
                float clipAmount = (destRect.y + destRect.h) - (yOrigin + imageMaxHeight);
                destRect.h -= clipAmount;
                srcRect.h -= static_cast<int>(clipAmount / scale);
            }

            if (destRect.h <= 0) {
                continue;
            }

            SDL::renderCopyF(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
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
                    cachedGlyph.destRect.x = xPos;
                    cachedGlyph.destRect.y = 0;  // Adjusted later
                    cachedGlyph.destRect.w = static_cast<float>(glyph.rect.w) * scale;
                    cachedGlyph.destRect.h = static_cast<float>(glyph.rect.h) * scale;
                    cachedGlyph.advance = static_cast<float>(glyph.advance) * scale;

                    cachedGlyphs_.push_back(cachedGlyph);

                    xPos += static_cast<float>(glyph.advance) * scale;
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
                        wordWidth += static_cast<float>(glyph.advance) * scale;
                    }
                }
                // Add space width if this is not the first word on the line
                if (!currentLine.empty()) {
                    Font::GlyphInfo spaceGlyph;
                    if (font->getRect(' ', spaceGlyph)) {
                        wordWidth += static_cast<float>(spaceGlyph.advance) * scale;
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
                    lineWidth += static_cast<float>(glyph.advance) * scale;
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
                    cachedGlyph.destRect.x = xPos;
                    cachedGlyph.destRect.y = yPos;
                    cachedGlyph.destRect.w = static_cast<float>(glyph.rect.w) * scale;
                    cachedGlyph.destRect.h = static_cast<float>(glyph.rect.h) * scale;
                    cachedGlyph.advance = static_cast<float>(glyph.advance) * scale;

                    cachedGlyphs_.push_back(cachedGlyph);

                    xPos += static_cast<float>(glyph.advance) * scale;
                }
            }

            yPos += static_cast<float>(font->getHeight()) * scale;
        }
        textHeight_ = yPos;
    }

    needsUpdate_ = false;
}