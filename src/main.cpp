#include "RE/A/Actor.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/B/BGSInventoryItem.h"
#include "RE/B/BGSObjectInstance.h"
#include "RE/C/ChargenMenu.h"
#include "RE/M/MenuPaperDoll.h"
#include "RE/B/BSTSmartPointer.h"
#include "RE/S/ScaleformGFxASMovieRootBase.h"
#include "RE/S/ScaleformGFxFunctionHandler.h"
#include "RE/S/ScaleformGFxValue.h"
#include "RE/T/TESObjectARMO.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Bare Frontier 0.1.5j - Prevent stock weight-editor activation under virtual Nude focus.
// This is deliberately limited to ChargenMenu's SEPARATE MenuActor.
// It never equips/unequips on PlayerCharacter, modifies records, or edits save data.
namespace
{
    using GFxValue = RE::Scaleform::GFx::Value;
    using Clock = std::chrono::steady_clock;

    std::atomic_bool g_chargenOpen{ false };
    bool g_nude = false;
    bool g_hover = false;
    bool g_navFocus = false; // Our row sits between weight and Body Type. Native BodyPage focus remains unchanged until exit.
    bool g_warnedNativeEdit = false; // Diagnostic: native editor activating under virtual Nude focus.
    bool g_installed = false;
    bool g_loggedBodyFrame = false;
    RE::MenuActor* g_changedActor = nullptr;

    struct SavedArmor
    {
        RE::TESObjectARMO* armor;
        RE::BSTSmartPointer<RE::TBO_InstanceData> instanceData;
    };
    std::vector<SavedArmor> g_removed;

    bool ReadInt(const GFxValue& obj, const char* name, std::int32_t& out)
    {
        GFxValue v;
        if (!obj.GetMember(name, &v)) return false;
        if (v.IsInt()) { out = v.GetInt(); return true; }
        if (v.IsUInt()) { out = static_cast<std::int32_t>(v.GetUInt()); return true; }
        if (v.IsNumber()) { out = static_cast<std::int32_t>(v.GetNumber()); return true; }
        return false;
    }

    // Use the active ActionScript timeline frame, not the tab's numeric index.
    // Vanilla ChargenMenu reports BodyPage at CurrentPage=1, whereas
    // Expired6978's CharGenMenu adds a Presets tab and uses CurrentPage=2.
    // Both UIs keep the BodyPage timeline frame label.
    // Fail closed when an unknown UI does not expose either frame label.
    bool IsBodyPageActive(const GFxValue& main)
    {
        if (!main.IsDisplayObject()) return false;
        GFxValue frame;
        if (main.GetMember("currentFrameLabel", &frame) && frame.IsString()) {
            const char* const name = frame.GetString();
            if (name && *name) return std::strcmp(name, "BodyPage") == 0;
        }
        if (main.GetMember("currentLabel", &frame) && frame.IsString()) {
            const char* const name = frame.GetString();
            if (name && *name) return std::strcmp(name, "BodyPage") == 0;
        }
        return false;
    }

    RE::ChargenMenu* GetChargenMenu()
    {
        if (!g_chargenOpen.load()) return nullptr;
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return nullptr;
        auto menu = ui->GetMenu(RE::BSFixedString{ "ChargenMenu" });
        return menu ? static_cast<RE::ChargenMenu*>(menu.get()) : nullptr;
    }

    RE::MenuActor* GetPreviewActor()
    {
        auto* menu = GetChargenMenu();
        return (menu && menu->paperDoll) ? menu->paperDoll->menuActor : nullptr;
    }

    // Only the editor's MenuActor can ever be modified. Reject the player's
    // real actor, uninitialized previews, and preview identity changes.
    bool IsSafePreview(RE::MenuActor* actor)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return actor && player && static_cast<RE::Actor*>(actor) != static_cast<RE::Actor*>(player);
    }

    bool ResolveUI(GFxValue& root, GFxValue& body, GFxValue* mainOut = nullptr)
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui || !g_chargenOpen.load()) return false;
        auto movie = ui->GetMenuMovie(RE::BSFixedString{ "ChargenMenu" });
        if (!movie || !movie->asMovieRoot) return false;
        auto* gfx = movie->asMovieRoot.get();
        GFxValue main;
        if (!gfx->GetVariable(&root, "root1") || !root.IsObject() ||
            !root.GetMember("Menu_mc", &main) || !main.IsObject() ||
            !main.GetMember("BodyPage_mc", &body) || !body.IsDisplayObject()) return false;
        if (mainOut) *mainOut = main;
        return true;
    }

    void UpdateLabel()
    {
        GFxValue root, body, label;
        if (!ResolveUI(root, body) ||
            !body.GetMember("BareFrontierNudeLabel_tf", &label) || !label.IsDisplayObject()) return;
        const char* text = g_nude ? "NUDE: ON" : "NUDE: OFF";
        // Mouse rollover and gamepad/keyboard focus share the stock visual style.
        label.SetMember("text", GFxValue(text));
        label.SetMember("background", GFxValue(g_hover || g_navFocus));
        label.SetMember("backgroundColor", GFxValue(std::uint32_t{ 0x233F4C }));
        label.SetMember("textColor", GFxValue(std::uint32_t{ (g_hover || g_navFocus) ? 0xEDEDE9u : 0x233F4Cu }));
    }

    // v0.1.5d: the custom label can highlight while Starfield's stock controls
    // keep their old Selected timeline frame. Release ONLY those visual frames
    // while our row is hovered; do not change native control handlers or data.
    bool IsUnselectedFrame(const GFxValue& control)
    {
        GFxValue current;
        if (!control.GetMember("currentLabel", &current) || !current.IsString()) return false;
        const char* const label = current.GetString();
        return label && (_stricmp(label, "Unselected") == 0);
    }

    // Scaleform Value exposes array iteration through VisitElements, rather
    // than GetArraySize/GetElement (which are ObjectInterface-only APIs).
    class FrameLabels final : public GFxValue::ArrayVisitor
    {
    public:
        std::string unselected;
        std::vector<std::string> available;
        void Visit(std::uint32_t, const GFxValue& entry) override
        {
            GFxValue label;
            if (!entry.IsObject() || !entry.GetMember("name", &label) || !label.IsString()) return;
            const char* const name = label.GetString();
            if (!name) return;
            if (available.size() < 12) available.emplace_back(name);
            if (unselected.empty() && _stricmp(name, "Unselected") == 0) unselected = name;
        }
    };

    bool ReleaseStockSelection(const GFxValue& body, const char* name, bool diagnostic)
    {
        GFxValue stock;
        if (!body.GetMember(name, &stock) || !stock.IsDisplayObject()) return false;
        if (IsUnselectedFrame(stock)) return true;

        // Read the stock clip's actual frame labels before using GotoAndStop.
        // Never request an unknown label, or write to native selection variables.
        GFxValue labels;
        FrameLabels collected;
        if (stock.GetMember("currentLabels", &labels) && labels.IsArray()) {
            labels.VisitElements(&collected);
        }
        const bool ok = !collected.unselected.empty() &&
            stock.GotoAndStop(collected.unselected) && IsUnselectedFrame(stock);
        if (diagnostic && !ok) {
            GFxValue current;
            if (stock.GetMember("currentLabel", &current) && current.IsString()) {
                REX::WARN("BF/015d: {} stayed on timeline label '{}'", name, current.GetString());
            } else {
                REX::WARN("BF/015d: {} has no readable timeline label", name);
            }
            for (const auto& frame : collected.available) {
                REX::INFO("BF/015d: stock {} timeline available frame '{}'", name, frame);
            }
        }
        return ok;
    }

    void ReleaseNativeHighlights(bool diagnostic)
    {
        if (!(g_hover || g_navFocus) || !g_chargenOpen.load()) return;
        GFxValue root, body, main;
        if (!ResolveUI(root, body, &main) || !IsBodyPageActive(main)) return;
        const bool radial = ReleaseStockSelection(body, "BodyModifier_mc", diagnostic);
        const bool bodyType = ReleaseStockSelection(body, "BodyTypeStepper_mc", diagnostic);
        const bool walkStyle = ReleaseStockSelection(body, "LocomotionStepper_mc", diagnostic);
        const bool skinTone = ReleaseStockSelection(body, "SkintoneStepper_mc", diagnostic);
        if (diagnostic) {
            REX::INFO("BF/015d: SOLO HOVER stock selections cleared: radial={} bodyType={} walkStyle={} skinTone={}",
                radial, bodyType, walkStyle, skinTone);
        }
    }

    // Check the *actual inventory state* rather than trusting the boolean
    // return from EquipObject/UnequipObject. In Starfield 1.16.244 the
    // preview's UnequipObject may return false AFTER it unequips an item.
    bool PreviewHasItem(RE::MenuActor* actor, RE::TESObjectARMO* armor)
    {
        if (!IsSafePreview(actor) || !armor) return false;
        const auto guard = actor->inventoryList.LockRead();
        auto* inventory = *guard;
        if (!inventory) return false;
        for (const auto& item : inventory->data) {
            if (item.object == armor) return true;
        }
        return false;
    }

    bool PreviewWearsArmor(RE::MenuActor* actor, RE::TESObjectARMO* armor)
    {
        if (!IsSafePreview(actor) || !armor) return false;
        const auto guard = actor->inventoryList.LockRead();
        auto* inventory = *guard;
        if (!inventory) return false;
        for (const auto& item : inventory->data) {
            if (item.object == armor && item.IsEquipped()) return true;
        }
        return false;
    }

    bool RemovePreviewBodyClothing()
    {
        auto* actor = GetPreviewActor();
        if (!IsSafePreview(actor)) {
            REX::WARN("BF/015b: preview unavailable; OFF unchanged");
            return false;
        }
        if (!g_removed.empty()) {
            REX::WARN("BF/015b: pending restoration; refusing to remove any more clothing");
            return true;
        }
        auto* equip = RE::ActorEquipManager::GetSingleton();
        if (!equip) {
            REX::WARN("BF/015b: ActorEquipManager unavailable; OFF unchanged");
            return false;
        }
        std::vector<SavedArmor> eligible;
        {
            // Snapshot before the operation; never hold the inventory lock while equipping.
            const auto guard = actor->inventoryList.LockRead();
            auto* inventory = *guard;
            if (!inventory) {
                REX::WARN("BF/015b: preview inventory is null");
                return false;
            }
            for (const auto& item : inventory->data) {
                if (!item.object || !item.IsEquipped()) continue;
                auto* armor = item.object->As<RE::TESObjectARMO>();
                if (!armor) continue;
                const auto mask = armor->bipedModelData.bipedObjectSlots;
                REX::INFO("BF/015b: equipped preview ARMO {:08X}, mask={:08X}",
                    armor->GetFormID(), mask);
                if ((mask & (1u << 3)) != 0) {
                    eligible.push_back({ armor, item.instanceData });
                }
            }
        }
        if (eligible.empty() || eligible.size() > 4) {
            REX::WARN("BF/015b: eligible BODY outfit count={}, refusing change", eligible.size());
            return false;
        }
        g_changedActor = actor;
        for (const auto& saved : eligible) {
            // Save an independent reference to the exact armor and instance data
            // BEFORE executing the unequip call. Return values can be misleading.
            RE::BGSObjectInstance obj{ saved.armor, saved.instanceData.get() };
            const bool apiResult = equip->UnequipObject(actor, obj, nullptr,
                false, false, false, true, nullptr);
            const bool stillEquipped = PreviewWearsArmor(actor, saved.armor);
            REX::INFO("BF/015b: UnequipObject {:08X} returned={} actuallyEquipped={}",
                saved.armor->GetFormID(), apiResult, stillEquipped);
            if (!stillEquipped) {
                g_removed.push_back(saved);
            } else {
                REX::WARN("BF/015b: outfit did not come off; no restore needed for {:08X}",
                    saved.armor->GetFormID());
            }
        }
        if (g_removed.empty()) {
            g_changedActor = nullptr;
            REX::WARN("BF/015b: preview equipment unchanged; OFF unchanged");
            return false;
        }
        REX::INFO("BF/015b: ON confirmed; tracking {} outfit(s) for restoration", g_removed.size());
        return true;
    }

    void RestorePreviewClothing(const char* reason)
    {
        if (g_removed.empty()) {
            g_nude = false;
            g_changedActor = nullptr;
            return;
        }
        auto* actor = GetPreviewActor();
        auto* equip = RE::ActorEquipManager::GetSingleton();
        if (!actor || actor != g_changedActor || !IsSafePreview(actor) || !equip) {
            REX::WARN("BF/015b: cannot verify same preview for restoration ({}); will never touch player", reason);
            // Retain saved references for another attempt while menu is alive.
            g_nude = true;
            return;
        }

        std::vector<SavedArmor> unresolved;
        for (const auto& saved : g_removed) {
            if (PreviewWearsArmor(actor, saved.armor)) {
                REX::INFO("BF/015b: outfit {:08X} already equipped ({})", saved.armor->GetFormID(), reason);
                continue;
            }
            RE::BGSObjectInstance obj{ saved.armor, saved.instanceData.get() };
            const bool apiResult = equip->EquipObject(actor, obj, nullptr,
                false, false, false, true, false);
            bool equipped = PreviewWearsArmor(actor, saved.armor);
            REX::INFO("BF/015b: EquipObject {:08X} returned={} actuallyEquipped={} ({})",
                saved.armor->GetFormID(), apiResult, equipped, reason);

            // Starfield's higher-level equip path may succeed when the direct
            // manager reports success/failure inconsistently on menu actors.
            // Only use it for an outfit already in this SAME preview inventory.
            if (!equipped && PreviewHasItem(actor, saved.armor)) {
                REX::INFO("BF/015b: trying Actor::EquipItem on existing PREVIEW item {:08X}",
                    saved.armor->GetFormID());
                actor->EquipItem(saved.armor, false, true);
                equipped = PreviewWearsArmor(actor, saved.armor);
                REX::INFO("BF/015b: Actor::EquipItem {:08X} actuallyEquipped={}",
                    saved.armor->GetFormID(), equipped);
            }
            if (!equipped) unresolved.push_back(saved);
        }
        g_removed = std::move(unresolved);
        g_nude = !g_removed.empty();
        if (!g_nude) {
            REX::INFO("BF/015b: OFF confirmed; original preview outfit restored ({})", reason);
            g_changedActor = nullptr;
        } else {
            REX::ERROR("BF/015b: RESTORE INCOMPLETE: {} outfit(s) still not equipped ({})",
                g_removed.size(), reason);
        }
    }

    void TogglePreview()
    {
        if (!g_chargenOpen.load()) return;
        GFxValue root, body, main;
        if (!ResolveUI(root, body, &main)) return;
        if (!IsBodyPageActive(main)) {
            REX::INFO("BF/015f: click ignored; BodyPage frame not active");
            return;
        }
        if (g_nude || !g_removed.empty()) {
            RestorePreviewClothing("toggle-OFF");
        } else {
            g_nude = RemovePreviewBodyClothing();
        }
        UpdateLabel();
        // Equipment updates can reassert the native Body Editor Selected frame.
        // Force the visual focus to stay exclusive immediately after the operation.
        if (g_navFocus || g_hover) ReleaseNativeHighlights(false);
    }

    void QueueToggle()
    {
        if (auto* tasks = SFSE::GetTaskInterface()) {
            tasks->AddTask([] { TogglePreview(); });
        } else {
            REX::WARN("BF/015: SFSE TaskInterface unavailable; click ignored");
        }
    }

    // D-pad Up/Down, Cross/A, and keyboard use normal Scaleform KEY_DOWN in
    // both vanilla and CharGenMenu.  The stock BodyPage keeps a PRIVATE focus
    // index; do not overwrite its ActionScript private state or globally hook
    // controller buttons. Instead, capture just the transition into a virtual
    // Nude row between the native weight and Body Type controls. On exit,
    // delegate to the stock, public BodyPage.SetFocus(index, force) routine.
    // Index here is a BODY-PAGE focus item (weight=0/bodyType=1), NOT a tab index.
    bool IsStockBodyNavigationAvailable(GFxValue& body)
    {
        GFxValue editing;
        if (!body.Invoke("ShowBack", &editing) || !editing.IsBoolean()) {
            // Unknown or incompatible page: fail closed, leave its keys intact.
            return false;
        }
        // Weight/sliders edit mode: never reinterpret d-pad as navigation.
        return !editing.GetBoolean();
    }

    bool StopUIEvent(GFxValue& event)
    {
        // Cancel the native key-down only for handled Nude navigation/actions.
        // stopImmediatePropagation suppresses the page's stock bubble handler.
        return event.Invoke("stopImmediatePropagation");
    }

    void SetNudeNavFocus(bool focused)
    {
        g_navFocus = focused;
        g_warnedNativeEdit = false;
        // Controller selection replaces mouse-hover styling until pointer moves.
        g_hover = false;
        UpdateLabel();
        if (focused) ReleaseNativeHighlights(false);
    }

    void ProcessNavigationKey(const RE::Scaleform::GFx::FunctionHandler::Params& params)
    {
        if (!g_chargenOpen.load() || !params.args || params.argCount == 0 ||
            !params.args[0].IsObject()) return;

        GFxValue root, body, main;
        if (!ResolveUI(root, body, &main) || !IsBodyPageActive(main)) return;

        std::int32_t keyCode = -1;
        if (!ReadInt(params.args[0], "keyCode", keyCode)) return;

        // Focus on the stock parent with selected Nude row is deliberately
        // virtual; the stock stage.focus is not moved to an unrelated field.
        // Only the keys below are consumed while that row is active.
        const bool isUp = keyCode == 38;
        const bool isDown = keyCode == 40;
        const bool isAccept = keyCode == 13 || keyCode == 32;
        const bool isHorizontal = keyCode == 37 || keyCode == 39;
        if (!isUp && !isDown && !isAccept && !isHorizontal) return;

        GFxValue target, name;
        std::string targetName;
        if (params.args[0].GetMember("target", &target) && target.IsObject() &&
            target.GetMember("name", &name) && name.IsString() && name.GetString()) {
            targetName = name.GetString();
        }

        // Avoid intercepting the real body's weight editor while it is active,
        // or any CharGenMenu popup/slider mode.  Only enter from known stock
        // row names, and only if ShowBack says editing is inactive.
        if (!g_navFocus && !(isDown && targetName == "BodyModifier_mc") &&
            !(isUp && targetName == "BodyTypeStepper_mc")) return;

        if (!IsStockBodyNavigationAvailable(body)) {
            if (g_navFocus) {
                REX::WARN("BF/015i/NAV: virtual Nude focus lost because native Body editor reports edit mode; keyCode={}", keyCode);
                g_navFocus = false;
                g_warnedNativeEdit = false;
                UpdateLabel();
            }
            return;
        }

        GFxValue event = params.args[0];
        if (!StopUIEvent(event)) {
            REX::WARN("BF/015h/NAV: cannot suppress native keyDown; leaving stock navigation unchanged");
            return;
        }

        if (!g_navFocus) {
            // Scaleform stopImmediatePropagation stops the BodyPage KEY_DOWN
            // handler, but the menu ALSO receives the native gamepad Accept
            // through ProcessUserEvent. With FocusItem still on WEIGHT (0),
            // that second path may call SetWeightActive and activate the radial
            // editor under our virtual Nude row. Before selecting Nude, park
            // the *stock* focus on BODY TYPE (1): SetWeightActive becomes a
            // harmless no-op, and the UI keeps its public/private state valid.
            // Our capture handler still consumes Left/Right and Cross while
            // Nude is selected; ordinary BODY TYPE input works on exit.
            GFxValue parkedArgs[] = { GFxValue(std::uint32_t{ 1 }), GFxValue(true) };
            if (!body.Invoke("SetFocus", nullptr, parkedArgs, 2)) {
                REX::WARN("BF/015j/NAV: cannot park stock focus on BODY TYPE; Nude entry skipped");
                return;
            }
            GFxValue stockSelect;
            if (!body.Invoke("ShowSelect", &stockSelect) ||
                !stockSelect.IsBoolean() || stockSelect.GetBoolean()) {
                REX::WARN("BF/015j/NAV: stock selection cannot be validated; Nude entry skipped");
                return;
            }
            SetNudeNavFocus(true);
            REX::INFO("BF/015j/NAV: entered Nude from {} keyCode={}; native focus parked on BODY TYPE", targetName, keyCode);
            return;
        }

        if (isAccept || isHorizontal) {
            // Re-park only if another UI callback reset focus to WEIGHT.
            // Avoid unnecessary SetFocus calls/sounds during rapid toggles.
            GFxValue stockSelect;
            if (body.Invoke("ShowSelect", &stockSelect) &&
                stockSelect.IsBoolean() && stockSelect.GetBoolean()) {
                GFxValue parkedArgs[] = { GFxValue(std::uint32_t{ 1 }), GFxValue(true) };
                if (!body.Invoke("SetFocus", nullptr, parkedArgs, 2)) {
                    REX::WARN("BF/015j/NAV: failed to re-park native focus; activation blocked");
                    return;
                }
                ReleaseNativeHighlights(false);
                REX::WARN("BF/015j/NAV: native focus unexpectedly returned to WEIGHT; re-parked on BODY TYPE");
            }
            REX::INFO("BF/015h/NAV: Nude activated by keyCode={}", keyCode);
            QueueToggle();
            return;
        }

        // Return focus to the correct stock item, preserving native
        // FocusItemsA/FocusItem via public SetFocus rather than poking them.
        // DOWN always selects Body Type; UP selects the body editor.
        const std::uint32_t stockIndex = isDown ? 1u : 0u;
        GFxValue args[] = { GFxValue(stockIndex), GFxValue(true) };
        if (!body.Invoke("SetFocus", nullptr, args, 2)) {
            REX::WARN("BF/015h/NAV: BodyPage.SetFocus({}) failed", stockIndex);
            // Fail closed: don't leave stuck visual focus on Nude.
        }
        g_navFocus = false;
        g_hover = false;
        UpdateLabel();
        REX::INFO("BF/015h/NAV: exited Nude to {}", isDown ? "BODY TYPE" : "BODY EDITOR");
    }

    class UIEventHandler final : public RE::Scaleform::GFx::FunctionHandler
    {
    public:
        void Call(const Params& params) override
        {
            const auto id = reinterpret_cast<std::uintptr_t>(params.userData);
            if (!g_chargenOpen.load()) return;
            if (id == 1) {
                g_navFocus = false;
                REX::INFO("BF/015: Body-tab nude row CLICK");
                QueueToggle();
            } else if (id == 2) {
                // ActionScript can dispatch rollover when labels redraw even without
                // a physical pointer move. Keep controller focus until mouseMove.
                if (g_navFocus) {
                    REX::INFO("BF/015i/NAV: synthetic/stationary rollover did not steal controller focus");
                    ReleaseNativeHighlights(false);
                } else {
                    g_hover = true;
                    UpdateLabel();
                    ReleaseNativeHighlights(true);
                }
            } else if (id == 3) {
                if (!g_navFocus) {
                    g_hover = false;
                    UpdateLabel();
                }
                // Let stock rollover handlers select neighboring settings.
            } else if (id == 4) {
                ProcessNavigationKey(params);
            } else if (id == 5) {
                // Any real mouse movement cancels virtual controller focus.
                if (g_navFocus) {
                    g_navFocus = false;
                    UpdateLabel();
                    REX::INFO("BF/015h/NAV: pointer movement released controller focus");
                }
            }
        }
    };

    // Intentionally process-lifetime: Scaleform owns ref-counted references
    // while each Chargen movie is active, then releases them on closing.
    UIEventHandler* GetUIHandler()
    {
        static auto* handler = new UIEventHandler();
        return handler;
    }

    bool HookMouseEvent(RE::Scaleform::GFx::ASMovieRootBase* gfx,
                        GFxValue& label, const char* eventName, std::uintptr_t id)
    {
        GFxValue callback;
        gfx->CreateFunction(&callback, GetUIHandler(), reinterpret_cast<void*>(id));
        if (callback.IsUndefined()) return false;
        GFxValue args[] = { GFxValue(eventName), callback };
        return label.Invoke("addEventListener", nullptr, args, 2);
    }

    void TryInstallRow()
    {
        if (g_installed || !g_chargenOpen.load()) return;
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto movie = ui->GetMenuMovie(RE::BSFixedString{ "ChargenMenu" });
        if (!movie || !movie->asMovieRoot) return;
        auto* gfx = movie->asMovieRoot.get();
        GFxValue root, body;
        if (!ResolveUI(root, body)) return;

        // Borrow *renderable* embedded native font from BODY TYPE.
        GFxValue bodyType, labelHolder, nativeLabel;
        if (!body.GetMember("BodyTypeStepper_mc", &bodyType) || !bodyType.IsDisplayObject() ||
            !bodyType.GetMember("LabelText_mc", &labelHolder) || !labelHolder.IsDisplayObject() ||
            !labelHolder.GetMember("text_tf", &nativeLabel) || !nativeLabel.IsDisplayObject()) return;
        GFxValue format;
        if (!nativeLabel.Invoke("getTextFormat", &format) || !format.IsObject()) return;
        GFxValue font;
        if (!format.GetMember("font", &font) || !font.IsString()) return;

        GFxValue label;
        gfx->CreateObject(&label, "flash.text.TextField");
        if (!label.IsDisplayObject()) return;
        bool ok = true;
        // Top aligned with the native settings column, with space below the radial.
        ok &= label.SetMember("name", GFxValue("BareFrontierNudeLabel_tf"));
        ok &= label.SetMember("x", GFxValue(35.0));
        ok &= label.SetMember("y", GFxValue(294.0));
        ok &= label.SetMember("width", GFxValue(501.0));
        ok &= label.SetMember("height", GFxValue(30.0));
        ok &= label.SetMember("selectable", GFxValue(false));
        ok &= label.SetMember("mouseEnabled", GFxValue(true));
        ok &= label.SetMember("multiline", GFxValue(false));
        ok &= label.SetMember("wordWrap", GFxValue(false));
        ok &= label.SetMember("embedFonts", GFxValue(true));
        ok &= format.SetMember("size", GFxValue(22.0));
        ok &= format.SetMember("color", GFxValue(std::uint32_t{ 0x233F4C }));
        ok &= label.SetMember("defaultTextFormat", format);
        ok &= label.SetMember("text", GFxValue("NUDE: OFF"));
        GFxValue argsFormat[] = { format };
        ok &= label.Invoke("setTextFormat", nullptr, argsFormat, 1);
        // Native Scaleform MouseEvent API; no altered SWF files or keyboard hook.
        const bool click = HookMouseEvent(gfx, label, "click", 1);
        const bool over = HookMouseEvent(gfx, label, "rollOver", 2);
        const bool out = HookMouseEvent(gfx, label, "rollOut", 3);
        if (!ok || !click) {
            REX::ERROR("BF/015: Body row initialization failed (props={} click={} over={} out={})",
                ok, click, over, out);
            return;
        }
        GFxValue attached;
        GFxValue addArgs[] = { label };
        if (!body.Invoke("addChild", &attached, addArgs, 1)) {
            REX::ERROR("BF/015: BodyPage_mc.addChild failed");
            return;
        }
        // Stable reference on the parent, not just an anonymous child.
        body.SetMember("BareFrontierNudeLabel_tf", label);

        // Capture-phase listener runs before stock BodyPage bubble-phase
        // keyDown listener. Only stop keys when moving into/out of our row or
        // activating it. Mouse movement releases controller-only focus.
        GFxValue keyCallback, mouseCallback;
        gfx->CreateFunction(&keyCallback, GetUIHandler(), reinterpret_cast<void*>(4));
        gfx->CreateFunction(&mouseCallback, GetUIHandler(), reinterpret_cast<void*>(5));
        GFxValue keyArgs[] = { GFxValue("keyDown"), keyCallback, GFxValue(true), GFxValue(std::int32_t{ 1000 }), GFxValue(false) };
        GFxValue mouseArgs[] = { GFxValue("mouseMove"), mouseCallback };
        const bool keyListener = !keyCallback.IsUndefined() &&
            body.Invoke("addEventListener", nullptr, keyArgs, 5);
        const bool pointerListener = !mouseCallback.IsUndefined() &&
            body.Invoke("addEventListener", nullptr, mouseArgs, 2);
        REX::INFO("BF/015h/NAV: BodyPage capture keyDown={} pointerMove={} (vanilla and CharGenMenu)",
            keyListener, pointerListener);
        g_installed = true;
        REX::INFO("BF/015: CLICKABLE Body row inserted (font={}, click={}, rollover={}, rollout={})",
            font.GetString(), click, over, out);
        UpdateLabel();
    }

    void OnUITick()
    {
        if (!g_chargenOpen.load()) return;
        static auto last = Clock::now() - std::chrono::seconds(1);
        const auto now = Clock::now();
        // Stock ActionScript sometimes reasserts an old selection during
        // equip/unequip. Refresh focus styling promptly for hover AND controller.
        if (now - last < ((g_hover || g_navFocus) ? std::chrono::milliseconds(75) :
                                               std::chrono::milliseconds(250))) return;
        last = now;
        if (!g_installed) TryInstallRow();
        // Diagnostic: confirms the actual Body frame and reports the tab index
        // WITHOUT using the index for behavior. Logs once per menu opening.
        if (!g_loggedBodyFrame) {
            GFxValue root, body, main;
            if (ResolveUI(root, body, &main) && IsBodyPageActive(main)) {
                std::int32_t page = -1;
                const bool gotPage = ReadInt(main, "CurrentPage", page);
                REX::INFO("BF/015f: BodyPage frame detected; CurrentPage={} readable={} (index ignored)",
                    page, gotPage);
                g_loggedBodyFrame = true;
            }
        }
        if (g_hover || g_navFocus) {
            ReleaseNativeHighlights(false);
            GFxValue root, body, main;
            if (ResolveUI(root, body, &main)) {
                if (!IsBodyPageActive(main)) {
                    // Menu frame changes may temporarily interrupt focus while
                    // the preview is refreshed; log so we can distinguish from
                    // native EditMode or pointer focus transitions.
                    if (g_navFocus) {
                        std::int32_t page = -1;
                        ReadInt(main, "CurrentPage", page);
                        REX::INFO("BF/015i/NAV: Nude focus cleared because BodyPage frame inactive (page={})", page);
                    }
                    g_hover = false;
                    g_navFocus = false;
                    g_warnedNativeEdit = false;
                    UpdateLabel();
                } else if (g_navFocus) {
                    GFxValue editing;
                    const bool isEditing = body.Invoke("ShowBack", &editing) &&
                        editing.IsBoolean() && editing.GetBoolean();
                    if (isEditing && !g_warnedNativeEdit) {
                        REX::WARN("BF/015i/NAV: native Body editor is in edit mode while Nude has virtual focus; possible Accept double dispatch");
                        g_warnedNativeEdit = true;
                    } else if (!isEditing) {
                        g_warnedNativeEdit = false;
                    }
                }
            }
        }
        // Re-equip before the editor can be destroyed when the user leaves
        // Body. This supplements the supported MenuOpenCloseEvent fallback.
        if (g_nude) {
            GFxValue root, body, main;
            if (ResolveUI(root, body, &main) && !IsBodyPageActive(main)) {
                std::int32_t page = -1; // diagnostics only, never used to gate behavior
                const bool gotPage = ReadInt(main, "CurrentPage", page);
                REX::INFO("BF/015f: left BodyPage frame (page={}, readable={}); restoring preview clothing",
                    page, gotPage);
                RestorePreviewClothing("left-Body-tab");
                UpdateLabel();
            }
        }
        // Detect the editor generating a replacement preview actor.
        if (g_nude && g_changedActor && GetPreviewActor() != g_changedActor) {
            REX::WARN("BF/015: preview actor replaced; clearing old visual state");
            g_removed.clear();
            g_changedActor = nullptr;
            g_nude = false;
            UpdateLabel();
        }
    }

    class MenuEventSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        static MenuEventSink* GetSingleton()
        {
            static MenuEventSink value;
            return &value;
        }
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent& event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (event.menuName == "ChargenMenu") {
                if (event.opening) {
                    g_chargenOpen.store(true);
                    g_installed = false;
                    g_loggedBodyFrame = false;
                    g_hover = false;
                    g_navFocus = false;
                    g_warnedNativeEdit = false;
                    g_nude = false;
                    g_removed.clear();
                    g_changedActor = nullptr;
                    REX::INFO("BF/015: Chargen OPEN - automatic clickable row pending");
                } else {
                    // Supported menu lifecycle fallback, if the preview is
                    // still attached. ON -> OFF or leaving Body restores earlier.
                    if (g_nude) {
                        REX::INFO("BF/015: menu closing with nude state: restoring if preview is still available");
                        RestorePreviewClothing("menu-close fallback");
                    }
                    g_chargenOpen.store(false);
                    g_removed.clear();
                    g_changedActor = nullptr;
                    g_nude = false;
                    g_hover = false;
                    g_navFocus = false;
                    g_warnedNativeEdit = false;
                    g_installed = false;
                    REX::INFO("BF/015: Chargen CLOSED; state reset, player unchanged");
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void OnSFSEMessage(SFSE::MessagingInterface::Message* message) noexcept
    {
        if (!message || message->type != SFSE::MessagingInterface::kPostLoad) return;
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->RegisterSink(MenuEventSink::GetSingleton());
            // Do not register CharGen_CloseMenu: CommonLibSF maps its
            // GetEventSource relocation to ID 0, which raises REL/IDDB.cpp(459)
            // and aborts Starfield before character creation can run.
            REX::INFO("BF/015: registered menu lifecycle sink");
        } else {
            REX::ERROR("BF/015: UI singleton unavailable");
        }
        if (auto* tasks = SFSE::GetTaskInterface()) {
            tasks->AddPermanentTask([] { OnUITick(); });
            REX::INFO("BF/015: registered lightweight UI initialization tick");
        } else {
            REX::ERROR("BF/015: SFSE TaskInterface not available; cannot auto-insert row");
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* sfse)
{
    SFSE::Init(sfse);
    REX::INFO("Bare Frontier v0.1.5j loading - native Accept focus disarm and controller highlight stability");
    const auto* messaging = SFSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSFSEMessage)) {
        REX::ERROR("BF/015: SFSE messaging listener registration failed");
        return false;
    }
    REX::INFO("Bare Frontier v0.1.5j loaded");
    return true;
}
