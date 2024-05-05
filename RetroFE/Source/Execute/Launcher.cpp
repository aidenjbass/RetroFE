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

#include "Launcher.h"
#include "../Collection/Item.h"
#include "../Utility/Log.h"
#include "../Database/Configuration.h"
#include "../Utility/Utils.h"
#include "../RetroFE.h"
#include "../SDL.h"
#include "../Database/GlobalOpts.h"
#include <cstdlib>
#include <locale>
#include <sstream>
#include <fstream>
#include "../Graphics/Page.h"
#include <thread>
#include <atomic>
#include <filesystem>
#ifdef WIN32
#include <Windows.h>
#include <cstring>
#endif

namespace fs = std::filesystem;

Launcher::Launcher(Configuration &c)
    : config_(c)
{
}

bool Launcher::run(std::string collection, Item *collectionItem, Page *currentPage)
{
    // Initialize with the default launcher for the collection
    std::string launcherName = collectionItem->collectionInfo->launcher;

    // Check for per-item launcher override
    std::string launcherFile = Utils::combinePath(Configuration::absolutePath, "collections", collection, "launchers", collectionItem->name + ".conf");
    if (std::ifstream launcherStream(launcherFile); launcherStream.good()) {
        std::string line;
        if (std::getline(launcherStream, line)) {
            // Construct localLauncher key
            std::string localLauncherKey = "localLaunchers." + collection + "." + line;
            if (config_.propertyPrefixExists(localLauncherKey)) {
                // Use localLauncher if exists
                launcherName = collection + "." + line;
            }
            else {
                // Use specified launcher from conf file
                launcherName = line;
            }
        }
    }

    // If no per-item launcher override, check for a collection-specific launcher
    if (launcherName == collectionItem->collectionInfo->launcher) {
        std::string collectionSpecificLauncherKey = "collectionLaunchers." + collection;
        if (config_.propertyPrefixExists(collectionSpecificLauncherKey)) {
            launcherName = collectionItem->collectionInfo->name; // Use the collection-specific launcher
        }
    }

    // Convert launcherName to lowercase for consistency
   //launcherName = Utils::toLower(launcherName);

    std::string executablePath;
    std::string selectedItemsDirectory;
    std::string selectedItemsPath;
    std::string extensionstr;
    std::string matchedExtension;
    std::string args;


    if(!launcherExecutable(executablePath, launcherName)) {
        LOG_ERROR("Launcher", "Failed to find launcher executable (launcher: " + launcherName + " executable: " + executablePath + " collection: " + collectionItem->collectionInfo->name + " item: " + collectionItem->name + ")");
        return false;
    }
    if(!extensions(extensionstr, collection)) {
        LOG_ERROR("Launcher", "No file extensions configured for collection \"" + collection + "\"");
        return false;
    }
    if(!collectionDirectory(selectedItemsDirectory, collection)) {
        LOG_ERROR("Launcher", "Could not find files in directory \"" + selectedItemsDirectory + "\" for collection \"" + collection + "\"");
        return false;
    }
    if(!launcherArgs(args, launcherName)) {
        LOG_ERROR("Launcher", "No launcher arguments specified for launcher " + launcherName);
        return false;
    }


    // Overwrite selectedItemsDirectory if already set in the file
    if (collectionItem->filepath != "") {
        selectedItemsDirectory = collectionItem->filepath;
    }
    LOG_DEBUG("LauncherDebug", "selectedItemsPath pre-find file: " + selectedItemsPath);
    LOG_DEBUG("LauncherDebug", "selectedItemsDirectory pre - find file : " + selectedItemsDirectory);
    LOG_DEBUG("LauncherDebug", "matchedExtension pre - find file : " + matchedExtension);
    LOG_DEBUG("LauncherDebug", "extensionstr pre - find file : " + extensionstr);
    LOG_DEBUG("LauncherDebug", "collectionItem->name pre - find file: " + collectionItem->name);
    LOG_DEBUG("LauncherDebug", "collectionItem->file pre - find file: " + collectionItem->file);

    // It is ok to continue if the file could not be found. We could be launching a merged romset
    if (collectionItem->file == "")
        findFile(selectedItemsPath, matchedExtension, selectedItemsDirectory, collectionItem->name, extensionstr);
    else
        findFile(selectedItemsPath, matchedExtension, selectedItemsDirectory, collectionItem->file, extensionstr);

    LOG_DEBUG("LauncherDebug", "args: " + args);
    LOG_DEBUG("LauncherDebug", "selectedItemsPath: " + selectedItemsPath);


    args = replaceVariables(args,
                            selectedItemsPath,
                            collectionItem->name,
                            Utils::getFileName(selectedItemsPath),
                            selectedItemsDirectory,
                            collection);
    
    LOG_DEBUG("LauncherDebug", "executablePath: " + executablePath);

    executablePath = replaceVariables(executablePath,
                                      selectedItemsPath,
                                      collectionItem->name,
                                      Utils::getFileName(selectedItemsPath),
                                      selectedItemsDirectory,
                                      collection);

    std::string currentDirectoryKey = "launchers." + launcherName + ".currentDirectory";
    std::string currentDirectory    = Utils::getDirectory(executablePath);

    config_.getProperty(currentDirectoryKey, currentDirectory);

    currentDirectory = replaceVariables(currentDirectory,
                                        selectedItemsPath,
                                        collectionItem->name,
                                        Utils::getFileName(selectedItemsPath),
                                        selectedItemsDirectory,
                                        collection);

    if(!execute(executablePath, args, currentDirectory, true, currentPage)) {
        LOG_ERROR("Launcher", "Failed to launch.");
        return false;
    }

    bool reboot = false;
	config_.getProperty("launchers." + launcherName + ".reboot", reboot);

    return reboot;
}

void Launcher::startScript()
{
#ifdef WIN32
    std::string exe = Utils::combinePath(Configuration::absolutePath, "start.bat");
#else
    std::string exe = Utils::combinePath(Configuration::absolutePath, "start.sh");
#endif
    if(fs::exists(exe)) {
        execute(exe, "", Configuration::absolutePath, false);
    }
}

void Launcher::exitScript()
{
#ifdef WIN32
    std::string exe = Utils::combinePath(Configuration::absolutePath, "exit.bat");
#else
    std::string exe = Utils::combinePath(Configuration::absolutePath, "exit.sh");
#endif
    if(fs::exists(exe)) {
        execute(exe, "", Configuration::absolutePath, false);
    }
}

void Launcher::LEDBlinky( int command, std::string collection, Item *collectionItem )
{
	std::string LEDBlinkyDirectory = "";
	config_.getProperty( OPTION_LEDBLINKYDIRECTORY, LEDBlinkyDirectory );
	if (LEDBlinkyDirectory == "") {
        return;
    }
    std::string exe  = Utils::combinePath(LEDBlinkyDirectory, "LEDBlinky.exe");
	std::string args = std::to_string( command );
	bool wait = false;
	if ( command == 2 )
		wait = true;
	if ( command == 8 ) {
		std::string launcherName = collectionItem->collectionInfo->launcher;
		std::string launcherFile = Utils::combinePath( Configuration::absolutePath, "collections", collectionItem->collectionInfo->name, "launchers", collectionItem->name + ".conf" );
		if (std::ifstream launcherStream( launcherFile.c_str( ) ); launcherStream.good( )) // Launcher file found
		{
			std::string line;
			if (std::getline( launcherStream, line)) // Launcher found
			{
				launcherName = line;
			}
		}
		launcherName = Utils::toLower( launcherName );
		std::string emulator = collection;
		config_.getProperty("launchers." + launcherName + ".LEDBlinkyEmulator", emulator );
		args = args + " \"" + emulator + "\"";
	}
	if ( command == 3 || command == 9 ) {
		std::string launcherName = collectionItem->collectionInfo->launcher;
		std::string launcherFile = Utils::combinePath( Configuration::absolutePath, "collections", collectionItem->collectionInfo->name, "launchers", collectionItem->name + ".conf" );
		if (std::ifstream launcherStream( launcherFile.c_str( ) ); launcherStream.good( )) // Launcher file found
		{
			std::string line;
			if (std::getline( launcherStream, line)) // Launcher found
			{
				launcherName = line;
			}
		}
		launcherName = Utils::toLower( launcherName );
		std::string emulator = launcherName;
		config_.getProperty("launchers." + launcherName + ".LEDBlinkyEmulator", emulator );
		args = args + " \"" + collectionItem->name + "\" \"" + emulator + "\"";
		if ( emulator == "" )
			return;
	}
	if ( LEDBlinkyDirectory != "" && !execute( exe, args, LEDBlinkyDirectory, wait )) {
        LOG_WARNING("LEDBlinky", "Failed to launch." );
	}
	return;
}


std::string Launcher::replaceVariables(std::string str,
                                       std::string itemFilePath,
                                       std::string itemName,
                                       std::string itemFilename,
                                       std::string itemDirectory,
                                       std::string itemCollectionName)
{
    str = Utils::replace(str, "%ITEM_FILEPATH%", itemFilePath);
    str = Utils::replace(str, "%ITEM_NAME%", itemName);
    str = Utils::replace(str, "%ITEM_FILENAME%", itemFilename);
    str = Utils::replace(str, "%ITEM_DIRECTORY%", itemDirectory);
    str = Utils::replace(str, "%ITEM_COLLECTION_NAME%", itemCollectionName);
    str = Utils::replace(str, "%RETROFE_PATH%", Configuration::absolutePath);
    str = Utils::replace(str, "%COLLECTION_PATH%", Utils::combinePath(Configuration::absolutePath, "collections", itemCollectionName));
#ifdef WIN32
    str = Utils::replace(str, "%RETROFE_EXEC_PATH%", Utils::combinePath(Configuration::absolutePath, "retrofe", "RetroFE.exe"));
    const char* comspec = std::getenv("COMSPEC");
    if (comspec) {
        str = Utils::replace(str, "%CMD%", std::string(comspec));
    }
#else
    str = Utils::replace(str, "%RETROFE_EXEC_PATH%", Utils::combinePath(Configuration::absolutePath, "RetroFE"));
#endif

    return str;
}

bool Launcher::execute(std::string executable, std::string args, std::string currentDirectory, bool wait, Page* currentPage)
{
    bool retVal = false;
    std::string executionString = "\"" + executable + "\" " + args;

    LOG_INFO("Launcher", "Attempting to launch: " + executionString);
    LOG_INFO("Launcher", "     from within folder: " + currentDirectory);

    std::atomic<bool> stop_thread = true;
    std::thread proc_thread;
    bool multiple_display = SDL::getScreenCount() > 1;
    bool animateDuringGame = true;
    config_.getProperty(OPTION_ANIMATEDURINGGAME, animateDuringGame);
    if (animateDuringGame && multiple_display && currentPage != nullptr) {
        stop_thread = false;
        proc_thread = std::thread([this, &stop_thread, &currentPage]() {
            this->keepRendering(std::ref(stop_thread), *currentPage);
            });
    }

#ifdef WIN32
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    char applicationName[2048];
    char currDir[2048];
    memset(&applicationName, 0, sizeof(applicationName));
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&processInfo, 0, sizeof(processInfo));
    strncpy(applicationName, executionString.c_str(), sizeof(applicationName));
    strncpy(currDir, currentDirectory.c_str(), sizeof(currDir));
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.wShowWindow = SW_SHOWDEFAULT;

    if(!CreateProcess(nullptr, applicationName, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, currDir, &startupInfo, &processInfo))
#else
    const std::size_t last_slash_idx = executable.rfind(Utils::pathSeparator);
    if (last_slash_idx != std::string::npos) {
        std::string applicationName = executable.substr(last_slash_idx + 1);
        executionString = "cd \"" + currentDirectory + "\" && exec \"./" + applicationName + "\" " + args;
    }
    if(system(executionString.c_str()) != 0)
#endif
    {
        LOG_WARNING("Launcher", "Failed to run: " + executable);
    }

    else
    {
#ifdef WIN32
        // lower priority
        SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

		if ( wait ) {
			while(WAIT_OBJECT_0 != MsgWaitForMultipleObjects(1, &processInfo.hProcess, FALSE, INFINITE, QS_ALLINPUT)) {
				MSG msg;
				while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
					DispatchMessage(&msg);
				}
			}
        }

        //resume priority
        bool highPriority = false;
        config_.getProperty(OPTION_HIGHPRIORITY, highPriority);
        if (highPriority) {
            SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
        }
        else {
            SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        }

        // result = GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
#endif
        retVal = true;
    }

    if (multiple_display && stop_thread == false) {
        stop_thread = true;
        proc_thread.join();
    }

    LOG_INFO("Launcher", "Completed");

    return retVal;
}

void Launcher::keepRendering(std::atomic<bool> &stop_thread, Page &currentPage)
{
    float lastTime = 0;
    float currentTime = 0;
    float deltaTime = 0;
    double sleepTime;
    double fpsTime = 1000.0 / static_cast<double>(60);

    while (!stop_thread) {
        lastTime = currentTime;
        currentTime = static_cast<float>(SDL_GetTicks()) / 1000;

        if (currentTime < lastTime) {
            currentTime = lastTime;
        }

        deltaTime = currentTime - lastTime;
        sleepTime = fpsTime - deltaTime * 1000;
            
        if (sleepTime > 0 && sleepTime < 1000) {
            SDL_Delay(static_cast<unsigned int>(sleepTime));
        }
        currentPage.update(float(0));
        SDL_LockMutex(SDL::getMutex());

        // start on secondary monitor
        // todo support future main screen swap
        for (int i = 1; i < SDL::getScreenCount(); ++i) {
            SDL_SetRenderDrawColor(SDL::getRenderer(i), 0x0, 0x0, 0x00, 0xFF);
            SDL_RenderClear(SDL::getRenderer(i));
        }

        currentPage.draw();

        for (int i = 1; i < SDL::getScreenCount(); ++i) {
            SDL_RenderPresent(SDL::getRenderer(i));
        }

        SDL_UnlockMutex(SDL::getMutex());
    }
}

bool Launcher::launcherName(std::string &launcherName, std::string collection)
{
       // find the launcher for the particular item 
    if (std::string launcherKey = "collections." + collection + ".launcher"; !config_.getProperty(launcherKey, launcherName)) {
        std::stringstream ss;

        ss << "Launch failed. Could not find a configured launcher for collection \""
           << collection
           << "\" (could not find a property for \""
           << launcherKey
           << "\")";

        LOG_ERROR("Launcher", ss.str());

        return false;
    }

    std::stringstream ss;
    ss        << "collections."
              << collection
              << " is configured to use launchers."
              << launcherName
              << "\"";

    LOG_DEBUG("Launcher", ss.str());

    return true;
}



bool Launcher::launcherExecutable(std::string& executable, std::string launcherName) {
    // Try with the localLauncher prefix
    std::string executableKey = "localLaunchers." + launcherName + ".executable";
    if (!config_.getProperty(executableKey, executable)) {
        // Try with the collectionLauncher prefix
        executableKey = "collectionLaunchers." + launcherName + ".executable";
        if (!config_.getProperty(executableKey, executable)) {
            // Finally, try with the global launcher prefix
            executableKey = "launchers." + launcherName + ".executable";
            if (!config_.getProperty(executableKey, executable)) {
                LOG_ERROR("Launcher", "No launcher found for: " + executableKey);
                return false;
            }
        }
    }
    return true;
}



bool Launcher::launcherArgs(std::string& args, std::string launcherName) {
    // Try with the localLauncher prefix
    std::string argsKey = "localLaunchers." + launcherName + ".arguments";
    if (!config_.getProperty(argsKey, args)) {
        // Try with the collectionLauncher prefix
        argsKey = "collectionLaunchers." + launcherName + ".arguments";
        if (!config_.getProperty(argsKey, args)) {
            // Finally, try with the global launcher prefix
            argsKey = "launchers." + launcherName + ".arguments";
            if (!config_.getProperty(argsKey, args)) {
                LOG_ERROR("Launcher", "No arguments specified for: " + argsKey);
                return false;
            }
        }
    }
    return true;
}



bool Launcher::extensions(std::string &extensions, std::string collection)
{
    if(std::string extensionsKey = "collections." + collection + ".list.extensions"; !config_.getProperty(extensionsKey, extensions)) {
        LOG_ERROR("Launcher", "No extensions specified for: " + extensionsKey);
        return false;
    }

    extensions = Utils::replace(extensions, " ", "");
    extensions = Utils::replace(extensions, ".", "");

    return true;
}

bool Launcher::collectionDirectory(std::string &directory, std::string collection)
{
    std::string itemsPathValue;
    std::string mergedCollectionName;

    // find the items path folder (i.e. ROM path)
    config_.getCollectionAbsolutePath(collection, itemsPathValue);
    directory += itemsPathValue + Utils::pathSeparator;

    return true;
}

bool Launcher::findFile(std::string& foundFilePath, std::string& foundFilename, const std::string& directory, const std::string& filenameWithoutExtension, const std::string& extensions) {
    bool fileFound = false;
    std::stringstream ss(extensions);
    std::string extension;

    while (!fileFound && std::getline(ss, extension, ',')) {
        fs::path filePath = fs::path(directory) / (filenameWithoutExtension + "." + extension);

        if (fs::exists(filePath)) {
            foundFilePath = filePath.string();
            foundFilename = extension;
            fileFound = true;
            LOG_INFO("Launcher", "File found: " + foundFilePath);
        }
        else {
            LOG_WARNING("Launcher", "File not found: " + filePath.string());
        }
    }

    if (!fileFound) {
        LOG_WARNING("Launcher", "Could not find any files with the name \"" + filenameWithoutExtension + "\" in folder \"" + directory + "\"");
    }

    return fileFound;
}

