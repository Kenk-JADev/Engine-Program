#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/BattleSystem.h" // XP-Kampfmenue (Battler/BattleAction)
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"
// ImGui-Editor ist entfernt. GameUI-Overlay war historisch ImGui-basiert;
// ohne RPGMAKER3D_ENABLE_IMGUI sind Draw()-Pfade No-Ops (RmlUi/Logic bleibt).
#ifdef RPGMAKER3D_ENABLE_IMGUI
#include <imgui.h>
#endif
#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include <cmath>
#include <cctype>
#include <memory>

namespace rpg {

// --- MessageWindow ---
void MessageWindow::Show(const std::string& text) {
    mText = text;
    mDisplayed.clear();
    mCharIndex = 0;
    mTimer = 0.0f;
    mVisible = true;
    mWaitingForInput = false;
    mChoices.clear();
    mSpeakerName.clear();
    mFaceName.clear();
    mPosition = 0;
}
void MessageWindow::Show(const std::string& text, const std::string& speaker, int position, const std::string& face) {
    Show(text);
    mSpeakerName = speaker;
    mPosition = position;
    mFaceName = face;
}
void MessageWindow::ShowWithChoices(const std::string& text, const std::vector<ChoiceOption>& choices) {
    Show(text);
    mChoices = choices;
    mSelectedChoice = 0;
}
void MessageWindow::AdvanceInput() {
    if (!mVisible) return;
    if (mCharIndex < mText.size()) {
        mDisplayed = mText;
        mCharIndex = mText.size();
        return;
    }
    if (mChoices.empty()) {
        mVisible = false;
    }
}

void MessageWindow::ConfirmChoice(int overrideIdx) {
    if (mChoices.empty()) return;
    // overrideIdx: -2 = aktuelle Auswahl, -1 = Abbruch, sonst direkter Index
    int idx = (overrideIdx == -2) ? mSelectedChoice : overrideIdx;
    auto cb = onChoice;
    mChoices.clear();
    mVisible = false;
    if (cb) cb(idx);
}

void MessageWindow::Update(float dt) {
    if (!mVisible) return;
    if (mCharIndex < mText.size()) {
        mTimer += dt;
        while (mTimer >= mCharDelay && mCharIndex < mText.size()) {
            mDisplayed += mText[mCharIndex++];
            mTimer -= mCharDelay;
        }
    } else {
        if (!mChoices.empty()) mWaitingForInput = true;
    }
}
void MessageWindow::Draw() {
    if (!mVisible) return;
#ifdef RPGMAKER3D_ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    float w = io.DisplaySize.x * 0.72f;
    float h = 160.0f;
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - w) * 0.5f, io.DisplaySize.y - h - 28.0f));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.92f));
    ImGui::Begin("##MessageBox", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("%s", mDisplayed.c_str());
    ImGui::Dummy(ImVec2(0, 8));
    if (mCharIndex >= mText.size()) {
        if (mChoices.empty()) {
            ImGui::TextDisabled("E / Enter / Space  -  weiter");
            if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::Button("OK", ImVec2(100, 0))) {
                mVisible = false;
            }
        } else {
            ImGui::Separator();
            for (size_t i=0;i<mChoices.size();++i) {
                if (ImGui::Selectable(mChoices[i].text.c_str(), (int)i==mSelectedChoice)) {
                    mSelectedChoice = (int)i;
                    if (onChoice) onChoice(mSelectedChoice);
                    mVisible = false;
                }
            }
        }
    } else {
        ImGui::TextDisabled("...");
        if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::Button("Skip")) {
            mDisplayed = mText;
            mCharIndex = mText.size();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
#else
    // Ohne ImGui: Fortschritt laeuft ueber AdvanceInput() (Engine-Input E/Enter/Space).
    (void)mDisplayed;
#endif
}

// --- TitleScreen (XP: Neues Spiel / Weiterspielen / Beenden) ---
void TitleScreen::Show() {
    mVisible = true;
    // "Weiterspielen" nur aktiv, wenn mindestens ein Slot belegt ist (XP)
    bool anySave = false;
    for (int slot = 1; slot <= 4; ++slot) {
        Game::SaveSlotInfo info;
        if (Game::Get().GetSaveSlotInfo(slot, info) && info.exists) {
            anySave = true;
            break;
        }
    }
    std::vector<MenuWindow::Entry> items = {
        {"Neues Spiel", true},
        {"Weiterspielen", anySave},
        {"Beenden", true}};
    const std::string titleText = Database::Get().System().gameTitle.empty()
        ? "RPG Maker 3D" : Database::Get().System().gameTitle;
    auto& menu = GameUI::Get().Menu();
    menu.Show(titleText, items, [this](int idx) {
        std::function<void()> cb;
        if (idx == 0) cb = onNewGame;
        else if (idx == 1) cb = onContinue;
        else cb = onExit;
        if (cb) cb();
    }, false); // kein Esc-Abbrechen auf dem Titel
}
void TitleScreen::Update(float dt) { (void)dt; }
void TitleScreen::Draw() {
    // Anzeige laeuft ueber RmlUi (#menu_box, zentriert) - kein ImGui noetig.
}

// --- PauseMenu (delegiert an das XP-Spielmenue) ---
void PauseMenu::Show() { GameUI::Get().OpenGameMenu(); }
void PauseMenu::Hide() { GameUI::Get().Menu().Hide(); }
bool PauseMenu::IsVisible() const { return GameUI::Get().Menu().IsVisible(); }
void PauseMenu::Draw() {
    // Anzeige laeuft ueber RmlUi (#menu_box) - kein ImGui-Pfad mehr noetig.
}

// --- MenuWindow ---
void MenuWindow::Show(const std::string& title, const std::vector<Entry>& items,
                      std::function<void(int)> onPickFn, bool cancelable) {
    mTitle = title;
    mItems = items;
    onPick = std::move(onPickFn);
    mCancelable = cancelable;
    mCursor = 0;
    // Cursor auf ersten aktivierten Eintrag setzen
    while (mCursor < (int)mItems.size() && !mItems[mCursor].enabled) ++mCursor;
    if (mCursor >= (int)mItems.size()) mCursor = 0;
    onCancel = nullptr;
    mVisible = true;
}

void MenuWindow::Hide() {
    mVisible = false;
    onPick = nullptr;
    onCancel = nullptr;
}

void MenuWindow::MoveCursor(int dir) {
    if (mItems.empty()) return;
    int next = mCursor;
    for (size_t guard = 0; guard < mItems.size(); ++guard) {
        next = (next + dir + (int)mItems.size()) % (int)mItems.size();
        if (mItems[next].enabled) { mCursor = next; return; }
    }
}

void MenuWindow::Confirm() {
    if (!mVisible || mItems.empty() || mCursor < 0 || mCursor >= (int)mItems.size()) return;
    if (!mItems[mCursor].enabled) return;
    auto cb = onPick;
    const int idx = mCursor;
    if (cb) cb(idx); // cb darf das Menue neu aufbauen (Show erneut aufrufen)
}

void MenuWindow::Cancel() {
    if (!mVisible || !mCancelable) return;
    auto cb = onCancel;
    if (cb) { cb(); return; }
    Hide();
}

// --- GameUI ---
GameUI& GameUI::Get() {
    static GameUI instance;
    return instance;
}
void GameUI::Update(float dt) {
    mMessage.Update(dt);
    mTitle.Update(dt);
    UpdateScreenTexts(dt);
    UpdatePictures(dt);
}
void GameUI::Draw() {
    if (mTitle.IsVisible()) mTitle.Draw();
    else if (mPause.IsVisible()) mPause.Draw();
    else if (mMessage.IsVisible()) mMessage.Draw();
    // Screen texts and pictures always on top (HUD)
    DrawPictures();
    DrawScreenTexts();
}
void GameUI::ShowMessage(const std::string& text) {
    mMessage.Show(text);
}
void GameUI::ShowMessage(const std::string& text, const std::string& speaker, int position, const std::string& face) {
    mMessage.Show(text, speaker, position, face);
}
void GameUI::ShowChoices(const std::string& text, const std::vector<std::string>& options, std::function<void(int)> callback, bool cancelAllowed) {
    std::vector<ChoiceOption> choices;
    for (size_t i=0;i<options.size();++i) choices.push_back({options[i], (int)i});
    mMessage.onChoice = callback;
    mChoiceCancelAllowed = cancelAllowed;
    mMessage.ShowWithChoices(text, choices);
}

// === Zahleneingabe (Event-Befehl 103) ===
void GameUI::ShowNumberInput(const std::string& prompt, int digits, int initial, std::function<void(int)> onDone) {
    mNumberActive = true;
    mNumberPrompt = prompt;
    mNumberDigits = digits > 0 && digits <= 8 ? digits : 4;
    mNumberCursor = mNumberDigits - 1; // rechteste Ziffer zuerst (wie XP)
    // Wert auf Ziffernzahl begrenzen
    int maxVal = 1;
    for (int i = 0; i < mNumberDigits; ++i) maxVal *= 10;
    mNumberValue = initial % maxVal;
    if (mNumberValue < 0) mNumberValue = 0;
    mNumberDone = std::move(onDone);
    RPG_LOG_INFO("[UI] Zahleneingabe aktiv (" + std::to_string(mNumberDigits) + " Stellen)");
}

// === Namenseingabe (Event-Befehl 303) ===
void GameUI::ShowNameInput(const std::string& prompt, const std::string& initial, int maxChars, std::function<void(const std::string&)> onDone) {
    mNameActive = true;
    mNamePrompt = prompt;
    mNameInitial = initial;
    mNameText = initial;
    mNameMaxChars = maxChars > 0 && maxChars <= 16 ? maxChars : 8;
    if ((int)mNameText.size() > mNameMaxChars) mNameText.resize(mNameMaxChars);
    mNameDone = std::move(onDone);
    RPG_LOG_INFO("[UI] Namenseingabe aktiv (max " + std::to_string(mNameMaxChars) + " Zeichen)");
}

// ============================================================================
// XP-Spielmenue (Esc) / Speicherbildschirm / Laden - alles ueber MenuWindow
// ============================================================================

namespace {
// --- Ausruestungs-Helfer --------------------------------------------------
// (Max-HP/MP/-Kurven sind inzwischen GameActor-Methoden in Game.cpp -
// dieselbe Formel nutzt auch das Kampfsystem.)
const char* kArmorSlotNames[4] = {"Schild", "Helm", "Körper", "Accessoire"};

const ArmorData* FindArmorDef(int id) {
    if (id <= 0) return nullptr;
    for (const auto& d : Database::Get().Armors())
        if (d.id == id) return &d;
    return nullptr;
}
const WeaponData* FindWeaponDef(int id) {
    if (id <= 0) return nullptr;
    for (const auto& w : Database::Get().Weapons())
        if (w.id == id) return &w;
    return nullptr;
}
} // namespace

void GameUI::OpenGameMenu() {
    // XP: "Menueaufruf verboten" respektieren
    if (!Game::Get().System().HasMenuAccess()) return;

    std::vector<MenuWindow::Entry> items;
    const bool hasMembers = !Game::Get().Party().Members().empty();
    items.push_back({"Gegenstände", true});
    items.push_back({"Fertigkeiten", hasMembers});
    items.push_back({"Ausrüstung", hasMembers});
    items.push_back({"Status", hasMembers});
    items.push_back({"Speichern", Game::Get().System().HasSaveAccess()});
    items.push_back({"Spiel beenden", true});
    items.push_back({"Zurück", true});

    mMenu.Show("Menü", items, [this](int idx) {
        switch (idx) {
            case 0: OpenItemsMenu(); break;
            case 1: OpenSkillsMenu(); break;
            case 2: OpenEquipMenu(); break;
            case 3: OpenStatusMenu(); break;
            case 4:
                // Nach dem Speichern/Abbruch wieder ins Menue (XP-Verhalten)
                ShowSaveScreen(true, [this]() { OpenGameMenu(); });
                break;
            case 5: {
                // XP „Spiel beenden": Zum Titelbildschirm / Verlassen / Abbrechen
                std::vector<MenuWindow::Entry> q = {
                    {"Zum Titelbildschirm", true},
                    {"Spiel verlassen", true},
                    {"Zurück", true}};
                auto& pause = mPause;
                mMenu.Show("Spiel beenden?", q,
                    [this, &pause](int a) {
                        if (a == 0) {
                            mMenu.Hide();
                            auto cb = pause.onExitToTitle;
                            if (cb) cb();
                        } else if (a == 1) {
                            mMenu.Hide();
                            auto cb = pause.onQuitGame ? pause.onQuitGame
                                                       : pause.onExitToTitle;
                            if (cb) cb();
                        } else {
                            OpenGameMenu();
                        }
                    });
                mMenu.onCancel = [this]() { OpenGameMenu(); };
                break;
            }
            default: mMenu.Hide(); break; // Zurück
        }
    });
    mMenu.onCancel = [this]() {
        mMenu.Hide();
        auto cb = mPause.onResume;
        if (cb) cb();
    };
}

void GameUI::OpenItemsMenu() {
    std::vector<MenuWindow::Entry> items;
    std::vector<int> itemIds; // Index -> Item-ID (fuer Beschreibung)
    auto& party = Game::Get().Party();

    // sortiert nach ID, damit die Liste stabil bleibt
    std::vector<std::pair<int,int>> bag(party.Items().begin(), party.Items().end());
    std::sort(bag.begin(), bag.end());
    for (const auto& kv : bag) {
        const auto* it = Database::Get().GetItem(kv.first);
        const std::string name = it ? it->name : ("Gegenstand #" + std::to_string(kv.first));
        items.push_back({name + "   x " + std::to_string(kv.second), true});
        itemIds.push_back(kv.first);
    }
    if (items.empty()) items.push_back({"(leer)", false});

    mMenu.Show("Gegenstände  (Gold: " + std::to_string(party.GetGold()) + " G)",
        items, [this, itemIds](int idx) {
            if (idx < 0 || idx >= (int)itemIds.size()) return;
            const auto* it = Database::Get().GetItem(itemIds[idx]);
            if (!it) return;
            // XP: Verbrauchsgueter mit Heilwirkung werden BENUTZT (Ziel waehlen),
            // alle anderen zeigen ihren Beschreibungstext.
            if (it->consumable && (it->hpRecovery > 0 || it->mpRecovery > 0)) {
                OpenItemTargetMenu(it->id);
            } else if (!it->description.empty()) {
                ShowMessage(it->description);
            }
        });
    mMenu.onCancel = [this]() { OpenGameMenu(); };
}

void GameUI::OpenItemTargetMenu(int itemId) {
    const auto* it = Database::Get().GetItem(itemId);
    if (!it) { OpenItemsMenu(); return; }

    std::vector<MenuWindow::Entry> items;
    auto& members = Game::Get().Party().Members();
    for (const auto& a : members) {
        // XP: Tote Mitglieder koennen nicht das Ziel von Heil-Items sein
        items.push_back({a.name + "   HP " + std::to_string(a.hp) +
                         " | MP " + std::to_string(a.mp), a.hp > 0});
    }
    if (items.empty()) items.push_back({"(kein Gruppenmitglied)", false});

    mMenu.Show(it->name + " benutzen: Ziel wählen", items, [this, itemId](int idx) {
        const auto* it2 = Database::Get().GetItem(itemId);
        auto& party = Game::Get().Party();
        if (it2 && idx >= 0 && idx < (int)party.Members().size()) {
            auto& a = party.Members()[(size_t)idx];
            // Heil-Obergrenzen aus den Datenbank-Werten (initialStats + Kurve)
            a.hp = std::min(a.hp + it2->hpRecovery, a.MaxHp());
            a.mp = std::min(a.mp + it2->mpRecovery, a.MaxMp());
            party.GainItem(itemId, -1);
            EventSystem_PlayAudio(Database::Get().System().decisionSe, 3, false);
            ShowMessage(a.name + " erholt sich:  +" + std::to_string(it2->hpRecovery) +
                        " HP, +" + std::to_string(it2->mpRecovery) + " MP");
        }
        OpenItemsMenu(); // zurueck zur Liste (Anzahl wird aktualisiert)
    });
    mMenu.onCancel = [this]() { OpenItemsMenu(); };
}

void GameUI::OpenStatusMenu() {
    std::vector<MenuWindow::Entry> items;
    const auto members = Game::Get().Party().Members(); // Snapshot
    for (const auto& a : members)
        items.push_back({a.name + "   Lv " + std::to_string(a.level), true});
    if (items.empty()) items.push_back({"(kein Gruppenmitglied)", false});

    mMenu.Show("Status", items, [this, members](int idx) {
        if (idx < 0 || idx >= (int)members.size()) return;
        const auto& a = members[(size_t)idx];
        ShowMessage(a.name + "  –  Level " + std::to_string(a.level) +
                    "\nHP " + std::to_string(a.hp) + " / " + std::to_string(a.MaxHp()) +
                    " | MP " + std::to_string(a.mp) + " / " + std::to_string(a.MaxMp()) +
                    "\nEXP " + std::to_string(a.exp));
    });
    mMenu.onCancel = [this]() { OpenGameMenu(); };
}

// ============================================================================
// Fertigkeiten (XP: Heil-Skills aus dem Menue benutzbar, kostet MP)
// ============================================================================
void GameUI::OpenSkillsMenu() {
    auto& party = Game::Get().Party();
    if (party.Members().empty()) {
        ShowMessage("Keine Gruppenmitglieder.");
        OpenGameMenu();
        return;
    }
    std::vector<MenuWindow::Entry> mem;
    for (const auto& a : party.Members())
        mem.push_back({a.name + "   MP " + std::to_string(a.mp) +
                       " / " + std::to_string(a.MaxMp()), true});
    mMenu.Show("Fertigkeiten: Mitglied wählen", mem,
        [this](int mi) { OpenSkillListMenu(mi); });
    mMenu.onCancel = [this]() { OpenGameMenu(); };
}

void GameUI::OpenSkillListMenu(int memberIndex) {
    auto& party = Game::Get().Party();
    if (memberIndex < 0 || memberIndex >= (int)party.Members().size()) {
        OpenSkillsMenu();
        return;
    }
    const auto& actor = party.Members()[(size_t)memberIndex];
    std::vector<MenuWindow::Entry> items;
    std::vector<int> skillIds;
    for (int sid : actor.skills) {
        const auto* sk = Database::Get().GetSkill(sid);
        const std::string name = sk ? sk->name : ("Fertigkeit #" + std::to_string(sid));
        const int cost = sk ? sk->mpCost : 0;
        // Aus dem Menue benutzbar: Heil-Skills (Scope auf Gruppe zielend)
        const bool heal = sk && sk->scope >= 3 && sk->power > 0;
        items.push_back({name + "   " + std::to_string(cost) + " MP",
                         heal && actor.mp >= cost});
        skillIds.push_back(sid);
    }
    if (items.empty()) items.push_back({"(keine Fertigkeiten)", false});

    mMenu.Show(actor.name + ": Fertigkeiten   (MP " + std::to_string(actor.mp) +
               " / " + std::to_string(actor.MaxMp()) + ")", items,
        [this, memberIndex, skillIds](int idx) {
            if (idx >= 0 && idx < (int)skillIds.size())
                OpenSkillTargetMenu(memberIndex, skillIds[(size_t)idx]);
        });
    mMenu.onCancel = [this]() { OpenSkillsMenu(); };
}

void GameUI::OpenSkillTargetMenu(int memberIndex, int skillId) {
    const auto* sk = Database::Get().GetSkill(skillId);
    if (!sk) { OpenSkillsMenu(); return; }
    std::vector<MenuWindow::Entry> items;
    auto& members = Game::Get().Party().Members();
    for (const auto& a : members)
        items.push_back({a.name + "   HP " + std::to_string(a.hp) +
                         " / " + std::to_string(a.MaxHp()), a.hp > 0});
    if (items.empty()) items.push_back({"(kein Gruppenmitglied)", false});

    mMenu.Show(sk->name + ": Ziel wählen", items,
        [this, memberIndex, skillId](int ti) {
            const auto* sk2 = Database::Get().GetSkill(skillId);
            auto& party = Game::Get().Party();
            if (sk2 && memberIndex >= 0 && memberIndex < (int)party.Members().size() &&
                ti >= 0 && ti < (int)party.Members().size()) {
                auto& caster = party.Members()[(size_t)memberIndex];
                auto& target = party.Members()[(size_t)ti];
                if (caster.mp >= sk2->mpCost) {
                    caster.mp -= sk2->mpCost;
                    target.hp = std::min(target.hp + sk2->power, target.MaxHp());
                    EventSystem_PlayAudio(Database::Get().System().decisionSe, 3, false);
                    ShowMessage(target.name + " erholt sich um " +
                                std::to_string(sk2->power) + " HP.  (-" +
                                std::to_string(sk2->mpCost) + " MP)");
                } else {
                    EventSystem_PlayAudio(Database::Get().System().buzzerSe, 3, false);
                }
            }
            OpenSkillListMenu(memberIndex);
        });
    mMenu.onCancel = [this, memberIndex]() { OpenSkillListMenu(memberIndex); };
}

// ============================================================================
// Ausruestung (XP: Waffe + Schild/Helm/Koerper/Accessoire wechseln)
// ============================================================================
void GameUI::OpenEquipMenu() {
    auto& party = Game::Get().Party();
    if (party.Members().empty()) {
        ShowMessage("Keine Gruppenmitglieder.");
        OpenGameMenu();
        return;
    }
    std::vector<MenuWindow::Entry> mem;
    for (const auto& a : party.Members()) mem.push_back({a.name, true});
    mMenu.Show("Ausrüstung: Mitglied wählen", mem,
        [this](int mi) { OpenEquipSlotMenu(mi, -1); });
    mMenu.onCancel = [this]() { OpenGameMenu(); };
}

void GameUI::OpenEquipSlotMenu(int memberIndex, int slotKind) {
    auto& party = Game::Get().Party();
    if (memberIndex < 0 || memberIndex >= (int)party.Members().size()) {
        OpenEquipMenu();
        return;
    }
    auto& actor = party.Members()[(size_t)memberIndex];

    if (slotKind == -1) {
        // Uebersicht: Waffe + Ruestungs-Slots (Schild/Helm/Koerper/Accessoire)
        std::vector<MenuWindow::Entry> items;
        const auto* w = FindWeaponDef(actor.weaponId);
        items.push_back({std::string("Waffe: ") + (w ? w->name : "—"), true});
        for (int t = 0; t < 4; ++t) {
            std::string nm = "—";
            for (int aid : actor.armors) {
                if (const auto* ad = FindArmorDef(aid);
                    ad && (int)ad->armorType == t) { nm = ad->name; break; }
            }
            items.push_back({std::string(kArmorSlotNames[t]) + ": " + nm, true});
        }
        mMenu.Show(actor.name + ": Ausrüstung", items, [this, memberIndex](int idx) {
            if (idx == 0) OpenEquipSlotMenu(memberIndex, -2); // Waffe
            else OpenEquipSlotMenu(memberIndex, idx - 1);      // 0..3 Ruestungstyp
        });
        mMenu.onCancel = [this]() { OpenEquipMenu(); };
        return;
    }

    // Slot-Auswahl: Inventar-Kandidaten + „(abnehmen)"
    const bool isWeapon = (slotKind == -2);
    int curId = 0;
    if (isWeapon) {
        curId = actor.weaponId;
    } else {
        for (int aid : actor.armors) {
            if (const auto* ad = FindArmorDef(aid);
                ad && (int)ad->armorType == slotKind) { curId = aid; break; }
        }
    }

    std::vector<MenuWindow::Entry> items;
    std::vector<int> cand; // Index -> Gegenstands-ID (0 = abnehmen)
    if (curId > 0) { items.push_back({"(abnehmen)", true}); cand.push_back(0); }

    if (isWeapon) {
        std::vector<std::pair<int,int>> bag(
            party.Weapons().begin(), party.Weapons().end());
        std::sort(bag.begin(), bag.end());
        for (const auto& kv : bag) {
            const auto* wd = FindWeaponDef(kv.first);
            if (!wd) continue;
            std::string label = wd->name + "   ATK " + std::to_string(wd->atk);
            if (kv.first == curId) { label += "   [angelegt]"; }
            items.push_back({label, kv.first != curId});
            cand.push_back(kv.first);
        }
        if (items.empty()) items.push_back({"(keine Waffen im Inventar)", false});
    } else {
        std::vector<std::pair<int,int>> bag(
            party.Armors().begin(), party.Armors().end());
        std::sort(bag.begin(), bag.end());
        for (const auto& kv : bag) {
            const auto* ad = FindArmorDef(kv.first);
            if (!ad || (int)ad->armorType != slotKind) continue;
            std::string label = ad->name + "   ABW " + std::to_string(ad->def) +
                                " / GABW " + std::to_string(ad->mdf);
            if (kv.first == curId) { label += "   [angelegt]"; }
            items.push_back({label, kv.first != curId});
            cand.push_back(kv.first);
        }
        if (items.empty())
            items.push_back({std::string("(") + kArmorSlotNames[slotKind] +
                             " im Inventar leer)", false});
    }

    const std::string what = isWeapon ? "Waffe" : kArmorSlotNames[slotKind];
    mMenu.Show(actor.name + ": " + what + " wählen", items,
        [this, memberIndex, slotKind, isWeapon, cand](int idx) {
            if (idx < 0 || idx >= (int)cand.size()) {
                OpenEquipSlotMenu(memberIndex, -1);
                return;
            }
            auto& party2 = Game::Get().Party();
            auto& a = party2.Members()[(size_t)memberIndex];
            const int newId = cand[(size_t)idx];
            if (isWeapon) {
                if (a.weaponId > 0) party2.GainWeapon(a.weaponId, +1);
                if (newId > 0) party2.GainWeapon(newId, -1);
                a.weaponId = newId;
            } else {
                // vorhandene Ruestung dieses Typs ablegen
                for (size_t i = 0; i < a.armors.size(); ++i) {
                    if (const auto* ad = FindArmorDef(a.armors[i]);
                        ad && (int)ad->armorType == slotKind) {
                        party2.GainArmor(a.armors[i], +1);
                        a.armors.erase(a.armors.begin() + (ptrdiff_t)i);
                        break;
                    }
                }
                if (newId > 0) {
                    party2.GainArmor(newId, -1);
                    a.armors.push_back(newId);
                }
            }
            EventSystem_PlayAudio(Database::Get().System().equipSe, 3, false);
            OpenEquipSlotMenu(memberIndex, -1);
        });
    mMenu.onCancel = [this, memberIndex]() { OpenEquipSlotMenu(memberIndex, -1); };
}

void GameUI::ShowSaveScreen(bool saveMode, std::function<void()> onClosed) {
    // XP: "Speichern verboten" (Event-Befehl 134) respektieren
    if (saveMode && !Game::Get().System().HasSaveAccess()) {
        ShowMessage("Speichern ist zur Zeit nicht möglich.");
        if (onClosed) onClosed();
        return;
    }
    // Abschluss genau einmal melden (Pick ODER Abbruch)
    auto closed = std::make_shared<std::function<void()>>(std::move(onClosed));
    auto finish = [this, closed]() {
        mMenu.Hide();
        if (*closed) {
            auto cb = std::move(*closed);
            *closed = nullptr;
            cb();
        }
    };

    std::vector<MenuWindow::Entry> items;
    for (int slot = 1; slot <= 4; ++slot) {
        Game::SaveSlotInfo info;
        Game::Get().GetSaveSlotInfo(slot, info);
        std::string line = "Datei " + std::to_string(slot) + ":  ";
        if (info.exists) {
            line += info.mapName;
            if (!info.actorName.empty())
                line += "  –  " + info.actorName + " Lv " + std::to_string(info.actorLevel);
            line += "   (" + std::to_string(info.gold) + " G, " +
                    std::to_string(info.saveCount) + "x gespeichert)";
        } else {
            line += "— leer —";
        }
        items.push_back({line, saveMode || info.exists});
    }

    mMenu.Show(saveMode ? "Spielstand speichern" : "Spielstand laden", items,
        [this, saveMode, finish](int idx) {
            const int slot = idx + 1;
            const bool ok = saveMode ? Game::Get().Save(slot) : Game::Get().Load(slot);
            const auto& sys = Database::Get().System();
            EventSystem_PlayAudio(ok ? (saveMode ? sys.saveSe : sys.loadSe)
                                     : sys.buzzerSe, 3, false);
            if (!ok)
                ShowMessage(saveMode ? "Speichern fehlgeschlagen." : "Laden fehlgeschlagen.");
            finish(); // XP: Bildschirm schliesst nach der Aktion
        });
    mMenu.onCancel = finish;
}

void GameUI::ShowShop(const std::vector<int>& itemIds, std::function<void()> onClosed) {
    // Kompatibilitaets-Variante: nur Items -> ShopGood{Item, id}
    std::vector<ShopGood> goods;
    goods.reserve(itemIds.size());
    for (int id : itemIds) goods.push_back({ShopGood::Kind::Item, id});
    ShowShopGoods(goods, std::move(onClosed));
}

void GameUI::ShowShopGoods(const std::vector<ShopGood>& goods, std::function<void()> onClosed) {
    mShopGoods = goods;
    mShopOnClosed = std::move(onClosed);
    mShopActive = true;

    auto closeShop = [this]() {
        mShopActive = false;
        mMenu.Hide();
        auto cb = std::move(mShopOnClosed);
        mShopOnClosed = nullptr;
        if (cb) cb();
    };

    // Ware aufloesen: Name/Preis je nach Art (Item/Waffe/Ruestung)
    auto goodInfo = [](const ShopGood& g, std::string& name, int& price) -> bool {
        switch (g.kind) {
            case ShopGood::Kind::Item:
                if (const auto* d = Database::Get().GetItem(g.id)) {
                    name = d->name; price = d->price; return true;
                }
                break;
            case ShopGood::Kind::Weapon:
                if (const auto* d = FindWeaponDef(g.id)) {
                    name = "[Waffe] " + d->name; price = d->price; return true;
                }
                break;
            case ShopGood::Kind::Armor:
                if (const auto* d = FindArmorDef(g.id)) {
                    name = "[Rüstung] " + d->name; price = d->price; return true;
                }
                break;
        }
        name = "Ware #" + std::to_string(g.id); price = 0;
        return false;
    };

    // 3 Phasen (Hauptauswahl / Kaufen / Verkaufen) als shared-Functions,
    // damit sie sich gegenseitig und selbst wieder aufrufen koennen.
    auto phaseMain = std::make_shared<std::function<void()>>();
    auto phaseBuy = std::make_shared<std::function<void()>>();
    auto phaseSell = std::make_shared<std::function<void()>>();

    *phaseMain = [this, closeShop, phaseBuy, phaseSell]() {
        std::vector<MenuWindow::Entry> items = {
            {"Kaufen", !mShopGoods.empty()},
            {"Verkaufen", true},
            {"Abbrechen", true}};
        mMenu.Show("Laden   (Gold: " + std::to_string(Game::Get().Party().GetGold()) + " G)",
            items, [closeShop, phaseBuy, phaseSell](int idx) {
                if (idx == 0) (*phaseBuy)();
                else if (idx == 1) (*phaseSell)();
                else closeShop();
            });
        mMenu.onCancel = closeShop;
    };

    *phaseBuy = [this, phaseMain, phaseBuy, goodInfo]() {
        std::vector<MenuWindow::Entry> items;
        for (const auto& g : mShopGoods) {
            std::string name; int price = 0;
            const bool ok = goodInfo(g, name, price);
            items.push_back({name + "   –   " + std::to_string(price) + " G", ok});
        }
        if (items.empty()) items.push_back({"(leer)", false});
        mMenu.Show("Kaufen   (Gold: " + std::to_string(Game::Get().Party().GetGold()) + " G)",
            items, [this, phaseBuy, goodInfo](int idx) {
                if (idx < 0 || idx >= (int)mShopGoods.size()) return;
                const ShopGood& g = mShopGoods[(size_t)idx];
                std::string name; int price = 0;
                if (!goodInfo(g, name, price)) return;
                auto& party = Game::Get().Party();
                const auto& sys = Database::Get().System();
                if (party.GetGold() >= price) {
                    party.GainGold(-price);
                    switch (g.kind) {
                        case ShopGood::Kind::Item:   party.GainItem(g.id, 1); break;
                        case ShopGood::Kind::Weapon: party.GainWeapon(g.id, 1); break;
                        case ShopGood::Kind::Armor:  party.GainArmor(g.id, 1); break;
                    }
                    EventSystem_PlayAudio(sys.shopSe, 3, false);
                } else {
                    EventSystem_PlayAudio(sys.buzzerSe, 3, false);
                }
                (*phaseBuy)(); // Neuaufbau: Gold-Anzeige aktualisieren
            });
        mMenu.onCancel = [phaseMain]() { (*phaseMain)(); };
    };

    *phaseSell = [this, phaseMain, phaseSell, goodInfo]() {
        // Alles Verkaeufliche: Items + Waffen + Ruestungen (mit Anzahl)
        struct SellEntry { ShopGood good; int count; };
        std::vector<SellEntry> bag;
        for (const auto& kv : Game::Get().Party().Items())
            if (kv.second > 0) bag.push_back({{ShopGood::Kind::Item, kv.first}, kv.second});
        for (const auto& kv : Game::Get().Party().Weapons())
            if (kv.second > 0) bag.push_back({{ShopGood::Kind::Weapon, kv.first}, kv.second});
        for (const auto& kv : Game::Get().Party().Armors())
            if (kv.second > 0) bag.push_back({{ShopGood::Kind::Armor, kv.first}, kv.second});
        std::sort(bag.begin(), bag.end(), [](const SellEntry& a, const SellEntry& b) {
            if (a.good.kind != b.good.kind) return a.good.kind < b.good.kind;
            return a.good.id < b.good.id;
        });
        std::vector<MenuWindow::Entry> items;
        for (const auto& e : bag) {
            std::string name; int price = 0;
            const bool ok = goodInfo(e.good, name, price);
            const int sell = price / 2; // XP: Verkauf = halber Preis
            items.push_back({name + " x " + std::to_string(e.count) +
                "   –   " + std::to_string(sell) + " G", ok && sell > 0});
        }
        if (items.empty()) items.push_back({"(leer)", false});
        mMenu.Show("Verkaufen   (Gold: " + std::to_string(Game::Get().Party().GetGold()) + " G)",
            items, [this, bag, phaseSell, goodInfo](int idx) {
                if (idx < 0 || idx >= (int)bag.size()) return;
                const ShopGood& g = bag[(size_t)idx].good;
                std::string name; int price = 0;
                if (!goodInfo(g, name, price) || price <= 0) return;
                auto& party = Game::Get().Party();
                switch (g.kind) {
                    case ShopGood::Kind::Item:   party.GainItem(g.id, -1); break;
                    case ShopGood::Kind::Weapon: party.GainWeapon(g.id, -1); break;
                    case ShopGood::Kind::Armor:  party.GainArmor(g.id, -1); break;
                }
                party.GainGold(price / 2);
                EventSystem_PlayAudio(Database::Get().System().shopSe, 3, false);
                (*phaseSell)(); // Neuaufbau (Anzahl/Gold)
            });
        mMenu.onCancel = [phaseMain]() { (*phaseMain)(); };
    };

    (*phaseMain)();
}

// ============================================================================
// XP-Kampfmenue: Aktionswahl ueber MenuWindow (ersetzt die Zifferntasten 1-4)
// Ablauf: Befehle -> (Skill/Item-Liste) -> Zielwahl. Esc geht einen Schritt
// zurueck; das Befehlsmenue selbst ist - wie in XP - nicht abbrechbar.
// ============================================================================
bool GameUI::IsBattleMenuOpen() const { return mMenu.IsVisible(); }

void GameUI::ConfirmBattleAction(int actorIndex, BattleActionType type, int id,
                                 int targetIndex, bool targetIsActor) {
    BattleAction act;
    act.type = type;
    act.subjectIndex = actorIndex;
    act.skillId = (type == BattleActionType::Skill) ? id : 0;
    act.itemId = (type == BattleActionType::Item) ? id : 0;
    act.targetIndex = targetIndex;
    act.targetIsActor = targetIsActor;
    mMenu.onCancel = nullptr; // ab jetzt ist die Aktion entschieden
    mMenu.Hide();
    BattleSystem::Get().SetAction(act);
}

void GameUI::OpenBattleCommands() {
    auto& bs = BattleSystem::Get();
    if (bs.Actors().empty()) return;
    int ai = bs.GetInputActorIndex();
    if (ai < 0 || ai >= (int)bs.Actors().size()) ai = 0;
    const Battler& actor = bs.Actors()[(size_t)ai];

    // Fertigkeiten des zugehoerigen Party-Mitglieds (per actorId abgesichert)
    bool hasSkills = false;
    for (const auto& m : Game::Get().Party().Members())
        if (m.actorId == actor.id) { hasSkills = !m.skills.empty(); break; }
    // Im Kampf benutzbare Gegenstaende (Heil- oder Schadens-Items)?
    bool hasItems = false;
    for (const auto& kv : Game::Get().Party().Items()) {
        if (kv.second <= 0) continue;
        if (const auto* it = Database::Get().GetItem(kv.first))
            if (it->hpRecovery != 0 || it->mpRecovery > 0) { hasItems = true; break; }
    }

    std::vector<MenuWindow::Entry> items;
    items.push_back({"Angriff", true});
    items.push_back({"Fertigkeit", hasSkills});
    items.push_back({"Gegenstand", hasItems});
    items.push_back({"Verteidigen", true});
    items.push_back({"Flucht", bs.CanEscape()});

    const std::string title = actor.name +
        "   HP " + std::to_string(actor.hp) + " / " + std::to_string(actor.maxHp) +
        "   MP " + std::to_string(actor.mp) + " / " + std::to_string(actor.maxMp);
    mMenu.Show(title, items, [this, ai](int idx) {
        switch (idx) {
            case 0: { // Angriff -> Ziel (bei nur einem Gegner direkt)
                int alive = 0, last = 0;
                auto& bs2 = BattleSystem::Get();
                for (size_t i = 0; i < bs2.Enemies().size(); ++i)
                    if (!bs2.Enemies()[i].isDead) { ++alive; last = (int)i; }
                if (alive <= 1)
                    ConfirmBattleAction(ai, BattleActionType::Attack, 0, last, false);
                else
                    OpenBattleTargetMenu(ai, 0, 0);
                break;
            }
            case 1: OpenBattleSkillMenu(ai); break;
            case 2: OpenBattleItemMenu(ai); break;
            case 3: ConfirmBattleAction(ai, BattleActionType::Guard, 0, 0, false); break;
            case 4: ConfirmBattleAction(ai, BattleActionType::Escape, 0, 0, false); break;
            default: break;
        }
    }, false); // nicht abbrechbar - in XP waehlt jeder Kaempfer zwingend
}

void GameUI::OpenBattleSkillMenu(int actorIndex) {
    auto& bs = BattleSystem::Get();
    if (actorIndex < 0 || actorIndex >= (int)bs.Actors().size()) {
        OpenBattleCommands();
        return;
    }
    const Battler& battler = bs.Actors()[(size_t)actorIndex];
    const GameActor* member = nullptr;
    for (const auto& m : Game::Get().Party().Members())
        if (m.actorId == battler.id) { member = &m; break; }

    std::vector<MenuWindow::Entry> items;
    std::vector<int> skillIds;
    if (member) {
        for (int sid : member->skills) {
            const auto* sk = Database::Get().GetSkill(sid);
            const std::string name = sk ? sk->name : ("Fertigkeit #" + std::to_string(sid));
            const int cost = sk ? sk->mpCost : 0;
            items.push_back({name + "   " + std::to_string(cost) + " MP",
                             battler.mp >= cost});
            skillIds.push_back(sid);
        }
    }
    if (items.empty()) items.push_back({"(keine Fertigkeiten)", false});

    mMenu.Show(battler.name + ": Welche Fertigkeit?", items,
        [this, actorIndex, skillIds](int idx) {
            if (idx < 0 || idx >= (int)skillIds.size()) return;
            const int sid = skillIds[(size_t)idx];
            const auto* sk = Database::Get().GetSkill(sid);
            const bool allyScope = sk && sk->scope >= 3; // XP: zielt auf eigene Seite
            if (allyScope) {
                OpenBattleAllyMenu(actorIndex, 1, sid);
            } else {
                int alive = 0, last = 0;
                auto& bs2 = BattleSystem::Get();
                for (size_t i = 0; i < bs2.Enemies().size(); ++i)
                    if (!bs2.Enemies()[i].isDead) { ++alive; last = (int)i; }
                if (alive <= 1)
                    ConfirmBattleAction(actorIndex, BattleActionType::Skill, sid, last, false);
                else
                    OpenBattleTargetMenu(actorIndex, 1, sid);
            }
        });
    mMenu.onCancel = [this]() { OpenBattleCommands(); };
}

void GameUI::OpenBattleItemMenu(int actorIndex) {
    // Im Kampf benutzbar: Heil-Items (HP/MP) und Schadens-Items (Bombe etc.)
    std::vector<MenuWindow::Entry> items;
    std::vector<int> itemIds;
    for (const auto& kv : Game::Get().Party().Items()) {
        if (kv.second <= 0) continue;
        const auto* it = Database::Get().GetItem(kv.first);
        if (!it) continue;
        const bool usable = (it->hpRecovery != 0 || it->mpRecovery > 0);
        items.push_back({it->name + "   x" + std::to_string(kv.second), usable});
        itemIds.push_back(kv.first);
    }
    if (items.empty()) items.push_back({"(keine Gegenstände)", false});

    mMenu.Show("Welchen Gegenstand?", items, [this, actorIndex, itemIds](int idx) {
        if (idx < 0 || idx >= (int)itemIds.size()) return;
        const int iid = itemIds[(size_t)idx];
        const auto* it = Database::Get().GetItem(iid);
        if (it && it->hpRecovery < 0) {
            // Schadens-Item -> Gegner waehlen
            int alive = 0, last = 0;
            auto& bs2 = BattleSystem::Get();
            for (size_t i = 0; i < bs2.Enemies().size(); ++i)
                if (!bs2.Enemies()[i].isDead) { ++alive; last = (int)i; }
            if (alive <= 1)
                ConfirmBattleAction(actorIndex, BattleActionType::Item, iid, last, false);
            else
                OpenBattleTargetMenu(actorIndex, 2, iid);
        } else {
            // Heil-Item -> Verbuendeten waehlen (direkt, wenn nur einer lebt)
            int alive = 0, last = 0;
            auto& bs2 = BattleSystem::Get();
            for (size_t i = 0; i < bs2.Actors().size(); ++i)
                if (!bs2.Actors()[i].isDead) { ++alive; last = (int)i; }
            if (alive <= 1)
                ConfirmBattleAction(actorIndex, BattleActionType::Item, iid, last, true);
            else
                OpenBattleAllyMenu(actorIndex, 2, iid);
        }
    });
    mMenu.onCancel = [this]() { OpenBattleCommands(); };
}

void GameUI::OpenBattleTargetMenu(int actorIndex, int mode, int id) {
    // Gegner-Zielwahl (mode: 0=Angriff, 1=Skill, 2=Item)
    auto& bs = BattleSystem::Get();
    std::vector<MenuWindow::Entry> items;
    std::vector<int> targets;
    for (size_t i = 0; i < bs.Enemies().size(); ++i) {
        const auto& e = bs.Enemies()[i];
        items.push_back({e.name + "   HP " + std::to_string(e.hp) + " / " +
                         std::to_string(e.maxHp), !e.isDead});
        targets.push_back((int)i);
    }
    if (items.empty()) items.push_back({"(keine Gegner)", false});

    mMenu.Show("Welchen Gegner?", items, [this, actorIndex, mode, id, targets](int idx) {
        if (idx < 0 || idx >= (int)targets.size()) return;
        const BattleActionType t = (mode == 1) ? BattleActionType::Skill
                                 : (mode == 2) ? BattleActionType::Item
                                               : BattleActionType::Attack;
        ConfirmBattleAction(actorIndex, t, id, targets[(size_t)idx], false);
    });
    mMenu.onCancel = [this, actorIndex, mode]() {
        if (mode == 1) OpenBattleSkillMenu(actorIndex);
        else if (mode == 2) OpenBattleItemMenu(actorIndex);
        else OpenBattleCommands();
    };
}

void GameUI::OpenBattleAllyMenu(int actorIndex, int mode, int id) {
    // Verbuendeten-Zielwahl fuer Heilungen (mode: 1=Skill, 2=Item)
    auto& bs = BattleSystem::Get();
    std::vector<MenuWindow::Entry> items;
    std::vector<int> targets;
    for (size_t i = 0; i < bs.Actors().size(); ++i) {
        const auto& a = bs.Actors()[i];
        items.push_back({a.name + "   HP " + std::to_string(a.hp) + " / " +
                         std::to_string(a.maxHp), !a.isDead});
        targets.push_back((int)i);
    }
    if (items.empty()) items.push_back({"(keine Mitglieder)", false});

    mMenu.Show("Auf wen?", items, [this, actorIndex, mode, id, targets](int idx) {
        if (idx < 0 || idx >= (int)targets.size()) return;
        const BattleActionType t = (mode == 1) ? BattleActionType::Skill
                                               : BattleActionType::Item;
        ConfirmBattleAction(actorIndex, t, id, targets[(size_t)idx], true);
    });
    mMenu.onCancel = [this, actorIndex, mode]() {
        if (mode == 1) OpenBattleSkillMenu(actorIndex);
        else OpenBattleItemMenu(actorIndex);
    };
}

void GameUI::UpdateModalInput(Input& input) {
    // --- Menue (Spielmenue/Speicherbildschirm/Laden) hat oberste Prioritaet ---
    if (mMenu.IsVisible()) {
        if (input.IsKeyPressed(Key::Up) || input.IsKeyPressed(Key::W)) mMenu.MoveCursor(-1);
        if (input.IsKeyPressed(Key::Down) || input.IsKeyPressed(Key::S)) mMenu.MoveCursor(+1);
        if (input.IsKeyPressed(Key::Enter) || input.IsKeyPressed(Key::E) || input.IsKeyPressed(Key::Space))
            mMenu.Confirm();
        if (input.IsKeyPressed(Key::Escape)) mMenu.Cancel();
        return;
    }

    // --- Choices haben oberste Prioritaet ---
    if (mMessage.IsVisible() && mMessage.HasChoices() && mMessage.IsTextComplete()) {
        const int count = mMessage.GetChoiceCount();
        if (count > 0) {
            int sel = mMessage.GetSelectedChoice();
            if (input.IsKeyPressed(Key::Up) || input.IsKeyPressed(Key::W))
                mMessage.SetSelectedChoice((sel + count - 1) % count);
            if (input.IsKeyPressed(Key::Down) || input.IsKeyPressed(Key::S))
                mMessage.SetSelectedChoice((sel + 1) % count);
            if (input.IsKeyPressed(Key::Enter) || input.IsKeyPressed(Key::E) || input.IsKeyPressed(Key::Space))
                mMessage.ConfirmChoice(); // aktuelle Auswahl
            if (input.IsKeyPressed(Key::Escape) && mChoiceCancelAllowed)
                mMessage.ConfirmChoice(-1); // Abbruch (nur wenn erlaubt, XP)
        }
        return;
    }

    // --- Zahleneingabe ---
    if (mNumberActive) {
        auto digitAt = [&](int pos) {
            int div = 1;
            for (int i = 0; i < pos; ++i) div *= 10;
            return (mNumberValue / div) % 10;
        };
        auto setDigit = [&](int pos, int d) {
            int div = 1;
            for (int i = 0; i < pos; ++i) div *= 10;
            mNumberValue += (d - digitAt(pos)) * div;
        };
        if (input.IsKeyPressed(Key::Left))
            mNumberCursor = (mNumberCursor + 1) % mNumberDigits; // XP: Cursor wandert in Zehner-Richtung
        if (input.IsKeyPressed(Key::Right))
            mNumberCursor = (mNumberCursor + mNumberDigits - 1) % mNumberDigits;
        if (input.IsKeyPressed(Key::Up))
            setDigit(mNumberCursor, (digitAt(mNumberCursor) + 1) % 10);
        if (input.IsKeyPressed(Key::Down))
            setDigit(mNumberCursor, (digitAt(mNumberCursor) + 9) % 10);
        // Direkte Zifferneingabe 0-9
        for (int k = 0; k <= 9; ++k) {
            Key key = static_cast<Key>(static_cast<int>(Key::Num0) + k);
            if (input.IsKeyPressed(key)) {
                setDigit(mNumberCursor, k);
                mNumberCursor = (mNumberCursor + mNumberDigits - 1) % mNumberDigits; // weiter nach links->rechts
            }
        }
        if (input.IsKeyPressed(Key::Enter) || input.IsKeyPressed(Key::E) || input.IsKeyPressed(Key::Space)) {
            mNumberActive = false;
            auto cb = std::move(mNumberDone);
            if (cb) cb(mNumberValue);
        }
        return;
    }

    // --- Namenseingabe ---
    if (mNameActive) {
        const bool shift = input.IsKeyDown(Key::LShift);
        // Buchstaben A-Z (inkl. deutscher Umlaute ueber Compose ist nicht moeglich;
        // Umlaute sind per Alt+U/O/A vorgesehen)
        for (int k = 0; k < 26; ++k) {
            Key key = static_cast<Key>(static_cast<int>(Key::A) + k);
            if (input.IsKeyPressed(key) && (int)mNameText.size() < mNameMaxChars) {
                char c = (char)('A' + k);
                if (!shift) c = (char)std::tolower(c);
                mNameText.push_back(c);
            }
        }
        // Ziffern
        for (int k = 0; k <= 9; ++k) {
            Key key = static_cast<Key>(static_cast<int>(Key::Num0) + k);
            if (input.IsKeyPressed(key) && (int)mNameText.size() < mNameMaxChars)
                mNameText.push_back((char)('0' + k));
        }
        // Umlaute (Alt+U = ue etc.): UTF-8 zwei Bytes
        if (input.IsKeyDown(Key::LAlt) && (int)mNameText.size() + 1 < mNameMaxChars) {
            if (input.IsKeyPressed(Key::A)) mNameText += "\xC3\x84"; // Ae
            if (input.IsKeyPressed(Key::O)) mNameText += "\xC3\x96"; // Oe
            if (input.IsKeyPressed(Key::U)) mNameText += "\xC3\x9C"; // Ue
        }
        if (input.IsKeyPressed(Key::Space) && (int)mNameText.size() < mNameMaxChars)
            mNameText.push_back(' ');
        if (input.IsKeyPressed(Key::Backspace)) {
            if (!mNameText.empty()) {
                // UTF-8-sicher: ggf. Fortsetzungsbytes (0x80..0xBF) mitentfernen
                do { mNameText.pop_back(); }
                while (!mNameText.empty() && ((unsigned char)mNameText.back() & 0xC0) == 0x80);
            }
        }
        if (input.IsKeyPressed(Key::Enter)) {
            mNameActive = false;
            if (mNameText.empty()) mNameText = mNameInitial;
            auto cb = std::move(mNameDone);
            if (cb) cb(mNameText);
        }
        if (input.IsKeyPressed(Key::Escape)) {
            mNameActive = false;
            auto cb = std::move(mNameDone);
            if (cb) cb(mNameInitial); // Abbruch = Name bleibt
        }
        return;
    }
}

void GameUI::DrawPlayHud(bool playtest) {
    if (mTitle.IsVisible() || mPause.IsVisible()) return;
#ifdef RPGMAKER3D_ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12, 12));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##PlayHUD", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
    if (playtest) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "PLAYTEST");
        ImGui::SameLine();
        ImGui::TextDisabled("F5 Stop");
    }
    auto& party = Game::Get().Party();
    int hp = 0, maxhp = 0;
    if (!party.Members().empty()) {
        hp = party.Members()[0].hp;
        maxhp = std::max(hp, 100);
    }
    ImGui::Text("HP %d  |  Gold %d", hp, party.GetGold());
    Vec3 p = Game::Get().Player().GetPosition();
    ImGui::Text("Pos %.1f, %.1f", p.x, p.z);
    if (!EventSystem::Get().IsAnyEventRunning()) {
        ImGui::TextDisabled("E: Sprechen  |  WASD: Bewegen");
    } else if (EventSystem::Get().IsWaitingForMessage()) {
        ImGui::TextColored(ImVec4(1,0.9f,0.4f,1), "Dialog...");
    }
    ImGui::End();
    (void)io;
    (void)maxhp;
#else
    (void)playtest;
#endif
}

// === Screen Text System ===
int GameUI::AddScreenText(const std::string& text, Vec2 screenPos, Color color, float duration, bool centered, float scale) {
    ScreenText st;
    st.id = mNextScreenTextId++;
    st.text = text;
    st.screenPos = screenPos;
    st.color = color;
    st.duration = duration;
    st.elapsed = 0.0f;
    st.centered = centered;
    st.fontScale = scale;
    st.worldSpace = false;
    st.withBackground = false;
    mScreenTexts.push_back(st);
    return st.id;
}

int GameUI::AddWorldText(const std::string& text, Vec3 worldPos, Color color, float duration, float scale) {
    ScreenText st;
    st.id = mNextScreenTextId++;
    st.text = text;
    st.worldPos = worldPos;
    st.color = color;
    st.duration = duration;
    st.elapsed = 0.0f;
    st.worldSpace = true;
    st.centered = true;
    st.fontScale = scale;
    st.withBackground = true;
    st.bgColor = Color(0.0f, 0.0f, 0.0f, 0.55f);
    mScreenTexts.push_back(st);
    return st.id;
}

void GameUI::RemoveScreenText(int id) {
    mScreenTexts.erase(std::remove_if(mScreenTexts.begin(), mScreenTexts.end(),
        [id](const ScreenText& s){ return s.id == id; }), mScreenTexts.end());
}

void GameUI::SetScreenText(int id, const std::string& text) {
    for (auto& t : mScreenTexts)
        if (t.id == id) { t.text = text; return; }
}

void GameUI::ClearScreenTexts() {
    mScreenTexts.clear();
}

void GameUI::UpdateScreenTexts(float dt) {
    for (auto& st : mScreenTexts) {
        st.elapsed += dt;
        // Floating up for world texts
        if (st.worldSpace) {
            st.worldPos.y += dt * 0.5f;
        }
        // Tween handling for screen texts
        if (st.isTweening) {
            st.tweenElapsed += dt;
            float t = st.tweenDuration > 0.001f ? (st.tweenElapsed / st.tweenDuration) : 1.0f;
            if (t >= 1.0f) {
                t = 1.0f;
                st.isTweening = false;
                st.screenPos = st.tweenTargetPos;
                if (st.tweenScale) st.fontScale = st.tweenTargetScale;
            } else {
                float eased = ApplyEasing(t, st.easingType);
                st.screenPos.x = st.tweenStartPos.x + (st.tweenTargetPos.x - st.tweenStartPos.x) * eased;
                st.screenPos.y = st.tweenStartPos.y + (st.tweenTargetPos.y - st.tweenStartPos.y) * eased;
                if (st.tweenScale) {
                    st.fontScale = st.tweenStartScale + (st.tweenTargetScale - st.tweenStartScale) * eased;
                }
            }
        }
    }
    // Remove expired
    mScreenTexts.erase(std::remove_if(mScreenTexts.begin(), mScreenTexts.end(),
        [](const ScreenText& s){ return s.duration > 0.0f && s.elapsed >= s.duration; }), mScreenTexts.end());
}

void GameUI::MoveScreenText(int id, Vec2 targetPos, float duration, int easing) {
    for (auto& st : mScreenTexts) {
        if (st.id == id) {
            if (duration <= 0.01f) {
                st.screenPos = targetPos;
                st.isTweening = false;
            } else {
                st.tweenStartPos = st.screenPos;
                st.tweenTargetPos = targetPos;
                st.tweenDuration = duration;
                st.tweenElapsed = 0.0f;
                st.isTweening = true;
                st.easingType = easing;
            }
            break;
        }
    }
}

void GameUI::TweenScreenText(int id, Vec2 targetPos, float targetScale, float duration, int easing) {
    for (auto& st : mScreenTexts) {
        if (st.id == id) {
            st.tweenStartPos = st.screenPos;
            st.tweenTargetPos = targetPos;
            st.tweenStartScale = st.fontScale;
            st.tweenTargetScale = targetScale;
            st.tweenScale = true;
            st.tweenDuration = duration;
            st.tweenElapsed = 0.0f;
            st.isTweening = true;
            st.easingType = easing;
            break;
        }
    }
}

void GameUI::DrawScreenTexts() {
    if (mScreenTexts.empty()) return;
#ifdef RPGMAKER3D_ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();

    for (const auto& st : mScreenTexts) {
        if (st.text.empty()) continue;
        float alpha = 1.0f;
        if (st.fading && st.duration > 0.0f) {
            float remaining = st.duration - st.elapsed;
            if (remaining < 1.0f) alpha = remaining;
        }
        if (alpha <= 0.0f) continue;

        ImVec4 col(st.color.r, st.color.g, st.color.b, st.color.a * alpha);
        ImVec2 pos;

        if (st.worldSpace) {
            pos = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            pos.x += st.worldPos.x * 20.0f;
            pos.y -= st.worldPos.z * 20.0f + st.worldPos.y * 10.0f;
        } else {
            pos = ImVec2(st.screenPos.x * io.DisplaySize.x + st.pixelOffset.x,
                         st.screenPos.y * io.DisplaySize.y + st.pixelOffset.y);
        }

        std::string windowName = "##ScreenText_" + std::to_string(st.id);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, st.centered ? ImVec2(0.5f, 0.5f) : ImVec2(0,0));
        ImGui::SetNextWindowBgAlpha(st.withBackground ? st.bgColor.a * alpha : 0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(st.bgColor.r, st.bgColor.g, st.bgColor.b, st.bgColor.a * alpha));
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Begin(windowName.c_str(), nullptr, flags);
        ImGui::Text("%s", st.text.c_str());
        ImGui::End();
        ImGui::PopStyleColor(2);
    }
#endif
}

// === Picture / Screen Sprite System ===

namespace {
    std::unordered_map<std::string, std::shared_ptr<Texture>> s_PictureCache;

    std::function<std::string(const std::string&)> s_pictureResolver;

    std::string ResolvePicturePath(const std::string& filename) {
        // 1) Engine-injizierter Resolver: <Projekt>/Graphics/Pictures|Titles/…
        if (s_pictureResolver) {
            const std::string r = s_pictureResolver(filename);
            if (!r.empty()) return r;
        }
        // 2) Engine-Asset-Fallbacks
        std::vector<std::string> tryPaths = {
            filename,
            "assets/textures/" + filename,
            "assets/pictures/" + filename,
            "./SampleProject/assets/textures/" + filename,
            "./SampleProject/assets/pictures/" + filename,
            "./SampleProject/assets/" + filename,
            "assets/" + filename
        };
        for (auto& p : tryPaths) {
            if (std::filesystem::exists(p)) return p;
        }
        return filename;
    }
}

void GameUI::SetPicturePathResolver(
    std::function<std::string(const std::string&)> fn) {
    s_pictureResolver = std::move(fn);
}

std::string GameUI::ResolvePicturePath(const std::string& filename) { // static
    return rpg::ResolvePicturePath(filename);
}

bool GameUI::LoadPictureTexture(ScreenPicture& pic) {
    if (pic.filename.empty()) return false;
    std::string path = ResolvePicturePath(pic.filename);
    auto it = s_PictureCache.find(path);
    std::shared_ptr<Texture> tex;
    if (it != s_PictureCache.end()) {
        tex = it->second;
    } else {
        tex = std::make_shared<Texture>();
        if (!tex->LoadFromFile(path)) {
            // Try fallback checkerboard
            tex->CreateCheckerboard();
            RPG_LOG_WARN("Picture not found, using checkerboard: " + path);
        }
        s_PictureCache[path] = tex;
    }
    pic.textureId = tex->GetID();
    pic.loaded = (pic.textureId != 0);
    // Store shared_ptr to keep alive - we leak into cache, but that's okay for now
    // To keep texture alive, we keep in cache
    return pic.loaded;
}

int GameUI::ShowPicture(const std::string& filename, Vec2 screenPos, float scale, float opacity, float duration, const std::string& name) {
    return ShowPicture(filename, name, screenPos, scale, opacity, duration);
}

int GameUI::ShowPicture(const std::string& filename, const std::string& name, Vec2 screenPos, float scale, float opacity, float duration) {
    ScreenPicture pic;
    pic.id = mNextPictureId++;
    pic.filename = filename;
    pic.name = name.empty() ? filename : name;
    pic.screenPos = screenPos;
    pic.scale = scale;
    pic.opacity = opacity;
    pic.duration = duration;
    pic.elapsed = 0.0f;
    pic.centered = true;
    LoadPictureTexture(pic);
    mPictures.push_back(pic);
    RPG_LOG_INFO("ShowPicture id=" + std::to_string(pic.id) + " name=" + pic.name + " file=" + filename);
    return pic.id;
}

float GameUI::ApplyEasing(float t, int easingType) {
    // t in 0..1
    t = std::clamp(t, 0.0f, 1.0f);
    switch (easingType) {
        case 0: // linear
            return t;
        case 1: // easeInQuad
            return t * t;
        case 2: // easeOutQuad (default, nice)
            return 1.0f - (1.0f - t) * (1.0f - t);
        case 3: // easeInOutQuad
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        case 4: // easeOutBounce
        {
            const float n1 = 7.5625f;
            const float d1 = 2.75f;
            if (t < 1.0f / d1) {
                return n1 * t * t;
            } else if (t < 2.0f / d1) {
                float t2 = t - 1.5f / d1;
                return n1 * t2 * t2 + 0.75f;
            } else if (t < 2.5f / d1) {
                float t2 = t - 2.25f / d1;
                return n1 * t2 * t2 + 0.9375f;
            } else {
                float t2 = t - 2.625f / d1;
                return n1 * t2 * t2 + 0.984375f;
            }
        }
        case 5: // easeInOutSine
            return -(std::cos(3.14159265f * t) - 1.0f) / 2.0f;
        case 6: // easeOutElastic (overshoot)
        {
            const float c4 = (2.0f * 3.14159265f) / 3.0f;
            if (t == 0) return 0;
            if (t == 1) return 1;
            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }
        default:
            return t;
    }
}

void GameUI::MovePicture(int id, Vec2 targetPos, float duration, int easing) {
    for (auto& pic : mPictures) {
        if (pic.id == id) {
            if (duration <= 0.01f) {
                pic.screenPos = targetPos;
                pic.isTweening = false;
            } else {
                pic.tweenStartPos = pic.screenPos;
                pic.tweenTargetPos = targetPos;
                pic.tweenDuration = duration;
                pic.tweenElapsed = 0.0f;
                pic.isTweening = true;
                pic.easingType = easing;
            }
            break;
        }
    }
}

void GameUI::MovePicture(const std::string& name, Vec2 targetPos, float duration, int easing) {
    for (auto& pic : mPictures) {
        if (pic.name == name) {
            if (duration <= 0.01f) {
                pic.screenPos = targetPos;
                pic.isTweening = false;
            } else {
                pic.tweenStartPos = pic.screenPos;
                pic.tweenTargetPos = targetPos;
                pic.tweenDuration = duration;
                pic.tweenElapsed = 0.0f;
                pic.isTweening = true;
                pic.easingType = easing;
            }
            break;
        }
    }
}

void GameUI::TweenPicture(int id, Vec2 targetPos, float targetScale, float targetOpacity, float targetRotation, float duration, int easing) {
    for (auto& pic : mPictures) {
        if (pic.id == id) {
            // Position
            pic.tweenStartPos = pic.screenPos;
            pic.tweenTargetPos = targetPos;
            // Scale
            pic.tweenStartScale = pic.scale;
            pic.tweenTargetScale = targetScale;
            pic.tweenScale = true;
            // Opacity
            pic.tweenStartOpacity = pic.opacity;
            pic.tweenTargetOpacity = targetOpacity;
            pic.tweenOpacity = true;
            // Rotation
            pic.tweenStartRotation = pic.rotation;
            pic.tweenTargetRotation = targetRotation;
            pic.tweenRotation = true;

            pic.tweenDuration = duration;
            pic.tweenElapsed = 0.0f;
            pic.isTweening = true;
            pic.easingType = easing;
            break;
        }
    }
}

void GameUI::TweenPictureScale(int id, float targetScale, float duration, int easing) {
    for (auto& pic : mPictures) {
        if (pic.id == id) {
            pic.tweenStartScale = pic.scale;
            pic.tweenTargetScale = targetScale;
            pic.tweenScale = true;
            pic.tweenDuration = std::max(pic.tweenDuration, duration);
            pic.tweenElapsed = 0.0f;
            pic.isTweening = true;
            pic.easingType = easing;
            break;
        }
    }
}

void GameUI::TweenPictureOpacity(int id, float targetOpacity, float duration, int easing) {
    for (auto& pic : mPictures) {
        if (pic.id == id) {
            pic.tweenStartOpacity = pic.opacity;
            pic.tweenTargetOpacity = targetOpacity;
            pic.tweenOpacity = true;
            pic.tweenDuration = std::max(pic.tweenDuration, duration);
            pic.tweenElapsed = 0.0f;
            pic.isTweening = true;
            pic.easingType = easing;
            break;
        }
    }
}

void GameUI::TweenPictureRotation(int id, float targetRotation, float duration, int easing) {
    for (auto& pic : mPictures) {
        if (pic.id == id) {
            pic.tweenStartRotation = pic.rotation;
            pic.tweenTargetRotation = targetRotation;
            pic.tweenRotation = true;
            pic.tweenDuration = std::max(pic.tweenDuration, duration);
            pic.tweenElapsed = 0.0f;
            pic.isTweening = true;
            pic.easingType = easing;
            break;
        }
    }
}

void GameUI::RemovePicture(int id) {
    mPictures.erase(std::remove_if(mPictures.begin(), mPictures.end(),
        [id](const ScreenPicture& p){ return p.id == id; }), mPictures.end());
}

void GameUI::RemovePicture(const std::string& name) {
    mPictures.erase(std::remove_if(mPictures.begin(), mPictures.end(),
        [&name](const ScreenPicture& p){ return p.name == name; }), mPictures.end());
}

void GameUI::ClearPictures() {
    mPictures.clear();
}

void GameUI::SetPictureOpacity(int id, float opacity) {
    for (auto& pic : mPictures) if (pic.id == id) pic.opacity = opacity;
}

void GameUI::SetPictureScale(int id, float scale) {
    for (auto& pic : mPictures) if (pic.id == id) pic.scale = scale;
}

void GameUI::SetPictureRotation(int id, float degrees) {
    for (auto& pic : mPictures) if (pic.id == id) pic.rotation = degrees;
}

void GameUI::SetPictureSize(int id, float sizeX, float sizeY) {
    for (auto& pic : mPictures) if (pic.id == id) pic.size = Vec2(sizeX, sizeY);
}

void GameUI::UpdatePictures(float dt) {
    for (auto& pic : mPictures) {
        pic.elapsed += dt;

        // Tween handling
        if (pic.isTweening) {
            pic.tweenElapsed += dt;
            float t = pic.tweenDuration > 0.001f ? (pic.tweenElapsed / pic.tweenDuration) : 1.0f;
            if (t >= 1.0f) {
                t = 1.0f;
                pic.isTweening = false;
                // Snap to target
                pic.screenPos = pic.tweenTargetPos;
                if (pic.tweenScale) pic.scale = pic.tweenTargetScale;
                if (pic.tweenOpacity) pic.opacity = pic.tweenTargetOpacity;
                if (pic.tweenRotation) pic.rotation = pic.tweenTargetRotation;
                pic.tweenScale = pic.tweenOpacity = pic.tweenRotation = false;
            } else {
                float eased = ApplyEasing(t, pic.easingType);
                // Lerp position
                pic.screenPos.x = pic.tweenStartPos.x + (pic.tweenTargetPos.x - pic.tweenStartPos.x) * eased;
                pic.screenPos.y = pic.tweenStartPos.y + (pic.tweenTargetPos.y - pic.tweenStartPos.y) * eased;
                if (pic.tweenScale) {
                    pic.scale = pic.tweenStartScale + (pic.tweenTargetScale - pic.tweenStartScale) * eased;
                }
                if (pic.tweenOpacity) {
                    pic.opacity = pic.tweenStartOpacity + (pic.tweenTargetOpacity - pic.tweenStartOpacity) * eased;
                }
                if (pic.tweenRotation) {
                    pic.rotation = pic.tweenStartRotation + (pic.tweenTargetRotation - pic.tweenStartRotation) * eased;
                }
            }
        }
    }
    // Remove expired (duration based)
    mPictures.erase(std::remove_if(mPictures.begin(), mPictures.end(),
        [](const ScreenPicture& p){ return p.duration > 0.0f && p.elapsed >= p.duration; }), mPictures.end());
}

void GameUI::DrawPictures() {
    if (mPictures.empty()) return;
#ifdef RPGMAKER3D_ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* fg = ImGui::GetForegroundDrawList();

    for (const auto& pic : mPictures) {
        if (!pic.loaded || pic.textureId == 0) continue;
        float alpha = pic.opacity;
        if (pic.fading && pic.duration > 0.0f) {
            float remaining = pic.duration - pic.elapsed;
            if (remaining < 1.0f) alpha *= remaining;
        }
        if (alpha <= 0.01f) continue;

        ImVec2 center(pic.screenPos.x * io.DisplaySize.x, pic.screenPos.y * io.DisplaySize.y);
        float baseSize = 128.0f * pic.scale;
        ImVec2 size(baseSize, baseSize);
        if (pic.size.x > 0.01f && pic.size.y > 0.01f) {
            size.x = pic.size.x * io.DisplaySize.x * pic.scale;
            size.y = pic.size.y * io.DisplaySize.y * pic.scale;
        }

        if (std::abs(pic.rotation) < 0.01f) {
            std::string windowName = "##Picture_" + std::to_string(pic.id);
            ImGui::SetNextWindowPos(center, ImGuiCond_Always, pic.centered ? ImVec2(0.5f, 0.5f) : ImVec2(0,0));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
            ImGui::Begin(windowName.c_str(), nullptr, flags);
            ImGui::Image((ImTextureID)(intptr_t)pic.textureId, size, ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,alpha), ImVec4(0,0,0,0));
            ImGui::End();
            ImGui::PopStyleVar();
        } else {
            float rad = pic.rotation * 3.14159265f / 180.0f;
            float c = std::cos(rad);
            float s = std::sin(rad);
            ImVec2 half(size.x * 0.5f, size.y * 0.5f);
            ImVec2 corners[4];
            ImVec2 local[4] = {
                ImVec2(-half.x, -half.y),
                ImVec2(half.x, -half.y),
                ImVec2(half.x, half.y),
                ImVec2(-half.x, half.y)
            };
            for (int i=0;i<4;++i) {
                float x = local[i].x, y = local[i].y;
                float rx = x * c - y * s;
                float ry = x * s + y * c;
                corners[i] = ImVec2(center.x + rx, center.y + ry);
            }
            ImVec2 uvs[4] = { ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1) };
            fg->AddImageQuad((ImTextureID)(intptr_t)pic.textureId,
                corners[0], corners[1], corners[2], corners[3],
                uvs[0], uvs[1], uvs[2], uvs[3],
                ImGui::GetColorU32(ImVec4(1,1,1,alpha)));
        }
    }
#endif
}

} // namespace rpg
