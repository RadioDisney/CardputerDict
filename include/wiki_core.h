#pragma once
#include "dict_core.h"
#include "ui_text.h"
#include <array>

namespace wiki {
constexpr uint32_t kHeaderBytes = 64, kRecordBytes = 20, kMaxTitleBytes = 768;
constexpr uint32_t kMaxChunkBytes = 128u * 1024 * 1024;
inline uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
inline uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
inline uint64_t le64(const uint8_t* p) { return uint64_t(le32(p)) | uint64_t(le32(p + 4)) << 32; }
struct Header { uint32_t count=0, titleBytes=0, chunkBytes=0; uint64_t textBytes=0; };
inline bool parseHeader(const uint8_t* bytes, size_t size, Header& out) {
    if (size != kHeaderBytes || memcmp(bytes,"CPWIKI1\0",8) || le32(bytes+8)!=1 || le32(bytes+32)!=kRecordBytes ||
        le32(bytes+36)!=(~dict::crc32(bytes,36))) return false;
    for (size_t i=40;i<kHeaderBytes;++i) if (bytes[i]) return false;
    Header h{le32(bytes+12),le32(bytes+16),le32(bytes+20),le64(bytes+24)};
    if (!h.count || h.count>UINT32_MAX/kRecordBytes || !h.titleBytes || !h.textBytes ||
        h.chunkBytes<4096 || h.chunkBytes>kMaxChunkBytes || (h.textBytes-1)/h.chunkBytes>=100000) return false;
    out=h; return true;
}
struct Entry { std::string title; uint64_t offset=0; uint32_t length=0; };
class TextReader {
public:
    virtual ~TextReader() = default;
    virtual size_t readAt(uint64_t offset,uint8_t* buffer,size_t length)=0;
};
class Archive {
public:
    bool open(const uint8_t* header,size_t size,dict::Reader& index,dict::Reader& titles,TextReader& text) {
        loaded_=false;
        if (!parseHeader(header,size,header_) || index.size()!=uint64_t(header_.count)*kRecordBytes || titles.size()!=header_.titleBytes) return false;
        index_=&index; titles_=&titles; text_=&text; loaded_=true; return true;
    }
    bool loaded() const { return loaded_; }
    const Header& header() const { return header_; }
    bool entry(uint32_t ordinal,Entry& out) {
        if (!loaded_ || ordinal>=header_.count) return false;
        uint8_t b[kRecordBytes];
        if (index_->readAt(ordinal*kRecordBytes,b,sizeof(b))!=sizeof(b)) return false;
        const auto offset=le32(b); const auto length=le16(b+4); const auto textOffset=le64(b+8); const auto textLength=le32(b+16);
        if (le16(b+6) || !length || length>kMaxTitleBytes || offset>header_.titleBytes || length>header_.titleBytes-offset ||
            !textLength || textOffset>header_.textBytes || textLength>header_.textBytes-textOffset) return false;
        std::string title(length,'\0');
        if (titles_->readAt(offset,reinterpret_cast<uint8_t*>(&title[0]),length)!=length || title.find('\0')!=std::string::npos) return false;
        out={std::move(title),textOffset,textLength}; return true;
    }
    bool suggest(const std::string& query,std::vector<Entry>& out,size_t limit=5) {
        out.clear(); if (!loaded_ || query.empty()) return true;
        uint32_t low=0,high=header_.count;
        Entry item;
        while (low<high) {
            const uint32_t mid=low+(high-low)/2;
            if (!entry(mid,item)) return false;
            if (dict::compare(item.title,query)<0) low=mid+1; else high=mid;
        }
        for (uint32_t i=low;i<header_.count && out.size()<limit;++i) {
            if (!entry(i,item)) {out.clear();return false;}
            if (!dict::startsWith(item.title,query)) break;
            out.push_back(item);
        }
        return true;
    }
    size_t read(const Entry& e,uint32_t offset,uint8_t* buffer,size_t length) {
        return !loaded_ || offset>=e.length ? 0 : text_->readAt(e.offset+offset,buffer,std::min(length,size_t(e.length-offset)));
    }
private:
    Header header_; dict::Reader *index_=nullptr,*titles_=nullptr; TextReader* text_=nullptr; bool loaded_=false;
};
struct LineSpan {uint32_t begin=0,end=0;};
struct Frame { std::vector<std::string> lines; std::vector<LineSpan> spans; uint32_t begin=0,next=0; bool end=false; };
class Pager {
public:
    using Measure=std::function<int(const std::string&)>;
    using Progress=std::function<bool(uint32_t,uint32_t)>;
    bool open(Archive& archive,const Entry& entry,int width,int rows,Measure measure,Progress progress={}) {
        if (!archive.loaded() || !entry.length || width<=0 || rows<=0 || rows>16) return false;
        archive_=&archive; entry_=entry; width_=width; rows_=rows; measure_=std::move(measure);progress_=std::move(progress);
        cache_={};cacheSize_=0;current_=0;lastKnown_=false;cached_=0;
        return load(0,0);
    }
    const Frame& frame() const { return frame_; }
    uint32_t page() const { return current_; }
    uint32_t length() const { return entry_.length; }
    const Entry& entry() const { return entry_; }
    uint32_t knownPages() const { return lastKnown_ ? last_+1 : 0; }
    bool move(int direction) {
        if (!archive_) return false;
        if (direction>=0) return frame_.end ? load(0,0) : load(current_+1,frame_.next);
        if (current_) return seek(current_-1);
        if (lastKnown_) return seek(last_);
        // Discover the end on demand with bounded memory, keeping the current
        // frame intact if storage fails or the caller cancels the scan.
        uint32_t number=0,offset=0;
        for (size_t i=0;i<cacheSize_;++i) if (cache_[i].page>number) {number=cache_[i].page;offset=cache_[i].offset;}
        Frame next;
        for (;;) {
            if (progress_ && !progress_(offset,entry_.length)) return false;
            if (!readPage(offset,next)) return false;
            remember(number,offset);
            if (next.end) {lastKnown_=true;last_=number;current_=number;frame_=std::move(next);return true;}
            offset=next.next;++number;
        }
    }
private:
    struct Location {uint32_t page=0,offset=0;};
    Archive* archive_=nullptr;Entry entry_;Measure measure_;Progress progress_;
    int width_=228,rows_=4;Frame frame_;uint32_t current_=0,last_=0;bool lastKnown_=false;
    std::array<Location,64> cache_{};size_t cacheSize_=0;
    std::array<uint8_t,1024> buffer_{};uint32_t start_=0;size_t cached_=0;
    void remember(uint32_t page,uint32_t offset) {
        const size_t slot=page%cache_.size();cache_[slot]={page,offset};cacheSize_=std::max(cacheSize_,slot+1);
    }
    bool byte(uint32_t at,uint8_t& value) {
        if (at>=entry_.length) return false;
        if (!cached_ || at<start_ || at-start_>=cached_) {
            start_=at;cached_=archive_->read(entry_,at,buffer_.data(),buffer_.size());
            if (!cached_) return false;
        }
        value=buffer_[at-start_];return true;
    }
    bool readPage(uint32_t offset,Frame& result) {
        if (offset>=entry_.length) return false;
        Frame next;next.begin=offset;std::string line;uint32_t lineStart=offset;
        while (offset<entry_.length && next.lines.size()<size_t(rows_)) {
            uint8_t first;if (!byte(offset,first)) return false;
            if (first=='\r') {++offset;continue;}
            if (first=='\n') {next.lines.push_back(line);next.spans.push_back({lineStart,offset});line.clear();++offset;lineStart=offset;continue;}
            std::string glyph;const size_t wanted=std::min(dict::utf8Length(first),size_t(entry_.length-offset));
            for (size_t i=0;i<wanted;++i) {uint8_t b;if (!byte(offset+i,b)) return false;glyph+=char(b);}
            const auto decoded=ui::decode(glyph,0);
            if (decoded.code==0xfffd) glyph="\xef\xbf\xbd";
            if (first=='\t' || first==0) glyph=" ";
            if (!line.empty() && (line.size()+glyph.size()>1024 || measure_(line+glyph)>width_)) {next.lines.push_back(line);next.spans.push_back({lineStart,offset});line.clear();lineStart=offset;continue;}
            line+=glyph;offset+=uint32_t(decoded.bytes);
        }
        if (!line.empty() && next.lines.size()<size_t(rows_)) {next.lines.push_back(line);next.spans.push_back({lineStart,offset});}
        if (next.lines.empty()) {next.lines.emplace_back();next.spans.push_back({offset,offset});}
        next.next=offset;next.end=offset>=entry_.length;result=std::move(next);return true;
    }
    bool load(uint32_t number,uint32_t offset) {
        Frame next;if (!readPage(offset,next)) return false;
        remember(number,offset);current_=number;frame_=std::move(next);
        if (frame_.end) {lastKnown_=true;last_=number;}return true;
    }
    bool seek(uint32_t target) {
        uint32_t number=0,offset=0;
        for (size_t i=0;i<cacheSize_;++i) if (cache_[i].page<=target && cache_[i].page>=number) {number=cache_[i].page;offset=cache_[i].offset;}
        Frame next;
        while (number<target) {
            if (progress_ && !progress_(offset,entry_.length)) return false;
            if (!readPage(offset,next) || next.end) return false;
            remember(number,offset);offset=next.next;++number;
        }
        return load(number,offset);
    }
};
} // namespace wiki
