#pragma once
#include <cstring>
#include <goodlookinui/LookTable.h>
#include <vector>

// Colour triggers: optional. The developer decides what a parameter choice does.
//
//   Off        nothing changes (default).
//   Control    per SECTION: the section bound to a parameter takes the knob style,
//              knob colour and plate mapped to the chosen value.
//   Faceplate  the whole UI (background, panels, text) takes the faceplate
//              mapped to the chosen value of the driving parameter.
//
// `defaultMode` applies until a project saves its own choice. Which parameters
// drive which controls is listed in PluginEditor.cpp (`rules`); what each
// choice looks like is listed here, keyed by the choice's display name.
enum class TriggerMode { Off, Control, Faceplate };

namespace LookTriggers {
inline constexpr TriggerMode defaultMode = TriggerMode::Off;
inline constexpr const char* defaultFaceplate = "Graphite";
inline constexpr const char* faceplateDriver = "preampType";

// One entry per (driving parameter, choice). Every parameter has its own looks, so
// the same choice name can look different on each parameter.
//   colour    #RRGGBB: cap colour (capped knob styles) or pointer colour (material styles)
//   faceplate goodlookinui::faceplates code used in Whole-UI mode, or nullptr
//   style     knob class code (goodlookinui::knobStyles) for that section's knobs, or nullptr = keep
//   plate     goodlookinui::faceplates code for that SECTION's plate (panel background), or nullptr = keep
struct Entry {
    const char* parameter;
    const char* choice;
    const char* colour;
    const char* faceplate;
    const char* style;
    const char* plate;
};
inline constexpr Entry entries[] = {
    // Low band type
    {"lowType",   "Bax-EQ",     "#F26B21", nullptr, "Skt",  "Black"},    // black knobs, orange line, dark box
    {"lowType",   "Brit",       "#B58C59", nullptr, "Brit", "Granite"},  // console knobs, grey plate
    {"lowType",   "FSF",        "#F2D21E", nullptr, "FS",   "Royal"},    // yellow caps, blue plate
    // Mid 1 type
    {"mid1Type",  "N-EQ",       "#8F949A", nullptr, "N",    "Marine"},   // chrome-collar knobs, dark blue plate
    {"mid1Type",  "Brit",       "#3F6FD0", nullptr, "Brit", "Granite"},
    {"mid1Type",  "A-Type",     "#3F8FD8", nullptr, "A",    "Black"},    // blue A knobs, black plate
    // Mid 2 type
    {"mid2Type",  "N-EQ",       "#8F949A", nullptr, "N",    "Marine"},
    {"mid2Type",  "Brit",       "#4FA35A", nullptr, "Brit", "Granite"},
    {"mid2Type",  "A-Type",     "#3F8FD8", nullptr, "A",    "Cream"},    // blue A knobs, cream plate
    // High band type
    {"highType",  "Bax-EQ",     "#F26B21", nullptr, "Skt",  "Black"},
    {"highType",  "Brit",       "#D8455A", nullptr, "Brit", "Granite"},
    {"highType",  "FSF",        "#F2D21E", nullptr, "FS",   "Royal"},
    // Low cut slope
    {"lcSlope",   "-6 dB/oct",  "#CBCBBC", nullptr, "Snk",  nullptr},
    {"lcSlope",   "-12 dB/oct", "#D9D2A0", nullptr, "Mtl",  nullptr},
    {"lcSlope",   "-18 dB/oct", "#E3B97A", nullptr, "Slv",  nullptr},
    {"lcSlope",   "-24 dB/oct", "#E69A6A", nullptr, "Knl",  nullptr},
    // High cut slope
    {"hcSlope",   "-6 dB/oct",  "#AAAFA6", nullptr, "Rd",   nullptr},
    {"hcSlope",   "-12 dB/oct", "#9FC0C8", nullptr, "Ptr",  nullptr},
    {"hcSlope",   "-18 dB/oct", "#7FB0D4", nullptr, "Chr",  nullptr},
    {"hcSlope",   "-24 dB/oct", "#6A95D8", nullptr, "Wh",   nullptr},
    // Preamp type (also drives the whole-UI faceplate)
    {"preampType","Brit",       "#C96F4A", "Silver",   "Brit", "Granite"},
    {"preampType","N-Type",     "#A32323", "Navy",     "A",    "Marine"},
    {"preampType","FSF",        "#F2D21E", "Teal",     "FS",   "Royal"},
    {"preampType","Off",        "#7A7F82", "Graphite", "Snk",  nullptr},
    {"preampType","A-Type",     "#3F8FD8", "Cream",    "A",    "Black"},
};
inline const Entry* find(const char* parameter, const char* choice) {
    for (const auto& e : entries)
        if (std::strcmp(e.parameter, parameter) == 0 && std::strcmp(e.choice, choice) == 0) return &e;
    return nullptr;
}

// The plugin's built-in looks, in the toolkit's LookTable form.
inline std::vector<goodlookinui::Look> defaultLooks() {
    std::vector<goodlookinui::Look> out;
    for (const auto& e : entries)
        out.push_back({e.parameter, e.choice, e.colour ? e.colour : "", e.faceplate ? e.faceplate : "", e.style ? e.style : "", e.plate ? e.plate : ""});
    return out;
}
} // namespace LookTriggers
