#include "wiki_core.h"
#include <cassert>
#include <iostream>
#include <map>
#include <vector>

struct Bytes final : dict::Reader {
    std::vector<uint8_t> data;
    uint32_t size() const override {return data.size();}
    size_t readAt(uint32_t at,uint8_t* p,size_t n) override {
        if (at>=data.size()) return 0;n=std::min(n,data.size()-at);memcpy(p,data.data()+at,n);return n;
    }
};
struct Text final : wiki::TextReader {
    uint64_t base=uint64_t(1)<<32;std::string data;bool broken=false;uint64_t seen=0;
    size_t readAt(uint64_t at,uint8_t* p,size_t n) override {
        seen=at;if(broken || at<base || at-base>=data.size()) return 0;
        n=std::min({n,data.size()-size_t(at-base),size_t(37)});memcpy(p,data.data()+size_t(at-base),n);return n;
    }
};
void put32(std::vector<uint8_t>& b,size_t at,uint32_t v) {for(int i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void put64(std::vector<uint8_t>& b,size_t at,uint64_t v) {put32(b,at,uint32_t(v));put32(b,at+4,uint32_t(v>>32));}
std::vector<uint8_t> header(uint32_t count,uint32_t titles,uint64_t text) {
    std::vector<uint8_t> b(64);memcpy(b.data(),"CPWIKI1\0",8);put32(b,8,1);put32(b,12,count);put32(b,16,titles);put32(b,20,128*1024*1024);
    put64(b,24,text);put32(b,32,20);put32(b,36,~dict::crc32(b.data(),36));return b;
}
int width(const std::string& s) {return dict::utf8Count(s);}
int main() {
    static_assert(sizeof(wiki::Pager)<2048,"Pager must keep a bounded cache, not an article-sized allocation");
    Bytes index,titles;Text text;
    for(int i=0;i<600;++i)text.data+="第一段苹果百科ABCDEFGHIJK\n\n";
    text.data+="正文最后一行";
    assert(text.data.size()>dict::kMaxDefinitionBytes);
    std::vector<std::string> names={"Apple","测试","测验","蘋果","苹果"};
    std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return dict::indexCompare(a,b)<0;});
    for(const auto& name:names) {
        const uint32_t ordinal=index.data.size(),at=titles.data.size();index.data.resize(ordinal+20);
        put32(index.data,ordinal,at);index.data[ordinal+4]=uint8_t(name.size());put64(index.data,ordinal+8,text.base);put32(index.data,ordinal+16,text.data.size());
        titles.data.insert(titles.data.end(),name.begin(),name.end());
    }
    auto h=header(names.size(),titles.size(),text.base+text.data.size());wiki::Header parsed;
    assert(wiki::parseHeader(h.data(),h.size(),parsed));
    for(size_t i=0;i<h.size();++i) {auto bad=h;bad[i]^=1;assert(!wiki::parseHeader(bad.data(),bad.size(),parsed));}
    wiki::Archive archive;assert(archive.open(h.data(),h.size(),index,titles,text));
    std::vector<wiki::Entry> hits;
    assert(archive.suggest("aP",hits) && hits.size()==1 && hits[0].title=="Apple");
    assert(archive.suggest("测",hits) && hits.size()==2 && hits[0].title=="测试" && hits[1].title=="测验");
    assert(archive.suggest("不在词库",hits) && hits.empty());
    assert(archive.suggest("苹果",hits) && hits.size()==1);auto article=hits[0];
    wiki::Pager pager;assert(pager.open(archive,article,12,4,width));
    std::vector<wiki::Frame> pages;
    uint32_t next=0;
    do {
        const auto frame=pager.frame();assert(frame.begin==next && frame.next>frame.begin && frame.lines.size()<=4);
        assert(frame.spans.size()==frame.lines.size());
        for(size_t row=0;row<frame.lines.size();++row) {
            const auto span=frame.spans[row];
            assert(width(frame.lines[row])<=12 && span.begin>=frame.begin && span.end<=frame.next && span.begin<=span.end);
            assert(text.data.substr(span.begin,span.end-span.begin)==frame.lines[row]);
        }
        pages.push_back(frame);next=frame.next;
        if(frame.end)break;
        assert(pager.move(1));
    } while(pages.size()<10000);
    assert(pager.frame().end && next==text.data.size() && text.seen>=text.base);
    assert(pages.size()>64 && pager.knownPages()==pages.size());
    for(size_t i=pages.size()-1;i>0;--i) {assert(pager.move(-1));assert(pager.frame().lines==pages[i-1].lines && pager.page()==i-1);}
    assert(pager.move(-1) && pager.frame().lines==pages.back().lines);
    assert(pager.move(1) && pager.page()==0);
    wiki::Pager fresh;assert(fresh.open(archive,article,12,4,width));assert(fresh.move(-1) && fresh.frame().lines==pages.back().lines);
    text.broken=true;assert(!fresh.move(1));text.broken=false;
    auto corrupt=index.data;index.data[4]=0;assert(!archive.entry(0,article));index.data=corrupt;
    text.data=std::string(20000,'x');h=header(names.size(),titles.size(),text.base+text.data.size());
    for(size_t i=0;i<index.data.size();i+=20)put32(index.data,i+16,text.data.size());
    assert(archive.open(h.data(),h.size(),index,titles,text) && archive.entry(0,article));
    assert(pager.open(archive,article,20,4,[](const std::string&){return 0;}));
    assert(pager.frame().lines[0].size()==1024 && pager.frame().next==4096);
    std::cout<<"Wikipedia core passed: fixed disk search, >4GiB offsets, UTF-8 across short reads, long complete articles, circular paging, bounded history, corruption and zero-width text.\n";
}
