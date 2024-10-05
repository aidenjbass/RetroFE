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

#include <string>
#include "Component.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <unordered_map>
#include <mutex>
#include <vector>

class Image : public Component {
public:
    Image(const std::string& file, const std::string& altFile, Page& p, int monitor, bool additive);
    ~Image() override;

    void allocateGraphicsMemory() override;
    void freeGraphicsMemory() override;
    void draw() override;

    std::string_view filePath() override;

    // Static method to clean up the texture cache
    static void cleanupTextureCache();

private:
    // Texture Cache Implementation
    static std::unordered_map<std::string, SDL_Texture*> textureCache_;
    static std::mutex textureCacheMutex_;

    // Helper functions
    static bool loadFileToBuffer(const std::string& filePath, std::vector<char>& buffer);
    static bool isGIF(const std::vector<char>& buffer);
    static bool isWebP(const std::vector<char>& buffer);

    // Member variables
    std::string file_;
    std::string altFile_;
    SDL_Texture* texture_ = nullptr;

    IMG_Animation* animation_ = nullptr;           // Holds the animation data
    std::vector<SDL_Texture*> frameTextures_;       // Store textures for each frame
    int currentFrame_ = 0;                          // Current frame in the animation
    Uint32 lastFrameTime_ = 0;                      // Time when the last frame was updated
};
