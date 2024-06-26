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
    float height = AbsoluteHeight();
    float width = AbsoluteWidth();

    if (height < MinHeight || width < MinWidth)
    {
        float scaleH = MinHeight / height;
        float scaleW = MinWidth / width;

        if(width >= MinWidth && height < MinHeight)
        {
            height = MinHeight;
        }
        else if(width < MinWidth && height >= MinHeight)
        {
            height = scaleW * height;
        }
        else
        {
            height = (scaleH > scaleW) ? MinHeight : (height * scaleW);
        }
    }
    if (width > MaxWidth || height > MaxHeight) {
        float scaleH = MaxHeight / height;
        float scaleW = MaxWidth / width;

        if(width <= MaxWidth && height > MaxHeight) {
            height = MaxHeight;
        }
        else if(width > MaxWidth && height <= MaxHeight) {
            height = scaleW * height;
        }
        else {
            height = (scaleH < scaleW) ? MaxHeight : (height * scaleW);
        }
    }

    return height;
}

float ViewInfo::ScaledWidth() const
{
    float height = AbsoluteHeight();
    float width = AbsoluteWidth();

    if (height < MinHeight || width < MinWidth) {
        float scaleH = MinHeight / height;
        float scaleW = MinWidth / width;

        if(height >= MinHeight && width < MinWidth) {
            width = MinWidth;
        }
        else if(height < MinHeight && width >= MinWidth) {
            width = scaleH * width;
        }
        else {
            width = (scaleH > scaleW) ? MinWidth : (width * scaleH);
        }
    }
    if (width > MaxWidth || height > MaxHeight) {
        float scaleH = MaxHeight / height;
        float scaleW = MaxWidth / width;

        if(height <= MaxHeight && width > MaxWidth) {
            width = MaxWidth;
        }
        else if(height > MaxHeight && width <= MaxWidth) {
            width = scaleH * width;
        }
        else {
            width = (scaleH > scaleW) ? MaxWidth : (width * scaleH);
        }
    }

    return width;
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
