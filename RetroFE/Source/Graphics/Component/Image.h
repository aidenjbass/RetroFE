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
#include <SDL2/SDL.h>
#include <string>

class Image : public Component
{
public:
    Image(const std::string& file, const std::string& altFile, Page &p, int monitor, bool additive);
    ~Image() override;
    void freeGraphicsMemory() override;
    void allocateGraphicsMemory() override;
    void draw() override;
    std::string_view filePath() override;

protected:
    SDL_Texture* texture_{ nullptr };
    std::string  file_;
    std::string  altFile_;
};
