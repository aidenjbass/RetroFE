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
	float endTime, float baseColumnPadding, float baseRowPadding, size_t maxRows)
	: Component(p)
	, fontInst_(font)
	, textFormat_(textFormat)
	, scrollingSpeed_(scrollingSpeed)
	, startPosition_(startPosition)
	, currentPosition_(-startPosition)
	, startTime_(startTime)
	, waitStartTime_(startTime)
	, waitEndTime_(0.0f)
	, baseColumnPadding_(baseColumnPadding)
	, baseRowPadding_(baseRowPadding)
	, displayOffset_(displayOffset)
	, maxRows_(maxRows)
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
	} else if (waitStartTime_ > 0) {
		waitStartTime_ -= dt;
	} else {
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
			float rowPadding = baseRowPadding_ * drawableHeight;
			float paddingBetweenColumns = baseColumnPadding_ * drawableHeight;

			// Cache column widths and compute total table width
			cacheColumnWidths(font, scale, table, paddingBetweenColumns);
			float totalTableWidth = cachedTotalTableWidth_;

			// Adjust scale if the table width exceeds the viewable area
			if (totalTableWidth > baseViewInfo.Width) {
				float downScaleFactor = baseViewInfo.Width / totalTableWidth;
				scale *= downScaleFactor;
				drawableHeight *= downScaleFactor;
				rowPadding *= downScaleFactor;
			}

			// Calculate total table height, factoring in scaled dimensions
			size_t rowsToRender = std::min(table.rows.size(), maxRows_);
			auto rowsSize = static_cast<float>(rowsToRender);
			float idSize = table.id.empty() ? 0.0f : 1.0f; // Include title row if present
			float headerHeight = drawableHeight; // Height for headers
			float totalTableHeight = (drawableHeight + rowPadding) * (rowsSize + idSize) + headerHeight;

			// Determine if scrolling is required
			bool needsScrolling = (totalTableHeight > baseViewInfo.Height);

			if (needsScrolling) {
				currentPosition_ += scrollingSpeed_ * dt;

				// Reset scrolling when it completes
				if (currentPosition_ >= totalTableHeight) {
					currentPosition_ = -startPosition_; // Reset to start position
					waitEndTime_ = startTime_;         // Optional pause after scrolling
					needsRedraw_ = true;              // Trigger redraw
				} else {
					needsRedraw_ = true; // Keep redrawing while scrolling
				}
			} else {
				currentPosition_ = 0.0f; // Ensure non-scrolling tables remain visible
			}

			if (highScoreTable_->tables.size() > 1) {
				if (needsScrolling) {
					currentTableDisplayTime_ = totalTableHeight / scrollingSpeed_;
				} else {
					currentTableDisplayTime_ = displayTime_;
				}

				tableDisplayTimer_ += dt;

				// Handle table switching when display time elapses
				if (tableDisplayTimer_ >= currentTableDisplayTime_ && currentPosition_ == 0.0f) {
					tableDisplayTimer_ = 0.0f;
					currentPosition_ = -startPosition_; // Reset scroll for new table
					currentTableIndex_ = (currentTableIndex_ + 1) % highScoreTable_->tables.size();
					reloadTexture(); // Refresh the texture for the new table
					needsRedraw_ = true;
				}
			}
		}
	}

	// Handle new item selection (if relevant to your UI)
	if (newItemSelected || (newScrollItemSelected && getMenuScrollReload())) {
		currentTableIndex_ = 0;
		tableDisplayTimer_ = 0.0f;  // Reset timer for the new game's table
		currentPosition_ = -startPosition_;  // Reset scroll position
		needsRedraw_ = true;
		reloadTexture(true);
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
		cacheValid_ = false;
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

	// Check if the table requires a redraw
	HighScoreTable& table = highScoreTable_->tables[currentTableIndex_];
	if (table.forceRedraw) {
		needsRedraw_ = true;
		cacheValid_ = false;
		table.forceRedraw = false;  // Reset the flag directly
	}

	// Retrieve font and texture
	Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
	SDL_Texture* texture = font ? font->getTexture() : nullptr;
	if (!texture) {
		LOG_ERROR("ReloadableHiscores", "Font texture is null.");
		return;
	}

	// Retrieve renderer
	SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);
	if (!renderer) {
		LOG_ERROR("ReloadableHiscores", "Unable to retrieve SDL_Renderer.");
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
			LOG_ERROR("ReloadableHiscores", "Unable to get current render target.");
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
			float scale = baseViewInfo.FontSize / static_cast<float>(font->getHeight());
			float drawableHeight = static_cast<float>(font->getAscent()) * scale;
			float rowPadding = baseRowPadding_ * drawableHeight;
			float paddingBetweenColumns = baseColumnPadding_ * drawableHeight;

			// Cache column widths and total table width
			cacheColumnWidths(font, scale, table, paddingBetweenColumns);
			float totalTableWidth = cachedTotalTableWidth_;
			float downScaleFactor = 1.0f;
			// Downscale if the table width exceeds the viewable area
			if (imageMaxWidth < totalTableWidth) {
				downScaleFactor = (imageMaxWidth / totalTableWidth) * 0.9f;
				scale *= downScaleFactor;
				drawableHeight *= downScaleFactor;
				rowPadding *= downScaleFactor;
				paddingBetweenColumns *= downScaleFactor;
				
				// Invalidate cache due to resizing
				cacheValid_ = false;
				
				// Recalculate column widths and table width
				cacheColumnWidths(font, scale, table, paddingBetweenColumns);
				totalTableWidth = cachedTotalTableWidth_;

			}

			float scrollOffset = currentPosition_;

			// Recalculate origin for centering
			float xOrigin = baseViewInfo.XRelativeToOrigin() + (imageMaxWidth - totalTableWidth) / 2.0f;
			float yOrigin = baseViewInfo.YRelativeToOrigin();

			// Set clipping rectangle
			SDL_Rect clipRect = { static_cast<int>(xOrigin), static_cast<int>(yOrigin),
				static_cast<int>(std::min(totalTableWidth, imageMaxWidth)),
				static_cast<int>(imageMaxHeight) };
			SDL_RenderSetClipRect(renderer, &clipRect);

			// Clear the intermediate texture
			SDL_SetRenderTarget(renderer, intermediateTexture_);
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
			SDL_RenderClear(renderer);

			// Render Title, Headers, and Rows (centered using xOrigin and recalculated widths)
			float adjustedYOrigin = yOrigin;

			// Render title
			if (!table.id.empty()) {
				const std::string& title = table.id;
				float titleWidth = static_cast<float>(font->getWidth(title)) * scale;
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
				float headerWidth = static_cast<float>(font->getWidth(header)) * scale;
				float xAligned = xPos + (cachedColumnWidths_[col] - headerWidth) / 2.0f;

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
						charX += static_cast<float>(glyph.advance) * scale;
					}
				}
				xPos += cachedColumnWidths_[col] + paddingBetweenColumns;
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
			size_t renderedRows = 0; // Track the number of rows rendered
			for (size_t rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex) {
				// Stop rendering if the number of rendered rows reaches the maxRows_ limit
				if (renderedRows >= maxRows_) {
					break;
				}

				float currentY = baseY + static_cast<float>(rowIndex) * (drawableHeight + rowPadding);

				float rowTop = currentY;
				float rowBottom = currentY + drawableHeight;

				if (rowBottom < yOrigin || rowTop > yOrigin + imageMaxHeight) {
					continue;  // Skip rows fully outside the visible area
				}

				xPos = xOrigin;
				for (size_t col = 0; col < table.columns.size(); ++col) {
					if (col >= table.rows[rowIndex].size()) continue;

					const std::string& cell = table.rows[rowIndex][col];
					float cellWidth = static_cast<float>(font->getWidth(cell)) * scale;
					float xAligned = xPos + (cachedColumnWidths_[col] - cellWidth) / 2.0f;

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
							charX += static_cast<float>(glyph.advance) * scale;
						}
					}
					xPos += cachedColumnWidths_[col] + paddingBetweenColumns;
				}

				++renderedRows; // Increment the rendered rows count
			}

			SDL_RenderSetClipRect(renderer, nullptr);
			SDL_SetRenderTarget(renderer, originalTarget);
		}
		// Step 6: Define the destination rectangle where the intermediate texture should be drawn
		SDL_FRect destRect = {
			baseViewInfo.XOrigin,
			baseViewInfo.YOrigin,
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

	// Return early if the table hasn't changed and scale/padding are the same
	if (cacheValid_ && currentTableIndex_ == cachedTableIndex_ &&
		lastScale_ == scale && lastPaddingBetweenColumns_ == paddingBetweenColumns) {
		return; // Cache is valid, no need to recalculate
	}

	// Update cached scale and padding
	lastScale_ = scale;
	lastPaddingBetweenColumns_ = paddingBetweenColumns;

	// Initialize or reset cached column widths and total width
	cachedColumnWidths_.clear();
	cachedColumnWidths_.resize(table.columns.size(), 0.0f);
	cachedTotalTableWidth_ = 0.0f;

	// Iterate over all columns in the table
	size_t columnCount = table.columns.size();
	for (size_t colIndex = 0; colIndex < columnCount; ++colIndex) {
		float maxColumnWidth = 0.0f;

		// Compare header width
		if (colIndex < table.columns.size()) {
			maxColumnWidth = std::max(maxColumnWidth, static_cast<float>(font->getWidth(table.columns[colIndex])) * scale);
		}

		// Compare all rows' cell widths
		for (const auto& row : table.rows) {
			if (colIndex < row.size()) {
				float cellWidth = static_cast<float>(font->getWidth(row[colIndex])) * scale;
				maxColumnWidth = std::max(maxColumnWidth, cellWidth);
			}
		}

		// Store the maximum width for the current column
		cachedColumnWidths_[colIndex] = maxColumnWidth;
	}

	// Calculate total table width, including padding between columns
	cachedTotalTableWidth_ = paddingBetweenColumns; // Add padding before the first column
	for (size_t i = 0; i < cachedColumnWidths_.size(); ++i) {
		cachedTotalTableWidth_ += cachedColumnWidths_[i]; // Add column width
		if (i < cachedColumnWidths_.size() - 1) { // Add padding only between columns
			cachedTotalTableWidth_ += paddingBetweenColumns;
		}
	}

	// Debugging log for column widths and total table width
	LOG_DEBUG("ReloadableHiscores", "Cached Column Widths: ");
	for (const auto& width : cachedColumnWidths_) {
		LOG_DEBUG("Column Width", std::to_string(width));
	}
	LOG_DEBUG("Cached Total Table Width", std::to_string(cachedTotalTableWidth_));

	cacheValid_ = true;
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


