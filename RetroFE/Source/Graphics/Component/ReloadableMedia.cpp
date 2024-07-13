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
    if (newItemSelected || (newScrollItemSelected && getMenuScrollReload())) {
        newItemSelected = false;
        newScrollItemSelected = false;

        Component* newComponent = reloadTexture();

        if (newComponent) {
            newComponent->playlistName = page.getPlaylistName();
            newComponent->allocateGraphicsMemory();
            baseViewInfo.ImageWidth = newComponent->baseViewInfo.ImageWidth;
            baseViewInfo.ImageHeight = newComponent->baseViewInfo.ImageHeight;
            newComponent->update(dt);
        }
        else {
            // No new component found, clean up the old one
            delete loadedComponent_;
            loadedComponent_ = nullptr;
        }
    }
    else if (loadedComponent_) {
        // No new selection, update the current component
        loadedComponent_->update(dt);
    }

    // Run the base class update last to prevent the NewItemSelected flag from being detected
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


Component* ReloadableMedia::reloadTexture()
{
    std::string typeLC = Utils::toLower(type_);
    Item* selectedItem = page.getSelectedItem(displayOffset_);

    if (!selectedItem) {
        if (loadedComponent_) {
            delete loadedComponent_;
            loadedComponent_ = nullptr;
        }
        return nullptr;
    }

    config_.getProperty("currentCollection", currentCollection_);

    std::vector<std::string> names;
    names.push_back(selectedItem->name);
    names.push_back(selectedItem->fullTitle);
    if (selectedItem->cloneof.length() > 0) {
        names.push_back(selectedItem->cloneof);
    }

    if (typeLC == "isfavorite") {
        names.emplace_back(selectedItem->isFavorite ? "yes" : "no");
    }
    if (typeLC == "ispaused") {
        names.emplace_back(page.isPaused() ? "yes" : "no");
    }
    if (typeLC == "islocked") {
        names.emplace_back(page.isLocked() ? "yes" : "no");
    }

    names.emplace_back("default");

    // Determine the basename for reuse check
    std::string basename;
    if (typeLC == "numberbuttons") {
        basename = selectedItem->numberButtons;
    }
    else if (typeLC == "numberplayers") {
        basename = selectedItem->numberPlayers;
    }
    else if (typeLC == "year") {
        basename = selectedItem->year;
    }
    else if (typeLC == "title") {
        basename = selectedItem->title;
    }
    else if (typeLC == "developer") {
        basename = selectedItem->developer.empty() ? selectedItem->manufacturer : selectedItem->developer;
    }
    else if (typeLC == "manufacturer") {
        basename = selectedItem->manufacturer;
    }
    else if (typeLC == "genre") {
        basename = selectedItem->genre;
    }
    else if (typeLC == "ctrltype") {
        basename = selectedItem->ctrlType;
    }
    else if (typeLC == "joyways") {
        basename = selectedItem->joyWays;
    }
    else if (typeLC == "rating") {
        basename = selectedItem->rating;
    }
    else if (typeLC == "score") {
        basename = selectedItem->score;
    }
    else if (typeLC == "playcount") {
        basename = std::to_string(selectedItem->playCount);
    }
    else if (typeLC == "firstletter") {
        basename = selectedItem->fullTitle.at(0);
    }
    else if (typeLC == "position" && !selectedItem->collectionInfo->items.empty()) {
        size_t position = page.getSelectedIndex() + 1;
        if (position == 1) {
            basename = "1";
        }
        else if (position == page.getCollectionSize()) {
            basename = std::to_string(numberOfImages_);
        }
        else {
            basename = std::to_string(static_cast<int>(ceil(static_cast<float>(position) / static_cast<float>(page.getCollectionSize()) * static_cast<float>(numberOfImages_))));
        }
    }
    else if (typeLC.rfind("playlist", 0) == 0) {
        basename = page.getPlaylistName();
    }
    else if (isVideo_) {
        basename = selectedItem->name;
    }
    else if (typeLC == "logo") {
        basename = selectedItem->name;
    }
    else {
        basename = "default";
    }

    // Reuse the existing component if the typeLC and basename match
    if (loadedComponent_ && currentTypeLC_ == typeLC && currentBasename_ == basename) {
        return loadedComponent_;
    }

    Component* foundComponent = nullptr;

    for (unsigned int n = 0; n < names.size() && !foundComponent; ++n) {
        std::string name = names[n];

        if (isVideo_) {
            if (name != "default" && typeLC.rfind("playlist", 0) == 0) {
                name = page.getPlaylistName();
            }
            else {
                name = selectedItem->name;
            }

            if (systemMode_) {
                foundComponent = findComponent(collectionName, type_, type_, "", true, true);
                if (!foundComponent) {
                    foundComponent = findComponent(selectedItem->collectionInfo->name, type_, type_, "", true, true);
                }
            }
            else {
                if (selectedItem->leaf) {
                    foundComponent = findComponent(collectionName, type_, name, "", false, true);
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->collectionInfo->name, type_, name, "", false, true);
                    }
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->collectionInfo->name, type_, type_, selectedItem->filepath, false, true);
                    }
                }
                else {
                    foundComponent = findComponent(collectionName, type_, name, "", false, true);
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->collectionInfo->name, type_, name, "", false, true);
                    }
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->name, type_, type_, "", true, true);
                    }
                }
            }
        }
        else {
            std::string type = type_;  // Define and initialize type here

            if (name == "default") {
                name = "default";
            }
            else if (typeLC == "numberbuttons") {
                name = selectedItem->numberButtons;
            }
            else if (typeLC == "numberplayers") {
                name = selectedItem->numberPlayers;
            }
            else if (typeLC == "year") {
                name = selectedItem->year;
            }
            else if (typeLC == "title") {
                name = selectedItem->title;
            }
            else if (typeLC == "developer") {
                name = selectedItem->developer.empty() ? selectedItem->manufacturer : selectedItem->developer;
            }
            else if (typeLC == "manufacturer") {
                name = selectedItem->manufacturer;
            }
            else if (typeLC == "genre") {
                name = selectedItem->genre;
            }
            else if (typeLC == "ctrltype") {
                name = selectedItem->ctrlType;
            }
            else if (typeLC == "joyways") {
                name = selectedItem->joyWays;
            }
            else if (typeLC == "rating") {
                name = selectedItem->rating;
            }
            else if (typeLC == "score") {
                name = selectedItem->score;
            }
            else if (typeLC == "playcount") {
                name = std::to_string(selectedItem->playCount);
            }
            else if (typeLC.rfind("playlist", 0) == 0) {
                name = page.getPlaylistName();
            }
            else if (typeLC == "firstletter") {
                name = selectedItem->fullTitle.at(0);
            }
            else if (typeLC == "position" && !selectedItem->collectionInfo->items.empty()) {
                size_t position = page.getSelectedIndex() + 1;
                if (position == 1) {
                    name = "1";
                }
                else if (position == page.getCollectionSize()) {
                    name = std::to_string(numberOfImages_);
                }
                else {
                    name = std::to_string(static_cast<int>(ceil(static_cast<float>(position) / static_cast<float>(page.getCollectionSize()) * static_cast<float>(numberOfImages_))));
                }
            }
            else if (typeLC == "logo") {
                basename = selectedItem->name;
            }

            if (!selectedItem->leaf) {
                (void)config_.getProperty("collections." + selectedItem->name + "." + type, name);
            }

            bool overwriteXML = false;
            config_.getProperty(OPTION_OVERWRITEXML, overwriteXML);
            if (!overwriteXML) {
                std::string name_tmp;
                selectedItem->getInfo(type, name_tmp);
                if (!name_tmp.empty()) {
                    name = name_tmp;
                }
            }

            Utils::replaceSlashesWithUnderscores(name);

            if (randomSelect_) {
                int randImage = 1 + rand() % randomSelect_;
                name = name + " - " + std::to_string(randImage);
            }

            if (systemMode_) {
                foundComponent = findComponent(collectionName, type, type, "", true, false);
                if (!foundComponent) {
                    foundComponent = findComponent(selectedItem->collectionInfo->name, type, type, "", true, false);
                }
                if (!foundComponent && !selectedItem->leaf) {
                    foundComponent = findComponent(selectedItem->name, type, type, "", true, false);
                }
            }
            else {
                if (selectedItem->leaf) {
                    foundComponent = findComponent(collectionName, type, name, "", false, false);
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->collectionInfo->name, type, name, "", false, false);
                    }
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->collectionInfo->name, type, type, selectedItem->filepath, false, false);
                    }
                }
                else {
                    foundComponent = findComponent(collectionName, type, name, "", false, false);
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->collectionInfo->name, type, name, "", false, false);
                    }
                    if (!foundComponent) {
                        foundComponent = findComponent(selectedItem->name, type, type, "", true, false);
                    }
                }
            }
        }

        if (foundComponent) {
            break;
        }
    }

    if (!foundComponent && textFallback_) {
        foundComponent = new Text(selectedItem->fullTitle, page, FfntInst_, baseViewInfo.Monitor);
    }

    if (foundComponent != loadedComponent_) {
        delete loadedComponent_;
        loadedComponent_ = foundComponent;

        // Update the tracked attributes
        currentTypeLC_ = typeLC;
        currentBasename_ = basename;
    }

    return loadedComponent_;
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


