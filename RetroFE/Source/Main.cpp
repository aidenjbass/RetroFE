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

#include "Video/GStreamerVideo.h"
#include "Database/Configuration.h"
#include "Database/DB.h"
#include "Collection/CollectionInfoBuilder.h"
#include "Execute/Launcher.h"
#include "Utility/Log.h"
#include "Utility/Utils.h"
#include "RetroFE.h"
#include "Version.h"
#include "SDL.h"

#include "Database/GlobalOpts.h"

#include <cstdlib>
#include <fstream>
#include <time.h>
#include <locale>
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#ifdef WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;
static bool ImportConfiguration(Configuration* c);
std::vector<std::string> settingsFromCLI;

// Check if we're starting retrofe from terminal on Win or Unix
bool isOutputATerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(STDOUT_FILENO);
#endif
}

// Check if start of fullString contains startOfString
bool startsWith(const std::string& fullString, const std::string& startOfString) {
    return fullString.substr(0, startOfString.length()) == startOfString;
}

// Check if start of fullString contains startOfString and then remove
bool startsWithAndStrip(std::string& fullString, const std::string& startOfString) {
    if (fullString.substr(0, startOfString.length()) == startOfString) {
        fullString = fullString.substr(startOfString.length());
        return true;
    }
    return false;
}

// Function to initialize the DB object
bool initializeDB(DB*& db, const std::string& dbPath) {
    db = new DB(dbPath);
    if (!db->initialize()) {
        LOG_ERROR("RetroFE", "Could not initialize database");
        return false;
    }
    return true;
}

// Function to initialize the MetadataDatabase object
bool initializeMetadataDatabase(MetadataDatabase*& metadb, DB* db, Configuration& config) {
    metadb = new MetadataDatabase(*db, config);
    if (!metadb->initialize()) {
        LOG_ERROR("RetroFE", "Could not initialize meta database");
        return false;
    }
    return true;
}

std::string showusage()
{
    return
    "#\n"
    "# LOGGING OPTIONS\n"
    "#\n"
    "-log                           any combo of ERROR,INFO,NOTICE,WARNING,DEBUG,FILECACHE or ALL\n"
    "\n"
    "#\n"
    "# DISPLAY OPTIONS\n"
    "#\n"
    "-numScreens                    defines the number of monitors used (int)\n"
    "-fullscreen                    run the frontend in fullscreen (bool)\n"
    "-horizontal                    pixel width (int)\n"
    "-vertical                      pixel height (int)\n"
    "-fullscreenx                   runs the frontend in fullscreen for monitor x (int)\n"
    "-horizontalx                   pixel width for monitor x (int)\n"
    "-verticalx                     pixel height for monitor x (int)\n"
    "-mirrorx                       divides monitor x into two halves (bool)\n"
    "-rotationx                     rotation of monitor x (1,2,3,4)\n"
    "\n"
    "#\n"
    "# WINDOW OPTIONS\n"
    "#\n"
    "-windowBorder                  show window border (bool)\n"
    "-windowResize                  allow window to be resized (bool)\n"
    "-fps                           requested FPS while in an active state (int)\n"
    "-fpsIdle                       request FPS while in an idle state (int)\n"
    "-hideMouse                     defines whether the mouse cursor is hidden (bool)\n"
    "-animateDuringGame             pause animated marquees while in the game to reserve processing (bool)\n"
    "\n"
    "#\n"
    "# VIDEO OPTIONS\n"
    "#\n"
    "-videoEnable                   defines whether video is rendered (bool)\n"
    "-videoLoop                     number of times to play video, 0 forever (int)\n"
    "-disableVideoRestart           pauses video while scrolling (bool)\n"
    "-disablePauseOnScroll          restart video when selected (bool)\n"
    "\n"
    "#\n"
    "# RENDERER OPTIONS\n"
    "#\n"
    "-vSync                         vertical synchronization (bool)\n"
    "-HardwareVideoAccel            hardware decoding (bool)\n"
    "-AvdecMaxThreads               number of threads for avdec software decoding (int)\n"
    "-MuteVideo                     video playback is muted (bool)\n"
    "-SDLRenderDriver               direct3d, direct3d11, direct3d12, opengl, opengles2, opengles, metal, and software\n"
    "-ScaleQuality                  scaling quality (0,1,2)\n"
    "-highPriority                  retrofe windows process priority (bool)\n"
    "-unloadSDL                     close SDL when launching a game, needed for RPI (bool)\n"
    "-minimize_on_focus_loss        minimize retrofe when focus is lost (bool)\n"
    "\n"
    "#\n"
    "# CUSTOMIZATION OPTIONS\n"
    "#\n"
    "-layout                        theme to be used in RetroFE, a folder name in /layouts (string)\n"
    "-randomLayout                  randomly choose a layout on launch, CSV list of layout names \n"
    "-firstPlaylist                 start on this playlist if available (string)\n"
    "-cyclePlaylist                 set of playlists that can be cycled through, CSV list of playlist names\n"
    "-firstCollection               start on this collection if available (string)\n"
    "-cycleCollection               set of collections that can be cycled through, CSV list of collection names\n"
    "-lastplayedSize                size of the auto-generated last played playlist, 0 to disable (int)\n"
    "-lastplayedSkipCollection      exclude a collection from being added to last played, settings for example (string)\n"
    "-enterOnCollection             enter the collection when using collection up/down controls (bool)\n"
    "-startCollectionEnter          enter the first collection on RetroFE boot (bool)\n"
    "-exitOnFirstPageBack           exit RetroFE when the back button is pressed on the first page (bool)\n"
    "-rememberMenu                  remember the last highlighted item if re-entering a menu (bool)\n"
    "-subsSplit                     split merged collections based on subs(true) or sort as one list(false) (bool)\n"
    "-cfwLetterSub                  jump subs in a collection by sub instead of by the letter of the item (bool)\n"
    "-prevLetterSubToCurrent        jump to the start of the current letter instead of the previous letter if jump to letter enabled (bool)\n"
    "-randomStart                   start on a random item when RetroFE boots (bool)\n"
    "-kiosk                         start on the first playlist in cyclePlaylist with navigation and favorites locked, can be toggled with a setting in controls.conf (bool)\n"
    "-globalFavLast                 save last played and favorites to a new collection (bool)\n"
    "-infoExitOnScroll              hide info text boxes when scrolling (bool)\n"
    "\n"
    "#\n"
    "# ATTRACT MODE OPTIONS\n"
    "#\n"
    "-attractModeCyclePlaylist      cycle through all playlists or defined in cyclePlaylist (bool)\n"
    "-attractModeTime               number of seconds to wait before scrolling to another random point (int)\n"
    "-attractModeNextTime           number of seconds to wait before scrolling to another random point while attract mode is active (int)\n"
    "-attractModePlaylistTime       number of seconds to wait before attract mode jumps to another playlist, 0 to lock (int)\n"
    "-attractModeSkipPlaylist       skip CSV list of playlists while in attract mode\n"
    "-attractModeCollectionTime     number of seconds before attract mode switches to the next collection, 0 to lock (int)\n"
    "-attractModeSkipCollection     skip CSV list of collections while in attract mode\n"
    "-attractModeMinTime            minimum number of milliseconds attract mode will scroll (int)\n"
    "-attractModeMaxTime            maximum number of milliseconds attract mode will scroll (int)\n"
    "-attractModeFast               scroll(false) or jump(true) to the next random point while in attract mode (bool)\n"
    "\n"
    "#\n"
    "# INPUT OPTIONS\n"
    "#\n"
    "-collectionInputClear          clear input queue on collection change (bool)\n"
    "-playlistInputClear            clear input queue on playlist change (bool)\n"
    "-jumpInputClear                clear input queue while jumping through the menu (bool)\n"
    "-controllerComboExit           close retrofe with the controller combo set in controls.conf (bool)\n"
    "-controllerComboSettings       open settings playlist with the controller combo set in controls.conf (bool)\n"
    "-settingsCollectionPlaylist    used by settings toggle to go to the playlist in collection:playlist format, defaults to settings.txt in the current collection\n"
    "\n"
    "#\n"
    "# METADATA OPTIONS\n"
    "#\n"
    "-metaLock                      locks retrofe from looking for XML changes and uses meta.db, faster loading when true (bool)\n"
    "-overwriteXML                  allows metadata XMLs to be overwritten by files in a collection (bool)\n"
    "-showParenthesis               hide item information between () (bool)\n"
    "-showSquareBrackets            hide item information between [] (bool)\n"
    "\n"
    "#\n"
    "# WINDOWS ONLY OPTIONS\n"
    "#\n"
    "-LEDBlinkyDirectory            path to LEDBlinky installation (string)\n"
    "\n"
    "#\n"
    "# MEDIA SEARCH PATH OPTIONS\n"
    "#\n"
    "-baseMediaPath                 path to media if stored outside the build (string)\n"
    "-baseItemPath                  path to items if stored outside the build (string)\n"
    ;
}

int main(int argc, char** argv)
{
    Configuration::initialize();
    Configuration config;
    std::string dbPath = Utils::combinePath(Configuration::absolutePath, "meta.db");
    
    // Check to see if an argument was passed
    if (argc > 1)
    {
        std::string program = argv[0];
        std::string param = argv[1];
        
        if (param == "-createcollection" ||
            param == "--createcollection" ||
            param == "-cc")
        {
            if (argc == 3)
            {
                std::string param = argv[1];
                std::string value = argv[2];
                CollectionInfoBuilder::createCollectionDirectory(value);
                return 0;
            }
            else
            {
                std::cout << "Expected 1 argument for -createcollection, got " << argc - 2 << std::endl;
                return 0;
            }
        }
        else if (param == "-version" ||
            param == "--version" ||
            param == "-v")
        {
            std::cout << "RetroFE version " << Version::getString() << std::endl << std::flush;
            return 0;
        }
        else if (param == "-showusage" ||
            param == "--showusage" ||
            param == "-su")
        {
            std::cout << showusage() << std::endl;
            return 0;
        }
        else if (param == "-rebuilddatabase" ||
            param == "--rebuilddatabase" ||
            param == "-rebuilddb" ||
            param == "-rbdb" ||
            param == "-rdb")
        {
            DB* db = nullptr;
            MetadataDatabase* metadb = nullptr;
            if (!initializeDB(db, dbPath)) {
                // Handle initialization error, clean up if necessary
                return 0;
            }
            if (!initializeMetadataDatabase(metadb, db, config)) {
                // Handle initialization error, clean up if necessary
                delete db; // Clean up DB object
                return 0;
            }
            metadb->resetDatabase();
            return 0;
        }
        else if (param == "-showconfig" ||
            param == "--showconfig" ||
            param == "-sc")
        {
            // TODO; list settings.conf
            return 0;
        }
        else if (param == "-createconfig" ||
            param == "--createconfig" ||
            param == "-C")
        {
            // TODO; create default settings.conf
            return 0;
        }
        else if ((argc % 2 != 0 or argc % 2 == 0) and param != "help" and param != "-help")
        {
            // Pass global settings via CLI
            for (int i = 1; i <= argc - 1 ; i+=2) {
                // The odd argument should always be the key, and even will be value
                if (argv[i+1] == nullptr)
                {
                    // If you don't pass a value with a key we need catch that here
                    std::cout << "Expected 1 argument for " << argv[i] << " got " << 0 << std::endl;
                    return 0;
                }
                std::string CLIkey = argv[i];
                std::string CLIvalue = argv[i+1];
                if (startsWithAndStrip(CLIkey, "-") and !startsWith(CLIvalue,"-"))
                {
                    if (CLIkey == "log")
                    {
                        config.setProperty("log", CLIvalue);
                        config.StartLogging(&config);
                    }
                    settingsFromCLI.push_back(CLIkey + "=" + CLIvalue + "\n");
                }
                else if(startsWith(CLIvalue,"-"))
                {
                    std::cout << "Expected 1 argument for -" << CLIkey << " got " << 0 << std::endl;
                    return 0;
                }
                else
                {
                    std::cout << "To pass settings via CLI pairs use [-key] [value] format" << std::endl;
                    return 0;
                }
            }
        }
        else
        {
            std::cout << "Absolute Path: " << Configuration::absolutePath << std::endl;
            std::cout << "RetroFE Version: " << Version::getString() << std::endl;
            std::cout << std::endl;
            std::cout << "RetroFE is a cross-platform desktop frontend designed for MAME cabinets and game centers, with a focus on simplicity and customization.\nIt's licensed under the terms of the GNU General Public License, version 3 or later (GPLv3)." << std::endl;
            std::cout << std::endl;
            std::cout << "Usage:" << std::endl;
            std::cout << "      -h    -help                                    Show this message" << std::endl;
            std::cout << "      -v    -version                                 Print the version of RetroFE" << std::endl;
            std::cout << "      -cc   -createcollection <collection name>      Create a collection directory structure" << std::endl;
            std::cout << "      -rdb  -rebuilddatabase                         Rebuild the database" << std::endl;
            std::cout << "      -su   -showusage                               Print a list of all global settings" << std::endl;
            std::cout << "      -sc   -showconfig                              Print a list of current settings" << std::endl;
            std::cout << "      -C    -createconfig                            Create a settings.conf with default values" << std::endl;
            std::cout << std::endl;
            std::cout << "For more information, visit\nhttps://github.com/CoinOPS-Official/RetroFE/\nhttp://retrofe.nl/" << std::endl;
            return 0;
        }
    }

    // Initialize locale language
    setlocale(LC_ALL, "");

    // Initialize random seed
    srand(static_cast<unsigned int>(time(nullptr)));

    gst_init(nullptr, nullptr);

    try {
        
        while (true)
        {
            if (!ImportConfiguration(&config))
            {
                // Exit with a heads up...
                std::string logFile = Utils::combinePath(Configuration::absolutePath, "log.txt");
                if(isOutputATerminal())
                {
                    fprintf(stderr, "RetroFE has failed to start due to a configuration error\nCheck the log for details: %s\n", logFile.c_str());
                }
                else
                {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Configuration Error", ("RetroFE has failed to start due to a configuration error\nCheck the log for details: \n" + logFile).c_str(), NULL);
                }
                config.setProperty("log", "ERROR,NOTICE,WARNING");
                ImportConfiguration(&config);
                exit(EXIT_FAILURE);
            }
            RetroFE p(config);
            if (p.run()) // Check if we need to reboot after running
                config.clearProperties();
            else
                break;
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR("EXCEPTION", e.what());
    }

    Logger::deInitialize();

    return 0;
}

static bool ImportConfiguration(Configuration* c)
{
    std::string configPath = Configuration::absolutePath;

#ifdef WIN32
    std::string osType = "windows";
#elif __APPLE__
    std::string osType = "apple";
#else
    std::string osType = "linux";
#endif

    fs::path launchersPath = Utils::combinePath(Configuration::absolutePath, "launchers." + osType);

    fs::path collectionsPath = Utils::combinePath(Configuration::absolutePath, "collections");

    std::string settingsConfPath = Utils::combinePath(configPath, "settings");
    
    if(!fs::exists(Utils::combinePath(Configuration::absolutePath, "settings.conf")))
    {
        if(isOutputATerminal())
        {
            std::cout << "RetroFE failed to find a valid settings.conf in the current directory" << std::endl;
        }
        else
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Configuration", "RetroFE failed to find a valid settings.conf in the current directory", NULL);
        }
        exit(EXIT_FAILURE);
    }
    
    if (!c->import("", settingsConfPath + ".conf"))
    {
        LOG_ERROR("RetroFE", "Could not import \"" + settingsConfPath + ".conf\"");
        return false;
    }
    for (int i = 1; i < 16; i++) {
        std::string settingsFile = settingsConfPath + std::to_string(i) + ".conf";
        if (fs::exists(settingsFile)) 
        {
            c->import("", "", settingsFile, false);
        }
    }
    std::string savedSettingsFile = settingsConfPath + "_saved.conf";
    if (fs::exists(savedSettingsFile)) 
    {
        c->import("", "", savedSettingsFile, false);
    }
    if (!settingsFromCLI.empty()) 
    {
        // If settingsFromCLI isn't empty let's do something with it
        std::string result;
        for (const auto& str : settingsFromCLI) {
            result += str;
        }
        c->import("", "CLI", result, false);
    }

    // log version
    LOG_INFO("RetroFE", "Version " + Version::getString() + " starting");

#ifdef WIN32
    LOG_INFO("RetroFE", "OS: Windows");
#elif __APPLE__
    LOG_INFO("RetroFE", "OS: Mac");
#else
    LOG_INFO("RetroFE", "OS: Linux");
#endif
    
    // Check if GStreamer initialization was successful
    if (gst_is_initialized())
    {
#ifdef WIN32
        std::string path = Utils::combinePath(Configuration::absolutePath, "retrofe");
        GstRegistry* registry = gst_registry_get();
        gst_registry_scan_path(registry, path.c_str());
#endif
        LOG_INFO("RetroFE", "GStreamer successfully initialized");
    }
    else
    {
        LOG_ERROR("RetroFE", "Failed to initialize GStreamer");
        return false;
    }

    LOG_INFO("RetroFE", "Absolute path: " + Configuration::absolutePath);

    // Process launchers
    if (fs::is_directory(launchersPath))
    {
        for (const auto& entry : fs::directory_iterator(launchersPath))
        {
            if (entry.is_regular_file())
            {
                fs::path filePath = entry.path();
                if (filePath.extension() == ".conf")
                {
                    std::string basename = filePath.stem().string();
                    std::string prefix = "launchers." + Utils::toLower(basename);
                    std::string importFile = filePath.string();

                    if (!c->import(prefix, importFile))
                    {
                        LOG_ERROR("RetroFE", "Could not import \"" + importFile + "\"");
                    }
                }
            }
        }
    }
    else
    {
        LOG_NOTICE("RetroFE", "Launchers directory does not exist or is not a directory: " + launchersPath.string());
    }


    // Process collections
    if (!fs::exists(collectionsPath) || !fs::is_directory(collectionsPath)) {
        LOG_ERROR("RetroFE", "Could not read directory \"" + collectionsPath.string() + "\"");
        return false;
    }

    for (const auto& entry : fs::directory_iterator(collectionsPath)) {
        std::string collection = entry.path().filename().string();
        if (fs::is_directory(entry) && !collection.empty() && collection[0] != '_' && collection != "." && collection != "..") {
            std::string prefix = "collections." + collection;
            bool settingsImported = false;
            std::string settingsFile = Utils::combinePath(collectionsPath, collection, "settings.conf");
            if (fs::exists(settingsFile)) {
                settingsImported |= c->import(collection, prefix, settingsFile, false);
            }

            for (int i = 1; i < 16; i++) {
                std::string numberedSettingsFile = Utils::combinePath(collectionsPath, collection, "settings" + std::to_string(i) + ".conf");
                if (fs::exists(numberedSettingsFile)) {
                    settingsImported |= c->import(collection, prefix, numberedSettingsFile, false);
                }
            }

            std::string infoFile = Utils::combinePath(collectionsPath, collection, "info.conf");
            if (fs::exists(infoFile)) {
                c->import(collection, prefix, infoFile, false);
            }

            // Process collection-specific launcher overrides
            std::string osSpecificLauncherFile = Utils::combinePath(collectionsPath, collection, "launcher." + osType + ".conf");
            std::string defaultLauncherFile = Utils::combinePath(collectionsPath, collection, "launcher.conf");
            std::string launcherKey = "collectionLaunchers." + collection; // Unique key for collection-specific launchers

            std::string importFile = fs::exists(osSpecificLauncherFile) ? osSpecificLauncherFile
                : fs::exists(defaultLauncherFile) ? defaultLauncherFile
                : "";

            // Import the launcher file if it exists under the unique key
            if (!importFile.empty()) {
                c->import(collection, launcherKey, importFile, false);
                LOG_INFO("RetroFE", "Imported collection-specific launcher for: " + collection);
            }

            fs::path localLaunchersPath = Utils::combinePath(collectionsPath, collection, "launchers." + osType + ".local");
            fs::path defaultLocalLaunchersPath = Utils::combinePath(collectionsPath, collection, "launchers.local");

            // Check if OS-specific launchers directory exists, otherwise use the default launchers directory
            if (!fs::is_directory(localLaunchersPath) && fs::is_directory(defaultLocalLaunchersPath)) {
                localLaunchersPath = defaultLocalLaunchersPath;
            }

            if (fs::is_directory(localLaunchersPath))
            {
                for (const auto& launcherEntry : fs::directory_iterator(localLaunchersPath)) {
                    if (launcherEntry.is_regular_file()) {
                        fs::path filePath = launcherEntry.path();
                        if (filePath.extension() == ".conf") {
                            std::string basename = filePath.stem().string();
                            std::string prefix = "localLaunchers." + collection + "." + Utils::toLower(basename);
                            std::string importFile = filePath.string();

                            if (!c->import(collection, prefix, importFile)) {
                                LOG_ERROR("RetroFE", "Could not import local launcher \"" + importFile + "\" for collection \"" + collection + "\"");
                            }
                            else {
                                LOG_INFO("RetroFE", "Imported local launcher \"" + basename + "\" for collection \"" + collection + "\"");
                            }
                        }
                    }
                }
            }
            // Set the launcher property if it's not already set in settings.conf
            std::string launcherPropertyKey = "collections." + collection + ".launcher";
            if (!c->propertyExists(launcherPropertyKey) && !importFile.empty()) {
                c->setProperty(launcherPropertyKey, collection);
            }

            // Update collectionLaunchers property
            std::string collectionLaunchers = "collectionLaunchers";
            std::string launchers = "";
            c->getProperty(collectionLaunchers, launchers);
            if (!importFile.empty() && (collection.size() < 3 || collection.substr(collection.size() - 3) != "SUB")) {
                c->setProperty(collectionLaunchers, launchers + collection + ",");
            }



            if (!settingsImported) {
                LOG_ERROR("RetroFE", "Could not import any collection settings for " + collection);
            }
            else {
                LOG_INFO("RetroFE", "Imported settings for collection: " + collection);
            }

            if (!importFile.empty()) {
                LOG_INFO("RetroFE", "Imported launcher configuration for collection: " + collection);
            }
        }
    }

    LOG_INFO("RetroFE", "Imported configuration");
    return true;
}
