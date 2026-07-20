#include <raylib.h>

#include <optional>
#include <string>

#include "jf/battle/BattleController.hpp"
#include "jf/core/GameApp.hpp"
#include "jf/core/Grid.hpp"
#include "jf/core/Locale.hpp"
#include "jf/data/GameData.hpp"
#include "ui_base.hpp"
#include "ui_battle.hpp"
#include "ui_camp.hpp"
#include "ui_deployment.hpp"
#include "ui_exploration.hpp"
#include "ui_facilities.hpp"
#include "ui_overlays.hpp"
#include "ui_shared.hpp"

namespace jfui {

// Base screen sub-view toggle and related state (docs/implementation_
// roadmap.md "Phase 3.5"): purely a rendering concern (which panel the Base
// screen shows), so - like gSettingsOpen (ui_overlays.cpp) - it lives here
// rather than in GameApp. Facility node actions still validate against
// GameApp's own Screen::Base state regardless of which sub-view is
// currently drawn. Ashbough Forest is always unlocked, so it's a safe
// default region; the Begin Expedition button re-validates against GameApp
// before starting.
BaseScreenState gBaseScreen;

// docs/inventory_overflow.md「倉庫整理画面」: independent of GameApp::Screen
// (same convention as gSettingsOpen/gCampItemMenuOpen above) so it can
// overlay either Camp (opened automatically when returnToBase() is blocked
// by the 200-Stack pending ceiling) or Base (opened manually for routine
// cleanup).
bool gWarehouseCleanupOpen = false;

// docs/save_system.md「破損復旧画面」state and docs/save_system.md「保存状態
// HUD」state machine now live in ui_overlays.cpp (definitions) - declared
// extern in ui_overlays.hpp since main()'s own event loop and autosave tick
// read/write them directly. gSaveStore/gAutoSaveEnabled likewise.

void handleGridClick(jf::GameApp& app, jf::GridPos pos) {
    jf::BattleController& controller = app.battle();
    switch (controller.inputState()) {
        case jf::BattleInputState::SelectUnit: {
            if (jf::Unit* unit = controller.battle().unitAt(pos)) {
                controller.selectUnit(*unit);
            }
            break;
        }
        case jf::BattleInputState::SelectMove:
            controller.selectMoveTile(pos);
            break;
        case jf::BattleInputState::SelectTarget:
            controller.selectTargetTile(pos);
            break;
        case jf::BattleInputState::SelectHealTarget:
            controller.selectHealTarget(pos);
            break;
        case jf::BattleInputState::SelectItemTarget:
            app.selectNeutralBattleHealingTarget(pos);
            break;
        case jf::BattleInputState::SelectBoardTarget:
            app.selectBoardTarget(pos);
            break;
        case jf::BattleInputState::SelectSkillTarget:
            controller.selectSkillTarget(pos);
            break;
        case jf::BattleInputState::SelectInteractTarget:
            controller.selectInteractTarget(pos);
            break;
        default:
            break;
    }
}

// Locales must load before loadAppFont() (it reads jf::allJapaneseGlyphText()
// for the Glyph Atlas) and before any tr() call. If even this fails, there's
// no Locale table to draw a translated message from, so this one error
// screen stays plain English rather than routing through tr(). Returns false
// (having already run the blocking error screen to WindowShouldClose()) iff
// startup should abort here.
bool loadLocalesAndFontOrShowError() {
    bool localesLoaded = jf::loadLocales("data");
    if (!localesLoaded) localesLoaded = jf::loadLocales("../data");
    if (!localesLoaded) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Failed to load locale files from data/locales/.", 20, 20, 18, RED);
            EndDrawing();
        }
        return false;
    }
    loadAppFont();
    return true;
}

// Same "blocking error screen, empty return means abort" shape as
// loadLocalesAndFontOrShowError() above, for the data/ load that follows it.
std::optional<jf::GameData> loadGameDataOrShowError() {
    auto gameData = jf::loadGameData("data");
    if (!gameData) {
        // Fall back for the case where the executable is launched from a
        // different working directory than the copied data/ folder.
        gameData = jf::loadGameData("../data");
    }
    if (!gameData) {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            drawText(tr("ui.error.data_load_failed"), 20, 20, 18, RED);
            EndDrawing();
        }
    }
    return gameData;
}

// One frame's worth of screen dispatch: which top-level screen owns the
// frame, plus the overlays (Settings, Save HUD) every screen draws on top.
// `sceneClicked` is `clicked` gated on the Settings modal not owning input;
// see main()'s own comment on why the two are different.
void drawCurrentScreen(jf::GameApp& app, Vector2 mouse, float dt, bool clicked, bool sceneClicked,
                       std::uint64_t savedRevision, Language savedLanguage, std::uint64_t savedExpeditionRevision) {
    if (gSaveRecoveryOpen) {
        beginLogicalFrame();
        drawSaveRecoveryScreen(app, mouse, clicked);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::Battle) {
        jf::BattleController& controller = app.battle();

        if (sceneClicked) {
            jf::GridPos pos;
            if (tileFromScreen(mouse, pos)) {
                handleGridClick(app, pos);
            }
        }

        beginLogicalFrame();
        ClearBackground(Color{16, 18, 26, 255});
        drawPhaseBanner(controller);
        drawGrid(controller, dt);
        drawBattleHud(app, mouse, sceneClicked);
        drawTurnChangeAnnouncement(controller.battle().phase(), dt);

        if (controller.inputState() != jf::BattleInputState::ConfirmAttack &&
            controller.inputState() != jf::BattleInputState::ConfirmSkillAttack &&
            controller.inputState() != jf::BattleInputState::ConfirmObjectAttack &&
            controller.inputState() != jf::BattleInputState::Victory &&
            controller.inputState() != jf::BattleInputState::Defeat) {
            drawHoverInfo(app.gameData(), controller, mouse);
        }

        if (controller.inputState() == jf::BattleInputState::ConfirmAttack) {
            if (auto preview = controller.pendingPreview()) {
                drawCombatPreviewPopup(*preview);
            }
        } else if (controller.inputState() == jf::BattleInputState::ConfirmSkillAttack) {
            // M4 item 3: same popup, fed by the attack-shape skill's own
            // preview (already folds in any flat bonus damage).
            if (auto preview = controller.pendingSkillPreview()) {
                drawCombatPreviewPopup(*preview);
            }
        } else if (controller.inputState() == jf::BattleInputState::ConfirmObjectAttack) {
            if (auto preview = controller.pendingObjectPreview()) {
                drawObjectAttackPreviewPopup(*preview);
            }
        }

        if (controller.inputState() == jf::BattleInputState::Victory) {
            drawVictoryOverlay(app, mouse, sceneClicked);
        } else if (controller.inputState() == jf::BattleInputState::Defeat) {
            drawDefeatOverlay(app, mouse, sceneClicked);
        }
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::Camp) {
        beginLogicalFrame();
        drawCampScreen(app, mouse, sceneClicked);
        drawWarehouseCleanupOverlay(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::Exploration) {
        beginLogicalFrame();
        drawExplorationScreen(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else if (app.screen() == jf::Screen::PreBattleDeployment) {
        beginLogicalFrame();
        drawPreBattleDeploymentScreen(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    } else {
        beginLogicalFrame();
        if (gBaseScreen.showFacilities) drawFacilitiesScreen(app, mouse, sceneClicked);
        else if (gBaseScreen.viewedUnitId) drawUnitScreen(app, mouse, sceneClicked);
        else drawBaseScreen(app, mouse, sceneClicked);
        drawWarehouseCleanupOverlay(app, mouse, sceneClicked);
        drawSettingsOverlay(app, mouse, clicked);
        drawSaveStatusHud(app, mouse, clicked, savedRevision, savedLanguage, savedExpeditionRevision);
        endLogicalFrame();
    }
}

// docs/save_system.md「保存状態HUD」: autosaves once persistent/expedition
// state or language actually changed since the last save, plus up to 3
// automatic retries (at 0.5/1/2s) after a failed attempt.
void runAutoSaveTick(jf::GameApp& app, std::uint64_t& savedRevision, Language& savedLanguage,
                     std::uint64_t& savedExpeditionRevision) {
    if (!gAutoSaveEnabled) return;
    const bool isDirty = savedRevision != app.persistentRevision() || savedLanguage != gLanguage ||
                        savedExpeditionRevision != app.expeditionRevision();
    if (isDirty) {
        attemptAutoSave(app, savedRevision, savedLanguage, savedExpeditionRevision);
    } else if (gSaveHudState == SaveHudState::Failed && gSaveHudRetryCount < kSaveHudMaxAutoRetries &&
              GetTime() >= gSaveHudNextRetryAt) {
        // docs/save_system.md「保存状態HUD」: up to 3 automatic retries at
        // 0.5/1/2s: attemptAutoSave() re-serializes current state each
        // time, so this always saves the latest data, not a stale snapshot
        // from the original failed attempt.
        attemptAutoSave(app, savedRevision, savedLanguage, savedExpeditionRevision);
    }
}

} // namespace jfui

using namespace jfui;

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(kScreenWidth, kScreenHeight, "JOJIFrontier");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    if (!loadLocalesAndFontOrShowError()) {
        CloseWindow();
        return 1;
    }

    auto gameData = loadGameDataOrShowError();
    if (!gameData) {
        CloseWindow();
        return 1;
    }

    jf::GameApp app(*gameData);
    gSaveStore.emplace(jf::defaultSavePath());
    std::string saveLoadError;
    auto loadedSave = gSaveStore->load(&saveLoadError);
    if (loadedSave) {
        if (app.applySaveData(*loadedSave))
            gLanguage = loadedSave->language == "ja" ? Language::Japanese : Language::English;
    }
    // A corrupt or unsupported save is never overwritten automatically.
    // `saveLoadError` is non-empty here only when SaveStore::load() truly
    // found nothing usable (its own primary-then-".bak" fallback already
    // failed both) - see load()'s doc comment. That's the docs/save_system.md
    // 「破損復旧画面」trigger; a fresh install with no save file at all
    // leaves saveLoadError empty and proceeds normally.
    gSaveRecoveryOpen = !loadedSave && !saveLoadError.empty();
    gAutoSaveEnabled = !gSaveRecoveryOpen;
    std::uint64_t savedRevision = app.persistentRevision();
    std::uint64_t savedExpeditionRevision = app.expeditionRevision();
    Language savedLanguage = gLanguage;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Vector2 mouse = logicalMousePosition();
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        // While the Settings modal is open, it owns all clicks; nothing
        // underneath should react even though it's still drawn (dimmed).
        bool sceneClicked = clicked && !gSettingsOpen;

        // F11 (and macOS's usual Cmd+Ctrl+F fullscreen shortcut) toggle a
        // real window maximize/restore, in case the native title-bar zoom
        // button isn't reachable (e.g. some window managers/sandboxes).
        bool wantsMaximizeToggle =
            IsKeyPressed(KEY_F11) ||
            (IsKeyPressed(KEY_F) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) &&
             (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)));
        if (wantsMaximizeToggle) {
            if (IsWindowState(FLAG_WINDOW_MAXIMIZED)) RestoreWindow();
            else MaximizeWindow();
        }

        app.update(dt);
        drawCurrentScreen(app, mouse, dt, clicked, sceneClicked, savedRevision, savedLanguage,
                          savedExpeditionRevision);
        runAutoSaveTick(app, savedRevision, savedLanguage, savedExpeditionRevision);
    }

    unloadAppFont();
    CloseWindow();
    return 0;
}
