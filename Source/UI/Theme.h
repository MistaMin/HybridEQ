#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <goodlookinui/Design.h>

// Shared colour scheme: matte black brushed-metal hardware channel strip
// (vintage console inspired). Each band's accent colour matches the
// node colour on the EQ curve and the coloured knob cap for that band.
namespace Theme {

// EQ band accents (must stay in sync with EQDisplay node colours)
inline const juce::Colour lowCutCol   = juce::Colour(0xFFCBCBBC);
inline const juce::Colour highCutCol  = juce::Colour(0xFFAAAFA6);
inline const juce::Colour lowBandCol  = juce::Colour(0xFFB58C59);
inline const juce::Colour mid1Col     = juce::Colour(0xFF5793B8);
inline const juce::Colour mid2Col     = juce::Colour(0xFF6EA68B);
inline const juce::Colour highBandCol = juce::Colour(0xFFB96057);
inline const juce::Colour outputCol   = juce::Colour(0xFFB9BDB3);
inline const juce::Colour preampCol   = juce::Colour(0xFFC7A66A);

// Editor chrome (matte black brushed metal)
inline juce::Colour editorTop    = juce::Colour(0xFF242C31);
inline juce::Colour editorBottom = juce::Colour(0xFF151C20);
inline juce::Colour panelTop     = juce::Colour(0xFF394247);
inline juce::Colour panelBottom  = juce::Colour(0xFF2A3034);
inline const juce::Colour panelOutline = juce::Colour(0xFF121213);
inline const juce::Colour panelHighlight = juce::Colour(0x1FFFFFFF);
inline juce::Colour textDark     = juce::Colour(0xFFDDE1D5);
inline juce::Colour textMid      = juce::Colour(0xFF909C9F);
inline const juce::Colour buttonTop    = juce::Colour(0xFFCDD0BD);
inline const juce::Colour buttonBottom = juce::Colour(0xFF9DAB9E);
inline const juce::Colour buttonOutline = juce::Colour(0xFF161616);
inline const juce::Colour buttonText   = juce::Colour(0xFF26312E);
inline const juce::Colour buttonInactiveTop    = juce::Colour(0xFF4A4A4C);
inline const juce::Colour buttonInactiveBottom = juce::Colour(0xFF3A3A3C);
inline const juce::Colour buttonTextInactive   = juce::Colour(0xFF8A8886);

// Knob bezel + cap (reference-style hardware rotary with coloured cap)
inline const juce::Colour knobTop     = juce::Colour(0xFF2A2A2C);
inline const juce::Colour knobBottom  = juce::Colour(0xFF141416);
inline const juce::Colour knobRim     = juce::Colour(0xFF465457);
inline const juce::Colour knobTick    = juce::Colour(0xFF8E8B82);
inline const juce::Colour knobText    = juce::Colour(0xFFD9DFD1);
inline const juce::Colour knobTextInactive = juce::Colour(0xFF706E68);
inline const juce::Colour knobInactiveBody = juce::Colour(0xFF4C4C4E);
inline const juce::Colour knobInactiveRim  = juce::Colour(0xFF3A3A3C);

// Whole-UI faceplate colours. Mutable so a look change repaints everything.
inline juce::Colour bgFill     = juce::Colour(0xff10171b);
inline juce::Colour border     = juce::Colour(0xff485156);
inline juce::Colour titleBar   = juce::Colour(0xff121b20);
inline juce::Colour rail       = juce::Colour(0xff0c1114);
inline juce::Colour footerText = juce::Colour(0xff526065);
inline juce::Colour faceAccent = juce::Colour(0xffc7a66a);
inline int panelFinish = 0;

struct Look {
    juce::Colour bgTop, bgBottom, panelTop, panelBottom, text, textMid, accent, bgFill, border, titleBar, rail, footer;
    int finish = 0;
    static Look from(const goodlookinui::Faceplate& f) {
        return {juce::Colour(f.bgTop), juce::Colour(f.bgBottom), juce::Colour(f.panelTop), juce::Colour(f.panelBottom),
                juce::Colour(f.text), juce::Colour(f.textMid), juce::Colour(f.accent), juce::Colour(f.bgFill),
                juce::Colour(f.border), juce::Colour(f.titleBar), juce::Colour(f.rail), juce::Colour(f.footer), f.finish};
    }
    Look mix(const Look& o, float t) const {
        auto m = [t](juce::Colour a, juce::Colour b) { return a.interpolatedWith(b, t); };
        return {m(bgTop,o.bgTop), m(bgBottom,o.bgBottom), m(panelTop,o.panelTop), m(panelBottom,o.panelBottom), m(text,o.text),
                m(textMid,o.textMid), m(accent,o.accent), m(bgFill,o.bgFill), m(border,o.border), m(titleBar,o.titleBar),
                m(rail,o.rail), m(footer,o.footer), t < 0.5f ? finish : o.finish};
    }
};
inline Look current() {
    return {editorTop, editorBottom, panelTop, panelBottom, textDark, textMid, faceAccent, bgFill, border, titleBar, rail, footerText, panelFinish};
}
inline void apply(const Look& l) {
    editorTop = l.bgTop; editorBottom = l.bgBottom; panelTop = l.panelTop; panelBottom = l.panelBottom;
    textDark = l.text; textMid = l.textMid; faceAccent = l.accent; bgFill = l.bgFill; border = l.border;
    titleBar = l.titleBar; rail = l.rail; footerText = l.footer; panelFinish = l.finish;
}

// Dark scope area behind the EQ curve
inline const juce::Colour scopeBg       = juce::Colour(0xFF111C21);
inline const juce::Colour scopeOutline  = juce::Colour(0xFF465457);
inline const juce::Colour gridMinor     = juce::Colour(0xFF1C2B31);
inline const juce::Colour gridMajor     = juce::Colour(0xFF2C4148);
inline const juce::Colour gridText      = juce::Colour(0xFF7F969C);
inline const juce::Colour curveFill     = juce::Colour(0x22FFFFFF);
inline const juce::Colour curveStroke   = juce::Colour(0xFFC8E8D2);
inline const juce::Colour harmonicsAmber = juce::Colour(0xFFC7A66A);
inline const juce::Colour harmonicsTrack = juce::Colour(0xFF57565A);

} // namespace Theme
