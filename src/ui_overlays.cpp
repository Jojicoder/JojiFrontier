// Warehouse Cleanup, Settings, Save Recovery, and Save Status HUD overlays -
// drawn on top of (or instead of) whichever top-level screen owns the frame.
// Split out of main.cpp; no behavior change.
#include <raylib.h>

#include <cstddef>
#include <optional>
#include <string>

#include "jf/core/GameApp.hpp"
#include "jf/core/Locale.hpp"
#include "ui_overlays.hpp"
#include "ui_shared.hpp"

namespace jfui {

bool gSettingsOpen = false;

// docs/inventory_overflow.md「倉庫整理画面」discard-confirmation target.
// Purely local to this overlay's own two draw functions below.
enum class WarehouseDiscardKind { Material, Item, Overflow };
struct WarehouseDiscardTarget {
    WarehouseDiscardKind kind;
    jf::LootId materialId;
    jf::ItemType itemType = jf::ItemType::FirstAidKit;
    std::size_t overflowIndex = 0;
    int quantity = 0;
    std::string displayName;
};
std::optional<WarehouseDiscardTarget> gWarehouseDiscardConfirm;

// docs/save_system.md「破損復旧画面」: set at startup (see main()) when the
// primary save could not be read AND no automatic ".bak" fallback inside
// SaveStore::load() succeeded either - i.e. there is truly nothing to fall
// back to silently. Drawn instead of (not on top of) the normal screen
// dispatch, since there's no meaningful "underneath" game state to show yet.
bool gSaveRecoveryOpen = false;
bool gSaveRecoveryStartNewConfirm = false;
std::string gSaveRecoveryMessage;

// docs/save_system.md「保存状態HUD」: minimal Idle/Saving/Saved/Failed state
// machine drawn in the screen's bottom-right corner. Desktop saves are
// synchronous file I/O, so `Saving` is only ever visible for a single frame;
// the state machine exists chiefly so `Failed`+manual retry actually works,
// and to match the spec's transition shape for when a slower (Web/IDBFS)
// backend is added.
SaveHudState gSaveHudState = SaveHudState::Idle;
double gSaveHudSavedUntil = 0.0;
std::string gSaveHudFailReason;
int gSaveHudRetryCount = 0;
double gSaveHudNextRetryAt = 0.0;
constexpr double kSaveHudRetryDelays[kSaveHudMaxAutoRetries] = {0.5, 1.0, 2.0};

// Save Data (Export/Import, docs/save_system.md). Lives alongside the other
// Settings-overlay UI state; gSaveStore itself is the same store main() uses
// for autosave, just hoisted to a global so the overlay can reach it without
// threading a parameter through every draw*Screen() call.
std::optional<jf::SaveStore> gSaveStore;
bool gAutoSaveEnabled = true;
std::optional<jf::SaveData> gPendingImport;
std::string gPendingImportFilename;
std::string gSaveStatusMessage;
std::string gSaveStatusMessageJa;
double gSaveStatusExpiresAt = 0.0;

void setSaveStatus(const std::string& en, const std::string& ja, double durationSeconds = 4.0) {
    gSaveStatusMessage = en;
    gSaveStatusMessageJa = ja;
    gSaveStatusExpiresAt = GetTime() + durationSeconds;
}

// The discard-confirmation sub-panel (shown when a discard button was just
// clicked). Split out of drawWarehouseCleanupOverlay(); advances `y` past
// itself when open, exactly as the original inline block did. No behavior
// change.
void drawWarehouseDiscardConfirm(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel, float& y) {
    if (!gWarehouseDiscardConfirm) return;
    const WarehouseDiscardTarget& target = *gWarehouseDiscardConfirm;
    Rectangle confirmPanel{panel.x + 26, y, panel.width - 52, 100};
    drawCard(confirmPanel, kColorCardAlt, withAlpha(Color{225, 120, 120, 255}, 220), 0.1f);
    std::string message = jf::tr("ui.warehouse.discard_confirm", gLanguage == Language::Japanese,
                                 {{"name", target.displayName}, {"quantity", std::to_string(target.quantity)}});
    drawText(message, static_cast<int>(confirmPanel.x + 14), static_cast<int>(confirmPanel.y + 14), 14,
             kColorTextPrimary);
    Rectangle confirmBtn{confirmPanel.x + 14, confirmPanel.y + 50, 150, 40};
    Rectangle cancelBtn{confirmPanel.x + 14 + 150 + 16, confirmPanel.y + 50, 150, 40};
    if (button(confirmBtn, tr("ui.button.confirm"), mouse, clicked)) {
        bool discarded = false;
        switch (target.kind) {
            case WarehouseDiscardKind::Material:
                discarded = app.discardStorage(target.materialId, target.quantity);
                break;
            case WarehouseDiscardKind::Item:
                discarded = app.discardItemStorage(target.itemType, target.quantity);
                break;
            case WarehouseDiscardKind::Overflow:
                discarded = app.discardOverflowStack(target.overflowIndex, target.quantity);
                break;
        }
        (void)discarded;  // Success or failure, the confirm panel closes either way;
                          // the list itself only reflects an entry's removal when
                          // the underlying discard actually reduced its quantity.
        gWarehouseDiscardConfirm.reset();
    }
    if (button(cancelBtn, tr("ui.button.cancel"), mouse, clicked)) gWarehouseDiscardConfirm.reset();
    y = confirmPanel.y + confirmPanel.height + 16.0f;
}

// The storage/consumables/pending-overflow discard lists. Split out of
// drawWarehouseCleanupOverlay(); the three sections share the local drawRow
// lambda and the running `y` cursor so they stay together in one function.
// No behavior change.
void drawWarehouseItemLists(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel, float& y) {
    auto drawRow = [&](const std::string& name, int quantity, WarehouseDiscardTarget target) {
        drawText(name + "  x" + std::to_string(quantity), static_cast<int>(panel.x + 26), static_cast<int>(y) + 6, 14,
                 kColorTextPrimary);
        Rectangle discardBtn{panel.x + panel.width - 26 - 110, y, 110, 32};
        target.displayName = name;
        if (button(discardBtn, tr("ui.button.discard"), mouse, clicked)) gWarehouseDiscardConfirm = target;
        y += 38.0f;
    };

    drawText(tr("ui.warehouse.storage_section"), static_cast<int>(panel.x + 26), static_cast<int>(y), 16,
             kColorAccentGold);
    y += 26.0f;
    bool anyStorage = false;
    for (const jf::LootStack& stack : app.baseState().storage) {
        if (app.baseState().materialStorageCap(stack.id) == 1) continue;  // key materials: never discardable
        anyStorage = true;
        WarehouseDiscardTarget target;
        target.kind = WarehouseDiscardKind::Material;
        target.materialId = stack.id;
        target.quantity = stack.quantity;
        drawRow(materialNameFor(stack.id), stack.quantity, target);
    }
    if (!anyStorage) {
        drawText(tr("ui.warehouse.empty"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13, kColorTextMuted);
        y += 26.0f;
    }

    y += 12.0f;
    drawText(tr("ui.warehouse.consumables_section"), static_cast<int>(panel.x + 26), static_cast<int>(y), 16,
             kColorAccentGold);
    y += 26.0f;
    bool anyItems = false;
    for (const jf::ItemDefinition& item : jf::kItemCatalog) {
        const int owned = app.baseState().ownedItemCount(item.type);
        if (owned <= 0) continue;
        anyItems = true;
        WarehouseDiscardTarget target;
        target.kind = WarehouseDiscardKind::Item;
        target.itemType = item.type;
        target.quantity = owned;
        drawRow(itemFullNameFor(item.type), owned, target);
    }
    if (!anyItems) {
        drawText(tr("ui.warehouse.empty"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13, kColorTextMuted);
        y += 26.0f;
    }

    y += 12.0f;
    drawText(tr("ui.warehouse.pending_section"), static_cast<int>(panel.x + 26), static_cast<int>(y), 16,
             kColorAccentGold);
    y += 26.0f;
    const auto& overflowStacks = app.rewardOverflow().stacks;
    if (overflowStacks.empty()) {
        drawText(tr("ui.warehouse.empty"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13, kColorTextMuted);
        y += 26.0f;
    }
    for (std::size_t i = 0; i < overflowStacks.size(); ++i) {
        const jf::OverflowStack& stack = overflowStacks[i];
        // "item:<int>" prefix (see OverflowStack::itemId's doc comment in
        // BaseState.hpp) distinguishes a consumable overflow entry from a
        // material one sharing the same display path.
        std::string name;
        if (stack.itemId.rfind("item:", 0) == 0) {
            const int rawType = std::stoi(stack.itemId.substr(5));
            name = itemFullNameFor(static_cast<jf::ItemType>(rawType));
        } else {
            name = materialNameFor(stack.itemId);
        }
        WarehouseDiscardTarget target;
        target.kind = WarehouseDiscardKind::Overflow;
        target.overflowIndex = i;
        target.quantity = stack.quantity;
        drawRow(name, stack.quantity, target);
    }
}

// docs/inventory_overflow.md「倉庫整理画面」: lists storage/consumables/
// pending-overflow and lets the player discard consumables and ordinary
// materials (never equipped gear, key materials, or Discoveries - key
// materials are refused by GameApp::discardStorage() itself). A single
// explicit Confirm button per docs (no long-press, no two-step beyond the
// one confirmation panel); the target list only drops an entry once the
// discard call actually reports success.
void drawWarehouseCleanupOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    if (!gWarehouseCleanupOpen) return;

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 170});
    Rectangle panel{static_cast<float>(kScreenWidth) / 2.0f - 280.0f, 60.0f, 560.0f, 680.0f};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 230), 0.08f);

    drawText(tr("ui.warehouse.title"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 20), 24,
             kColorTextPrimary);

    const bool blockedByOverflow = app.rewardOverflow().stacks.size() >= jf::RewardOverflowState::kMaxStacks;
    float y = panel.y + 60.0f;
    if (blockedByOverflow) {
        drawText(tr("ui.warehouse.overflow_blocked"), static_cast<int>(panel.x + 26), static_cast<int>(y), 13,
                 Color{225, 120, 120, 255});
        y += 28.0f;
    }

    drawWarehouseDiscardConfirm(app, mouse, clicked, panel, y);
    drawWarehouseItemLists(app, mouse, clicked, panel, y);

    Rectangle closeBtn{panel.x + panel.width - 26 - 120, panel.y + panel.height - 50, 120, 38};
    if (button(closeBtn, tr("ui.button.close"), mouse, clicked)) {
        gWarehouseCleanupOpen = false;
        gWarehouseDiscardConfirm.reset();
    }

    Rectangle retryBtn{panel.x + 26, panel.y + panel.height - 50, 240, 38};
    if (button(retryBtn, tr("ui.warehouse.try_return_again"), mouse, clicked)) {
        if (app.returnToBase()) gWarehouseCleanupOpen = false;
    }
}

// The language picker (English/Japanese) row. Split out of
// drawSettingsOverlay(); no behavior change.
void drawSettingsLanguageSection(Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.language"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 66), 15,
             kColorTextMuted);

    Rectangle enBtn{panel.x + 26, panel.y + 96, 150, 48};
    Rectangle jaBtn{panel.x + 26 + 150 + 16, panel.y + 96, 150, 48};

    if (button(enBtn, "English", "", mouse, clicked)) gLanguage = Language::English;
    if (button(jaBtn, kJaJapaneseNative, "", mouse, clicked)) gLanguage = Language::Japanese;

    Rectangle activeHighlight = gLanguage == Language::English ? enBtn : jaBtn;
    DrawRectangleRoundedLinesEx(activeHighlight, 0.28f, 8, 3.0f, withAlpha(kColorAccentGold, 255));
}

// The window maximize/restore row. Split out of drawSettingsOverlay(); no
// behavior change.
void drawSettingsWindowSection(Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.window"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 158), 15,
             kColorTextMuted);
    bool maximized = IsWindowState(FLAG_WINDOW_MAXIMIZED);
    Rectangle windowBtn{panel.x + 26, panel.y + 188, 328, 46};
    if (button(windowBtn, tr(maximized ? "ui.settings.restore_window" : "ui.settings.maximize_window"), mouse,
              clicked)) {
        if (maximized) RestoreWindow();
        else MaximizeWindow();
    }
    drawText(tr("ui.settings.maximize_shortcut"), static_cast<int>(panel.x + 26),
             static_cast<int>(panel.y + 238), 12, kColorTextFaint);
}

// The retire-expedition row. Split out of drawSettingsOverlay(); no behavior
// change.
void drawSettingsExpeditionSection(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.expedition_section"), static_cast<int>(panel.x + 26),
             static_cast<int>(panel.y + 266), 15, kColorTextMuted);
    Rectangle retireBtn{panel.x + 26, panel.y + 296, 328, 46};
    bool canRetire = app.screen() != jf::Screen::Base;
    if (canRetire) {
        if (button(retireBtn, tr("ui.settings.retire_expedition"), mouse, clicked)) {
            if (app.retireExpedition()) gSettingsOpen = false;
        }
    } else {
        disabledButton(retireBtn, tr("ui.settings.retire_expedition"));
    }
    drawText(tr("ui.settings.retire_expedition_note"),
             static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 346), 12, kColorTextFaint);
}

// The save-data export/import section, plus the transient save-status
// message. Split out of drawSettingsOverlay(); no behavior change.
void drawSettingsSaveDataSection(jf::GameApp& app, Vector2 mouse, bool clicked, const Rectangle& panel) {
    drawText(tr("ui.settings.save_data_section"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 380),
              15, kColorTextMuted);

    Rectangle exportBtn{panel.x + 26, panel.y + 408, 150, 46};
    Rectangle importBtn{panel.x + 26 + 150 + 16, panel.y + 408, 150, 46};

    if (button(exportBtn, tr("ui.settings.export"), mouse, clicked)) {
        const std::string language = gLanguage == Language::Japanese ? "ja" : "en";
        std::string exportError;
        std::string exportedPath = jf::exportSaveData(app.createSaveData(language), &exportError);
        if (!exportedPath.empty())
            setSaveStatus(jf::tr("ui.settings.export_ok_prefix", false) + exportedPath,
                          jf::tr("ui.settings.export_ok_prefix", true) + exportedPath);
        else
            setSaveStatus(jf::tr("ui.settings.export_failed_prefix", false) + exportError,
                          jf::tr("ui.settings.export_failed_prefix", true) + exportError);
    }

    const bool canImport = app.screen() == jf::Screen::Base;
    if (gPendingImport) {
        drawText(clipTextToWidth(gPendingImportFilename + " - " + tr("ui.settings.import_confirm"), 13,
                                  328),
                 static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 462), 13, kColorTextPrimary);
        Rectangle applyBtn{panel.x + 26, panel.y + 488, 150, 44};
        Rectangle cancelBtn{panel.x + 26 + 150 + 16, panel.y + 488, 150, 44};
        if (button(applyBtn, tr("ui.settings.import_apply"), mouse, clicked)) {
            std::string importError;
            if (gSaveStore && gSaveStore->importFrom(*gPendingImport, &importError) && app.applySaveData(*gPendingImport)) {
                gLanguage = gPendingImport->language == "ja" ? Language::Japanese : Language::English;
                gAutoSaveEnabled = true;
                setSaveStatus(jf::tr("ui.settings.import_applied", false), jf::tr("ui.settings.import_applied", true));
            } else {
                setSaveStatus(jf::tr("ui.settings.import_failed_prefix", false) + importError,
                              jf::tr("ui.settings.import_failed_prefix", true) + importError);
            }
            gPendingImport.reset();
            gPendingImportFilename.clear();
        }
        if (button(cancelBtn, tr("ui.button.cancel"), mouse, clicked)) {
            gPendingImport.reset();
            gPendingImportFilename.clear();
        }
    } else if (canImport) {
        if (button(importBtn, tr("ui.settings.import"), mouse, clicked)) {
            auto candidates = jf::listImportCandidates();
            if (candidates.empty()) {
                setSaveStatus(jf::tr("ui.settings.import_no_file", false), jf::tr("ui.settings.import_no_file", true));
            } else {
                std::string loadError;
                if (auto data = jf::loadImportCandidate(candidates.front().path, &loadError)) {
                    gPendingImport = data;
                    gPendingImportFilename = candidates.front().filename;
                } else {
                    setSaveStatus(jf::tr("ui.settings.import_failed_prefix", false) + loadError,
                                  jf::tr("ui.settings.import_failed_prefix", true) + loadError);
                }
            }
        }
    } else {
        disabledButton(importBtn, tr("ui.settings.import"));
        drawText(tr("ui.settings.import_base_only"), static_cast<int>(panel.x + 26),
                 static_cast<int>(panel.y + 462), 12, kColorTextFaint);
    }

    if (!gSaveStatusMessage.empty() && GetTime() < gSaveStatusExpiresAt) {
        drawText(clipTextToWidth(pick(gSaveStatusMessage, gSaveStatusMessageJa), 12, 328),
                 static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 544), 12, kColorAccentGold);
    }
}

// Small always-on-top corner button + modal for switching the display
// language. Purely a rendering/UI concern (see the Language enum above),
// so it lives entirely in this file and never touches GameApp/BattleState.
// Draw this last on every screen so it sits above any other overlay.
void drawSettingsOverlay(jf::GameApp& app, Vector2 mouse, bool clicked) {
    Rectangle cornerBtn{static_cast<float>(kScreenWidth) - 100.0f, 4.0f, 92.0f, 32.0f};
    if (button(cornerBtn, tr("ui.settings.title"), mouse, gSettingsOpen ? false : clicked)) {
        gSettingsOpen = !gSettingsOpen;
    }

    if (!gSettingsOpen) return;

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, Color{0, 0, 0, 150});

    Rectangle panel{static_cast<float>(kScreenWidth) / 2.0f - 190.0f, static_cast<float>(kScreenHeight) / 2.0f - 320.0f,
                    380.0f, 640.0f};
    drawCard(panel, kColorCard, withAlpha(kColorAccentGold, 230), 0.1f);

    drawText(tr("ui.settings.title"), static_cast<int>(panel.x + 26), static_cast<int>(panel.y + 22), 24,
             kColorTextPrimary);

    drawSettingsLanguageSection(mouse, clicked, panel);
    drawSettingsWindowSection(mouse, clicked, panel);
    drawSettingsExpeditionSection(app, mouse, clicked, panel);
    drawSettingsSaveDataSection(app, mouse, clicked, panel);

    Rectangle closeBtn{panel.x + 26, panel.y + 580, 328, 40};
    if (button(closeBtn, tr("ui.button.close"), mouse, clicked)) gSettingsOpen = false;
}

// docs/save_system.md「保存状態HUD」: performs one save attempt and advances
// the Idle/Saving/Saved/Failed state machine. Desktop I/O is synchronous, so
// callers see the outcome (Saved or Failed) immediately - `Saving` never
// actually gets drawn on its own frame here, but the state still exists so
// the transition shape matches the spec (and so a future async Web backend
// slots in without changing this function's contract).
void attemptAutoSave(jf::GameApp& app, std::uint64_t& savedRevision, Language& savedLanguage,
                     std::uint64_t& savedExpeditionRevision) {
    const std::string language = gLanguage == Language::Japanese ? "ja" : "en";
    std::string saveError;
    if (gSaveStore->save(app.createSaveData(language), &saveError)) {
        savedRevision = app.persistentRevision();
        savedLanguage = gLanguage;
        savedExpeditionRevision = app.expeditionRevision();
        gSaveHudState = SaveHudState::Saved;
        gSaveHudSavedUntil = GetTime() + 1.5;
        gSaveHudRetryCount = 0;
    } else {
        gSaveHudFailReason = saveError;
        gSaveHudState = SaveHudState::Failed;
        if (gSaveHudRetryCount < kSaveHudMaxAutoRetries) {
            gSaveHudNextRetryAt = GetTime() + kSaveHudRetryDelays[gSaveHudRetryCount];
            ++gSaveHudRetryCount;
        }
    }
}

// Small fixed-corner status readout (docs/save_system.md「保存状態HUD」),
// placed clear of the battle command HUD / Facilities / Settings buttons.
// Manual "Retry" stays available even after the 3 automatic retries are
// exhausted (spec: "手動の「再試行」は回数制限後も利用可能にする").
void drawSaveStatusHud(jf::GameApp& app, Vector2 mouse, bool clicked, std::uint64_t& savedRevision,
                      Language& savedLanguage, std::uint64_t& savedExpeditionRevision) {
    if (gSaveHudState == SaveHudState::Idle) return;
    if (gSaveHudState == SaveHudState::Saved && GetTime() >= gSaveHudSavedUntil) {
        gSaveHudState = SaveHudState::Idle;
        return;
    }
    constexpr float kHudW = 220.0f;
    constexpr float kHudH = 40.0f;
    Rectangle hud{static_cast<float>(kScreenWidth) - kHudW - 12.0f, static_cast<float>(kScreenHeight) - kHudH - 12.0f,
                 kHudW, kHudH};
    Color border = gSaveHudState == SaveHudState::Failed ? Color{225, 120, 120, 255} : kColorAccentGold;
    drawCard(hud, kColorCard, withAlpha(border, 220), 0.2f);
    std::string label = gSaveHudState == SaveHudState::Saving   ? tr("ui.save_hud.saving")
                        : gSaveHudState == SaveHudState::Saved  ? tr("ui.save_hud.saved")
                                                                 : tr("ui.save_hud.failed");
    drawText(label, static_cast<int>(hud.x + 12), static_cast<int>(hud.y + 10), 14,
             gSaveHudState == SaveHudState::Failed ? Color{225, 120, 120, 255} : kColorTextPrimary);
    if (gSaveHudState == SaveHudState::Failed) {
        Rectangle retryBtn{hud.x + kHudW - 84.0f, hud.y + 4.0f, 76.0f, kHudH - 8.0f};
        if (button(retryBtn, tr("ui.save_hud.retry"), mouse, clicked))
            attemptAutoSave(app, savedRevision, savedLanguage, savedExpeditionRevision);
    }
}

// docs/save_system.md「破損復旧画面」: shown instead of the normal screen
// dispatch when startup couldn't read any usable save (see main()). All 3
// choices end by clearing gSaveRecoveryOpen and re-enabling autosave, since
// each one leaves the app in a definite, savable state (recovered data,
// imported data, or a deliberate fresh start).
void drawSaveRecoveryScreen(jf::GameApp& app, Vector2 mouse, bool clicked) {
    ClearBackground(Color{18, 21, 30, 255});
    drawText(tr("ui.recovery.title"), 40, 40, 28, kColorAccentGold);
    drawText(gSaveRecoveryMessage.empty() ? tr("ui.recovery.message") : gSaveRecoveryMessage, 40, 90, 16,
             kColorTextPrimary);

    if (gSaveRecoveryStartNewConfirm) {
        drawText(tr("ui.recovery.start_new_confirm"), 40, 140, 15, Color{225, 120, 120, 255});
        Rectangle confirmBtn{40, 180, 200, 44};
        Rectangle cancelBtn{256, 180, 200, 44};
        if (button(confirmBtn, tr("ui.button.confirm"), mouse, clicked)) {
            gSaveStore->quarantineCorruptSave();
            gSaveRecoveryOpen = false;
            gSaveRecoveryStartNewConfirm = false;
            gAutoSaveEnabled = true;
        }
        if (button(cancelBtn, tr("ui.button.cancel"), mouse, clicked)) gSaveRecoveryStartNewConfirm = false;
        return;
    }

    Rectangle restoreBtn{40, 140, 260, 48};
    Rectangle importBtn{40, 200, 260, 48};
    Rectangle startNewBtn{40, 260, 260, 48};
    if (button(restoreBtn, tr("ui.recovery.restore_backup"), mouse, clicked)) {
        std::string restoreError;
        if (gSaveStore->restoreFromBackup(&restoreError)) {
            if (auto restored = gSaveStore->load()) {
                if (app.applySaveData(*restored))
                    gLanguage = restored->language == "ja" ? Language::Japanese : Language::English;
            }
            gSaveRecoveryOpen = false;
            gAutoSaveEnabled = true;
        } else {
            gSaveRecoveryMessage = tr("ui.recovery.restore_failed");
        }
    }
    if (button(importBtn, tr("ui.recovery.import_save"), mouse, clicked)) {
        gSaveRecoveryOpen = false;
        gAutoSaveEnabled = true;
        gSettingsOpen = true;  // Base screen's Import flow (already unrestricted
                               // at startup, since the app always begins at
                               // Screen::Base) takes it from here.
    }
    if (button(startNewBtn, tr("ui.recovery.start_new"), mouse, clicked)) gSaveRecoveryStartNewConfirm = true;
}

}  // namespace jfui
