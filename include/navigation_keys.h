#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace keyboard {
inline constexpr std::array<uint8_t,4> kDirectionKeyIds{{39,53,52,54}}; // up, down, left, right
constexpr int directionIndex(uint8_t key) {
    for(std::size_t i=0;i<kDirectionKeyIds.size();++i)if(kDirectionKeyIds[i]==key)return int(i);
    return -1;
}
constexpr bool repeatDirectionAllowed(bool fn,bool ctrl,bool alt,bool opt,bool selecting,bool blocked) {
    return !blocked && !fn && !alt && !opt && (!ctrl || selecting);
}
// These four physical keys carry arrow labels on Cardputer/Adv. Invert only
// their Fn layer: arrows become primary; punctuation moves behind Fn.
constexpr bool isDirectionKey(char base) {
    return base==';' || base=='.' || base==',' || base=='/';
}
constexpr bool directionFn(char base,bool physicalFn) {
    return isDirectionKey(base) ? !physicalFn : physicalFn;
}
} // namespace keyboard
