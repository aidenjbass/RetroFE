#pragma once

#include "InputHandler.h"

class MouseScrollHandler : public InputHandler
{
public:
    MouseScrollHandler(int scrollAxis);
    void reset();
    bool update(SDL_Event& e);
    bool isScrolling();
    int getScrollX();
    int getScrollY();
    
    bool pressed();
    void updateKeystate() {};
            
private:
    Sint32 scrollX_;
    Sint32 scrollY_;
    bool scrolling_;
    int scrollAxis_;
};
