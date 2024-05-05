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
#pragma once

#include "../Collection/CollectionInfo.h"
#include "ThreadPool.h"

#include <map>
#include <string>
#include <list>
#include <vector>

class Component;
class Configuration;
class ScrollingList;
class Text;
class Item;
class Sound;

class Page
{

public:

    ThreadPool pool_{ 4 };
    ;    enum ScrollDirection
    {
        ScrollDirectionForward,
        ScrollDirectionBack,
        ScrollDirectionIdle

    };

    Page(Configuration &c, int layoutWidth, int layoutHeight );
    virtual ~Page();
    void deInitialize();
    virtual void onNewItemSelected();
    virtual void onNewScrollItemSelected();
    void returnToRememberSelectedItem();
    void rememberSelectedItem();
    std::map<std::string, size_t> getLastPlaylistOffsets() const;
    void highlightLoadArt();
    bool pushCollection(CollectionInfo *collection);
    bool popCollection();
    void enterMenu();
    void exitMenu();
    void enterGame();
    void exitGame();
    std::string getPlaylistName() const;
    void favPlaylist();
    void nextPlaylist();
    void prevPlaylist();
    void selectPlaylist(const std::string& playlist);
    void nextCyclePlaylist(std::vector<std::string> list);
    void prevCyclePlaylist(std::vector<std::string> list);
    void pushMenu(ScrollingList *s, int index = -1);
    void updatePlaylistMenuPosition();
    bool isMenusFull() const;
    void setLoadSound(Sound *chunk);
    void setUnloadSound(Sound *chunk);
    void setHighlightSound(Sound *chunk);
    void setSelectSound(Sound *chunk);
    Item *getSelectedMenuItem();
    ScrollingList* getAnActiveMenu();
    bool addComponent(Component *c);
    void pageScroll(ScrollDirection direction);
    void letterScroll(ScrollDirection direction);
    void metaScroll(ScrollDirection direction, std::string attribute);
    void cfwLetterSubScroll(ScrollDirection direction);
    size_t getCollectionSize();
    size_t getSelectedIndex();
    void selectRandom();
    void selectRandomPlaylist(CollectionInfo* collection, std::vector<std::string> cycleVector);
    void start();
    void stop();
    void setCurrentLayout(int layout);
    int getCurrentLayout() const;
    int getLayoutWidthByMonitor(int monitor);
    int getLayoutHeightByMonitor(int monitor);
    void setLayoutWidthByMonitor(int monitor, int width);
    void setLayoutHeightByMonitor(int monitor, int height);
    void setScrolling(ScrollDirection direction);
    bool isHorizontalScroll();
    unsigned int getMenuDepth() const;
    Item *getSelectedItem();
    Item *getSelectedItem(int offset);
    void removeSelectedItem();
    void setScrollOffsetIndex(size_t i);
    size_t getScrollOffsetIndex();
    bool isIdle();
    bool isAttractIdle();
    bool isGraphicsIdle();
    bool isMenuIdle();
    void setStatusTextComponent(Text *t);
    void update(float dt);
    void updateReloadables(float dt);
    void cleanup();
    void draw();
    void freeGraphicsMemory();
    void allocateGraphicsMemory();
    void deInitializeFonts( ) const;
    void initializeFonts( ) const;
    void playSelect();
    bool isSelectPlaying();
    std::string getCollectionName();
    CollectionInfo *getCollection();
    void  setMinShowTime(float value);
    float getMinShowTime() const;
    std::string controlsType() const;
    void setControlsType(const std::string& type);
    void  menuScroll();
    void  highlightEnter();
    void  highlightExit();
    void  playlistEnter();
    void  playlistExit();
    void playlistNextEnter();
    void playlistNextExit();
    void playlistPrevEnter();
    void playlistPrevExit();
    void triggerEventOnAllMenus(const std::string& event);
    void  menuJumpEnter();
    void  menuJumpExit();
    void  attractEnter( );
    void  attract( );
    void  attractExit( );
    void gameInfoEnter();
    void gameInfoExit();
    void collectionInfoEnter();
    void collectionInfoExit();
    void buildInfoEnter();
    void buildInfoExit();
    void  jukeboxJump( );
    void  triggerEvent( const std::string& action );
    void  setText( const std::string& text, int id );
    void  addPlaylist();
    void  removePlaylist();
    void  togglePlaylist();
    void  reallocateMenuSpritePoints(bool updatePlaylistMenu = true) const;
    bool  isMenuScrolling() const;
    bool  isPlaying() const;
    void  resetScrollPeriod() const;
    void  updateScrollPeriod() const;
    bool  isMenuFastScrolling() const;
    void  scroll(bool forward);
    bool  hasSubs();
    int   getLayoutWidth(int layout);
    int   getLayoutHeight(int layout);
    void  setLayoutWidth(int layout, int width);
    void  setLayoutHeight(int layout, int height);
    void  setJukebox();
    bool  isJukebox() const;
    bool  isJukeboxPlaying();
    void  skipForward( );
    void  skipBackward( );
    void  skipForwardp( );
    void  skipBackwardp( );
    void  pause( );
    void  restart( );
    unsigned long long getCurrent( );
    unsigned long long getDuration( );
    bool  isPaused( );
    void setLocked(bool locked);
    bool isLocked() const;
    ScrollingList* getPlaylistMenu();
    void setPlaylistMenu(ScrollingList*);
    bool playlistExists(const std::string&);
    void setSelectedItem();
    bool fromPreviousPlaylist = false;
    bool fromPlaylistNav = false;
    static const int MAX_LAYOUTS = 6; // TODO Put this behind a key 

private:
    void playlistChange();
    std::string collectionName_;
    Configuration &config_;
    std::string controlsType_;
    bool locked_;
    int currentLayout_;

    struct MenuInfo_S
    {
        CollectionInfo* collection;
        CollectionInfo::Playlists_T::iterator playlist;
        bool queueDelete;
    };
    using CollectionVector_T = std::list<MenuInfo_S>;
    
    using MenuVector_T = std::vector<std::vector<ScrollingList *>>;
    void setActiveMenuItemsFromPlaylist(MenuInfo_S info, ScrollingList* menu);

    std::vector<ScrollingList *> activeMenu_;
    ScrollingList* anActiveMenu_;
    ScrollingList* playlistMenu_;
    unsigned int menuDepth_;
    MenuVector_T menus_;
    CollectionVector_T collections_;
    CollectionVector_T deleteCollections_;

    static const unsigned int NUM_LAYERS = 20;
    std::vector<Component *> LayerComponents;
    std::list<ScrollingList *> deleteMenuList_;
    std::list<CollectionInfo *> deleteCollectionList_;
    std::map<std::string, size_t> lastPlaylistOffsets_;

    bool scrollActive_;

    Item *selectedItem_;
    Text *textStatusComponent_;
    Sound *loadSoundChunk_;
    Sound *unloadSoundChunk_;
    Sound *highlightSoundChunk_;
    Sound *selectSoundChunk_;
    float minShowTime_;
    CollectionInfo::Playlists_T::iterator playlist_;
    std::vector<int> layoutWidth_;
    std::vector<int> layoutHeight_;
    std::vector<int> layoutWidthByMonitor_;
    std::vector<int> layoutHeightByMonitor_;
    bool jukebox_;
    bool useThreading_;

};
