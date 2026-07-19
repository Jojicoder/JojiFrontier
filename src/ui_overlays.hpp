// Public entry points and cross-file state for the Warehouse Cleanup,
// Settings, Save Recovery, and Save Status HUD overlays, defined in
// ui_overlays.cpp. Split out of main.cpp; no behavior change.
#pragma once

#include <raylib.h>

#include <cstdint>
#include <optional>

#include "jf/core/GameApp.hpp"
#include "jf/core/SaveSystem.hpp"
#include "ui_shared.hpp"

namespace jfui {

// Settings-overlay UI state; also read/written directly by main()'s own
// event loop and autosave tick, so it stays extern rather than moving
// entirely into ui_overlays.cpp.
extern bool gSettingsOpen;
extern std::optional<jf::SaveStore> gSaveStore;
extern bool gAutoSaveEnabled;

// docs/save_system.md「破損復旧画面」state; set once at startup in main()
// and cleared from within drawSaveRecoveryScreen()'s own choices.
extern bool gSaveRecoveryOpen;

// docs/save_system.md「保存状態HUD」state machine; main()'s autosave tick
// reads gSaveHudState/gSaveHudRetryCount/gSaveHudNextRetryAt directly to
// drive the same retry schedule the HUD displays.
enum class SaveHudState { Idle, Saving, Saved, Failed };
extern SaveHudState gSaveHudState;
extern int gSaveHudRetryCount;
extern double gSaveHudNextRetryAt;
constexpr int kSaveHudMaxAutoRetries = 3;

void attemptAutoSave(jf::GameApp& app, std::uint64_t& savedRevision, Language& savedLanguage,
                     std::uint64_t& savedExpeditionRevision);

void drawWarehouseCleanupOverlay(jf::GameApp& app, Vector2 mouse, bool clicked);
void drawSettingsOverlay(jf::GameApp& app, Vector2 mouse, bool clicked);
void drawSaveStatusHud(jf::GameApp& app, Vector2 mouse, bool clicked, std::uint64_t& savedRevision,
                       Language& savedLanguage, std::uint64_t& savedExpeditionRevision);
void drawSaveRecoveryScreen(jf::GameApp& app, Vector2 mouse, bool clicked);

}  // namespace jfui
