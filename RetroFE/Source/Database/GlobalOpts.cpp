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

//**************************************************************************
//  GLOBAL SETTINGS OPTIONS
//**************************************************************************

#include "GlobalOpts.h"
#include <iostream>
#include <string>

const options_entry global_options::s_option_entries[] =
{
    
    { nullptr,                        nullptr,     global_options::option_type::HEADER,   "LOGGING OPTIONS" },
    { OPTION_LOGGING,                 "NONE",      global_options::option_type::STRING,   "Set logging level, any combo of ERROR,INFO,NOTICE,WARNING,DEBUG,FILECACHE or ALL" },

    { nullptr,                        nullptr,     global_options::option_type::HEADER,   "DISPLAY OPTIONS" },
    { OPTION_NUMSCREENS,              "",          global_options::option_type::INTEGER,  "Defines the number of monitors used" },
    { OPTION_FULLSCREEN,              "",          global_options::option_type::BOOLEAN,  "Run the frontend in fullscreen" },
    { OPTION_HORIZONTAL,              "",          global_options::option_type::INTEGER,  "Pixel width" },
    { OPTION_VERTICAL,                "",          global_options::option_type::INTEGER,  "Pixel height" },
    { OPTION_FULLSCREENX,             "",          global_options::option_type::BOOLEAN,  "Run the frontend in fullscreen for monitor x" },
    { OPTION_HORIZONTALX,             "",          global_options::option_type::INTEGER,  "Pixel width for monitor x" },
    { OPTION_VERTICALX,               "",          global_options::option_type::INTEGER,  "Pixel height for monitor x" },
    { OPTION_MIRRORX,                 "",          global_options::option_type::BOOLEAN,  "Divides monitor x into two halves" },
    { OPTION_ROTATIONX,               "",          global_options::option_type::INTEGER,  "Rotation of monitor x (1, 2, 3, 4)" },

    { nullptr,                        nullptr,     global_options::option_type::HEADER,   "WINDOW OPTIONS" },
    { OPTION_WINDOWBORDER,            "",          global_options::option_type::BOOLEAN,  "Show window border" },
    { OPTION_WINDOWRESIZE,            "",          global_options::option_type::BOOLEAN,  "Allow window to be resized" },
    { OPTION_FPS,                     "",          global_options::option_type::INTEGER,  "Requested FPS while in an active state" },
    { OPTION_FPSIDLE,                 "",          global_options::option_type::INTEGER,  "Request FPS while in an idle state" },
    { OPTION_HIDEMOUSE,               "",          global_options::option_type::BOOLEAN,  "Defines whether the mouse cursor is hidden" },
    { OPTION_ANIMATEDURINGGAME,       "",          global_options::option_type::BOOLEAN,  "Pause animated marquees while in the game" },

    { nullptr,                        nullptr,     global_options::option_type::HEADER,   "VIDEO OPTIONS" },
    { OPTION_VIDEOENABLE,             "",          global_options::option_type::BOOLEAN,  "Defines whether video is rendered" },
    { OPTION_VIDEOLOOP,               "",          global_options::option_type::INTEGER,  "Number of times to play video, 0 forever" },
    { OPTION_DISABLEVIDEORESTART,     "",          global_options::option_type::BOOLEAN,  "Pauses video while scrolling" },
    { OPTION_DISABLEPAUSEONSCROLL,    "",          global_options::option_type::BOOLEAN,  "Restart video when selected" },

    { nullptr,                         nullptr,    global_options::option_type::HEADER,   "RENDERER OPTIONS" },
    { OPTION_VSYNC,                    "",         global_options::option_type::BOOLEAN,  "Vertical synchronization" },
    { OPTION_HARDWAREVIDEOACCEL,       "",         global_options::option_type::BOOLEAN,  "Hardware decoding" },
    { OPTION_AVDECMAXTHREADS,          "",         global_options::option_type::INTEGER,  "Number of threads for avdec software decoding" },
    { OPTION_MUTEVIDEO,                "",         global_options::option_type::BOOLEAN,  "Video playback is muted" },
    { OPTION_SDLRENDERDRIVER,          "",         global_options::option_type::STRING,   "SDL Render Driver (direct3d, direct3d11, direct3d12, opengl, opengles2, opengles, metal, and software)" },
    { OPTION_SCALEQUALITY,             "",         global_options::option_type::INTEGER,  "Scaling quality (0, 1, 2)" },
    { OPTION_HIGHPRIORITY,             "",         global_options::option_type::BOOLEAN,  "RetroFE Windows process priority" },
    { OPTION_UNLOADSDL,                "",         global_options::option_type::BOOLEAN,  "Close SDL when launching a game, needed for RPI" },
    { OPTION_MINIMIZE_ON_FOCUS_LOSS,   "",         global_options::option_type::BOOLEAN,  "Minimize RetroFE when focus is lost" },

    { nullptr,                         nullptr,    global_options::option_type::HEADER,   "CUSTOMIZATION OPTIONS" },
    { OPTION_LAYOUT,                   "",         global_options::option_type::STRING,   "Theme to be used in RetroFE, a folder name in /layouts" },
    { OPTION_RANDOMLAYOUT,             "",         global_options::option_type::STRING,   "Randomly choose a layout on launch, CSV list of layout names" },
    { OPTION_FIRSTPLAYLIST,            "",         global_options::option_type::STRING,   "Start on this playlist if available" },
    { OPTION_CYCLEPLAYLIST,            "",         global_options::option_type::STRING,   "Set of playlists that can be cycled through, CSV list of playlist names" },
    { OPTION_FIRSTCOLLECTION,          "",         global_options::option_type::STRING,   "Start on this collection if available" },
    { OPTION_CYCLECOLLECTION,          "",         global_options::option_type::STRING,   "Set of collections that can be cycled through, CSV list of collection names" },
    { OPTION_LASTPLAYEDSIZE,           "",         global_options::option_type::INTEGER,  "Size of the auto-generated last played playlist, 0 to disable" },
    { OPTION_LASTPLAYEDSKIPCOLLECTION, "",         global_options::option_type::STRING,   "Exclude a collection from being added to last played, settings for example" },
    { OPTION_ENTERONCOLLECTION,        "",         global_options::option_type::BOOLEAN,  "Enter the collection when using collection up/down controls" },
    { OPTION_STARTCOLLECTIONENTER,     "",         global_options::option_type::BOOLEAN,  "Enter the first collection on RetroFE boot" },
    { OPTION_EXITONFIRSTPAGEBACK,      "",         global_options::option_type::BOOLEAN,  "Exit RetroFE when the back button is pressed on the first page" },
    { OPTION_REMEMBERMENU,             "",         global_options::option_type::BOOLEAN,  "Remember the last highlighted item if re-entering a menu" },
    { OPTION_SUBSSPLIT,                "",         global_options::option_type::BOOLEAN,  "Split merged collections based on subs (true) or sort as one list (false)" },
    { OPTION_CFWLETTERSUB,             "",         global_options::option_type::BOOLEAN,  "Jump subs in a collection by sub instead of by the letter of the item" },
    { OPTION_PREVLETTERSUBTOCURRENT,   "",         global_options::option_type::BOOLEAN,  "Jump to the start of the current letter instead of the previous letter if jump to letter enabled" },
    { OPTION_RANDOMSTART,              "",         global_options::option_type::BOOLEAN,  "Start on a random item when RetroFE boots" },
    { OPTION_KIOSK,                    "",         global_options::option_type::BOOLEAN,  "Start on the first playlist in cyclePlaylist with navigation and favorites locked, can be toggled with a setting in controls.conf" },
    { OPTION_GLOBALFAVLAST,            "",         global_options::option_type::BOOLEAN,  "Save last played and favorites to a new collection" },
    { OPTION_INFOEXITONSCROLL,         "",         global_options::option_type::BOOLEAN,  "Hide info text boxes when scrolling" },

    { nullptr,                         nullptr,    global_options::option_type::HEADER,   "ATTRACT MODE OPTIONS" },
    { OPTION_ATTRACTMODECYCLEPLAYLIST, "",         global_options::option_type::BOOLEAN,  "Cycle through all playlists or defined in cyclePlaylist" },
    { OPTION_ATTRACTMODETIME,          "",         global_options::option_type::INTEGER,  "Number of seconds to wait before scrolling to another random point" },
    { OPTION_ATTRACTMODENEXTTIME,      "",         global_options::option_type::INTEGER,  "Number of seconds to wait before scrolling to another random point while attract mode is active" },
    { OPTION_ATTRACTMODEPLAYLISTTIME,  "",         global_options::option_type::INTEGER,  "Number of seconds to wait before attract mode jumps to another playlist, 0 to lock" },
    { OPTION_ATTRACTMODESKIPPLAYLIST,  "",         global_options::option_type::STRING,   "Skip CSV list of playlists while in attract mode" },
    { OPTION_ATTRACTMODECOLLECTIONTIME,"",         global_options::option_type::INTEGER,  "Number of seconds before attract mode switches to the next collection, 0 to lock" },
    { OPTION_ATTRACTMODESKIPCOLLECTION, "",        global_options::option_type::STRING,   "Skip CSV list of collections while in attract mode" },
    { OPTION_ATTRACTMODEMINTIME,        "",        global_options::option_type::INTEGER,  "Minimum number of milliseconds attract mode will scroll" },
    { OPTION_ATTRACTMODEMAXTIME,        "",        global_options::option_type::INTEGER,  "Maximum number of milliseconds attract mode will scroll" },
    { OPTION_ATTRACTMODEFAST,           "",        global_options::option_type::BOOLEAN,  "Scroll(false) or jump(true) to the next random point while in attract mode" },

    { nullptr,                         nullptr,    global_options::option_type::HEADER,   "INPUT OPTIONS" },
    { OPTION_COLLECTIONINPUTCLEAR,     "",         global_options::option_type::BOOLEAN,  "Clear input queue on collection change" },
    { OPTION_PLAYLISTINPUTCLEAR,       "",         global_options::option_type::BOOLEAN,  "Clear input queue on playlist change" },
    { OPTION_JUMPINPUTCLEAR,           "",         global_options::option_type::BOOLEAN,  "Clear input queue while jumping through the menu" },
    { OPTION_CONTROLLERCOMBOEXIT,      "",         global_options::option_type::BOOLEAN,  "Close RetroFE with the controller combo set in controls.conf" },
    { OPTION_CONTROLLERCOMBOSETTINGS,  "",         global_options::option_type::BOOLEAN,  "Open settings playlist with the controller combo set in controls.conf" },
    { OPTION_SETTINGSCOLLECTIONPLAYLIST,"",        global_options::option_type::STRING,   "Used by settings toggle to go to the playlist in collection:playlist format, defaults to settings.txt in the current collection" },

    { nullptr,                         nullptr,    global_options::option_type::HEADER,   "METADATA OPTIONS" },
    { OPTION_METALOCK,                 "",         global_options::option_type::BOOLEAN,  "Locks RetroFE from looking for XML changes and uses meta.db, faster loading when true" },
    { OPTION_OVERWRITEXML,             "",         global_options::option_type::BOOLEAN,  "Allows metadata XMLs to be overwritten by files in a collection" },
    { OPTION_SHOWPARENTHESIS,          "",         global_options::option_type::BOOLEAN,  "Hide item information between ()" },
    { OPTION_SHOWSQUAREBRACKETS,       "",         global_options::option_type::BOOLEAN,  "Hide item information between []" },

    { nullptr,                         nullptr,    global_options::option_type::HEADER,   "WINDOWS ONLY OPTIONS" },
    { OPTION_LEDBLINKYDIRECTORY,       "",         global_options::option_type::STRING,   "Path to LEDBlinky installation" },

    { nullptr,                        nullptr,     global_options::option_type::HEADER,   "MEDIA SEARCH PATH OPTIONS" },
    { OPTION_BASEMEDIAPATH,            "",         global_options::option_type::STRING,   "Path to media if stored outside the build" },
    { OPTION_BASEITEMPATH,             "",         global_options::option_type::STRING,   "Path to items if stored outside the build" },

    { nullptr }
};