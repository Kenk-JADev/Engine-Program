#pragma once
// RPG Maker 3D Qt Editor - Katalog aller Event-Befehle (XP-Layout)
//
// Dies ist die zentrale Wahrheitsquelle für:
//  - den "Event-Befehle"-Dialog (_buttons auf 3 Seiten wie in RPG Maker XP)
//  - die Parameter-Dialoge (automatisch aus ArgSpecs gebaut)
//  - die formatierte Anzeige in der Befehlsliste ("@>Text: ...")
//  - das Block-Einfügen (Choices mit Wenn-Zweigen, Bedingung mit Ende usw.)
//
// Die Kodierung der Befehlsparameter (param1..3, text, parameters[]) ist
// identisch zur Engine-Auswertung in src/EventSystem.cpp (siehe auch
// docs/EVENTS-XP.md).

#include <QString>
#include <QStringList>
#include <vector>

#include "rpgmaker3d/EventSystem.h"

namespace qt_editor {

struct ArgSpec {
    enum class Type {
        Int,            // Ganzzahl (SpinBox)
        SignedInt,      // Ganzzahl mit Vorzeichen
        Text,           // einzeiliger Text
        MultiText,      // mehrzeiliger Text
        Choice,         // Auswahl (ComboBox); Wert = Index oder optionsValues
        SwitchId,       // Schalter-Id (1..9999)
        VariableId,     // Variablen-Id (1..9999)
        ItemId,         // Gegenstands-Id
        WeaponId,       // Waffen-Id
        ArmorId,        // Rüstungs-Id
        ActorId,        // Akteur-Id
        TroopId,        // Truppen-Id
        CommonEventId,  // Gemeinsames-Event-Id
        SelfSwitchChar, // A/B/C/D
        FileName,       // Dateiname (z.B. "bgm_title.ogg")
        Route,          // Bewegungsroute (UDLR F T A X Wn ...)
        Digits,         // Ziffernzahl 1..8
        Bool            // Häkchen
    };

    QString key;        // "param1".."param3", "text", "a0".."a5" (parameters-Array)
    QString label;      // Beschriftung im Dialog
    Type type = Type::Int;
    int def = 0;
    int min = -999999;
    int max = 999999;
    QString defText;
    QStringList options;      // für Choice
    QList<int> optionValues;  // optional: abweichende Integer-Werte
    QString hint;
    bool transient = false;   // nicht speichern (nur Dialog-Hilfsfeld)

    int optionToValue(int index) const {
        if (index >= 0 && index < optionValues.size()) return optionValues[index];
        return index;
    }
    int valueToOption(int value) const {
        int i = optionValues.indexOf(value);
        return i >= 0 ? i : 0;
    }
};

struct CommandSpec {
    rpg::EventCommandCode code = rpg::EventCommandCode::None;
    QString label;        // Button-Beschriftung (z.B. "Text zeigen...")
    int page = 1;         // Seite im Befehlsdialog (1..3)
    QString description;  // Hilfetext im Parameterdialog
    std::vector<ArgSpec> args;
    bool structural = false;  // kein Button (401, 411, ...)
};

/// Katalog aller Befehle (XP-Seiten 1..3 + 3D-Erweiterungen auf Seite 3)
const std::vector<CommandSpec>& GetEventCommandCatalog();

/// Spec zu einem Code (nullptr falls strukturell/unbekannt)
const CommandSpec* FindCommandSpec(rpg::EventCommandCode code);

/// Formatierung für die Befehlsliste, XP-Stil ohne "@>"-Präfix
/// (z.B. "Text: Hallo", "Bedingung: Schalter 2 = AN", "Wenn [Ja]")
QString FormatEventCommand(const rpg::EventCommand& cmd, int eventContext = 0);

/// Liest einen Wert aus dem Befehl nach ArgSpec-Key (für Dialog-Vorbelegung)
int GetCommandArgInt(const rpg::EventCommand& cmd, const QString& key, int def = 0);
QString GetCommandArgText(const rpg::EventCommand& cmd, const QString& key);

/// Schreibt einen Wert in den Befehl nach ArgSpec-Key
void SetCommandArgInt(rpg::EventCommand& cmd, const QString& key, int value);
void SetCommandArgText(rpg::EventCommand& cmd, const QString& key, const QString& value);

/// Baut aus einem editierten Befehl den kompletten Einfüge-Block
/// (z.B. Choices -> Wenn-Zweige + Ende; Bedingung -> BranchEnd; Loop -> RepeatAbove)
std::vector<rpg::EventCommand> BuildCommandBlock(const rpg::EventCommand& edited, int indent);

/// Normalisiert einen Befehl nach dem Dialog (Flags packen, Optionen kodieren).
/// Muss nach jedem OK/Anwenden-Aufruf des Parameterdialogs laufen.
void FinalizeEventCommand(rpg::EventCommand& cmd);

/// Dekodiert einen Befehl VOR dem Öffnen des Dialogs (z.B. Choices aus
/// gepacktem text in a0..a4, Bit-Flags aus param2 in a0..a2 Bools).
void DecodeCommandForEdit(rpg::EventCommand& cmd);

/// Anzahl direkt folgender Fortsetzungszeilen (401/408/655) nach dem
/// Kopfbefehl an list[headerIndex] (für Text zeigen/Kommentar/Skript).
int ContinuationLineCount(const std::vector<rpg::EventCommand>& list, int headerIndex);

/// Vollständiger mehrzeiliger Text: Kopfzeile + Fortsetzungszeilen.
QString JoinContinuationLines(const std::vector<rpg::EventCommand>& list, int headerIndex);

/// Abbruch-Verhaltenstexte (Show Choices)
QStringList ChoiceCancelLabels(); // indexiert 0..5

/// XP-Tastencodes (Bedingung "Taste", Button Input Processing)
QStringList XpButtonLabels();     // {Code pro Zeile als "code:name"}
int XpButtonCodeAt(int index);

} // namespace qt_editor
