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


#include "Text.h"
#include "../../Utility/Log.h"
#include "../../SDL.h"
#include "../Font.h"
#include <sstream>


Text::Text( const std::string& text, Page &p, Font *font, int monitor )
    : Component(p)
    , textData_(text)
    , fontInst_(font)
    , cachedPositions_()
    , needsUpdate_(true)
{
    baseViewInfo.Monitor = monitor;
    baseViewInfo.Layout = page.getCurrentLayout();
}

Text::~Text() { Component::freeGraphicsMemory(); }



void Text::deInitializeFonts( )
{
    fontInst_->deInitialize( );
}

void Text::initializeFonts( )
{
    fontInst_->initialize( );
}

void Text::setText(const std::string& text, int id) {
    if (getId() == id && textData_ != text) {
        textData_ = text;
        needsUpdate_ = true;
    }
}

void Text::draw() {
    Component::draw();

    Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
    SDL_Texture* texture = font->getTexture();

    if (!texture || textData_.empty()) {
        return;
    }

    float imageHeight = static_cast<float>(font->getHeight());
    float scale = baseViewInfo.FontSize / imageHeight;
    float imageMaxWidth = (baseViewInfo.Width < baseViewInfo.MaxWidth && baseViewInfo.Width > 0)
        ? baseViewInfo.Width
        : baseViewInfo.MaxWidth;

    // Recalculate and cache positions if necessary
    if (needsUpdate_ || lastScale_ != scale || lastMaxWidth_ != imageMaxWidth) {
        updateGlyphPositions(font, scale, imageMaxWidth);
        needsUpdate_ = false;
        lastScale_ = scale;
        lastMaxWidth_ = imageMaxWidth;
    }

    float oldWidth = baseViewInfo.Width;
    float oldHeight = baseViewInfo.Height;
    float oldImageWidth = baseViewInfo.ImageWidth;
    float oldImageHeight = baseViewInfo.ImageHeight;

    baseViewInfo.Width = cachedWidth_ * scale;
    baseViewInfo.Height = baseViewInfo.FontSize;
    baseViewInfo.ImageWidth = cachedWidth_;
    baseViewInfo.ImageHeight = imageHeight;

    float xOrigin = baseViewInfo.XRelativeToOrigin();
    float yOrigin = baseViewInfo.YRelativeToOrigin();

    baseViewInfo.Width = oldWidth;
    baseViewInfo.Height = oldHeight;
    baseViewInfo.ImageWidth = oldImageWidth;
    baseViewInfo.ImageHeight = oldImageHeight;

    SDL_Rect destRect;

    // Render each cached glyph position
    for (const auto& pos : cachedPositions_) {
        destRect.x = static_cast<int>(xOrigin + pos.xOffset);  // Truncate instead of round
        destRect.y = static_cast<int>(yOrigin + pos.yOffset);  // Truncate instead of round
        destRect.w = static_cast<int>(pos.sourceRect.w * scale);
        destRect.h = static_cast<int>(pos.sourceRect.h * scale);

        SDL::renderCopy(
            texture,
            baseViewInfo.Alpha,
            &pos.sourceRect,
            &destRect,
            baseViewInfo,
            page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
            page.getLayoutHeightByMonitor(baseViewInfo.Monitor)
        );
    }
}

void Text::updateGlyphPositions(Font* font, float scale, float maxWidth) {
    cachedPositions_.clear();
    cachedPositions_.reserve(textData_.size());

    float currentWidth = 0;
    int baseAscent = font->getAscent();

    for (char c : textData_) {
        Font::GlyphInfo glyph;
        if (!font->getRect(c, glyph) || glyph.rect.h <= 0) continue;

        float scaledAdvance = glyph.advance * scale;
        if ((currentWidth + scaledAdvance) > maxWidth) break;

        int xOffset = static_cast<int>(currentWidth);  // Truncate here
        if (glyph.minX < 0) {
            xOffset += static_cast<int>(glyph.minX * scale);  // Truncate here
        }

        int yOffset = baseAscent < glyph.maxY ? static_cast<int>((baseAscent - glyph.maxY) * scale) : 0;

        cachedPositions_.push_back({
            glyph.rect,
            xOffset,
            yOffset,
            scaledAdvance
            });

        currentWidth += scaledAdvance;
    }

    cachedWidth_ = currentWidth;
    cachedHeight_ = baseViewInfo.FontSize;
}