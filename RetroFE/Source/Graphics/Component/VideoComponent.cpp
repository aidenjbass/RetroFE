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

#include "VideoComponent.h"
#include "../ViewInfo.h"
#include "../../Database/Configuration.h"
#include "../../Utility/Log.h"
#include "../../Utility/Utils.h"
#include "../../Video/GStreamerVideo.h"
#include "../../Video/VideoFactory.h"
#include "../../SDL.h"
#include <string>

VideoComponent::VideoComponent(Page &p, const std::string& videoFile, int monitor, int numLoops)
    : Component(p)
    , videoFile_(videoFile)
    , numLoops_(numLoops)
    , monitor_(monitor)
    , currentPage_(&p)
{

}

VideoComponent::~VideoComponent()
{
    freeGraphicsMemory();
}


bool VideoComponent::update(float dt)
{
    if (videoInst_) {
        isPlaying_ = ((GStreamerVideo*)(videoInst_))->isPlaying();
    }

    if (videoInst_ && isPlaying_) {
        videoInst_->setVolume(baseViewInfo.Volume);
        videoInst_->update(dt);
        videoInst_->volumeUpdate();
        if(!currentPage_->isMenuScrolling())
            videoInst_->loopHandler();

        // video needs to run a frame to start getting size info
        if (baseViewInfo.ImageHeight == 0 && baseViewInfo.ImageWidth == 0) {
            baseViewInfo.ImageHeight = static_cast<float>(videoInst_->getHeight());
            baseViewInfo.ImageWidth = static_cast<float>(videoInst_->getWidth());
        }

        bool isCurrentlyVisible = baseViewInfo.Alpha > 0.0;

        if (isCurrentlyVisible)
            hasBeenOnScreen_ = true;

        // Handle Pause/Resume based on visibility and PauseOnScroll setting
        if (baseViewInfo.PauseOnScroll && !currentPage_->isMenuFastScrolling()) {
            if (!isCurrentlyVisible && !isPaused()) {
                videoInst_->pause();
                if(Logger::isLevelEnabled("DEBUG"))
                    LOG_DEBUG("VideoComponent", "Paused " + Utils::getFileName(videoFile_));
            }
            else if (isCurrentlyVisible && isPaused()) {
                videoInst_->pause(); // This resumes the video
                if (Logger::isLevelEnabled("DEBUG"))
                    LOG_DEBUG("VideoComponent", "Resumed " + Utils::getFileName(videoFile_));
            }
        }

        // Handle Restart
        if (baseViewInfo.Restart && hasBeenOnScreen_) {
            videoInst_->restart();
            if (Logger::isLevelEnabled("DEBUG"))
                LOG_DEBUG("VideoComponent", "Seeking to beginning of " + Utils::getFileName(videoFile_));
            baseViewInfo.Restart = false;
        }
    }

    return Component::update(dt);
}


void VideoComponent::allocateGraphicsMemory()
{
    Component::allocateGraphicsMemory();

    if(!isPlaying_) {
        if (!videoInst_) {
            videoInst_ = VideoFactory::createVideo(monitor_, numLoops_);
        }
        if (videoFile_ != "") {
            isPlaying_ = videoInst_->play(videoFile_);
        }
    }
}


void VideoComponent::freeGraphicsMemory()
{
    //videoInst_->stop();
        
    Component::freeGraphicsMemory();
    if (Logger::isLevelEnabled("DEBUG"))
        LOG_DEBUG("VideoComponent", "Component Freed " + Utils::getFileName(videoFile_));
    
    if (videoInst_)  {
        delete videoInst_;
        isPlaying_ = false;
        if (Logger::isLevelEnabled("DEBUG"))
            LOG_DEBUG("VideoComponent", "Deleted " + Utils::getFileName(videoFile_));
        videoInst_ = nullptr;
        
    }
}


void VideoComponent::draw()
{
    if (baseViewInfo.Alpha > 0.0f) {
        SDL_Rect rect = { 0, 0, 0, 0 };

        rect.x = static_cast<int>(baseViewInfo.XRelativeToOrigin());
        rect.y = static_cast<int>(baseViewInfo.YRelativeToOrigin());
        rect.h = static_cast<int>(baseViewInfo.ScaledHeight());
        rect.w = static_cast<int>(baseViewInfo.ScaledWidth());

        videoInst_->draw();
        SDL_Texture* texture = videoInst_->getTexture();

        if (texture)
        {
            SDL::renderCopy(texture, baseViewInfo.Alpha, nullptr, &rect, baseViewInfo, page.getLayoutWidthByMonitor(baseViewInfo.Monitor), page.getLayoutHeightByMonitor(baseViewInfo.Monitor));
        }
    }
}

bool VideoComponent::isPlaying()
{
    return isPlaying_;
}

std::string_view VideoComponent::filePath()
{
    return videoFile_;
}

void VideoComponent::skipForward( )
{
    if ( videoInst_ )
        videoInst_->skipForward( );
}


void VideoComponent::skipBackward( )
{
    if ( videoInst_ )
        videoInst_->skipBackward( );
}


void VideoComponent::skipForwardp( )
{
    if ( videoInst_ )
        videoInst_->skipForwardp( );
}


void VideoComponent::skipBackwardp( )
{
    if ( videoInst_ )
        videoInst_->skipBackwardp( );
}


void VideoComponent::pause( )
{
    if ( videoInst_ )
        videoInst_->pause( );
}


void VideoComponent::restart( )
{
    if ( videoInst_ )
        videoInst_->restart( );
}


unsigned long long VideoComponent::getCurrent( )
{
    if ( videoInst_ )
        return videoInst_->getCurrent( );
    else
        return 0;
}


unsigned long long VideoComponent::getDuration( )
{
    if ( videoInst_ )
        return videoInst_->getDuration( );
    else
        return 0;
}


bool VideoComponent::isPaused( )
{
    if ( videoInst_ )
        return videoInst_->isPaused( );
    else
        return false;
}
