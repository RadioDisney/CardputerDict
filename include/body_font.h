#pragma once
#include "ipa_data.h"
#include "cjk_font.h"

// Keep Chinese line metrics while using an open font for Latin/IPA/Cyrillic glyphs.
class BodyFont final : public lgfx::IFont {
public:
    void getDefaultMetric(lgfx::FontMetrics* metrics) const override {
        kCjk14.getDefaultMetric(metrics);
    }
    bool updateFontMetric(lgfx::FontMetrics* metrics, uint16_t code) const override {
        if (code >= kIpaFont.first && code <= kIpaFont.last && kIpaFont.updateFontMetric(metrics, code)) return true;
        return kCjk14.updateFontMetric(metrics, code);
    }
    size_t drawChar(lgfx::LGFXBase* gfx, int32_t x, int32_t y, uint16_t code,
                    const lgfx::TextStyle* style, lgfx::FontMetrics* metrics, int32_t& filled) const override {
        lgfx::FontMetrics probe = *metrics;
        const lgfx::IFont* selected = &kCjk14;
        if (code >= kIpaFont.first && code <= kIpaFont.last && kIpaFont.updateFontMetric(&probe, code)) selected = &kIpaFont;
        return selected->drawChar(gfx, x, y, code, style, metrics, filled);
    }
};
inline const BodyFont kBodyFont;
