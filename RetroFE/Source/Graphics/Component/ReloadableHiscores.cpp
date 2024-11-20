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

#include "ReloadableHiscores.h"
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

ReloadableHiscores::ReloadableHiscores(Configuration& config, std::string textFormat,
	Page& p, int displayOffset, Font* font, float scrollingSpeed, float startPosition, float startTime,
	float endTime, float baseColumnPadding, float baseRowPadding)
	: Component(p)
	, config_(config)
	, fontInst_(font)
	, textFormat_(textFormat)
	, scrollingSpeed_(scrollingSpeed)
	, startPosition_(startPosition)
	, currentPosition_(-startPosition)
	, startTime_(startTime)
	, waitStartTime_(startTime)
	, endTime_(endTime)
	, waitEndTime_(0.0f)
	, baseColumnPadding_(baseColumnPadding)
	, baseRowPadding_(baseRowPadding)
	, displayOffset_(displayOffset)
	, needsRedraw_(true)
	, intermediateTexture_(nullptr) // Initialize to nullptr
{
	allocateGraphicsMemory();

}



ReloadableHiscores::~ReloadableHiscores() {
	if (intermediateTexture_) {
		SDL_DestroyTexture(intermediateTexture_);
		intermediateTexture_ = nullptr;
	}
}

bool ReloadableHiscores::update(float dt) {
	if (waitEndTime_ > 0) {
		waitEndTime_ -= dt;
	}
	else if (waitStartTime_ > 0) {
		waitStartTime_ -= dt;
	}
	else {
		if (highScoreTable_ && !highScoreTable_->tables.empty()) {
			// Ensure currentTableIndex_ is within bounds
			if (currentTableIndex_ >= highScoreTable_->tables.size()) {
				currentTableIndex_ = 0;
			}

			const HighScoreTable& table = highScoreTable_->tables[currentTableIndex_];

			// Calculate scaling and positioning
			Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;

			float scale = baseViewInfo.FontSize / static_cast<float>(font->getHeight());
			float drawableHeight = static_cast<float>(font->getAscent()) * scale;
			float rowPadding = baseRowPadding_ * drawableHeight;  // Adjusted for floating-point precision

			float rowsSize = static_cast<float>(table.rows.size());
			float idSize = static_cast<float>(table.id.size());


			// Calculate total table height
			float totalTableHeight = (drawableHeight + rowPadding) * (rowsSize + 1.0f + idSize);
			bool needsScrolling = (totalTableHeight > baseViewInfo.Height);

			if (needsScrolling) {
				currentPosition_ += scrollingSpeed_ * dt;
				needsRedraw_ = true;
			}
			else {
				currentPosition_ = 0.0f;
			}

			if (highScoreTable_->tables.size() > 1) {
				if (needsScrolling) {
					currentTableDisplayTime_ = totalTableHeight / scrollingSpeed_;
				}
				else {
					currentTableDisplayTime_ = displayTime_;
				}

				tableDisplayTimer_ += dt;
				if (tableDisplayTimer_ >= currentTableDisplayTime_) {
					tableDisplayTimer_ = 0.0f;
					currentPosition_ = 0.0f;
					currentTableIndex_ = (currentTableIndex_ + 1) % highScoreTable_->tables.size();
					needsRedraw_ = true;
				}
			}
		}
	}
	if (newItemSelected || (newScrollItemSelected && getMenuScrollReload())) {
		currentTableIndex_ = 0;
		tableDisplayTimer_ = 0.0f; // Reset timer for the new game's table
		currentPosition_ = 0.0f;   // Reset scroll position
		needsRedraw_ = true;
		reloadTexture();
		newItemSelected = false;
		newScrollItemSelected = false;
	}
	return Component::update(dt);
}

void ReloadableHiscores::allocateGraphicsMemory()
{
	Component::allocateGraphicsMemory();
	reloadTexture();
}


void ReloadableHiscores::freeGraphicsMemory()
{
	Component::freeGraphicsMemory();
}


void ReloadableHiscores::deInitializeFonts()
{
	fontInst_->deInitialize();
}


void ReloadableHiscores::initializeFonts()
{
	fontInst_->initialize();
}


void ReloadableHiscores::reloadTexture(bool resetScroll) {
	if (resetScroll) {
		currentPosition_ = -startPosition_;
		waitStartTime_ = startTime_;
		waitEndTime_ = 0.0f;
	}

	Item* selectedItem = page.getSelectedItem(displayOffset_);
	if (selectedItem != lastSelectedItem_) {
		lastSelectedItem_ = selectedItem;
		if (selectedItem) {
			highScoreTable_ = HiScores::getInstance().getHighScoreTable(selectedItem->name);
		}
		else {
			highScoreTable_ = nullptr;
		}
		cachedTableIndex_ = std::numeric_limits<size_t>::max(); // Invalidate column widths cache
	}
}

void ReloadableHiscores::draw() {
	Component::draw();

	// Early exit conditions
	if (!(highScoreTable_ && !highScoreTable_->tables.empty()) ||
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

	if (highScoreTable_ && !highScoreTable_->tables.empty()) {
		// ======== Start of "hiscores" Rendering with Intermediate Texture ========
		float imageMaxWidth = (baseViewInfo.Width < baseViewInfo.MaxWidth && baseViewInfo.Width > 0)
			? baseViewInfo.Width : baseViewInfo.MaxWidth;
		float imageMaxHeight = (baseViewInfo.Height < baseViewInfo.MaxHeight && baseViewInfo.Height > 0)
			? baseViewInfo.Height : baseViewInfo.MaxHeight;
		// Step 1: Save the current render target
		SDL_Texture* originalTarget = SDL_GetRenderTarget(renderer);
		if (!originalTarget) {
			std::cerr << "Error: Unable to get current render target." << std::endl;
			return;
		}

		// Step 2: Create intermediate texture
		if (!intermediateTexture_) {
			if (!createIntermediateTexture(renderer, static_cast<int>(imageMaxWidth), static_cast<int>(imageMaxHeight))) {
				LOG_ERROR("ReloadableScrollingText", "Failed to create intermediate texture.");
				return;
			}
		}
		if (needsRedraw_) {
			LOG_DEBUG("ReloadableHiscores", "Redraw triggered due to scrolling or table switch.");
			// Calculate scaling and positioning
			float scale = baseViewInfo.FontSize / static_cast<float>(font->getHeight());
			float xOrigin = baseViewInfo.XRelativeToOrigin();
			float yOrigin = baseViewInfo.YRelativeToOrigin();

			float drawableHeight = static_cast<float>(font->getAscent()) * scale;
			float rowPadding = baseRowPadding_ * drawableHeight;
			float paddingBetweenColumns = baseColumnPadding_ * drawableHeight;

			
			// Set clipping rectangle
			SDL_Rect clipRect = { static_cast<int>(xOrigin), static_cast<int>(yOrigin),
				static_cast<int>(imageMaxWidth), static_cast<int>(imageMaxHeight) };
			SDL_RenderSetClipRect(renderer, &clipRect);

			float scrollOffset = currentPosition_;

			// Step 3: Set the intermediate texture as the render target
			SDL_SetRenderTarget(renderer, intermediateTexture_);

			// Step 4: Clear the intermediate texture (transparent background)
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
			SDL_RenderClear(renderer);

			// Retrieve the current high score table
			const HighScoreTable& table = highScoreTable_->tables[currentTableIndex_];

			// Cache column widths if needed
			cacheColumnWidths(font, scale, table, paddingBetweenColumns);

			// Use cached column widths and total table width
			const auto& columnWidths = cachedColumnWidths_;
			float totalTableWidth = cachedTotalTableWidth_;

			bool hasTitle = !table.id.empty();
			float adjustedYOrigin = yOrigin;

			// Render title
			if (hasTitle) {
				const std::string& title = table.id;
				float titleWidth = font->getWidth(title) * scale;
				float titleX = xOrigin + (std::min(imageMaxWidth, totalTableWidth) - titleWidth) / 2.0f;
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
						SDL_RenderCopyF(renderer, texture, &srcRect, &destRect);
						titleX += static_cast<float>(glyph.advance) * scale;
					}
				}
				adjustedYOrigin += drawableHeight + rowPadding;
			}

			// Render headers
			float headerY = adjustedYOrigin;
			float xPos = xOrigin;
			for (size_t col = 0; col < table.columns.size(); ++col) {
				const std::string& header = table.columns[col];
				float headerWidth = font->getWidth(header) * scale;
				float xAligned = xPos + (columnWidths[col] - headerWidth) / 2.0f;

				float charX = xAligned;
				for (char c : header) {
					Font::GlyphInfo glyph;
					if (font->getRect(c, glyph)) {
						SDL_Rect srcRect = glyph.rect;
						SDL_FRect destRect = {
							charX,
							headerY,
							glyph.rect.w * scale,
							glyph.rect.h * scale
						};
						SDL_RenderCopyF(renderer, texture, &srcRect, &destRect);
						charX += glyph.advance * scale;
					}
				}
				xPos += columnWidths[col] + paddingBetweenColumns;
			}

			adjustedYOrigin += drawableHeight + rowPadding;

			// Calculate base position for all rows
			float baseY = adjustedYOrigin - scrollOffset;

			// Adjusted scrollClipRect calculation
			SDL_Rect scrollClipRect = {
				static_cast<int>(xOrigin),
				static_cast<int>(adjustedYOrigin),
				static_cast<int>(std::min(totalTableWidth, imageMaxWidth)),
				static_cast<int>(std::max(imageMaxHeight - (adjustedYOrigin - yOrigin), 0.0f))
			};
			SDL_RenderSetClipRect(renderer, &scrollClipRect);

			// Render rows
			for (size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex) {
				float currentY = baseY + static_cast<float>(rowIndex) * (drawableHeight + rowPadding);

				// Visibility check
				if (currentY + drawableHeight < yOrigin || currentY > yOrigin + imageMaxHeight) {
					continue;
				}

				float xPos = xOrigin;
				for (size_t col = 0; col < table.columns.size(); ++col) {
					if (col >= table.rows[rowIndex].size()) continue;

					const std::string& cell = table.rows[rowIndex][col];
					float cellWidth = font->getWidth(cell) * scale;
					float xAligned = xPos + (columnWidths[col] - cellWidth) / 2.0f;

					float charX = xAligned;
					for (char c : cell) {
						Font::GlyphInfo glyph;
						if (font->getRect(c, glyph)) {
							SDL_Rect srcRect = glyph.rect;
							SDL_FRect destRect = {
								charX,
								currentY,
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

			// Reset clipping rectangle after drawing
			SDL_RenderSetClipRect(renderer, nullptr);

			// Step 5: Restore the original render target
			SDL_SetRenderTarget(renderer, originalTarget);
		}
		// Step 6: Define the destination rectangle where the intermediate texture should be drawn
		SDL_FRect destRect = {
			baseViewInfo.XRelativeToOrigin(),
			baseViewInfo.YRelativeToOrigin(),
			baseViewInfo.Width,
			baseViewInfo.Height
		};

		// Step 7: Render the intermediate texture to the original render target
		SDL::renderCopyF(intermediateTexture_, baseViewInfo.Alpha, nullptr, &destRect, baseViewInfo,
			page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
			page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
	}
	needsRedraw_ = false;
}

void ReloadableHiscores::cacheColumnWidths(Font* font, float scale, const HighScoreTable& table, float paddingBetweenColumns) {
	// Only recalculate if the table index has changed
	if (currentTableIndex_ == cachedTableIndex_) {
		return; // Cache is valid, no need to recalculate
	}

	// Recalculate column widths
	cachedColumnWidths_.clear();
	cachedColumnWidths_.resize(table.columns.size(), 0.0f);
	cachedTotalTableWidth_ = 0.0f;

	for (size_t i = 0; i < table.columns.size(); ++i) {
		float headerWidth = font->getWidth(table.columns[i]) * scale;
		cachedColumnWidths_[i] = std::max(cachedColumnWidths_[i], headerWidth);

		for (const auto& row : table.rows) {
			if (i < row.size()) {
				float cellWidth = font->getWidth(row[i]) * scale;
				cachedColumnWidths_[i] = std::max(cachedColumnWidths_[i], cellWidth);
			}
		}

		cachedTotalTableWidth_ += cachedColumnWidths_[i];
		if (i < table.columns.size() - 1) {
			cachedTotalTableWidth_ += paddingBetweenColumns;
		}
	}

	// Update the cached table index
	cachedTableIndex_ = currentTableIndex_;
}

bool ReloadableHiscores::createIntermediateTexture(SDL_Renderer* renderer, int width, int height) {
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


