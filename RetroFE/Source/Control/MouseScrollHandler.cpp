#include "MouseScrollHandler.h"
#include <iostream>

MouseScrollHandler::MouseScrollHandler(int scrollAxis)
: scrollX_(0)
, scrollY_(0)
, scrolling_(false)
, scrollAxis_(scrollAxis)
{
}

void MouseScrollHandler::reset() {
    scrolling_ = false;
    scrollX_ = 0;
    scrollY_ = 0;
}

bool MouseScrollHandler::update(SDL_Event& e) {
    
    if(scrolling_) {
        std::cout << "in here" << std::endl;
        scrolling_ = false;
        return true;
    }

    if (e.type == SDL_MOUSEWHEEL && scrollAxis_ != 0) {
        if (scrollAxis_ == 1 && e.wheel.y > 0) {
            scrollY_ = e.wheel.y;
            printf("Mouse wheel scrolled: Y: %d, \n", scrollY_);
            scrolling_ = true;
        }
        else if (scrollAxis_ == 2 && e.wheel.y < 0) {
            scrollY_ = e.wheel.y;
            printf("Mouse wheel scrolled: Y: %d, \n", scrollY_);
            scrolling_ = true;
        }
        else if (scrollAxis_ == 3 && e.wheel.x > 0) {
            scrollX_ = e.wheel.x;
            printf("Mouse wheel scrolled: X: %d, \n", scrollX_);
            scrolling_ = true;
        }
        else if (scrollAxis_ == 4 && e.wheel.x < 0) {
            scrollX_ = e.wheel.x;
            printf("Mouse wheel scrolled: X: %d, \n", scrollX_);
            scrolling_ = true;
        }
                
        // Return true to indicate scrolling event handled
        return true;
    }
        
//    reset();
    return false;
}

bool MouseScrollHandler::isScrolling() {
    return scrolling_;
}

int MouseScrollHandler::getScrollX() {
    return scrollX_;
}

int MouseScrollHandler::getScrollY() {
    return scrollY_;
}

bool MouseScrollHandler::pressed() {
    return scrolling_;
}
