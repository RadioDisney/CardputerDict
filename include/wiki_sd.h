#pragma once
#include "wiki_core.h"
#include <SD.h>

namespace wiki {
class SdIndex final : public dict::Reader {
public:
    File file;
    uint32_t size() const override {return file ? file.size() : 0;}
    size_t readAt(uint32_t offset,uint8_t* data,size_t length) override {return file && file.seek(offset) ? file.read(data,length) : 0;}
};
class SdText final : public TextReader {
public:
    void close() {file_.close();part_=UINT32_MAX;}
    void configure(const Header& header) {close();chunk_=header.chunkBytes;size_=header.textBytes;}
    size_t readAt(uint64_t offset,uint8_t* data,size_t length) override {
        if (!chunk_ || offset>=size_) return 0;
        length=size_t(std::min(uint64_t(length),size_-offset));size_t done=0;
        while (done<length) {
            const uint32_t part=uint32_t(offset/chunk_),local=uint32_t(offset%chunk_);
            if (!file_ || part_!=part) {
                close();char path[64];snprintf(path,sizeof(path),"/wikipedia/text%05lu.bin",(unsigned long)part);
                file_=SD.open(path,FILE_READ);if (!file_) return done;part_=part;
            }
            const size_t wanted=std::min(length-done,size_t(chunk_-local));
            if (!file_.seek(local)) return done;
            const size_t got=file_.read(data+done,wanted);done+=got;offset+=got;delay(1);
            if (got!=wanted) return done;
        }
        return done;
    }
private:
    File file_;uint32_t part_=UINT32_MAX,chunk_=0;uint64_t size_=0;
};
class SdArchive {
public:
    Archive archive;
    bool sample() const {return sample_;}
    void close() {sample_=false;archive=Archive();index_.file.close();titles_.file.close();text_.close();}
    bool open() {
        if (archive.loaded()) return true;
        close();File headerFile=SD.open("/wikipedia/wiki.wki",FILE_READ);uint8_t bytes[kHeaderBytes];
        if (!headerFile || headerFile.size()!=sizeof(bytes) || headerFile.read(bytes,sizeof(bytes))!=sizeof(bytes)) return false;
        headerFile.close();Header header;
        if (!parseHeader(bytes,sizeof(bytes),header)) return false;
        index_.file=SD.open("/wikipedia/index.wix",FILE_READ);titles_.file=SD.open("/wikipedia/titles.wit",FILE_READ);
        if (!index_.file || !titles_.file) {close();return false;}
        text_.configure(header);
        uint8_t probe;
        if (text_.readAt(0,&probe,1)!=1 || text_.readAt(header.textBytes-1,&probe,1)!=1 ||
            !archive.open(bytes,sizeof(bytes),index_,titles_,text_)) {close();return false;}
        sample_=SD.exists("/wikipedia/SAMPLE.txt");
        Serial.printf("WIKI_READY entries=%lu text_bytes=%llu chunk_bytes=%lu\n",(unsigned long)header.count,
            (unsigned long long)header.textBytes,(unsigned long)header.chunkBytes);
        return true;
    }
private:
    SdIndex index_,titles_;SdText text_;bool sample_=false;
};
inline bool uploadName(const std::string& name) {
    if (name=="wiki.wki" || name=="index.wix" || name=="titles.wit" || name=="manifest.json" || name=="LICENSE.txt" || name=="SAMPLE.txt") return true;
    if (name.size()!=13 || name.substr(0,4)!="text" || name.substr(9)!=".bin") return false;
    for (size_t i=4;i<9;++i) if (name[i]<'0' || name[i]>'9') return false;
    return true;
}
} // namespace wiki
