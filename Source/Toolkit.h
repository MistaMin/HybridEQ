#pragma once
// Everything the plugin takes from GoodLookinUI (third_party/GoodLookinUI), with short local names.
#include <goodlookinui/Design.h>
#include <goodlookinui/LevelTracker.h>
#include <goodlookinui/LookTable.h>
#include <goodlookinui/Spectrum.h>
#include <goodlookinui/Settings.h>
#include <GoodLookinUI.h>
#include "UI/OptimizedSpectrumRenderer.h"
#include <SpectrumStudio.h>
#include <LookStudio.h>
#include <DesignFiles.h>
#include <Hardware.h>
#if GOODLOOKINUI_ENABLE_EDITOR
#include <Gallery.h>
#endif

namespace spectrum = goodlookinui::spectrum;
using LevelTracker = goodlookinui::LevelTracker;
using Look = goodlookinui::Look;
using LookTable = goodlookinui::LookTable;
using SettingsTable = goodlookinui::SettingsTable;
using SpectrumSettings = goodlookinui::juce_adapter::SpectrumSettings;
using SpectrumRenderer = hybrideq::SpectrumRenderer;
#if GOODLOOKINUI_ENABLE_EDITOR
using SpectrumStudio = goodlookinui::juce_adapter::SpectrumStudio;
using LookStudio = goodlookinui::juce_adapter::LookStudio;
using DesignAutosave = goodlookinui::juce_adapter::DesignAutosave;
using Gallery = goodlookinui::juce_adapter::Gallery;
#endif
