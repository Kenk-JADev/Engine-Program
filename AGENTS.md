# Engine-Program — Stand 2026-07-24 (PAKET 17)

## Absoluter Fortschrittsstand
- **Commit:** `690c7309` (PAKET 17) auf Branch `arena/019f6f2a-engine-program`
- **Vorher:** `d736721` (PAKET 16) — vollständig abgeschlossen
- **Repo:** https://github.com/Kenk-JADev/Engine-Program.git (nur Branch `arena/019f6f2a-engine-program` nutzen!)

## PAKET 16 (abgeschlossen, pushed d736721)
**XP Move-Routen Vervollständigung:**
- Zentraler Parser `EventSystem_ParseMoveRouteText` (Befehl 209 + Custom-Seitenroute)
- 19 neue Schritte: Wait, Jump-Listen, Turn-Serien, Switches, Speed/Freq, H1/H0, P1/P0, G (Grafik), E (Erase), SC (Script)
- Neue MapEvent-Laufzeitfelder: `moveSpeedRt`, `moveFrequencyRt`, `through`, `transparent`, `routeGraphic`
- Qt-Editor: Dialog mit 32 Schritten, UserRole-Volltoken (kein Text-Prefix-Matching)
- Engine-Renderer konsumiert routeGraphic/routeGraphicIndex/transparent
- Rest: Anime-Flags 31–34, Opacity 40/41, Script 44/45, Sprung-Parabel
- Doku: Token-Tabelle in `docs/EVENTS-XP.md`

## PAKET 17 (abgeschlossen, pushed 690c730)
**Kampf-Zustände (States) komplett:**
- Battler.states/stateTurns + Methoden (HasState/AddState/RemoveState/CurrentRestriction/TotalHpDrainRate/MostSevereStateName)
- XP restriction 0..4: 4=Handlungsunfähig, 1/2/3=Zwang auf zufällige Seite/Actor
- Slip-Damage (`ApplySlipDamage` mit hpDrainRate×maxHp ≥1, Gift-Tod)
- Auto-Removal: Timing 1 (holdTurn am eigenen Zug), Timing 2 (Runde)
- removeAtBattleEnd (battle_only) Filter beim Sync
- SkillData.plusStates/minusStates + stateRanks an Actor/Enemy (Rang A-F = 100/80/60/40/20/0%)
- Event 333 (Change State) vom Log-Stub zu echt
- Spielstand: actors-Block `"states":[ids]` (Game.cpp Save+Load)
- HUD: stateName im BattleStatusEntry + Gegner-Zeile `[State]`
- Editor: Skills 2 ID-Listen, Actor/Enemy RanksEdit „1=C,2=F“, States-Tab ohne Änderungen
- Demo: Skill id 3 „Giftstich“ (plusStates={1}), Krieger lernt ab Lv 3

## Technik
- Build: `cmake -S . -B build -DRPGMAKER3D_ENABLE_IMGUI=ON`
- CI: `.github/workflows/Main.yml` Zeile 209/235 `IMGUI=ON` (Nutzer ändert; Bot pusht keine Workflows)
- SampleProject: Keine Dateien loeschen (Generator regeneriert)
- QA: Windows-Editor + Windows-Game aus CI-Artifact

## Editor-Status
- Move-Routen-Dialog: 600×460, 32 Schritte, kombiniertes argEdit, `DescribeRouteStep`/`BuildRouteFullToken`
- States-Persistenz: vollständig (Skills plus/minus, Actor/Enemy stateRanks, States-Tab, Items kommen PAKET 18)

## TODO (weitere Schritte)
- [ ] Item-Zustände (plusStates/minusStates an Items, parallel zu Skills)
- [ ] Map-Giftschaden (Overworld slip damage)
- [ ] Event 313 Live-Sync (states sofort anwenden, nicht erst beim nächsten Battle-Setup)
- [ ] force action 339 + event-getriebene Skills
- [ ] Battle-Abbruch bei force_action (interrupt pending action)

**Wichtig:** Jeder neue Thread muss zuerst `Engine-Program/TODO_XP_PARITY.md` lesen und HEAD/FETCH_HEAD prüfen (Resets kamen vor!). Bei Unterschied `git reset --hard <Remote-SHA>`.

Dieses Dokument wird automatisch von Arena generiert — nicht manuell editieren. Letzter Stand ab Commit 690c730.
Letzter Punkt: Commit-Historie und TODO_XP_PARITY sind die einzige Wahrheit fuer den Fortschritt (Dok prueft HEAD/FETCH_HEAD).
