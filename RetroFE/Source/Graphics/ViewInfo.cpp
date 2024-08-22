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

#include "ViewInfo.h"
#include "../Database/Configuration.h"
#include "Animate/TweenTypes.h"

ViewInfo::ViewInfo()
{
}


ViewInfo::~ViewInfo() = default;

float ViewInfo::XRelativeToOrigin() const
{
    return X + XOffset - XOrigin*ScaledWidth();
}

float ViewInfo::YRelativeToOrigin() const
{
    return Y + YOffset - YOrigin*ScaledHeight();
}

float ViewInfo::ScaledHeight() const
{
    // Cache the absolute dimensions
    float height = AbsoluteHeight();
    float width = AbsoluteWidth();

    // Handle the scaling logic using the helper function
    return ScaleDimension(height, MinHeight, MaxHeight, width, false);
}

float ViewInfo::ScaledWidth() const
{
    // Cache the absolute dimensions
    float height = AbsoluteHeight();
    float width = AbsoluteWidth();

    // Handle the scaling logic using the helper function
    return ScaleDimension(width, MinWidth, MaxWidth, height, true);
}

float ViewInfo::ScaleDimension(float size, float minSize, float maxSize, float otherSize, bool isWidth) const
{
    if (size < minSize) {
        float scaleOther = minSize / otherSize;
        if (otherSize >= minSize) {
            return minSize;
        }
        return isWidth ? minSize : size * scaleOther;
    }

    if (size > maxSize) {
        float scaleOther = maxSize / otherSize;
        if (otherSize <= maxSize) {
            return maxSize;
        }
        return isWidth ? size * scaleOther : maxSize;
    }

    return size;
}

float ViewInfo::AbsoluteHeight() const
{
    if(Height < 0 && Width < 0) {
        return ImageHeight;
    }

    if (Height < 0 && ImageWidth != 0) {
        return ImageHeight * Width / ImageWidth;
    }

    return Height;
}

float ViewInfo::AbsoluteWidth() const
{
    if(Height < 0 && Width < 0) {
        return ImageWidth;
    }

    if (Width < 0 && ImageHeight != 0) {
        return ImageWidth * Height / ImageHeight;
    }

    return Width;
}
