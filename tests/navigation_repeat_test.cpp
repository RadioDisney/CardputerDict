#include "navigation_keys.h"
#include "key_repeat.h"
#include <cassert>
#include <iostream>
#include <vector>
int main() {
    for(const auto key:keyboard::kDirectionKeyIds)assert(keyboard::directionIndex(key)>=0);
    assert(keyboard::directionIndex(13)==-1);
    assert(keyboard::repeatDirectionAllowed(false,false,false,false,false,false));
    assert(keyboard::repeatDirectionAllowed(false,true,false,false,true,false));
    assert(!keyboard::repeatDirectionAllowed(false,true,false,false,false,false));
    for(int option=0;option<4;++option)assert(!keyboard::repeatDirectionAllowed(option==0,false,option==1,option==2,true,option==3));
    KeyRepeat directions[4];
    for(auto& key:directions) {
        std::vector<uint32_t> events;
        for(uint32_t now=0;now<=1100;now+=5)if(key.update(now,true))events.push_back(now);
        assert((events==std::vector<uint32_t>{0,450,575,700,825,950,1075}));
        assert(!key.update(1200,false));assert(key.update(1300,true));
        // Blocking lookup or switching to Fn punctuation suppresses until release.
        assert(!key.update(1350,true,false));assert(!key.update(3000,true));
        assert(!key.update(3001,false));assert(key.update(3002,true));
    }
    std::cout<<"Navigation repeat passed: all four raw keys, Ctrl selection allowed, Fn/Alt/Opt and lookup blocked, 450ms delay/125ms repeats, release and suppression.\n";
}
