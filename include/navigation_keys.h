#pragma once

namespace keyboard {
// These four physical keys carry arrow labels on Cardputer/Adv. Invert only
// their Fn layer: arrows become primary; punctuation moves behind Fn.
constexpr bool isDirectionKey(char base) {
    return base==';' || base=='.' || base==',' || base=='/';
}
constexpr bool directionFn(char base,bool physicalFn) {
    return isDirectionKey(base) ? !physicalFn : physicalFn;
}
} // namespace keyboard
