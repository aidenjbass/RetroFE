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

#include "ReloadableMedia.h"
#include "ImageBuilder.h"
#include "VideoBuilder.h"
#include "ReloadableText.h"
#include "../ViewInfo.h"
#include "../../Video/VideoFactory.h"
#include "../../Database/Configuration.h"
#include "../../Database/GlobalOpts.h"
#include "../../Utility/Log.h"
#include "../../Utility/Utils.h"
#include "../../SDL.h"
#include <fstream>
#include <vector>
#include <iostream>

ReloadableMedia::ReloadableMedia(Configuration& config, bool systemMode, bool layoutMode, bool commonMode, [[maybe_unused]] bool menuMode, const std::string& type, const std::string& imageType,
    Page& p, int displayOffset, bool isVideo, Font* font, bool jukebox, int jukeboxNumLoops, int randomSelect)
    : Component(p)
    , config_(config)
    , systemMode_(systemMode)
    , layoutMode_(layoutMode)
    , commonMode_(commonMode)
    , randomSelect_(randomSelect)
    , isVideo_(isVideo)
    , FfntInst_(font)
    , type_(type)
    , displayOffset_(displayOffset)
    , imageType_(imageType)
    , jukebox_(jukebox)
    , jukeboxNumLoops_(jukeboxNumLoops)
{
    allocateGraphicsMemory();
}

ReloadableMedia::~ReloadableMedia()
{
    if (loadedComponent_ != nullptr) {
        delete loadedComponent_;
        loadedComponent_ = nullptr;
    }
}

void ReloadableMedia::enableTextFallback_(bool value)
{
    textFallback_ = value;
}

bool ReloadableMedia::update(float dt)
{
    if (newItemSelected ||
        (newScrollItemSelected && getMenuScrollReload())) {
        newItemSelected = false;
        newScrollItemSelected = false;
        Component* foundComponent = reloadTexture();  // Removed the re-declaration here.
        if (foundComponent) {
            foundComponent->playlistName = page.getPlaylistName();
            foundComponent->allocateGraphicsMemory();
            baseViewInfo.ImageWidth = foundComponent->baseViewInfo.ImageWidth;
            baseViewInfo.ImageHeight = foundComponent->baseViewInfo.ImageHeight;
            foundComponent->update(dt);
            // if found and it's not the same as loaded, then finally delete the loaded component
            if (foundComponent != loadedComponent_) {
                delete loadedComponent_;
                loadedComponent_ = foundComponent;
            }
        }
        else {
            // delete previous loaded item if none found
            delete loadedComponent_;
            loadedComponent_ = nullptr;  // Set to nullptr to avoid dangling pointer.
        }
    }
    else if (loadedComponent_) {
        loadedComponent_->update(dt);
    }

    // needs to be ran at the end to prevent the NewItemSelected flag from being detected
    return Component::update(dt);
}


void ReloadableMedia::allocateGraphicsMemory()
{
    if(loadedComponent_) {
        loadedComponent_->allocateGraphicsMemory();
    }

    // NOTICE! needs to be done last to prevent flags from being missed
    Component::allocateGraphicsMemory();
}


void ReloadableMedia::freeGraphicsMemory()
{
    Component::freeGraphicsMemory();

    if(loadedComponent_) {
        loadedComponent_->freeGraphicsMemory();
    }
}


Component *ReloadableMedia::reloadTexture()
{
    std::string typeLC = Utils::toLower(type_);
    Item* selectedItem = page.getSelectedItem(displayOffset_);

    if(loadedComponent_ && !selectedItem) {
            
            delete loadedComponent_;
            loadedComponent_ = nullptr;
    }

    if(!selectedItem) return nullptr;

    config_.getProperty("currentCollection", currentCollection_);

    // build clone list
    std::vector<std::string> names;

    names.push_back(selectedItem->name);
    names.push_back(selectedItem->fullTitle);
    if(selectedItem->cloneof.length() > 0) {
        names.push_back(selectedItem->cloneof);
    }

    if (typeLC == "isfavorite") {
        if (selectedItem->isFavorite) {
            names.emplace_back("yes");
        }
        else {
            names.emplace_back("no");
        }
    }
    if (typeLC == "ispaused") {
        if (page.isPaused()) {
            names.emplace_back("yes");
        }
        else {
            names.emplace_back("no");
        }
    }
    if (typeLC == "islocked") {
        if (page.isLocked()) {
            names.emplace_back("yes");
        }
        else {
            names.emplace_back("no");
        }
    }

    names.emplace_back("default");
    // if same playlist then use existing loaded component
    Component* foundComponent = nullptr;
    if (loadedComponent_ != nullptr && 
        (typeLC.rfind("playlist", 0) == 0 && 
        page.getPlaylistName() == loadedComponent_->playlistName)
    ) {
        return loadedComponent_;
    }

    if(isVideo_) {
        for(unsigned int n = 0; n < names.size() && !foundComponent; ++n) {
            std::string basename = names[n];

            // support reloadable video based on playlist# type
            if (basename != "default" && typeLC.rfind("playlist", 0) == 0) {
                basename = page.getPlaylistName();
            }

            if(systemMode_) {
                // check the master collection for the system artifact
                foundComponent = findComponent(collectionName, type_, type_, "", true, true);

                // check the collection for the system artifact
                if(!foundComponent)
                {
                    foundComponent = findComponent(selectedItem->collectionInfo->name, type_, type_, "", true, true);
                }
            }
            else {

                // are we looking at a leaf or a submenu
                if (selectedItem->leaf) // item is a leaf 
                {

                  // check the master collection for the artifact 
                    foundComponent = findComponent(collectionName, type_, basename, "", false, true);

                  // check the collection for the artifact
                  if(!foundComponent) {
                      foundComponent = findComponent(selectedItem->collectionInfo->name, type_, basename, "", false, true);
                  }

                  // check the rom directory for the artifact
                  if(!foundComponent) {
                      foundComponent = findComponent(selectedItem->collectionInfo->name, type_, type_, selectedItem->filepath, false, true);
                  }
                }
                else // item is a submenu
                {
                  // check the master collection for the artifact 
                    foundComponent = findComponent(collectionName, type_, basename, "", false, true);

                  // check the collection for the artifact
                  if(!foundComponent) {
                      foundComponent = findComponent(selectedItem->collectionInfo->name, type_, basename, "", false, true);
                  }

                  // check the submenu collection for the system artifact
                  if (!foundComponent) {
                      foundComponent = findComponent(selectedItem->name, type_, type_, "", true, true);
                  }
                }
            }
            if(foundComponent) {
                return foundComponent;
            }
        }
    }

    // check for images, also if video could not be found (and was specified)
    for(unsigned int n = 0; n < names.size() && !foundComponent; ++n) {
        std::string basename = names[n];
        bool        defined  = false;
        std::string type     = type_;

        if ( isVideo_ ) {
            typeLC = Utils::toLower(imageType_);
            type   = imageType_;
        }

        if(basename == "default") {
            basename = "default";
            defined  = true;
        }
        else if(typeLC == "numberbuttons") {
            basename = selectedItem->numberButtons;
            defined  = true;
        }
        else if(typeLC == "numberplayers") {
            basename = selectedItem->numberPlayers;
            defined  = true;
        }
        else if(typeLC == "year") {
            basename = selectedItem->year;
            defined  = true;
        }
        else if(typeLC == "title") {
            basename = selectedItem->title;
            defined  = true;
        }
        else if(typeLC == "developer") {
            basename = selectedItem->developer;
            defined  = true;
            // Overwrite in case developer has not been specified
            if (basename == "") {
                basename = selectedItem->manufacturer;
            }
        }
        else if(typeLC == "manufacturer") {
            basename = selectedItem->manufacturer;
            defined  = true;
        }
        else if(typeLC == "genre") {
            basename = selectedItem->genre;
            defined  = true;
        }
        else if(typeLC == "ctrltype") {
            basename = selectedItem->ctrlType;
            defined  = true;
        }
        else if(typeLC == "joyways") {
            basename = selectedItem->joyWays;
            defined  = true;
        }
        else if(typeLC == "rating") {
            basename = selectedItem->rating;
            defined  = true;
        }
        else if(typeLC == "score") {
            basename = selectedItem->score;
            defined  = true;
        }
        else if (typeLC == "playcount") {
            basename = std::to_string(selectedItem->playCount);
            defined = true;
        }
        else if(typeLC.rfind( "playlist", 0 ) == 0) {
            basename = page.getPlaylistName();
            defined  = true;
        }
        else if (typeLC == "firstletter") {
            basename = selectedItem->fullTitle.at(0);
            defined  = true;
        }
        else if (typeLC == "position" && !selectedItem->collectionInfo->items.empty()) {
            if (size_t position = page.getSelectedIndex() + 1; position == 1) {
                basename = '1';
            }
            else if (position == page.getCollectionSize()) {
                basename = std::to_string(numberOfImages_);
            }
            else {
                basename = std::to_string(static_cast<int>(ceil(static_cast<float>(position) / static_cast<float>(page.getCollectionSize()) * static_cast<float>(numberOfImages_))));
            }
            defined = true;
        }

        if (!selectedItem->leaf) // item is not a leaf
        {
            (void)config_.getProperty("collections." + selectedItem->name + "." + type, basename );
        }

        bool overwriteXML = false;
        config_.getProperty( OPTION_OVERWRITEXML, overwriteXML );
        if ( !defined || overwriteXML ) // No basename was found yet; check the info in stead
        {
            std::string basename_tmp;
            selectedItem->getInfo( type, basename_tmp );
            if ( basename_tmp != "" )
            {
                basename = basename_tmp;
            }
        }

        Utils::replaceSlashesWithUnderscores(basename);

        // ability to randomly select image/video
        if (randomSelect_) {
            int randImage = 1 + rand() % randomSelect_;
            basename = basename + " - " + std::to_string(randImage);
        }

        if(systemMode_) {
            // check the master collection for the system artifact 
            foundComponent = findComponent(collectionName, type, type, "", true, false);

            // check collection for the system artifact
            if(!foundComponent) {
                foundComponent = findComponent(selectedItem->collectionInfo->name, type, type, "", true, false);
            }
            
            // check selected item that's a collection
            if (!foundComponent && !selectedItem->leaf) {
                foundComponent = findComponent(selectedItem->name, type, type, "", true, false);
            }
        }
        else {
            // are we looking at a leaf or a submenu
            if (selectedItem->leaf) // item is a leaf 
            {
                // check the master collection for the artifact
                foundComponent = findComponent(collectionName, type, basename, "", false, false);

            // check the collection for the artifact
            if(!foundComponent) {
                foundComponent = findComponent(selectedItem->collectionInfo->name, type, basename, "", false, false);
            }

            // check the rom directory for the artifact
            if(!foundComponent){
                foundComponent = findComponent(selectedItem->collectionInfo->name, type, type, selectedItem->filepath, false, false);
            }
        }
            else // item is a submenu
            {
                // check the master collection for the artifact 
                foundComponent = findComponent(collectionName, type, basename, "", false, false);

                // check the collection for the artifact
                if(!foundComponent) {
                  foundComponent = findComponent(selectedItem->collectionInfo->name, type, basename, "", false, false);
                }

                // check the submenu collection for the system artifact
                if (!foundComponent){
                  foundComponent = findComponent(selectedItem->name, type, type, "", true, false);
                }
            }
        }

        if (foundComponent != nullptr) {
            return foundComponent;
        }
    }

    // if image and artwork was not specified, fall back to displaying text
    if(!foundComponent && textFallback_) {
        return new Text(selectedItem->fullTitle, page, FfntInst_, baseViewInfo.Monitor);
    }
    return foundComponent;
}


Component* ReloadableMedia::findComponent(
    const std::string& collection,
    const std::string& type,
    const std::string& basename,
    std::string_view filepath, // pass by const reference
    bool systemMode,
    bool isVideo) 
{
    std::string imagePath;
    Component *component = nullptr;
    VideoBuilder videoBuild{};
    ImageBuilder imageBuild{};

    if (filepath != "") {
        imagePath = filepath; 
    } 
    else {
        // check the system folder
        if (layoutMode_) {
            // check if collection's assets are in a different theme
            std::string layoutName;
            config_.getProperty("collections." + collection + ".layout", layoutName);
            if (layoutName == "") {
                config_.getProperty(OPTION_LAYOUT, layoutName);
            }
            if (commonMode_) {
                imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", "_common");
            }
            else {
                imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", collection);
            }
            if (systemMode)
                imagePath = Utils::combinePath(imagePath, "system_artwork");
            else
                imagePath = Utils::combinePath(imagePath, "medium_artwork", type);
        }
        else {
            if (commonMode_) {
                imagePath = Utils::combinePath(Configuration::absolutePath, "collections", "_common" );
                if (systemMode)
                    imagePath = Utils::combinePath(imagePath, "system_artwork");
                else
                    imagePath = Utils::combinePath(imagePath, "medium_artwork", type);
            }
            else {
                config_.getMediaPropertyAbsolutePath(collection, type, systemMode, imagePath);
            }
        }
    }

    // if file already loaded, don't load again
    const std::vector<std::string>& extensions = isVideo ? ReloadableMedia::videoExtensions : ReloadableMedia::imageExtensions;
    
    if (loadedComponent_ != nullptr && !imagePath.empty()) {
        std::string filePath;
        if (Utils::findMatchingFile(Utils::combinePath(imagePath, basename), extensions, filePath)) {
            if (filePath == loadedComponent_->filePath()) {
                return loadedComponent_;
            }
        }
    }

    if(isVideo) {
        if ( jukebox_ )
            component = videoBuild.createVideo(imagePath, page, basename, baseViewInfo.Monitor, jukeboxNumLoops_);
        else
            component = videoBuild.createVideo(imagePath, page, basename, baseViewInfo.Monitor);
    }
    else {
        component = imageBuild.CreateImage(imagePath, page, basename, baseViewInfo.Monitor, baseViewInfo.Additive);
    }

    return component;

}


std::string filePath()
{
    return "";
}

void ReloadableMedia::draw()
{
    Component::draw();

    if(loadedComponent_) {
    	baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
    	baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
        loadedComponent_->baseViewInfo = baseViewInfo;
        if(baseViewInfo.Alpha > 0.0f)
            loadedComponent_->draw();
    }
}


bool ReloadableMedia::isJukeboxPlaying()
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->isPlaying();
    else
        return false;
}


void ReloadableMedia::skipForward( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipForward( );
}


void ReloadableMedia::skipBackward( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipBackward( );
}


void ReloadableMedia::skipForwardp( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipForwardp( );
}


void ReloadableMedia::skipBackwardp( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipBackwardp( );
}


void ReloadableMedia::pause( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->pause( );
}


void ReloadableMedia::restart( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->restart( );
}


unsigned long long ReloadableMedia::getCurrent( )
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->getCurrent( );
    else
        return 0;
}


unsigned long long ReloadableMedia::getDuration( )
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->getDuration( );
    else
        return 0;
}


bool ReloadableMedia::isPaused( )
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->isPaused( );
    else
        return false;
}


