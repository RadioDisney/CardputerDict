#pragma once
#include "ui_text.h"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace textsel {
struct Span { uint32_t begin=0,end=0; };
struct Cell { uint32_t begin=0,end=0; int x=0,width=0; };
struct Row {
    Span source;
    std::vector<Cell> cells;
    int endX=0;
};
enum class Direction { Left,Right,Up,Down };
enum class Boundary { None,Previous,Next };
using Measure=std::function<int(const std::string&)>;

// Source offsets always refer to original UTF-8 bytes, including hidden CRs
// and real newlines. Soft wrapping never inserts a byte into the selection.
inline Row makeRow(const std::string& raw,uint32_t begin,const Measure& measure) {
    Row row;row.source={begin,begin+uint32_t(raw.size())};
    int x=0,previousSpacing=0;bool havePrevious=false;
    for(size_t at=0;at<raw.size();) {
        const auto glyph=ui::decode(raw,at);
        if(glyph.code=='\r') {at+=glyph.bytes;continue;}
        std::string text=raw.substr(at,glyph.bytes);
        if(glyph.code=='\t' || !glyph.code)text=" ";
        if(glyph.code==0xfffd)text="\xef\xbf\xbd";
        const int spacing=ui::letterSpacing(glyph.code=='\t' || !glyph.code?' ':glyph.code);
        if(spacing && havePrevious)x+=previousSpacing;
        const int width=std::max(0,measure(text));
        const auto first=begin+uint32_t(at),last=first+uint32_t(glyph.bytes);
        if(!spacing && !row.cells.empty()) {
            row.cells.back().end=last;row.cells.back().width=std::max(row.cells.back().width,x+width-row.cells.back().x);
        } else row.cells.push_back({first,last,x,width});
        x+=width;
        if(spacing) {previousSpacing=spacing;havePrevious=true;}
        at+=glyph.bytes;
    }
    row.endX=x;return row;
}

class Selection {
public:
    void cancel() {active_=false;rows_.clear();row_=column_=0;anchor_=0;goalX_=-1;}
    bool active() const {return active_;}
    bool start(std::vector<Row> rows) {
        if(rows.empty())return false;
        rows_=std::move(rows);active_=true;row_=column_=0;goalX_=-1;anchor_=caret();return true;
    }
    bool setPage(std::vector<Row> rows,bool previous,Direction direction,bool extend) {
        if(!active_ || rows.empty())return false;
        rows_=std::move(rows);row_=previous?rows_.size()-1:0;
        if(direction==Direction::Up || direction==Direction::Down)column_=nearest(rows_[row_],std::max(0,goalX_));
        else column_=previous?rows_[row_].cells.size():0;
        if(!extend)anchor_=caret();return true;
    }
    Boundary move(Direction direction,bool extend) {
        if(!active_ || rows_.empty())return Boundary::None;
        if(direction==Direction::Up || direction==Direction::Down) {
            if(goalX_<0)goalX_=cursorX();
            if(direction==Direction::Up) {
                if(!row_)return Boundary::Previous;
                --row_;
            } else {
                if(row_+1==rows_.size())return Boundary::Next;
                ++row_;
            }
            column_=nearest(rows_[row_],goalX_);
        } else {
            goalX_=-1;
            if(direction==Direction::Left) {
                if(column_)--column_;
                else {
                    if(!row_)return Boundary::Previous;
                    const auto oldBegin=rows_[row_].source.begin;--row_;column_=rows_[row_].cells.size();
                    if(rows_[row_].source.end==oldBegin && column_)--column_;
                }
            } else {
                if(column_<rows_[row_].cells.size()) {
                    ++column_;
                    if(column_==rows_[row_].cells.size() && row_+1<rows_.size() &&
                       rows_[row_].source.end==rows_[row_+1].source.begin) {++row_;column_=0;}
                } else {
                    if(row_+1==rows_.size())return Boundary::Next;
                    ++row_;column_=0;
                }
            }
        }
        if(!extend)anchor_=caret();
        return Boundary::None;
    }
    uint32_t caret() const {
        if(rows_.empty())return 0;
        const auto& row=rows_[row_];return column_<row.cells.size()?row.cells[column_].begin:row.source.end;
    }
    Span range() const {return {std::min(anchor_,caret()),std::max(anchor_,caret())};}
    bool selected(const Cell& cell) const {const auto bounds=range();return cell.begin<bounds.end && cell.end>bounds.begin;}
    int cursorX() const {const auto& row=rows_[row_];return column_<row.cells.size()?row.cells[column_].x:row.endX;}
    size_t row() const {return row_;}
    size_t column() const {return column_;}
    const std::vector<Row>& rows() const {return rows_;}
private:
    std::vector<Row> rows_;size_t row_=0,column_=0;uint32_t anchor_=0;int goalX_=-1;bool active_=false;
    static size_t nearest(const Row& row,int x) {
        size_t result=row.cells.size();int distance=std::abs(row.endX-x);
        for(size_t i=0;i<row.cells.size();++i) {
            const int current=std::abs(row.cells[i].x-x);
            if(current<=distance) {distance=current;result=i;}
        }
        return result;
    }
};

inline std::string queryText(const std::string& raw) {
    std::string result;bool space=false;
    for(size_t at=0;at<raw.size();) {
        const auto c=ui::decode(raw,at);
        if(c.code<=32 || c.code==127 || c.code==0x3000) {space=!result.empty();}
        else {
            if(space)result+=' ';
            space=false;result+=raw.substr(at,c.bytes);
        }
        at+=c.bytes;
    }
    return result;
}
} // namespace textsel
