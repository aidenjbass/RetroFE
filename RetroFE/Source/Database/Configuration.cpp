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
#include "Configuration.h"
#include "../Utility/Log.h"
#include "../Utility/Utils.h"
#include "GlobalOpts.h"
#include <algorithm>
#include <locale>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string_view>
#include <set>
#include <cstdio>

#ifdef WIN32
#include <windows.h>
#elif __APPLE__
#include <libproc.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

std::string Configuration::absolutePath;
bool Configuration::HardwareVideoAccel = false;
int Configuration::AvdecMaxThreads = 2;
int Configuration::AvdecThreadType = 2;
bool Configuration::MuteVideo = false;

Configuration::Configuration() = default;

Configuration::~Configuration() = default;

void Configuration::initialize()
{
    const char *environment = std::getenv("RETROFE_PATH");
    std::string sPath; // Declare sPath here

#if defined(__linux) || defined(__APPLE__)
    std::string home_load = std::getenv("HOME") + std::string("/.retrofe");
    std::ifstream retrofe_path(home_load.c_str());
#endif

    // Check Environment for path
    if (environment != nullptr)
    {
        absolutePath = environment;
    }
#if defined(__linux) || defined(__APPLE__)
    // Or check for home based flat file works on linux/mac
    else if (retrofe_path && std::getline( retrofe_path, absolutePath ))
    {
    	retrofe_path.close();
    }
#endif
    // Or check executable for path
    else
    {
#ifdef WIN32
        HMODULE hModule = GetModuleHandle(NULL);
        CHAR exe[MAX_PATH];
        GetModuleFileName(hModule, exe, MAX_PATH);
        sPath = Utils::getDirectory(exe);
        sPath = Utils::getParentDirectory(sPath);
#elif __APPLE__
    	char exepath[PROC_PIDPATHINFO_MAXSIZE];
    	if( proc_pidpath (getpid(), exepath, sizeof(exepath)) <= 0 ) // Error to console if no path to write logs…
            fprintf(stderr, "Cannot set absolutePath: %s\nOverride with RETROFE_PATH env var\n", strerror(errno));
        sPath = Utils::getDirectory(exepath);

        // RetroFE can be started from the command line: 'retrofe' or as an app, by clicking RetroFE.app.
        // If this was started as an app bundle, relocate it's current working directory to the root folder.
        // as an example /usr/local/opt/retro/RetroFE.app/Contents/MacOS becomes /usr/local/opt/retrofe
        // Note: executing 'brew applinks retrofe' - should create symlink to /Applications/RetroFE.app
        size_t rootPos = sPath.find("/RetroFE.app/Contents/MacOS");
	if(rootPos!=std::string::npos) 
		sPath = sPath.erase(rootPos);
#else
        char exepath[1024] = {};
        char buffer[1024] = {}; // Separate buffer for readlink result
        sprintf(exepath, "/proc/%d/exe", getpid());
        ssize_t len = readlink(exepath, buffer, sizeof(buffer) - 1);
        if (len != -1)
        {
            buffer[len] = '\0'; // Null-terminate the result
            sPath = Utils::getDirectory(buffer);
        }
#endif

        absolutePath = sPath;
    }
}




void Configuration::clearProperties( )
{
    properties_.clear( );
}


bool Configuration::import(const std::string& keyPrefix, const std::string& file, bool mustExist)
{
    return import("", keyPrefix, file, mustExist);
}

bool Configuration::import(const std::string& collection, const std::string& keyPrefix, const std::string& file, bool mustExist)
{
    bool retVal = true;
    int lineCount = 0;
    std::string line;
    
    // Some dupe code in here we could probably bring out of branches but not yet
    if (keyPrefix == "CLI")
    {
        LOG_INFO("Configuration", "Importing command line arguments");
                
        // Use an istringstream to read lines from string
        std::istringstream iss(file);
                        
        while (std::getline(iss, line))
        {
            lineCount++;
            retVal = retVal && parseLine(collection, "", line, lineCount);

            // Check if the line contains the log level setting
            if (properties_.find(OPTION_LOG) != properties_.end()) {
                StartLogging(this); // Start logging with the specified level
            }
        }
    }
    else
    {
        LOG_INFO("Configuration", "Importing \"" + file + "\"");
        
        std::ifstream ifs(file.c_str());

        if (!ifs.is_open())
        {
            if (mustExist)
            {
                LOG_ERROR("Configuration", "Could not open " + file + "\"");
            }
            else
            {
                LOG_WARNING("Configuration", "Could not open " + file + "\"");
            }

            return false;
        }
       
        while (std::getline(ifs, line))
        {
            lineCount++;
            retVal = retVal && parseLine(collection, keyPrefix, line, lineCount);

            // Check if the line contains the log level setting
            if (properties_.find(OPTION_LOG) != properties_.end()) {
                StartLogging(this); // Start logging with the specified level
            }
        }

        ifs.close();
    }

    return retVal;
}


bool Configuration::parseLine(const std::string& collection, std::string keyPrefix, std::string line, int lineCount)
{
    bool retVal = false;
    std::string key;
    std::string value;
    size_t position;
    std::string delimiter = "=";

    // strip out any comments
    line = Utils::filterComments(line);
    
    if(line.empty() || (line.find_first_not_of(" \t\r") == std::string::npos))
    {
        retVal = true;
    }
    // all configuration fields must have an assignment operator
    else if ((position = line.find(delimiter)) != std::string::npos)
    {
        if (keyPrefix.size() != 0)
        {
            keyPrefix += ".";
        }

        key = keyPrefix + line.substr(0, position);

        key = trimEnds(key);


        value = line.substr(position + delimiter.length(), line.length());
        value = trimEnds(value);

        if (collection != "")
        {
            value = Utils::replace(value, "%ITEM_COLLECTION_NAME%", collection);
        }

        properties_[key] = value;

        std::stringstream ss;
        ss << "Dump: "  << "\"" << key << "\" = \"" << value << "\"";


        LOG_INFO("Configuration", ss.str());
        retVal = true;
    }
    else
    {
        std::stringstream ss;
        ss << "Missing an assignment operator (=) on line " << lineCount;
        LOG_ERROR("Configuration", ss.str());
    }

    return retVal;
}

std::string Configuration::trimEnds(std::string str)
{
    // strip off any initial tabs or spaces
    size_t trimStart = str.find_first_not_of(" \t");

    if(trimStart != std::string::npos)
    {
        size_t trimEnd = str.find_last_not_of(" \t");

        str = str.substr(trimStart, trimEnd - trimStart + 1);
    }

    return str;
}

bool Configuration::getRawProperty(const std::string& key, std::string& value)
{
    auto it = properties_.find(key); // Use iterator to search for the key
    if (it != properties_.end())
    {
        value = it->second; // Directly access the value from the iterator
        return true;
    }

    return false;
}

bool Configuration::getProperty(const std::string& key, std::string& value)
{
    bool retVal = getRawProperty(key, value);

    std::string baseMediaPath = absolutePath;
    std::string baseItemPath = absolutePath;

    baseMediaPath = Utils::combinePath(absolutePath, "collections");
    baseItemPath = Utils::combinePath(absolutePath, "collections");

    getRawProperty("baseMediaPath", baseMediaPath);
    getRawProperty("baseItemPath", baseItemPath);

    std::string_view valueView(value);

    if (valueView.find("%BASE_MEDIA_PATH%") != std::string_view::npos)
    {
        value = Utils::replace(value, "%BASE_MEDIA_PATH%", baseMediaPath);
    }
    if (valueView.find("%BASE_ITEM_PATH%") != std::string_view::npos)
    {
        value = Utils::replace(value, "%BASE_ITEM_PATH%", baseItemPath);
    }

    return retVal;
}


bool Configuration::getProperty(const std::string& key, int& value)
{
    std::string strValue;
    bool retVal = getProperty(key, strValue);

    if (retVal)
    {
        try {
            value = std::stoi(strValue);
        }
        catch (const std::invalid_argument&) {
            LOG_WARNING("RetroFE", "Invalid integer format for key: " + key);
        }
        catch (const std::out_of_range&) {
            LOG_WARNING("RetroFE", "Integer out of range for key: " + key);
        }
    }

    return retVal;
}


bool Configuration::getProperty(const std::string& key, bool& value)
{
    std::string strValue;
    bool retVal = getProperty(key, strValue);

    if (retVal)
    {
        std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
        value = (strValue == "yes" || strValue == "true" || strValue == "on");
    }

    return retVal;
}


void Configuration::setProperty(const std::string& key, const std::string& value)
{
    properties_[key] = value;
}

bool Configuration::propertiesEmpty() const
{
    return properties_.empty();
}

bool Configuration::propertyExists(const std::string& key)
{
    return (properties_.find(key) != properties_.end());
}

bool Configuration::propertyPrefixExists(const std::string& key)
{
    std::string search = key + ".";
    auto it = properties_.lower_bound(search);

    if (it != properties_.end() && it->first.compare(0, search.length(), search) == 0)
    {
        return true;
    }

    return false;
}


void Configuration::childKeyCrumbs(const std::string& parent, std::vector<std::string>& children)
{
    std::string search = parent + ".";
    auto it = properties_.lower_bound(search);
    std::set<std::string> uniqueChildren;

    while (it != properties_.end() && it->first.compare(0, search.length(), search) == 0)
    {
        std::string crumb = it->first.substr(search.length());
        std::size_t end = crumb.find_first_of(".");

        if (end != std::string::npos)
        {
            crumb = crumb.substr(0, end);
        }

        uniqueChildren.insert(crumb);
        ++it;
    }

    children.assign(uniqueChildren.begin(), uniqueChildren.end());
}


std::string Configuration::convertToAbsolutePath(const std::string& prefix, const std::string& path)
{
    std::filesystem::path fsPath(path);
    // Check if the path is already an absolute path
    if (!fsPath.is_absolute())
    {
        // Combine prefix and path
        std::filesystem::path absPath = std::filesystem::absolute(std::filesystem::path(prefix) / fsPath);
        return absPath.string();
    }
    return path;
}


bool Configuration::getPropertyAbsolutePath(const std::string& key, std::string &value)
{
    bool retVal = getProperty(key, value);

    if(retVal)
    {
        value = convertToAbsolutePath(absolutePath, value);
    }

    return retVal;
}

void Configuration::getMediaPropertyAbsolutePath(const std::string& collectionName, const std::string& mediaType, std::string &value)
{
    return getMediaPropertyAbsolutePath(collectionName, mediaType, false, value);
}


void Configuration::getMediaPropertyAbsolutePath(const std::string& collectionName, const std::string& mediaType, bool system, std::string &value)
{
    std::string key = "collections." + collectionName + ".media." + mediaType;
    if (system) 
    {
        key = "collections." + collectionName + ".media.system_artwork";
    }

    // use user-overridden setting if it exists
    if(getPropertyAbsolutePath(key, value))
    {
        return;
    }

    // use user-overridden base media path if it was specified
    std::string baseMediaPath;
    if(!getPropertyAbsolutePath("baseMediaPath", baseMediaPath))
    {
        // base media path was not specified, assume media files are in the collection
        baseMediaPath = Utils::combinePath(absolutePath, "collections");
    }

    if(system)
    {
        value = Utils::combinePath(baseMediaPath, collectionName, "system_artwork");
    }
    else
    {
        value = Utils::combinePath(baseMediaPath, collectionName, "medium_artwork", mediaType);
    }
}

void Configuration::getCollectionAbsolutePath(const std::string& collectionName, std::string &value)
{
    std::string key = "collections." + collectionName + ".list.path";

    if(getPropertyAbsolutePath(key, value))
    {
        return;
    }

    std::string baseItemPath;
    if(getPropertyAbsolutePath("baseItemPath", baseItemPath))
    {
        value = Utils::combinePath(baseItemPath, collectionName);
        return;
    }

    value = Utils::combinePath(absolutePath, "collections", collectionName, "roms");
}

bool Configuration::StartLogging(Configuration* config)
{

    if (std::string logFile = Utils::combinePath(Configuration::absolutePath, "log.txt"); !Logger::initialize(logFile, config))
    {
        // Can't write to logs give a heads up...
        fprintf(stderr, "Could not open log: %s for writing!\nRetroFE will now exit...\n", logFile.c_str());
        //LOG_ERROR("RetroFE", "Could not open \"" + logFile + "\" for writing");
        return false;
    }

    return true;
}

void Configuration::printProperties() const {
    // Separate items with and without prefixes
    std::vector<std::pair<std::string, std::string>> withPrefix, withoutPrefix;
    for (const auto& pair : properties_) {
        if (pair.first.find('.') != std::string::npos) {
            withPrefix.push_back(pair);
        }
        else {
            withoutPrefix.push_back(pair);
        }
    }
  
    // Sort each group
    std::sort(withoutPrefix.begin(), withoutPrefix.end(),
        [](const auto& a, const auto& b) {
            return Utils::toLower(a.first) < Utils::toLower(b.first);
        });
    std::sort(withPrefix.begin(), withPrefix.end(),
        [](const auto& a, const auto& b) {
            return Utils::toLower(a.first) < Utils::toLower(b.first);
        });
    for (const auto& pair : withoutPrefix) {
        fprintf(stdout, "%s=%s\n", pair.first.c_str(), pair.second.c_str());
    }
    for (const auto& pair : withPrefix) {
        fprintf(stdout, "%s=%s\n", pair.first.c_str(), pair.second.c_str());
    }
}

void Configuration::dumpPropertiesToFile(const std::string& filename) {
    // Separate items with and without prefixes
    std::vector<std::pair<std::string, std::string>> withPrefix, withoutPrefix;
    for (const auto& pair : properties_) {
        if (pair.first.find('.') != std::string::npos) {
            withPrefix.push_back(pair);
        }
        else {
            withoutPrefix.push_back(pair);
        }
    }
  
    // Sort each group
    std::sort(withoutPrefix.begin(), withoutPrefix.end(),
              [](const auto& a, const auto& b) {
        return Utils::toLower(a.first) < Utils::toLower(b.first);
    });
    std::sort(withPrefix.begin(), withPrefix.end(),
              [](const auto& a, const auto& b) {
        return Utils::toLower(a.first) < Utils::toLower(b.first);
    });
    
    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
        // Handle the error
        return;
    }
    for (const auto& pair : withoutPrefix) {
        file << pair.first << "=" << pair.second << std::endl;
    }
    for (const auto& pair : withPrefix) {
        file << pair.first << "=" << pair.second << std::endl;
    }
    file.close();
}
