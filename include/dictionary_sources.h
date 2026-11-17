#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace dictionaries {
inline constexpr const char* kEnglish="/dictionaries/ecdict.ifo";
inline constexpr const char* kChinese="/dictionaries/cedict.ifo";
inline constexpr const char* kWiki="@wiki";
inline std::vector<std::string> sources(std::vector<std::string> paths,bool wikipedia) {
    std::sort(paths.begin(),paths.end());paths.erase(std::unique(paths.begin(),paths.end()),paths.end());
    std::vector<std::string> result;
    const auto has=[&](const char* path){return std::find(paths.begin(),paths.end(),path)!=paths.end();};
    if(has(kEnglish))result.emplace_back(kEnglish);
    if(has(kChinese))result.emplace_back(kChinese);
    if(wikipedia)result.emplace_back(kWiki);
    for(const auto& path:paths) {
        if(path==kEnglish || path==kChinese || path=="/dictionaries/pinyin.ifo")continue;
        result.push_back(path);
    }
    return result;
}
inline std::string advance(const std::vector<std::string>& order,const std::string& current,int step) {
    if(order.empty())return kEnglish;
    const auto found=std::find(order.begin(),order.end(),current);
    const int count=int(order.size());
    const int position=found==order.end()?(step>=0?count-1:0):int(found-order.begin());
    return order[(position+step%count+count)%count];
}
// Maintenance is limited to named dictionary assets, never arbitrary paths,
// installed English/Chinese data, or the IME's internal pinyin dictionary.
inline bool removableStem(const std::string& stem) {
    if(stem.empty() || stem.size()>63 || stem[0]=='.')return false;
    auto folded=stem;
    for(auto& c:folded)if(c>='A' && c<='Z')c=char(c+'a'-'A');
    // FAT filenames are case-insensitive; protect case variants too.
    if(folded=="ecdict" || folded=="cedict" || folded=="pinyin")return false;
    return stem.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;
}
inline std::vector<std::string> removalFiles(const std::string& stem) {
    if(!removableStem(stem))return {};
    std::vector<std::string> result;
    for(const char* ext:{".ifo",".idx",".dict",".cidx",".syn",".dict.dz",".idx.gz",".syn.gz",".cidx.tmp"}) {
        result.push_back("/dictionaries/"+stem+ext);
        result.push_back("/dictionaries/._"+stem+ext);
    }
    return result;
}
} // namespace dictionaries
