#pragma once
#include <cstdint>

class CursorBlink {
public:
    static constexpr uint32_t kIntervalMs=500;
    void show(uint32_t now) {active_=visible_=true;next_=now+kIntervalMs;}
    void hide() {active_=visible_=false;}
    bool visible() const {return visible_;}
    bool update(uint32_t now,bool enabled) {
        if(!enabled) {const bool changed=visible_;hide();return changed;}
        if(!active_) {show(now);return true;}
        if(int32_t(now-next_)>=0) {visible_=!visible_;next_=now+kIntervalMs;return true;}
        return false;
    }
private:
    uint32_t next_=0;bool active_=false,visible_=false;
};
