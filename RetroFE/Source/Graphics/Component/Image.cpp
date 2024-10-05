/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
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
#include <string_view>
#include <fstream>
#include <vector>

 // Definition of static members
std::unordered_map<std::string, SDL_Texture*> Image::textureCache_;
std::mutex Image::textureCacheMutex_;

bool Image::loadFileToBuffer(const std::string& filePath, std::vector<char>& buffer) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Image", "Failed to open file: " + filePath);
        return false;
    }

    std::streamsize size = file.tellg();
    if (size < 0) {
        LOG_ERROR("Image", "Invalid file size for: " + filePath);
        return false;
    }
    file.seekg(0, std::ios::beg);

    buffer.resize(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size)) {
        LOG_ERROR("Image", "Failed to read file: " + filePath);
        return false;
    }

    return true;
}

bool Image::isGIF(const std::vector<char>& buffer) {
    if (buffer.size() < 6) return false;
    return (std::memcmp(buffer.data(), "GIF87a", 6) == 0 ||
        std::memcmp(buffer.data(), "GIF89a", 6) == 0);
}

bool Image::isWebP(const std::vector<char>& buffer) {
    if (buffer.size() < 12) return false;
    return (std::memcmp(buffer.data(), "RIFF", 4) == 0 &&
        std::memcmp(buffer.data() + 8, "WEBP", 4) == 0);
}

Image::Image(const std::string& file, const std::string& altFile, Page& p, int monitor, bool additive)
    : Component(p), file_(file), altFile_(altFile)
{
    baseViewInfo.Monitor = monitor;
    baseViewInfo.Additive = additive;
    baseViewInfo.Layout = page.getCurrentLayout();
}

Image::~Image() {
    Image::freeGraphicsMemory();
}

void Image::freeGraphicsMemory() {
    Component::freeGraphicsMemory();

    SDL_LockMutex(SDL::getMutex());

    if (texture_ != nullptr) {
        // Since texture_ is cached, do not destroy it here
        texture_ = nullptr;
    }

    if (!frameTextures_.empty()) {
        // Destroy frame textures as they are not cached individually
        for (SDL_Texture* frameTexture : frameTextures_) {
            SDL_DestroyTexture(frameTexture);
        }
        frameTextures_.clear();
    }

    if (animation_ != nullptr) {
        IMG_FreeAnimation(animation_);
        animation_ = nullptr;
    }

    SDL_UnlockMutex(SDL::getMutex());
}

void Image::allocateGraphicsMemory() {
    // Check if graphics memory is already allocated
    if (texture_ || !frameTextures_.empty()) {
        // Graphics memory already allocated
        return;
    }

    // Define a lambda to attempt loading a file with caching
    auto tryLoad = [&](const std::string& filePath) -> bool {
        // Attempt to retrieve texture from cache
        SDL_Texture* cachedTexture = nullptr;
        {
            std::lock_guard<std::mutex> lock(textureCacheMutex_);
            auto it = textureCache_.find(filePath);
            if (it != textureCache_.end()) {
                cachedTexture = it->second;
            }
        }

        if (cachedTexture) {
            texture_ = cachedTexture;
            // Query texture dimensions if necessary
            int width, height;
            if (SDL_QueryTexture(texture_, nullptr, nullptr, &width, &height) != 0) {
                LOG_ERROR("Image", "Failed to query texture: " + std::string(SDL_GetError()));
            }
            else {
                baseViewInfo.ImageWidth = static_cast<float>(width);
                baseViewInfo.ImageHeight = static_cast<float>(height);
            }
            LOG_INFO("Image", "Loaded texture from cache: " + filePath);
            return true;
        }

        // If texture is not in cache, proceed to load from file
        std::vector<char> buffer;
        if (!loadFileToBuffer(filePath, buffer)) {
            // Loading failed; already logged
            return false;
        }

        bool isAnimated = false;

        // Determine if the image is animated (GIF/WebP) based on magic numbers
        bool isGifOrWebP = false;
        if (buffer.size() >= 6 &&
            (std::memcmp(buffer.data(), "GIF87a", 6) == 0 ||
                std::memcmp(buffer.data(), "GIF89a", 6) == 0)) {
            isGifOrWebP = true;
        }
        else if (buffer.size() >= 12 &&
            std::memcmp(buffer.data(), "RIFF", 4) == 0 &&
            std::memcmp(buffer.data() + 8, "WEBP", 4) == 0) {
            isGifOrWebP = true;
        }

        // Create SDL_RWops from the buffer
        SDL_RWops* rw = SDL_RWFromConstMem(buffer.data(), static_cast<int>(buffer.size()));
        if (!rw) {
            LOG_ERROR("Image", "Failed to create RWops from buffer: " + std::string(SDL_GetError()));
            return false;
        }

        if (isGifOrWebP) {
            // Attempt to load as animation
            animation_ = IMG_LoadAnimation_RW(rw, 1); // '1' to free RWops after loading
            if (animation_) {
                // Preload all frames as textures
                for (int i = 0; i < animation_->count; ++i) {
                    SDL_Texture* frameTexture = SDL_CreateTextureFromSurface(SDL::getRenderer(baseViewInfo.Monitor), animation_->frames[i]);
                    if (frameTexture) {
                        frameTextures_.push_back(frameTexture);
                    }
                    else {
                        LOG_ERROR("Image", "Failed to create texture from animation frame: " + std::string(SDL_GetError()));
                    }
                }
                baseViewInfo.ImageWidth = static_cast<float>(animation_->w);
                baseViewInfo.ImageHeight = static_cast<float>(animation_->h);
                lastFrameTime_ = SDL_GetTicks();
                isAnimated = true;
                LOG_INFO("Image", "Loaded animated texture: " + filePath);
                return true;
            }
            else {
                LOG_ERROR("Image", "Failed to load animation (GIF/WebP): " + std::string(IMG_GetError()));
                // Proceed to load as static image
            }
        }

        if (!isAnimated) {
            // Load as static texture
            SDL_Texture* texture = IMG_LoadTexture_RW(SDL::getRenderer(baseViewInfo.Monitor), rw, 1); // '1' to free RWops after loading
            if (texture) {
                // Set the blend mode
                SDL_SetTextureBlendMode(texture, baseViewInfo.Additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);

                // Query texture dimensions
                int width, height;
                if (SDL_QueryTexture(texture, nullptr, nullptr, &width, &height) != 0) {
                    LOG_ERROR("Image", "Failed to query texture: " + std::string(SDL_GetError()));
                }
                else {
                    baseViewInfo.ImageWidth = static_cast<float>(width);
                    baseViewInfo.ImageHeight = static_cast<float>(height);
                }

                // Cache the loaded texture
                {
                    std::lock_guard<std::mutex> lock(textureCacheMutex_);
                    textureCache_[filePath] = texture;
                }

                // Assign the loaded texture
                texture_ = texture;
                LOG_INFO("Image", "Loaded and cached texture: " + filePath);
                return true;
            }
            else {
                LOG_ERROR("Image", "Failed to load static image: " + std::string(IMG_GetError()));
                return false;
            }
        }

        return false;
        };

    // Attempt to load the primary file
    if (tryLoad(file_)) {
        // Successfully loaded primary file
        return;
    }

    // If primary file failed, attempt to load the alternative file
    if (!altFile_.empty()) {
        if (tryLoad(altFile_)) {
            // Successfully loaded alternative file
            return;
        }
    }

    // If both attempts failed, log a comprehensive error
    LOG_ERROR("Image", "Failed to load both primary and alternative image files: " + file_ + " | " + altFile_);
}

void Image::draw() {
    Component::draw();
    SDL_Rect rect = { 0, 0, 0, 0 };
    rect.x = static_cast<int>(baseViewInfo.XRelativeToOrigin());
    rect.y = static_cast<int>(baseViewInfo.YRelativeToOrigin());
    rect.h = static_cast<int>(baseViewInfo.ScaledHeight());
    rect.w = static_cast<int>(baseViewInfo.ScaledWidth());

    // Prioritize static image rendering
    if (texture_) {
        SDL::renderCopy(texture_, baseViewInfo.Alpha, nullptr, &rect, baseViewInfo,
            page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
            page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
    }
    else if (!frameTextures_.empty()) {
        // Handle animation rendering
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastFrameTime_ > static_cast<Uint32>(animation_->delays[currentFrame_])) {
            currentFrame_ = (currentFrame_ + 1) % animation_->count;
            lastFrameTime_ = currentTime;
        }

        SDL_Texture* frameTexture = frameTextures_[currentFrame_];
        SDL::renderCopy(frameTexture, baseViewInfo.Alpha, nullptr, &rect, baseViewInfo,
            page.getLayoutWidthByMonitor(baseViewInfo.Monitor),
            page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
    }
}

std::string_view Image::filePath() {
    return file_;
}

// Static method to clean up the texture cache
void Image::cleanupTextureCache() {
    std::lock_guard<std::mutex> lock(textureCacheMutex_);
    for (auto& pair : textureCache_) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
            pair.second = nullptr;
            LOG_INFO("TextureCache", "Destroyed cached texture: " + pair.first);
        }
    }
    textureCache_.clear();
    LOG_INFO("TextureCache", "All cached textures have been destroyed.");
}
