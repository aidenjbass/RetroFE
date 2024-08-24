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
#include "Image.h"
#include "../ViewInfo.h"
#include "../../SDL.h"
#include "../../Utility/Log.h"
#if (__APPLE__)
    #include <SDL2_image/SDL_image.h>
#else
    #include <SDL2/SDL_image.h>
#endif

Image::Image(const std::string& file, const std::string& altFile, Page &p, int monitor, bool additive)
    : Component(p)
    , file_(file)
    , altFile_(altFile)
{
    baseViewInfo.Monitor = monitor;
    baseViewInfo.Additive = additive;
    baseViewInfo.Layout = page.getCurrentLayout();
}

Image::~Image()
{
    Component::freeGraphicsMemory();

    SDL_LockMutex(SDL::getMutex());
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    SDL_UnlockMutex(SDL::getMutex());
}

void Image::freeGraphicsMemory()
{
    Component::freeGraphicsMemory();

    SDL_LockMutex(SDL::getMutex());
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    SDL_UnlockMutex(SDL::getMutex());
}

void Image::allocateGraphicsMemory()
{
    if (!texture_) {

        // Load the image as a surface
        SDL_Surface* surface = IMG_Load(file_.c_str());
        if (!surface && !altFile_.empty()) {
            surface = IMG_Load(altFile_.c_str());
        }

        if (surface) {
            SDL_SetSurfaceRLE(surface, 1);
            texture_ = SDL_CreateTextureFromSurface(SDL::getRenderer(baseViewInfo.Monitor), surface);
            SDL_FreeSurface(surface);  // Free the optimized surface

            if (texture_) {
                int width, height;
                SDL_QueryTexture(texture_, nullptr, nullptr, &width, &height);
                baseViewInfo.ImageWidth = static_cast<float>(width);
                baseViewInfo.ImageHeight = static_cast<float>(height);

                // Set the blend mode
                SDL_SetTextureBlendMode(texture_, baseViewInfo.Additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
            }
        } else {
            // Handle error if surface loading fails
            LOG_ERROR("Image", "Failed to load image: " + std::string(IMG_GetError()));
        }

    }

    // Call the base class implementation
    Component::allocateGraphicsMemory();
}

std::string_view Image::filePath()
{
    return file_;
}

void Image::draw()
{
    Component::draw();
    if(texture_ && baseViewInfo.Alpha > 0.0f) {
        SDL_Rect rect = { 0, 0, 0, 0 };

        rect.x = static_cast<int>(baseViewInfo.XRelativeToOrigin());
        rect.y = static_cast<int>(baseViewInfo.YRelativeToOrigin());
        rect.h = static_cast<int>(baseViewInfo.ScaledHeight());
        rect.w = static_cast<int>(baseViewInfo.ScaledWidth());

        SDL::renderCopy(texture_, baseViewInfo.Alpha, nullptr, &rect, baseViewInfo, page.getLayoutWidthByMonitor(baseViewInfo.Monitor), page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
    }
}
