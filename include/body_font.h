#pragma once
#include "ipa_data.h"

// Keep Chinese line metrics while using an open font for Latin/IPA/Cyrillic glyphs.
class BodyFont final : public lgfx::IFont {
public:
    void getDefaultMetric(lgfx::FontMetrics* metrics) const override {
        fonts::efontCN_14.getDefaultMetric(metrics);
    }
    bool updateFontMetric(lgfx::FontMetrics* metrics, uint16_t code) const override {
        if (code >= kIpaFont.first && code <= kIpaFont.last && kIpaFont.updateFontMetric(metrics, code)) return true;
        return fonts::efontCN_14.updateFontMetric(metrics, code);
    }
    size_t drawChar(lgfx::LGFXBase* gfx, int32_t x, int32_t y, uint16_t code,
                    const lgfx::TextStyle* style, lgfx::FontMetrics* metrics, int32_t& filled) const override {
        lgfx::FontMetrics probe = *metrics;
        const lgfx::IFont* selected = &fonts::efontCN_14;
        if (code >= kIpaFont.first && code <= kIpaFont.last && kIpaFont.updateFontMetric(&probe, code)) selected = &kIpaFont;
        return selected->drawChar(gfx, x, y, code, style, metrics, filled);
    }
};
inline const BodyFont kBodyFont;
