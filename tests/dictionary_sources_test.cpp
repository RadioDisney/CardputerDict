#include "dictionary_sources.h"
#include <cassert>
#include <iostream>
using namespace dictionaries;
int main() {
    const auto order=sources({kEnglish,kChinese,"/dictionaries/demo.ifo","/dictionaries/custom.ifo","/dictionaries/pinyin.ifo",kEnglish},true);
    assert((order==std::vector<std::string>{kEnglish,kChinese,kWiki,"/dictionaries/custom.ifo","/dictionaries/demo.ifo"}));
    const auto cleaned=sources({kEnglish,kChinese,"/dictionaries/pinyin.ifo"},true);
    assert((cleaned==std::vector<std::string>{kEnglish,kChinese,kWiki}));
    assert((sources({kEnglish},false)==std::vector<std::string>{kEnglish}));
    assert((sources({},true)==std::vector<std::string>{kWiki}));assert(sources({},false).empty());
    std::string current=kEnglish;
    for(int cycle=0;cycle<3;++cycle)for(const auto* expected:{kChinese,kWiki,kEnglish}) {current=advance(cleaned,current,1);assert(current==expected);}
    for(int cycle=0;cycle<3;++cycle)for(const auto* expected:{kWiki,kChinese,kEnglish}) {current=advance(cleaned,current,-1);assert(current==expected);}
    assert(advance({},"unknown",1)==kEnglish);
    for(const auto* stem:{"ecdict","cedict","pinyin","ECDICT","CeDiCt","PINYIN","","../demo",".hidden","/audio","demo/other","demo.txt"})assert(removalFiles(stem).empty());
    const auto files=removalFiles("demo");assert(files.size()==18 && files[0]=="/dictionaries/demo.ifo");
    for(const auto& path:files)assert(path.rfind("/dictionaries/demo.",0)==0 || path.rfind("/dictionaries/._demo.",0)==0);
    std::cout<<"SD-driven sources passed: installed-only sources, additional dictionaries discovered, IME excluded, cleaned three-source cycle, protected-data removal boundaries.\n";
}
