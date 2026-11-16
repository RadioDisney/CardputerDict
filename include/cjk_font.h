#pragma once
#include <M5GFX.h>

// Preserve the primary font's line metrics and existing simplified glyphs;
// consult the Traditional Chinese font only when a glyph is missing.
class CjkFont final : public lgfx::IFont {
public:
    CjkFont(const lgfx::IFont& primary, const lgfx::IFont& fallback)
        : primary_(primary), fallback_(fallback) {}
    void getDefaultMetric(lgfx::FontMetrics* metrics) const override {
        primary_.getDefaultMetric(metrics);
    }
    bool updateFontMetric(lgfx::FontMetrics* metrics, uint16_t code) const override {
        return primary_.updateFontMetric(metrics, code) || fallback_.updateFontMetric(metrics, code);
    }
    size_t drawChar(lgfx::LGFXBase* gfx, int32_t x, int32_t y, uint16_t code,
                    const lgfx::TextStyle* style, lgfx::FontMetrics* metrics, int32_t& filled) const override {
        lgfx::FontMetrics probe = *metrics;
        const auto& selected = !primary_.updateFontMetric(&probe, code) && fallback_.updateFontMetric(&probe, code)
            ? fallback_ : primary_;
        return selected.drawChar(gfx, x, y, code, style, metrics, filled);
    }
private:
    const lgfx::IFont& primary_;
    const lgfx::IFont& fallback_;
};
inline const CjkFont kCjk12(fonts::efontCN_12, fonts::efontTW_12);
inline const CjkFont kCjk14(fonts::efontCN_14, fonts::efontTW_14);
inline const CjkFont kCjk16(fonts::efontCN_16, fonts::efontTW_16);
