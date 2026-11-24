#include "text_selection.h"
#include <cassert>
#include <iostream>
using namespace textsel;
int width(const std::string& text) {
    const auto glyph=ui::decode(text,0);
    if(ui::letterSpacing(glyph.code)==0)return 0;
    return glyph.code>=0x3400?14:7;
}
int main() {
    const auto mixed=makeRow("A苹果B",100,width);
    assert(mixed.cells.size()==4 && mixed.source.begin==100 && mixed.source.end==108);
    assert(mixed.cells[0].x==0 && mixed.cells[1].x==8 && mixed.cells[2].x==24 && mixed.cells[3].x==40 && mixed.endX==47);
    Selection pick;assert(!pick.start({}));assert(pick.start({mixed}));
    assert(pick.caret()==100);pick.move(Direction::Right,false);assert(pick.caret()==101);
    pick.move(Direction::Right,true);assert(pick.range().begin==101 && pick.range().end==104 && pick.selected(mixed.cells[1]) && !pick.selected(mixed.cells[0]));
    pick.move(Direction::Right,true);assert(pick.range().end==107);
    pick.move(Direction::Left,true);pick.move(Direction::Left,true);pick.move(Direction::Left,true);
    assert(pick.range().begin==100 && pick.range().end==101 && pick.selected(mixed.cells[0]));
    // Plain navigation clears the extension anchor, then Ctrl can extend again.
    pick.move(Direction::Right,false);assert(pick.range().begin==pick.range().end);
    const auto combining=makeRow("a\u0301b",0,width);assert(combining.cells.size()==2 && combining.cells[0].end==3 && combining.cells[1].x==8);
    const auto control=makeRow("A\r\t苹",10,width);assert(control.cells.size()==3 && control.cells[1].begin==12 && control.cells[1].x==8 && control.cells[2].x==16);
    // Soft wrapping preserves a contiguous English word without inserting LF.
    assert(pick.start({makeRow("app",0,width),makeRow("le",3,width)}));
    for(int i=0;i<5;++i)pick.move(Direction::Right,true);
    assert(pick.range().begin==0 && pick.range().end==5);
    pick.move(Direction::Left,true);assert(pick.caret()==4);
    pick.move(Direction::Left,true);assert(pick.caret()==3 && pick.row()==1);
    pick.move(Direction::Left,true);assert(pick.caret()==2 && pick.row()==0);
    // Hard newlines remain selectable source bytes, including empty lines.
    assert(pick.start({makeRow("AB",0,width),makeRow("",3,width),makeRow("果",4,width)}));
    pick.move(Direction::Down,true);assert(pick.caret()==3 && pick.row()==1);
    pick.move(Direction::Down,true);assert(pick.caret()==4 && pick.row()==2);
    assert(pick.range().begin==0 && pick.range().end==4);
    // Visual x remains fixed across short lines during vertical navigation.
    assert(pick.start({makeRow("ABCD",0,width),makeRow("苹",4,width),makeRow("EFGH",7,width)}));
    for(int i=0;i<3;++i)pick.move(Direction::Right,false);
    assert(pick.cursorX()==24);pick.move(Direction::Down,true);assert(pick.cursorX()==14);
    pick.move(Direction::Down,true);assert(pick.cursorX()==24 && pick.column()==3);
    assert(pick.move(Direction::Down,true)==Boundary::Next);
    // Page replacement is bounded, preserving the original range anchor.
    assert(pick.setPage({makeRow("新页",11,width)},false,Direction::Down,true));
    assert(pick.range().begin==3 && pick.range().end>=11);
    assert(pick.start({makeRow("苹果",1000,width)}));
    assert(pick.move(Direction::Left,true)==Boundary::Previous);
    assert(pick.setPage({makeRow("previous",992,width)},true,Direction::Left,true));
    assert(pick.caret()==1000);pick.move(Direction::Left,true);assert(pick.range().begin==999 && pick.range().end==1000);
    assert(queryText(" \t苹果\r\n ")=="苹果");assert(queryText("apple\r\n tree")=="apple tree");assert(queryText("　Ａ B　")=="Ａ B");
    const auto supplementary=makeRow("𠀀A",0,width);assert(supplementary.cells[0].end==4);
    pick.cancel();assert(!pick.active() && pick.range().begin==0 && pick.range().end==0);
    std::cout<<"Text selection passed: UTF-8/grapheme boundaries, half/full spacing, tabs/CR, soft/hard wrapping, range reversal, visual columns and bounded cross-page selection.\n";
}
