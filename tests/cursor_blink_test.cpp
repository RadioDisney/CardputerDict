#include "cursor_blink.h"
#include <cassert>
#include <iostream>
int main() {
    CursorBlink cursor;assert(!cursor.visible());
    cursor.show(100);assert(cursor.visible());
    assert(!cursor.update(599,true) && cursor.visible());
    assert(cursor.update(600,true) && !cursor.visible());
    assert(!cursor.update(1099,true) && !cursor.visible());
    assert(cursor.update(1100,true) && cursor.visible());
    cursor.show(1400);assert(!cursor.update(1899,true));assert(cursor.update(1900,true) && !cursor.visible());
    // A blocked frame advances once, without repeated catch-up redraws.
    assert(cursor.update(9000,true) && cursor.visible());assert(!cursor.update(9000,true));
    assert(cursor.update(9001,false) && !cursor.visible());assert(!cursor.update(9500,false));
    assert(cursor.update(9600,true) && cursor.visible());
    cursor.show(UINT32_MAX-200);assert(!cursor.update(298,true));assert(cursor.update(299,true) && !cursor.visible());
    std::cout<<"Cursor blink passed: 500ms phases, immediate visible reset on movement, stop, slow redraws and millis rollover.\n";
}
