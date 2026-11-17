#include "navigation_keys.h"
#include <cassert>
#include <iostream>
int main() {
    for(char key:{';','.',',','/'}) {
        assert(keyboard::isDirectionKey(key));
        assert(keyboard::directionFn(key,false)); // direct physical arrow -> navigation layer
        assert(!keyboard::directionFn(key,true)); // physical Fn+arrow -> literal/Shift punctuation layer
    }
    for(int code=0;code<256;++code) {
        const char key=char(code);
        if(keyboard::isDirectionKey(key))continue;
        assert(!keyboard::directionFn(key,false));
        assert(keyboard::directionFn(key,true));
    }
    std::cout<<"Direction Fn inversion passed: all four arrows use the navigation layer directly; Fn punctuation and every other shortcut remain available.\n";
}
