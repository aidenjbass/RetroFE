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


#include "SDL.h"
#include "Database/Configuration.h"
#include "Database/GlobalOpts.h"
#include "Utility/Log.h"
#if (__APPLE__)
    #include <SDL2_mixer/SDL_mixer.h>
#else
    #include <SDL2/SDL_mixer.h>
#endif
#include "Utility/Utils.h"

std::vector<SDL_Window *>   SDL::window_;
std::vector<SDL_Renderer *> SDL::renderer_;
SDL_mutex                  *SDL::mutex_ = nullptr;
std::vector<int>            SDL::displayWidth_;
std::vector<int>            SDL::displayHeight_;
std::vector<int>            SDL::windowWidth_;
std::vector<int>            SDL::windowHeight_;
std::vector<bool>           SDL::fullscreen_;
std::vector<int>            SDL::rotation_;
std::vector<bool>           SDL::mirror_;
int                         SDL::numScreens_ = 1;
int                         SDL::numDisplays_ = 1;
int                         SDL::screenCount_;

// Initialize SDL
bool SDL::initialize( Configuration &config )
{

    int         audioRate     = MIX_DEFAULT_FREQUENCY;
    Uint16      audioFormat   = MIX_DEFAULT_FORMAT; /* 16-bit stereo */
    int         audioChannels = 1;
    int         audioBuffers  = 4096;
    bool        hideMouse;
	
#ifdef WIN32
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)) 
    {
    LOG_ERROR("SDL", "Unable to set DPI awareness hint" );
    }
#endif    

    
    LOG_INFO("SDL", "Initializing" );
    if ( SDL_Init( SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS ) != 0 ) {
        std::string error = SDL_GetError( );
        LOG_ERROR("SDL", "Initialize failed: " + error );
        return false;
    }

    if ( config.getProperty( OPTION_HIDEMOUSE, hideMouse ) ) {
        if ( hideMouse ) {
            SDL_ShowCursor( SDL_FALSE );
        }
        else {
            SDL_ShowCursor( SDL_TRUE );
        }
    }
	
	bool HardwareVideoAccel = false;
	config.getProperty(OPTION_HARDWAREVIDEOACCEL, HardwareVideoAccel);
	Configuration::HardwareVideoAccel = HardwareVideoAccel; // Set the static member variable
	int AvdecMaxThreads = 2;
	config.getProperty(OPTION_AVDECMAXTHREADS, AvdecMaxThreads);
	Configuration::AvdecMaxThreads = AvdecMaxThreads;
    int AvdecThreadType = 2;
    config.getProperty(OPTION_AVDECTHREADTYPE, AvdecThreadType);
    Configuration::AvdecThreadType = AvdecThreadType;
	bool MuteVideo = false;
	config.getProperty(OPTION_MUTEVIDEO, MuteVideo);
	Configuration::MuteVideo = MuteVideo;
	// check for a few other necessary Configurations
    config.getProperty( OPTION_NUMSCREENS, numScreens_ );

    if ( numScreens_ <= 0 ) {
        LOG_ERROR("SDL", "Number of requested displays < 0." );
        return false;
    }

    numDisplays_ = SDL_GetNumVideoDisplays( );
    LOG_INFO("SDL", "Number of displays found: " + std::to_string( numDisplays_ ) );
    LOG_INFO("SDL", "Number of displays requested: " + std::to_string( numScreens_ ) );

    // Preset the SDL settings for all monitors
    int mainScreen = 0;
    screenCount_ = std::min(numScreens_, numDisplays_);
    for (int screenNum = 0; screenNum < screenCount_; ++screenNum) {

        SDL_DisplayMode mode;
        bool            windowBorder = false;
        bool            windowResize = false;
        Uint32          windowFlags  = SDL_WINDOW_OPENGL;
        std::string     screenIndex = std::to_string(screenNum);
        config.getProperty( OPTION_WINDOWBORDER, windowBorder );
        if ( !windowBorder )
            windowFlags |= SDL_WINDOW_BORDERLESS;
        config.getProperty( OPTION_WINDOWRESIZE, windowResize );
        if ( windowResize )
            windowFlags |= SDL_WINDOW_RESIZABLE;

        if ( !config.getProperty( OPTION_SCREENNUM + screenIndex, screenNum ) && screenNum != mainScreen) {
            LOG_ERROR("SDL", OPTION_SCREENNUM + screenIndex + " parameter not defined.");
            return false;
        }

        if ( SDL_GetCurrentDisplayMode( screenNum, &mode ) != mainScreen) {
            if (screenNum == mainScreen) {
                LOG_ERROR("SDL", "Display " + std::to_string(screenNum) + " does not exist.");
                return false;
            }
            else {
                LOG_WARNING("SDL", "Display " + std::to_string(screenNum) + " does not exist.");
                windowWidth_.push_back(0);
                windowHeight_.push_back(0);
                displayWidth_.push_back(0);
                displayHeight_.push_back(0);
                window_.push_back(NULL);
                renderer_.push_back(NULL);
            }
        }
        else {
            windowWidth_.push_back(mode.w);
            displayWidth_.push_back(mode.w);
            std::string hString = "";
            if (screenNum == mainScreen)
                config.getProperty( OPTION_HORIZONTAL, hString );

            config.getProperty( OPTION_HORIZONTAL + screenIndex, hString );
            if ( hString == "" ) {
                LOG_ERROR("Configuration", "Missing property \"horizontal\"" + screenIndex );
                return false;
            }
            // from environment variable
			else if ( hString == "envvar" ) {
				hString = Utils::getEnvVar("H_RES_" + screenIndex);
                if (hString == "" || !Utils::convertInt(hString)) {
                    LOG_WARNING("Configuration", "Invalid property value for \"horizontal\"" + screenIndex + " defaulted to 'stretch'");
                }
                else {
                    LOG_WARNING("Configuration", "H_RES_" + screenIndex + " for  \"horizontal\" set to " + hString);
                    windowWidth_[screenNum] = Utils::convertInt(hString);
                }
			}
			else if ( hString != "stretch" && (screenNum != mainScreen || !config.getProperty( OPTION_HORIZONTAL, windowWidth_[screenNum] )) && !config.getProperty( OPTION_HORIZONTAL + screenIndex, windowWidth_[screenNum] )) {
                LOG_ERROR("Configuration", "Invalid property value for \"horizontal\"" + screenIndex );
                return false;
            }

            windowHeight_.push_back(mode.h);
            displayHeight_.push_back(mode.h);
            std::string vString = "";
            if (screenNum == mainScreen)
                config.getProperty( OPTION_VERTICAL, vString );

            config.getProperty( OPTION_VERTICAL + screenIndex, vString );
            if ( vString == "" ) {
                LOG_ERROR("Configuration", "Missing property \"vertical\"" + screenIndex );
                return false;
            }
            // from environment variable
			else if ( vString == "envvar") {
				vString = Utils::getEnvVar("V_RES_" + screenIndex);
                if (vString == "" || !Utils::convertInt(vString)) {
                    LOG_WARNING("Configuration", "Invalid property value for \"vertical\"" + screenIndex + " defaulted to 'stretch'");
                }
                else {
                    LOG_WARNING("Configuration", "V_RES_" + screenIndex + " for  \"vertical\" set to " + vString);
                    windowHeight_[screenNum] = Utils::convertInt(vString);

                }
			}
			else if ( vString != "stretch" && (screenNum != mainScreen || !config.getProperty( OPTION_VERTICAL, windowHeight_[screenNum] )) && !config.getProperty( OPTION_VERTICAL + screenIndex, windowHeight_[screenNum] ) ) {
                LOG_ERROR("Configuration", "Invalid property value for \"vertical\"" + screenIndex );
                return false;
            }

            bool fullscreen = false;
            config.getProperty( OPTION_FULLSCREEN, fullscreen );
            if (screenNum == mainScreen && !config.getProperty( OPTION_FULLSCREEN, fullscreen ) && !config.getProperty( OPTION_FULLSCREEN + screenIndex, fullscreen ) ) {
                LOG_ERROR("Configuration", "Missing property: \"fullscreen\"" + screenIndex );
                return false;
            }
            fullscreen_.push_back( fullscreen );

            if ( fullscreen_[screenNum] ) {
                #ifdef WIN32
                    windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
                #elif defined(__APPLE__)
                    windowFlags |= SDL_WINDOW_BORDERLESS;
                #else
                    windowFlags |= SDL_WINDOW_FULLSCREEN;
                #endif
            }

            int rotation= 0;
            config.getProperty( OPTION_ROTATION + screenIndex, rotation );
            LOG_INFO("Configuration", "Setting rotation for screen " + screenIndex + " to " + std::to_string( rotation * 90 ) + " degrees." );
            rotation_.push_back( rotation );

            bool mirror = false;
            config.getProperty( OPTION_MIRROR + screenIndex, mirror );
            if ( mirror )
                LOG_INFO("Configuration", "Setting mirror mode for screen " + screenIndex + "." );
            mirror_.push_back( mirror );

            window_.push_back(NULL);
            renderer_.push_back(NULL);
            std::string fullscreenStr = fullscreen_[screenNum] ? "yes" : "no";
            std::stringstream ss;
            ss << "Creating "<< windowWidth_[screenNum] << "x" << windowHeight_[screenNum] << " window (fullscreen: " << fullscreenStr << ")" << " on display " << screenNum;
            LOG_INFO("SDL", ss.str( ));
            std::string retrofeTitle = "RetroFE " + std::to_string(screenNum);
            if (!window_[screenNum]) {
                window_[screenNum] = SDL_CreateWindow(retrofeTitle.c_str(), SDL_WINDOWPOS_CENTERED_DISPLAY(screenNum), SDL_WINDOWPOS_CENTERED_DISPLAY(screenNum), windowWidth_[screenNum], windowHeight_[screenNum], windowFlags);
            }
			
#ifdef WIN32
			std::string SDLRenderDriver = "direct3d";
			config.getProperty(OPTION_SDLRENDERDRIVER, SDLRenderDriver);
			if ( SDL_SetHint(SDL_HINT_RENDER_DRIVER, SDLRenderDriver.c_str()) != SDL_TRUE ) {
				LOG_ERROR("SDL", "Error setting renderer to" + SDLRenderDriver + ". Available direct3d, direct3d11, direct3d12, opengl, opengles2, opengles, metal, and software");
			}
#endif			
		
            std::string ScaleQuality = "1";
			config.getProperty(OPTION_SCALEQUALITY, ScaleQuality);
			if ( SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, ScaleQuality.c_str()) != SDL_TRUE ) {
				LOG_ERROR("SDL", "Improve scale quality. Continuing with low-quality settings 1 = linear. 0 = nearest, 2 = best (linear)" );
			}

            if ( window_[screenNum] == NULL ) {
                std::string error = SDL_GetError( );
                if (screenNum == mainScreen) {
                    LOG_ERROR("SDL", "Create window " + screenIndex + " on display " + std::to_string(screenNum) + " failed: " + error );
                    return false;
                }
                else {
                    LOG_WARNING("SDL", "Create window " + screenIndex + " on display " + std::to_string(screenNum) + " failed: " + error );
                }
            }
            else {
                if (screenNum == mainScreen) {
                    #ifndef __APPLE__
                        // If not MacOS, warp cursor top right
                        SDL_WarpMouseInWindow(window_[screenNum], windowWidth_[screenNum], 0 );
                    #endif
                    #ifdef __APPLE__
                        // If MacOS, warp cursor to center to prevent hot corner activation
                        SDL_WarpMouseInWindow(window_[screenNum], windowWidth_[screenNum] / 2, windowHeight_[screenNum] /2 );
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    #endif
                }
                bool vSync = false;
				config.getProperty(OPTION_VSYNC, vSync);
                if (!renderer_[screenNum]) {
                    if (vSync == true) {
                        renderer_[screenNum] = SDL_CreateRenderer(window_[screenNum], -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
                        LOG_INFO("SDL", "vSync Enabled");
                    }
                    else {
                        renderer_[screenNum] = SDL_CreateRenderer(window_[screenNum], -1, SDL_RENDERER_ACCELERATED);
                    }
                }
				if (renderer_[screenNum] == NULL)  {
					std::string error = SDL_GetError();
					LOG_ERROR("SDL", "Create renderer " + screenIndex + " failed: " + error);
					return false;
				}
				else  {
					SDL_RendererInfo info;
                    if (SDL_GetRendererInfo(renderer_[screenNum], &info) == 0) {
                        std::string screenIndexStr = std::to_string(screenNum);
                        std::string logMessage = "Current rendering backend for renderer " + screenIndexStr + ": ";
                        logMessage += info.name;
                        LOG_INFO("SDL", logMessage);
                        if (strcmp(info.name, "opengl") == 0) {
                            int GlSwapInterval = 1;
                            config.getProperty(OPTION_GLSWAPINTERVAL, GlSwapInterval);
                            if (SDL_GL_SetSwapInterval(GlSwapInterval) < 0) {
                                LOG_ERROR("SDL", "Unable to set OpenGL swap interval: " + std::string(SDL_GetError()));
                            }
                        }
                    }
					else  {
						LOG_ERROR("SDL", "Could not retrieve renderer info for renderer " + screenIndex + " Error: " + SDL_GetError());
					}
				}
            }
        }
    }



    if (bool minimizeOnFocusLoss;  config.getProperty( OPTION_MINIMIZEONFOCUSLOSS, minimizeOnFocusLoss ) ) {
        if ( minimizeOnFocusLoss ) {
            SDL_SetHintWithPriority( SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1", SDL_HINT_OVERRIDE );
        }
        else {
            SDL_SetHintWithPriority( SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0", SDL_HINT_OVERRIDE );
        }
    }

    mutex_ = SDL_CreateMutex( );

    if ( mutex_ == nullptr ) {
        std::string error = SDL_GetError( );
        LOG_ERROR("SDL", "Mutex creation failed: " + error );
        return false;
    }

    if ( Mix_OpenAudio( audioRate, audioFormat, audioChannels, audioBuffers ) == -1 ) {
        std::string error = Mix_GetError( );
        LOG_WARNING("SDL", "Audio initialize failed: " + error );
    }

    return true;

}


// Deinitialize SDL
bool SDL::deInitialize( )
{
    std::string error = SDL_GetError( );
    LOG_INFO("SDL", "DeInitializing" );

    if (window_[0] != NULL) {
        // If on MacOS disable relative mouse mode
        #ifdef __APPLE__
            SDL_SetRelativeMouseMode(SDL_FALSE);
        #endif
        // Center mouse in primary window
        SDL_WarpMouseInWindow(window_[0], windowWidth_[0] / 2, windowHeight_[0] / 2);
    }
    else {
        LOG_WARNING("SDL", "Window 0 is NULL, cannot center mouse within it");
    }

    Mix_CloseAudio( );
    Mix_Quit( );

    if ( mutex_ ) {
        SDL_DestroyMutex(mutex_);
        mutex_ = nullptr;
    }

    
    for ( int i = 0; i < screenCount_; ++i ) {
        if ( !renderer_.empty() ) {
            if ( renderer_[0] ) {
                SDL_DestroyRenderer( renderer_[0] );
            }
            renderer_.erase( renderer_.begin( ) );
        }

        if ( !window_.empty() ) {
            if ( window_[0] ) {
                SDL_DestroyWindow( window_[0] );
            }
            window_.erase( window_.begin( ) );
        }
    }
    displayWidth_.clear( );
    displayHeight_.clear( );
    windowWidth_.clear( );
    windowHeight_.clear( );
    fullscreen_.clear( );

    SDL_ShowCursor( SDL_TRUE );

    SDL_Quit( );

    return true;
}


// Get the renderer
SDL_Renderer* SDL::getRenderer( int index )
{
    return (index < screenCount_ ? renderer_[index] : renderer_[0]);
}

std::string SDL::getRendererBackend(int index) {
    SDL_Renderer* renderer = getRenderer(index);
    if (!renderer) {
        return "Invalid renderer index";
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) != 0) {
        return std::string("Error getting renderer info: ") + SDL_GetError();
    }

    return std::string(info.name);
}

// Get the mutex
SDL_mutex* SDL::getMutex( )
{
    return mutex_;
}


// Get the window
SDL_Window* SDL::getWindow( int index )
{
    return (index < screenCount_ ? window_[index] : window_[0]);
}


// Render a copy of a texture
bool SDL::renderCopy( SDL_Texture *texture, float alpha, SDL_Rect const *src, SDL_Rect const *dest, ViewInfo &viewInfo, int layoutWidth, int layoutHeight )
{

    // Skip rendering if the object is invisible anyway or if renderer does not exist
    if ( alpha == 0 || viewInfo.Monitor >= screenCount_ || !renderer_[viewInfo.Monitor] )
        return true;
    SDL_GetWindowSize( getWindow( viewInfo.Monitor ), &windowWidth_[viewInfo.Monitor], &windowHeight_[viewInfo.Monitor] );

    float scaleX = (float)windowWidth_[viewInfo.Monitor]  / (float)layoutWidth;
    float scaleY = (float)windowHeight_[viewInfo.Monitor] / (float)layoutHeight;

    // 90 or 270 degree rotation; change scale factors
    if ( rotation_[viewInfo.Monitor] % 2 == 1 ) {
        scaleX = (float)windowHeight_[viewInfo.Monitor] / (float)layoutWidth;
        scaleY = (float)windowWidth_[viewInfo.Monitor]  / (float)layoutHeight;
    }

    if ( mirror_[viewInfo.Monitor] )
        scaleY /= 2;

    // Don't print outside the screen in mirror mode
    if ( mirror_[viewInfo.Monitor] && (viewInfo.ContainerWidth < 0 || viewInfo.ContainerHeight < 0) ) {
        viewInfo.ContainerX      = 0;
        viewInfo.ContainerY      = 0;
        viewInfo.ContainerWidth  = static_cast<float>( layoutWidth );
        viewInfo.ContainerHeight = static_cast<float>( layoutHeight );
    }

    SDL_Rect srcRect{};
    SDL_Rect dstRect{};
    SDL_Rect srcRectCopy{};
    SDL_Rect dstRectCopy{};
    SDL_Rect srcRectOrig{};
    SDL_Rect dstRectOrig{};
    double   imageScaleX;
    double   imageScaleY;

    dstRect.w = dest->w;
    dstRect.h = dest->h;

    if ( fullscreen_[viewInfo.Monitor] ) {
        dstRect.x = dest->x + (displayWidth_[viewInfo.Monitor] - windowWidth_[viewInfo.Monitor])/2;
        dstRect.y = dest->y + (displayHeight_[viewInfo.Monitor] - windowHeight_[viewInfo.Monitor])/2;
    }
    else {
        dstRect.x = dest->x;
        dstRect.y = dest->y;
    }

    // Create the base fields to check against the container.
    if (src) {
        srcRect.x = src->x;
        srcRect.y = src->y;
        srcRect.w = src->w;
        srcRect.h = src->h;
    }
    else {
        srcRect.x = 0;
        srcRect.y = 0;
        int w = 0;
        int h = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
        srcRect.w = w;
        srcRect.h = h;
    }

    // Define the scale
    imageScaleX = (dstRect.w > 0) ? static_cast<double>( srcRect.w ) / static_cast<double>( dstRect.w ) : 0.0;
    imageScaleY = (dstRect.h > 0) ? static_cast<double>( srcRect.h ) / static_cast<double>( dstRect.h ) : 0.0;

    // Make two copies
    srcRectOrig.x = srcRect.x;
    srcRectOrig.y = srcRect.y;
    srcRectOrig.w = srcRect.w;
    srcRectOrig.h = srcRect.h;
    dstRectOrig.x = dstRect.x;
    dstRectOrig.y = dstRect.y;
    dstRectOrig.w = dstRect.w;
    dstRectOrig.h = dstRect.h;

    srcRectCopy.x = srcRect.x;
    srcRectCopy.y = srcRect.y;
    srcRectCopy.w = srcRect.w;
    srcRectCopy.h = srcRect.h;
    dstRectCopy.x = dstRect.x;
    dstRectCopy.y = dstRect.y;
    dstRectCopy.w = dstRect.w;
    dstRectCopy.h = dstRect.h;

    // If a container has been defined, limit the display to the container boundaries.
    if ( viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 &&
         dstRectCopy.w           > 0 && dstRectCopy.h            > 0 ) {

        // Correct if the image falls to the left of the container
        if ( dstRect.x < viewInfo.ContainerX ) {
            dstRect.x = static_cast<int>( viewInfo.ContainerX );
            dstRect.w = dstRectCopy.w + dstRectCopy.x - dstRect.x;
            srcRect.x = srcRectCopy.x + srcRectCopy.w * (dstRect.x - dstRectCopy.x) / dstRectCopy.w;
        }

        // Correct if the image falls to the right of the container
        if ( (dstRectCopy.x + dstRectCopy.w) > (viewInfo.ContainerX + viewInfo.ContainerWidth) ) {
            dstRect.w = static_cast<int>( viewInfo.ContainerX + viewInfo.ContainerWidth ) - dstRect.x;
        }

        // Correct if the image falls to the top of the container
        if ( dstRect.y < viewInfo.ContainerY ) {
            dstRect.y = static_cast<int>( viewInfo.ContainerY );
            dstRect.h = dstRectCopy.h + dstRectCopy.y - dstRect.y;
            srcRect.y = srcRectCopy.y + srcRectCopy.h * (dstRect.y - dstRectCopy.y) / dstRectCopy.h;
        }

        // Correct if the image falls to the bottom of the container
        if ( (dstRectCopy.y + dstRectCopy.h) > (viewInfo.ContainerY + viewInfo.ContainerHeight) ) {
            dstRect.h = static_cast<int>( viewInfo.ContainerY + viewInfo.ContainerHeight ) - dstRect.y;
        }

        // Define source width and height
        srcRect.w = static_cast<int>( dstRect.w * imageScaleX );
        srcRect.h = static_cast<int>( dstRect.h * imageScaleY );

    }

    double angle = viewInfo.Angle;
    if ( !mirror_[viewInfo.Monitor] )
        angle += rotation_[viewInfo.Monitor] * 90;

    dstRect.x = (int)(dstRect.x*scaleX);
    dstRect.y = (int)(dstRect.y*scaleY);
    dstRect.w = (int)(dstRect.w*scaleX);
    dstRect.h = (int)(dstRect.h*scaleY);

    if ( mirror_[viewInfo.Monitor] ) {
        if ( rotation_[viewInfo.Monitor] % 2 == 0 ) {
            if ( srcRect.h > 0 && srcRect.w > 0 ) {
                dstRect.y += windowHeight_[viewInfo.Monitor] / 2;
                SDL_SetTextureAlphaMod( texture, static_cast<char>( alpha * 255 ) );
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE );
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                angle    += 180;
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE );
            }
        }
        else {
            if ( srcRect.h > 0 && srcRect.w > 0 ) {
                int tmp   = dstRect.x;
                dstRect.x = windowWidth_[viewInfo.Monitor]/2 - dstRect.y - dstRect.h/2 - dstRect.w/2;
                dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
                angle    += 90;
                SDL_SetTextureAlphaMod( texture, static_cast<char>( alpha * 255 ) );
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE );
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                angle    += 180;
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE );
            }
        }
    }
    else {
        // 90 degree rotation
        if ( rotation_[viewInfo.Monitor] == 1 ) {
            int tmp   = dstRect.x;
            dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.y - dstRect.h/2 - dstRect.w/2;
            dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
        }
        // 180 degree rotation
        if ( rotation_[viewInfo.Monitor] == 2 ) {
            dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
            dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
        }
        // 270 degree rotation
        if ( rotation_[viewInfo.Monitor] == 3 ) {
            int tmp   = dstRect.x;
            dstRect.x = dstRect.y + dstRect.h/2 - dstRect.w/2;
            dstRect.y = windowHeight_[viewInfo.Monitor] - tmp - dstRect.h/2 - dstRect.w/2;
        }
    
        if ( srcRect.h > 0 && srcRect.w > 0 ) {
            SDL_SetTextureAlphaMod( texture, static_cast<char>( alpha * 255 ) );
            SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE );
        }
    }

    // Restore original parameters
    srcRect.x     = srcRectOrig.x;
    srcRect.y     = srcRectOrig.y;
    srcRect.w     = srcRectOrig.w;
    srcRect.h     = srcRectOrig.h;
    dstRect.x     = dstRectOrig.x;
    dstRect.y     = dstRectOrig.y;
    dstRect.w     = dstRectOrig.w;
    dstRect.h     = dstRectOrig.h;
    srcRectCopy.x = srcRectOrig.x;
    srcRectCopy.y = srcRectOrig.y;
    srcRectCopy.w = srcRectOrig.w;
    srcRectCopy.h = srcRectOrig.h;
    dstRectCopy.x = dstRectOrig.x;
    dstRectCopy.y = dstRectOrig.y;
    dstRectCopy.w = dstRectOrig.w;
    dstRectCopy.h = dstRectOrig.h;

    if ( viewInfo.Reflection.find("top") != std::string::npos ) {
        dstRect.h     = static_cast<unsigned int>( static_cast<float>(dstRect.h ) * viewInfo.ReflectionScale);
        dstRect.y     = dstRect.y - dstRect.h - viewInfo.ReflectionDistance;
        imageScaleY   = (dstRect.h > 0) ? static_cast<double>( srcRect.h ) / static_cast<double>( dstRect.h ) : 0.0;
        dstRectCopy.y = dstRect.y;
        dstRectCopy.h = dstRect.h;

        // If a container has been defined, limit the display to the container boundaries.
        if ( viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 &&
             dstRectCopy.w           > 0 && dstRectCopy.h            > 0 ) {

            // Correct if the image falls to the left of the container
            if ( dstRect.x < viewInfo.ContainerX ) {
                dstRect.x = static_cast<int>( viewInfo.ContainerX );
                dstRect.w = dstRectCopy.w + dstRectCopy.x - dstRect.x;
                srcRect.x = srcRectCopy.x + srcRectCopy.w * (dstRect.x - dstRectCopy.x) / dstRectCopy.w;
            }

            // Correct if the image falls to the right of the container
            if ( (dstRectCopy.x + dstRectCopy.w) > (viewInfo.ContainerX + viewInfo.ContainerWidth) ) {
                dstRect.w = static_cast<int>( viewInfo.ContainerX + viewInfo.ContainerWidth ) - dstRect.x;
            }

            // Correct if the image falls to the top of the container
            if ( dstRect.y < viewInfo.ContainerY ) {
                dstRect.y = static_cast<int>( viewInfo.ContainerY );
                dstRect.h = dstRectCopy.h + dstRectCopy.y - dstRect.y;
            }

            // Correct if the image falls to the bottom of the container
            if ( (dstRectCopy.y + dstRectCopy.h) > (viewInfo.ContainerY + viewInfo.ContainerHeight) ) {
                dstRect.h = static_cast<int>( viewInfo.ContainerY + viewInfo.ContainerHeight ) - dstRect.y;
                srcRect.y = srcRectCopy.y + srcRectCopy.h * (dstRectCopy.h - dstRect.h) / dstRectCopy.h;
            }

            // Define source width and height
            srcRect.w = static_cast<int>( dstRect.w * imageScaleX );
            srcRect.h = static_cast<int>( dstRect.h * imageScaleY );

        }

        angle = viewInfo.Angle;
        if ( !mirror_[viewInfo.Monitor] )
            angle += rotation_[viewInfo.Monitor] * 90;

        dstRect.x = (int)(dstRect.x*scaleX);
        dstRect.y = (int)(dstRect.y*scaleY);
        dstRect.w = (int)(dstRect.w*scaleX);
        dstRect.h = (int)(dstRect.h*scaleY);

        if ( mirror_[viewInfo.Monitor] ) {
            if ( rotation_[viewInfo.Monitor] % 2 == 0 ) {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    dstRect.y += windowHeight_[viewInfo.Monitor] / 2;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                }
            }
            else {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    int tmp   = dstRect.x;
                    dstRect.x = windowWidth_[viewInfo.Monitor]/2 - dstRect.y - dstRect.h/2 - dstRect.w/2;
                    dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
                    angle    += 90;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                }
            }
        }
        else {
            // 90 degree rotation
            if ( rotation_[viewInfo.Monitor] == 1 ) {
                int tmp   = dstRect.x;
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.y - dstRect.h/2 - dstRect.w/2;
                dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
            }
            // 180 degree rotation
            if ( rotation_[viewInfo.Monitor] == 2 ) {
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
            }
            // 270 degree rotation
            if ( rotation_[viewInfo.Monitor] == 3 ) {
                int tmp   = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h/2 - dstRect.w/2;
                dstRect.y = windowHeight_[viewInfo.Monitor] - tmp - dstRect.h/2 - dstRect.w/2;
            }
            if ( srcRect.h > 0 && srcRect.w > 0 ) {
                SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
            }
        }
    }

    // Restore original parameters
    srcRect.x     = srcRectOrig.x;
    srcRect.y     = srcRectOrig.y;
    srcRect.w     = srcRectOrig.w;
    srcRect.h     = srcRectOrig.h;
    dstRect.x     = dstRectOrig.x;
    dstRect.y     = dstRectOrig.y;
    dstRect.w     = dstRectOrig.w;
    dstRect.h     = dstRectOrig.h;
    srcRectCopy.x = srcRectOrig.x;
    srcRectCopy.y = srcRectOrig.y;
    srcRectCopy.w = srcRectOrig.w;
    srcRectCopy.h = srcRectOrig.h;
    dstRectCopy.x = dstRectOrig.x;
    dstRectCopy.y = dstRectOrig.y;
    dstRectCopy.w = dstRectOrig.w;
    dstRectCopy.h = dstRectOrig.h;

    if ( viewInfo.Reflection.find("bottom") != std::string::npos ) {
        dstRect.y     = dstRect.y + dstRect.h + viewInfo.ReflectionDistance;
        dstRect.h     = static_cast<unsigned int>( static_cast<float>(dstRect.h ) * viewInfo.ReflectionScale);
        imageScaleY   = (dstRect.h > 0) ? static_cast<double>( srcRect.h ) / static_cast<double>( dstRect.h ) : 0.0;
        dstRectCopy.y = dstRect.y;
        dstRectCopy.h = dstRect.h;

        // If a container has been defined, limit the display to the container boundaries.
        if ( viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 &&
             dstRectCopy.w           > 0 && dstRectCopy.h            > 0 ) {

            // Correct if the image falls to the left of the container
            if ( dstRect.x < viewInfo.ContainerX ) {
                dstRect.x = static_cast<int>( viewInfo.ContainerX );
                dstRect.w = dstRectCopy.w + dstRectCopy.x - dstRect.x;
                srcRect.x = srcRectCopy.x + srcRectCopy.w * (dstRect.x - dstRectCopy.x) / dstRectCopy.w;
            }
            // Correct if the image falls to the right of the container
            if ( (dstRectCopy.x + dstRectCopy.w) > (viewInfo.ContainerX + viewInfo.ContainerWidth) ) {
                dstRect.w = static_cast<int>( viewInfo.ContainerX + viewInfo.ContainerWidth ) - dstRect.x;
            }
            // Correct if the image falls to the top of the container
            if ( dstRect.y < viewInfo.ContainerY ) {
                dstRect.y = static_cast<int>( viewInfo.ContainerY );
                dstRect.h = dstRectCopy.h + dstRectCopy.y - dstRect.y;
            }
            // Correct if the image falls to the bottom of the container
            if ( (dstRectCopy.y + dstRectCopy.h) > (viewInfo.ContainerY + viewInfo.ContainerHeight) ){
                dstRect.h = static_cast<int>( viewInfo.ContainerY + viewInfo.ContainerHeight ) - dstRect.y;
                srcRect.y = srcRectCopy.y + srcRectCopy.h * (dstRectCopy.h - dstRect.h) / dstRectCopy.h;
            }
            // Define source width and height
            srcRect.w = static_cast<int>( dstRect.w * imageScaleX );
            srcRect.h = static_cast<int>( dstRect.h * imageScaleY );
        }

        angle = viewInfo.Angle;
        if ( !mirror_[viewInfo.Monitor] )
            angle += rotation_[viewInfo.Monitor] * 90;

        dstRect.x = (int)(dstRect.x*scaleX);
        dstRect.y = (int)(dstRect.y*scaleY);
        dstRect.w = (int)(dstRect.w*scaleX);
        dstRect.h = (int)(dstRect.h*scaleY);

        if ( mirror_[viewInfo.Monitor] ) {
            if ( rotation_[viewInfo.Monitor] % 2 == 0 ) {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    dstRect.y += windowHeight_[viewInfo.Monitor] / 2;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                }
            }
            else {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    int tmp   = dstRect.x;
                    dstRect.x = windowWidth_[viewInfo.Monitor]/2 - dstRect.y - dstRect.h/2 - dstRect.w/2;
                    dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
                    angle    += 90;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
                }
            }
        }
        else {
            // 90 degree rotation
            if ( rotation_[viewInfo.Monitor] == 1 ) {
                int tmp   = dstRect.x;
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.y - dstRect.h/2 - dstRect.w/2;
                dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
            }
            // 180 degree rotation
            if ( rotation_[viewInfo.Monitor] == 2 ) {
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
            }
            // 270 degree rotation
            if ( rotation_[viewInfo.Monitor] == 3 ) {
                int tmp   = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h/2 - dstRect.w/2;
                dstRect.y = windowHeight_[viewInfo.Monitor] - tmp - dstRect.h/2 - dstRect.w/2;
            }
            if ( srcRect.h > 0 && srcRect.w > 0 ) {
                SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL );
            }
        }
    }

    // Restore original parameters
    srcRect.x     = srcRectOrig.x;
    srcRect.y     = srcRectOrig.y;
    srcRect.w     = srcRectOrig.w;
    srcRect.h     = srcRectOrig.h;
    dstRect.x     = dstRectOrig.x;
    dstRect.y     = dstRectOrig.y;
    dstRect.w     = dstRectOrig.w;
    dstRect.h     = dstRectOrig.h;
    srcRectCopy.x = srcRectOrig.x;
    srcRectCopy.y = srcRectOrig.y;
    srcRectCopy.w = srcRectOrig.w;
    srcRectCopy.h = srcRectOrig.h;
    dstRectCopy.x = dstRectOrig.x;
    dstRectCopy.y = dstRectOrig.y;
    dstRectCopy.w = dstRectOrig.w;
    dstRectCopy.h = dstRectOrig.h;

    if ( viewInfo.Reflection.find("left") != std::string::npos ) {
        dstRect.w     = static_cast<unsigned int>( static_cast<float>(dstRect.w ) * viewInfo.ReflectionScale);
        dstRect.x     = dstRect.x - dstRect.w - viewInfo.ReflectionDistance;
        imageScaleX   = (dstRect.h > 0) ? static_cast<double>( srcRect.w ) / static_cast<double>( dstRect.w ) : 0.0;
        dstRectCopy.x = dstRect.x;
        dstRectCopy.w = dstRect.w;

        // If a container has been defined, limit the display to the container boundaries.
        if ( viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 &&
             dstRectCopy.w           > 0 && dstRectCopy.h            > 0 ) {
            // Correct if the image falls to the left of the container
            if ( dstRect.x < viewInfo.ContainerX ) {
                dstRect.x = static_cast<int>( viewInfo.ContainerX );
                dstRect.w = dstRectCopy.w + dstRectCopy.x - dstRect.x;
            }
            // Correct if the image falls to the right of the container
            if ( (dstRectCopy.x + dstRectCopy.w) > (viewInfo.ContainerX + viewInfo.ContainerWidth) ) {
                dstRect.w = static_cast<int>( viewInfo.ContainerX + viewInfo.ContainerWidth ) - dstRect.x;
                srcRect.x = srcRectCopy.x + srcRectCopy.w * (dstRectCopy.w - dstRect.w) / dstRectCopy.w;
            }
            // Correct if the image falls to the top of the container
            if ( dstRect.y < viewInfo.ContainerY ) {
                dstRect.y = static_cast<int>( viewInfo.ContainerY );
                dstRect.h = dstRectCopy.h + dstRectCopy.y - dstRect.y;
                srcRect.y = srcRectCopy.y + srcRectCopy.h * (dstRect.y - dstRectCopy.y) / dstRectCopy.h;
            }
            // Correct if the image falls to the bottom of the container
            if ( (dstRectCopy.y + dstRectCopy.h) > (viewInfo.ContainerY + viewInfo.ContainerHeight) ) {
                dstRect.h = static_cast<int>( viewInfo.ContainerY + viewInfo.ContainerHeight ) - dstRect.y;
            }
            // Define source width and height
            srcRect.w = static_cast<int>( dstRect.w * imageScaleX );
            srcRect.h = static_cast<int>( dstRect.h * imageScaleY );

        }

        angle = viewInfo.Angle;
        if ( !mirror_[viewInfo.Monitor] )
            angle += rotation_[viewInfo.Monitor] * 90;

        dstRect.x = (int)(dstRect.x*scaleX);
        dstRect.y = (int)(dstRect.y*scaleY);
        dstRect.w = (int)(dstRect.w*scaleX);
        dstRect.h = (int)(dstRect.h*scaleY);

        if ( mirror_[viewInfo.Monitor] ) {
            if ( rotation_[viewInfo.Monitor] % 2 == 0 ) {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    dstRect.y += windowHeight_[viewInfo.Monitor] / 2;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                }
            }
            else {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    int tmp   = dstRect.x;
                    dstRect.x = windowWidth_[viewInfo.Monitor]/2 - dstRect.y - dstRect.h/2 - dstRect.w/2;
                    dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
                    angle    += 90;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                }
            }
        }
        else {
            // 90 degree rotation
            if ( rotation_[viewInfo.Monitor] == 1 ) {
                int tmp   = dstRect.x;
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.y - dstRect.h/2 - dstRect.w/2;
                dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
            }
            // 180 degree rotation
            if ( rotation_[viewInfo.Monitor] == 2 ) {
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
            }
            // 270 degree rotation
            if ( rotation_[viewInfo.Monitor] == 3 ) {
                int tmp   = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h/2 - dstRect.w/2;
                dstRect.y = windowHeight_[viewInfo.Monitor] - tmp - dstRect.h/2 - dstRect.w/2;
            }
            if ( srcRect.h > 0 && srcRect.w > 0 ) {
                SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
            }
        }
    }

    // Restore original parameters
    srcRect.x     = srcRectOrig.x;
    srcRect.y     = srcRectOrig.y;
    srcRect.w     = srcRectOrig.w;
    srcRect.h     = srcRectOrig.h;
    dstRect.x     = dstRectOrig.x;
    dstRect.y     = dstRectOrig.y;
    dstRect.w     = dstRectOrig.w;
    dstRect.h     = dstRectOrig.h;
    srcRectCopy.x = srcRectOrig.x;
    srcRectCopy.y = srcRectOrig.y;
    srcRectCopy.w = srcRectOrig.w;
    srcRectCopy.h = srcRectOrig.h;
    dstRectCopy.x = dstRectOrig.x;
    dstRectCopy.y = dstRectOrig.y;
    dstRectCopy.w = dstRectOrig.w;
    dstRectCopy.h = dstRectOrig.h;

    if ( viewInfo.Reflection.find("right") != std::string::npos ) {
        dstRect.x     = dstRect.x + dstRect.w + viewInfo.ReflectionDistance;
        dstRect.w     = static_cast<unsigned int>( static_cast<float>(dstRect.w ) * viewInfo.ReflectionScale);
        imageScaleX   = (dstRect.h > 0) ? static_cast<double>( srcRect.w ) / static_cast<double>( dstRect.w ) : 0.0;
        dstRectCopy.x = dstRect.x;
        dstRectCopy.w = dstRect.w;

        // If a container has been defined, limit the display to the container boundaries.
        if ( viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 &&
             dstRectCopy.w           > 0 && dstRectCopy.h            > 0 ) {
            // Correct if the image falls to the left of the container
            if ( dstRect.x < viewInfo.ContainerX ) {
                dstRect.x = static_cast<int>( viewInfo.ContainerX );
                dstRect.w = dstRectCopy.w + dstRectCopy.x - dstRect.x;
            }
            // Correct if the image falls to the right of the container
            if ( (dstRectCopy.x + dstRectCopy.w) > (viewInfo.ContainerX + viewInfo.ContainerWidth) ) {
                dstRect.w = static_cast<int>( viewInfo.ContainerX + viewInfo.ContainerWidth ) - dstRect.x;
                srcRect.x = srcRectCopy.x + srcRectCopy.w * (dstRectCopy.w - dstRect.w) / dstRectCopy.w;
            }
            // Correct if the image falls to the top of the container
            if ( dstRect.y < viewInfo.ContainerY ) {
                dstRect.y = static_cast<int>( viewInfo.ContainerY );
                dstRect.h = dstRectCopy.h + dstRectCopy.y - dstRect.y;
                srcRect.y = srcRectCopy.y + srcRectCopy.h * (dstRect.y - dstRectCopy.y) / dstRectCopy.h;
            }
            // Correct if the image falls to the bottom of the container
            if ( (dstRectCopy.y + dstRectCopy.h) > (viewInfo.ContainerY + viewInfo.ContainerHeight) ) {
                dstRect.h = static_cast<int>( viewInfo.ContainerY + viewInfo.ContainerHeight ) - dstRect.y;
            }
            // Define source width and height
            srcRect.w = static_cast<int>( dstRect.w * imageScaleX );
            srcRect.h = static_cast<int>( dstRect.h * imageScaleY );

        }

        angle = viewInfo.Angle;
        if ( !mirror_[viewInfo.Monitor] )
            angle += rotation_[viewInfo.Monitor] * 90;

        dstRect.x = (int)(dstRect.x*scaleX);
        dstRect.y = (int)(dstRect.y*scaleY);
        dstRect.w = (int)(dstRect.w*scaleX);
        dstRect.h = (int)(dstRect.h*scaleY);

        if ( mirror_[viewInfo.Monitor] ) {
            if ( rotation_[viewInfo.Monitor] % 2 == 0 ) {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    dstRect.y += windowHeight_[viewInfo.Monitor] / 2;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                }
            }
            else {
                if ( srcRect.h > 0 && srcRect.w > 0 ) {
                    int tmp   = dstRect.x;
                    dstRect.x = windowWidth_[viewInfo.Monitor]/2 - dstRect.y - dstRect.h/2 - dstRect.w/2;
                    dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
                    angle    += 90;
                    SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                    dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                    dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
                    angle    += 180;
                    SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
                }
            }
        }
        else {
            // 90 degree rotation
            if ( rotation_[viewInfo.Monitor] == 1 ) {
                int tmp   = dstRect.x;
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.y - dstRect.h/2 - dstRect.w/2;
                dstRect.y = tmp - dstRect.h/2 + dstRect.w/2;
            }
            // 180 degree rotation
            if ( rotation_[viewInfo.Monitor] == 2 ) {
                dstRect.x = windowWidth_[viewInfo.Monitor] - dstRect.x - dstRect.w;
                dstRect.y = windowHeight_[viewInfo.Monitor] - dstRect.y - dstRect.h;
            }
            // 270 degree rotation
            if ( rotation_[viewInfo.Monitor] == 3 ) {
                int tmp   = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h/2 - dstRect.w/2;
                dstRect.y = windowHeight_[viewInfo.Monitor] - tmp - dstRect.h/2 - dstRect.w/2;
            }
            if ( srcRect.h > 0 && srcRect.w > 0 ) {
                SDL_SetTextureAlphaMod( texture, static_cast<char>( viewInfo.ReflectionAlpha * alpha * 255 ) );
                SDL_RenderCopyEx( renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL );
            }
        }
    }
    return true;
}
