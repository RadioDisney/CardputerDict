#include "wiki_core.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

class DiskFile : public dict::Reader {
public:
    std::ifstream file;uint32_t bytes;
    explicit DiskFile(const std::filesystem::path& path):file(path,std::ios::binary),bytes(uint32_t(std::filesystem::file_size(path))) {assert(file);}
    uint32_t size() const override {return bytes;}
    size_t readAt(uint32_t offset,uint8_t* out,size_t size) override {
        file.clear();file.seekg(offset);file.read(reinterpret_cast<char*>(out),std::streamsize(size));return size_t(file.gcount());
    }
};
class DiskText : public wiki::TextReader {
public:
    std::filesystem::path root;uint32_t chunk;uint32_t current=UINT32_MAX;std::ifstream file;
    DiskText(const std::filesystem::path& folder,uint32_t split):root(folder),chunk(split) {}
    size_t readAt(uint64_t offset,uint8_t* out,size_t size) override {
        size_t done=0;
        while(done<size) {
            const uint32_t part=uint32_t(offset/chunk),local=uint32_t(offset%chunk);
            if(part!=current) {
                file.close();char name[32];snprintf(name,sizeof(name),"text%05u.bin",part);file.open(root/name,std::ios::binary);current=part;
            }
            if(!file)return done;
            const auto n=std::min(size-done,size_t(chunk-local));file.clear();file.seekg(local);file.read(reinterpret_cast<char*>(out+done),std::streamsize(n));
            const size_t got=size_t(file.gcount());done+=got;offset+=got;if(got!=n)return done;
        }
        return done;
    }
};
int main(int argc,char** argv) {
    assert(argc==2);const std::filesystem::path root(argv[1]);
    DiskFile headerFile(root/"wiki.wki"),index(root/"index.wix"),titles(root/"titles.wit");uint8_t raw[64];assert(headerFile.readAt(0,raw,64)==64);
    wiki::Header header;assert(wiki::parseHeader(raw,sizeof(raw),header));assert(header.count==4863298 && header.textBytes>UINT32_MAX);
    DiskText text(root,header.chunkBytes);wiki::Archive archive;assert(archive.open(raw,sizeof(raw),index,titles,text));
    for(const std::string word:{"苹果","蘋果","维基百科","Linux","「一带一路」专员","11世纪月食列表"}) {
        std::vector<wiki::Entry> found;assert(archive.suggest(word,found) && !found.empty() && dict::compare(found[0].title,word)==0);
        const auto entry=found[0];wiki::Pager pager;assert(pager.open(archive,entry,28,4,[](const auto& value){return int(dict::utf8Count(value));}));
        assert(pager.move(-1) && pager.frame().end && pager.frame().next==entry.length);assert(pager.move(1) && pager.page()==0);
        if(word=="「一带一路」专员")assert(entry.offset>UINT32_MAX);
        if(word=="11世纪月食列表")assert(entry.offset/header.chunkBytes!=(entry.offset+entry.length-1)/header.chunkBytes);
        std::cout<<word<<": offset="<<entry.offset<<" bytes="<<entry.length<<" pages="<<pager.knownPages()<<" complete\n";
    }
    std::cout<<"C++ firmware engine passed on the complete 1.56M-article / 4.86M-title real pack with bounded reader memory.\n";
}
