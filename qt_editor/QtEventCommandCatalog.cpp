#include "QtEventCommandCatalog.h"

#include <QMap>
#include <cmath>
#include <cstdio> // std::snprintf (MSVC: nicht transitiv vorhanden)

namespace qt_editor {

using CC = rpg::EventCommandCode;

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

// ---------------------------------------------------------------------------
// Spec-Helfer
// ---------------------------------------------------------------------------
namespace {

ArgSpec A(const QString& key, const QString& label, ArgSpec::Type t = ArgSpec::Type::Int,
          int def = 0, int mn = 0, int mx = 999999, const QString& hint = QString()) {
    ArgSpec a; a.key = key; a.label = label; a.type = t; a.def = def; a.min = mn; a.max = mx; a.hint = hint;
    return a;
}
ArgSpec Ch(const QString& key, const QString& label, const QStringList& opts, int def = 0,
           const QString& hint = QString()) {
    ArgSpec a = A(key, label, ArgSpec::Type::Choice, def, 0, (int)opts.size() - 1, hint);
    a.options = opts;
    return a;
}
ArgSpec ChV(const QString& key, const QString& label, const QStringList& opts, const QList<int>& vals,
            int def = 0, const QString& hint = QString()) {
    ArgSpec a = Ch(key, label, opts, def, hint);
    a.optionValues = vals;
    return a;
}
ArgSpec Txt(const QString& key, const QString& label, const QString& defText = QString(),
            const QString& hint = QString()) {
    ArgSpec a = A(key, label, ArgSpec::Type::Text, 0, 0, 0, hint);
    a.defText = defText;
    return a;
}
ArgSpec MTxt(const QString& key, const QString& label, const QString& hint = QString()) {
    ArgSpec a = A(key, label, ArgSpec::Type::MultiText, 0, 0, 0, hint);
    return a;
}
QString OnOff(bool on) { return on ? QStringLiteral("AN") : QStringLiteral("AUS"); }
QString SignNum(int v) { return (v >= 0 ? QStringLiteral("+") : QString()) + QString::number(v); }

std::vector<CommandSpec> BuildCatalog() {
    std::vector<CommandSpec> c;
    auto add = [&c](CC code, const QString& label, int page, const QString& desc,
                    std::vector<ArgSpec> args = {}) {
        CommandSpec s; s.code = code; s.label = label; s.page = page;
        s.description = desc; s.args = std::move(args);
        c.push_back(s);
    };
    const QStringList anAus = {QStringLiteral("AUS"), QStringLiteral("AN")};
    const QStringList richung = {QStringLiteral("Unten"), QStringLiteral("Links"),
                                 QStringLiteral("Rechts"), QStringLiteral("Oben")};
    const QList<int> richungVals = {2, 4, 6, 8};

    // ================= Seite 1 =================
    add(CC::ShowText, QStringLiteral("Text zeigen..."), 1,
        QStringLiteral("Zeigt eine Nachricht im Dialogfenster. Folgebefehle 'Textzeile' (401) werden angehängt."),
        {MTxt(QStringLiteral("text"), QStringLiteral("Text"))});
    add(CC::ShowChoices, QStringLiteral("Auswahl zeigen..."), 1,
        QStringLiteral("Zeigt bis zu 4 Auswahlmöglichkeiten. Es werden automatisch 'Wenn'-Zweige angelegt."),
        {Txt(QStringLiteral("a4"), QStringLiteral("Fragetext (optional)")),
         Txt(QStringLiteral("a0"), QStringLiteral("Option 1"), QStringLiteral("Ja")),
         Txt(QStringLiteral("a1"), QStringLiteral("Option 2"), QStringLiteral("Nein")),
         Txt(QStringLiteral("a2"), QStringLiteral("Option 3")),
         Txt(QStringLiteral("a3"), QStringLiteral("Option 4")),
         ChV(QStringLiteral("param2"), QStringLiteral("Bei Abbruch"), ChoiceCancelLabels(),
             {0, 1, 2, 3, 4, 5}, 0)});
    add(CC::InputNumber, QStringLiteral("Zahlen eingeben..."), 1,
        QStringLiteral("Öffnet die Zahleneingabe und speichert das Ergebnis in einer Variable."),
        {A(QStringLiteral("param1"), QStringLiteral("Variable"), ArgSpec::Type::VariableId, 1, 1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Stellen"), ArgSpec::Type::Digits, 4, 1, 8)});
    add(CC::ChangeTextOptions, QStringLiteral("Text-Optionen ändern..."), 1,
        QStringLiteral("Position und Darstellung des Nachrichtenfensters ändern."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Position"),
            {QStringLiteral("Unten"), QStringLiteral("Mitte"), QStringLiteral("Oben")}),
         Ch(QStringLiteral("param2"), QStringLiteral("Fenster"),
            {QStringLiteral("Normal"), QStringLiteral("Dunkel"), QStringLiteral("Transparent")})});
    add(CC::ButtonInputProcessing, QStringLiteral("Tastenabfrage..."), 1,
        QStringLiteral("Wartet auf einen Tastendruck und speichert den Tastencode (XP: 2/4/6/8, 11-18) in einer Variable."),
        {A(QStringLiteral("param1"), QStringLiteral("Zielvariable"), ArgSpec::Type::VariableId, 1, 1, 9999)});
    add(CC::Wait, QStringLiteral("Warten..."), 1,
        QStringLiteral("Wartet die angegebene Anzahl Frames (40 Frames = 1 Sekunde)."),
        {A(QStringLiteral("param1"), QStringLiteral("Frames"), ArgSpec::Type::Int, 20, 1, 999)});
    add(CC::Comment, QStringLiteral("Kommentar..."), 1,
        QStringLiteral("Kommentar im Event (wird im Spiel ignoriert)."),
        {MTxt(QStringLiteral("text"), QStringLiteral("Kommentar"))});
    add(CC::ConditionalBranch, QStringLiteral("Bedingung..."), 1,
        QStringLiteral("Verzweigung: Befehle im Zweig nur ausführen, wenn die Bedingung zutrifft."),
        {/* wird vom Spezialdialog befüllt */});
    add(CC::Loop, QStringLiteral("Schleife"), 1,
        QStringLiteral("Beginnt eine Endlosschleife (mit 'Schleife verlassen' beendbar)."));
    add(CC::BreakLoop, QStringLiteral("Schleife verlassen"), 1,
        QStringLiteral("Springt aus der aktuellen Schleife heraus."));
    add(CC::ExitEventProcessing, QStringLiteral("Event-Verarbeitung beenden"), 1,
        QStringLiteral("Beendet die Ausführung dieses Events sofort."));
    add(CC::EraseEvent, QStringLiteral("Event löschen"), 1,
        QStringLiteral("Entfernt das Event bis zum nächsten Betreten der Map."));
    add(CC::CallCommonEvent, QStringLiteral("Gemeinsames Event aufrufen..."), 1,
        QStringLiteral("Ruft ein gemeinsames Event auf (Child-Interpreter)."),
        {A(QStringLiteral("param1"), QStringLiteral("Gemeinsames Event"), ArgSpec::Type::CommonEventId, 1, 1, 9999)});
    add(CC::Label, QStringLiteral("Marke..."), 1,
        QStringLiteral("Setzt eine Sprungmarke im Event."),
        {Txt(QStringLiteral("text"), QStringLiteral("Name"), QStringLiteral("Marke1"))});
    add(CC::JumpToLabel, QStringLiteral("Springe zu Marke..."), 1,
        QStringLiteral("Springt zur angegebenen Sprungmarke."),
        {Txt(QStringLiteral("text"), QStringLiteral("Name"), QStringLiteral("Marke1"))});
    add(CC::ControlSwitches, QStringLiteral("Schalter steuern..."), 1,
        QStringLiteral("Schaltet einen Schalter oder Bereich AN/AUS."),
        {A(QStringLiteral("param1"), QStringLiteral("Von Schalter"), ArgSpec::Type::SwitchId, 1, 1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Bis Schalter"), ArgSpec::Type::SwitchId, 1, 1, 9999),
         Ch(QStringLiteral("param3"), QStringLiteral("Wert"), anAus, 1)});
    add(CC::ControlVariables, QStringLiteral("Variablen steuern..."), 1,
        QStringLiteral("Setzt/verändert Variablen (Konstante, andere Variable oder Zufall)."),
        {A(QStringLiteral("param1"), QStringLiteral("Von Variable"), ArgSpec::Type::VariableId, 1, 1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Bis Variable"), ArgSpec::Type::VariableId, 1, 1, 9999),
         Ch(QStringLiteral("param3"), QStringLiteral("Operation"),
            {QStringLiteral("Zuweisen"), QStringLiteral("Addieren"), QStringLiteral("Subtrahieren"),
             QStringLiteral("Multiplizieren"), QStringLiteral("Dividieren"), QStringLiteral("Modulo")}),
         Ch(QStringLiteral("a0"), QStringLiteral("Wert-Art"),
            {QStringLiteral("Konstante"), QStringLiteral("Aus Variable"), QStringLiteral("Zufall (Min..Max)")}),
         A(QStringLiteral("a1"), QStringLiteral("Wert / Variable / Min"), ArgSpec::Type::SignedInt, 1, -999999, 999999),
         A(QStringLiteral("a2"), QStringLiteral("Max (nur Zufall)"), ArgSpec::Type::SignedInt, 10, -999999, 999999)});
    add(CC::ControlSelfSwitch, QStringLiteral("Selbstschalter steuern..."), 1,
        QStringLiteral("Schaltet den Selbstschalter (A-D) DIESES Events."),
        {A(QStringLiteral("text"), QStringLiteral("Selbstschalter"), ArgSpec::Type::SelfSwitchChar),
         Ch(QStringLiteral("param3"), QStringLiteral("Wert"), anAus, 1)});
    add(CC::ControlTimer, QStringLiteral("Timer steuern..."), 1,
        QStringLiteral("Startet oder stoppt den Spiel-Timer (für Bedingung 'Timer')."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Modus"),
            {QStringLiteral("Starten"), QStringLiteral("Stoppen")}),
         A(QStringLiteral("param2"), QStringLiteral("Sekunden"), ArgSpec::Type::Int, 60, 0, 99999)});
    add(CC::ChangeGold, QStringLiteral("Geld ändern..."), 1,
        QStringLiteral("Erhöht oder verringert das Gruppengold."),
        {A(QStringLiteral("param1"), QStringLiteral("Betrag (negativ = abziehen)"), ArgSpec::Type::SignedInt, 100, -999999, 999999)});
    add(CC::ChangeItems, QStringLiteral("Gegenstände ändern..."), 1,
        QStringLiteral("Gibt oder nimmt Gegenstände."),
        {A(QStringLiteral("param1"), QStringLiteral("Gegenstand"), ArgSpec::Type::ItemId, 1, 1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Anzahl (+/-)"), ArgSpec::Type::SignedInt, 1, -99, 99)});
    add(CC::ChangeWeapons, QStringLiteral("Waffen ändern..."), 1,
        QStringLiteral("Gibt oder nimmt Waffen."),
        {A(QStringLiteral("param1"), QStringLiteral("Waffe"), ArgSpec::Type::WeaponId, 1, 1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Anzahl (+/-)"), ArgSpec::Type::SignedInt, 1, -99, 99)});
    add(CC::ChangeArmor, QStringLiteral("Rüstungen ändern..."), 1,
        QStringLiteral("Gibt oder nimmt Rüstungen."),
        {A(QStringLiteral("param1"), QStringLiteral("Rüstung"), ArgSpec::Type::ArmorId, 1, 1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Anzahl (+/-)"), ArgSpec::Type::SignedInt, 1, -99, 99)});
    add(CC::ChangePartyMember, QStringLiteral("Gruppenmitglied ändern..."), 1,
        QStringLiteral("Fügt einen Akteur zur Gruppe hinzu oder entfernt ihn."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur"), ArgSpec::Type::ActorId, 1, 1, 9999),
         Ch(QStringLiteral("param3"), QStringLiteral("Aktion"),
            {QStringLiteral("Hinzufügen"), QStringLiteral("Entfernen")})});
    add(CC::ChangeWindowskin, QStringLiteral("Windowskin ändern..."), 1,
        QStringLiteral("Ändert das Aussehen der Fenster."),
        {A(QStringLiteral("text"), QStringLiteral("Skin-Datei"), ArgSpec::Type::FileName, 0, 0, 0)});
    add(CC::ChangeBattleBGM, QStringLiteral("Kampf-BGM ändern..."), 1,
        QStringLiteral("Ändert die Musik im Kampf."),
        {A(QStringLiteral("text"), QStringLiteral("BGM-Datei"), ArgSpec::Type::FileName, 0, 0, 0)});
    add(CC::ChangeBattleEndME, QStringLiteral("Sieg-ME ändern..."), 1,
        QStringLiteral("Ändert die Musik nach einem gewonnenen Kampf."),
        {A(QStringLiteral("text"), QStringLiteral("ME-Datei"), ArgSpec::Type::FileName, 0, 0, 0)});
    add(CC::ChangeSaveAccess, QStringLiteral("Speichern ändern..."), 1,
        QStringLiteral("Erlaubt oder sperrt das Speichern."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Speichern"),
            {QStringLiteral("Gesperrt"), QStringLiteral("Erlaubt")}, 1)});
    add(CC::ChangeMenuAccess, QStringLiteral("Menü ändern..."), 1,
        QStringLiteral("Erlaubt oder sperrt den Menüaufruf."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Menü"),
            {QStringLiteral("Gesperrt"), QStringLiteral("Erlaubt")}, 1)});
    add(CC::ChangeEncounter, QStringLiteral("Zufallskämpfe ändern..."), 1,
        QStringLiteral("Erlaubt oder sperrt Zufallskämpfe."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Zufallskämpfe"),
            {QStringLiteral("Gesperrt"), QStringLiteral("Erlaubt")}, 1)});

    // ================= Seite 2 =================
    add(CC::TransferPlayer, QStringLiteral("Spieler transferieren..."), 2,
        QStringLiteral("Teleportiert den Spieler (optional auf eine andere Map)."),
        {A(QStringLiteral("param3"), QStringLiteral("Map (0 = aktuelle)"), ArgSpec::Type::Int, 0, 0, 999),
         A(QStringLiteral("param1"), QStringLiteral("X"), ArgSpec::Type::Int, 0, -999, 999),
         A(QStringLiteral("param2"), QStringLiteral("Z"), ArgSpec::Type::Int, 0, -999, 999)});
    add(CC::SetEventLocation, QStringLiteral("Event-Position setzen..."), 2,
        QStringLiteral("Verschiebt ein Event auf neue Koordinaten."),
        {A(QStringLiteral("param1"), QStringLiteral("Event-ID"), ArgSpec::Type::Int, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("X"), ArgSpec::Type::Int, 0, -999, 999),
         A(QStringLiteral("param3"), QStringLiteral("Z"), ArgSpec::Type::Int, 0, -999, 999)});
    add(CC::ScrollMap, QStringLiteral("Map scrollen..."), 2,
        QStringLiteral("Scrollt die Karte (2D-Anzeige; in 3D folgt die Kamera dem Spieler)."),
        {ChV(QStringLiteral("param1"), QStringLiteral("Richtung"), richung, richungVals),
         A(QStringLiteral("param2"), QStringLiteral("Felder"), ArgSpec::Type::Int, 1, 0, 99),
         A(QStringLiteral("param3"), QStringLiteral("Tempo (1-6)"), ArgSpec::Type::Int, 3, 1, 6)});
    add(CC::ChangeMapSettings, QStringLiteral("Map-Einstellungen ändern..."), 2,
        QStringLiteral("Ändert Map-Einstellungen (Hintergrund/Fog)."),
        {Txt(QStringLiteral("text"), QStringLiteral("Panorama/Fog-Datei")),
         Ch(QStringLiteral("param1"), QStringLiteral("Ebene"),
            {QStringLiteral("Panorama"), QStringLiteral("Nebel")})});
    add(CC::ChangeFogColorTone, QStringLiteral("Nebel-Farbton ändern..."), 2,
        QStringLiteral("Färbt den Nebel ein."),
        {A(QStringLiteral("param1"), QStringLiteral("Rot"), ArgSpec::Type::Int, 255, 0, 255),
         A(QStringLiteral("param2"), QStringLiteral("Grün"), ArgSpec::Type::Int, 255, 0, 255),
         A(QStringLiteral("param3"), QStringLiteral("Blau"), ArgSpec::Type::Int, 255, 0, 255)});
    add(CC::ChangeFogOpacity, QStringLiteral("Nebel-Deckkraft ändern..."), 2,
        QStringLiteral("Deckkraft des Nebels einstellen."),
        {A(QStringLiteral("param1"), QStringLiteral("Deckkraft (0-128)"), ArgSpec::Type::Int, 64, 0, 128)});
    add(CC::ShowAnimation, QStringLiteral("Animation zeigen..."), 2,
        QStringLiteral("Spielt eine Animation/Partikel an einem Ziel ab."),
        {A(QStringLiteral("param1"), QStringLiteral("Ziel-Event (0=dieses, -1=Spieler)"), ArgSpec::Type::Int, -1, -1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Animations-ID"), ArgSpec::Type::Int, 1, 1, 999),
         Txt(QStringLiteral("text"), QStringLiteral("Name (optional)"))});
    add(CC::ChangeTransparentFlag, QStringLiteral("Transparenz ändern..."), 2,
        QStringLiteral("Macht den Spieler (un)sichtbar."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Zustand"),
            {QStringLiteral("Normal"), QStringLiteral("Transparent")})});
    add(CC::SetMoveRoute, QStringLiteral("Bewegungsroute setzen..."), 2,
        QStringLiteral("Erzwingt eine Bewegungsroute. Routentext: U D L R F T A X TD TL TR TU Wn (z.B. 'R R W20 L')."),
        {A(QStringLiteral("param1"), QStringLiteral("Ziel (0=dieses Event, -1=Spieler)"), ArgSpec::Type::Int, 0, -1, 9999),
         A(QStringLiteral("a0"), QStringLiteral("Wiederholen"), ArgSpec::Type::Bool, 1, 0, 1),
         A(QStringLiteral("a1"), QStringLiteral("Überspringbar"), ArgSpec::Type::Bool, 1, 0, 1),
         A(QStringLiteral("a2"), QStringLiteral("Auf Fertigstellung warten"), ArgSpec::Type::Bool, 0, 0, 1),
         A(QStringLiteral("text"), QStringLiteral("Route (z.B. 'R W40 L W40')"), ArgSpec::Type::Route)});
    add(CC::WaitForMoveCompletion, QStringLiteral("Auf Bewegung warten"), 2,
        QStringLiteral("Wartet, bis alle erzwungenen Bewegungsrouten fertig sind."));
    add(CC::PrepareTransition, QStringLiteral("Übergang vorbereiten"), 2,
        QStringLiteral("Bereitet einen Bildübergang vor."));
    add(CC::ExecuteTransition, QStringLiteral("Übergang ausführen..."), 2,
        QStringLiteral("Führt den vorbereiteten Übergang aus."),
        {A(QStringLiteral("text"), QStringLiteral("Übergangsgrafik"), ArgSpec::Type::FileName)});
    add(CC::ChangeScreenColorTone, QStringLiteral("Bildschirm-Farbton ändern..."), 2,
        QStringLiteral("Färbt den ganzen Bildschirm ein (-255..255 je Kanal)."),
        {A(QStringLiteral("param1"), QStringLiteral("Rot"), ArgSpec::Type::SignedInt, 0, -255, 255),
         A(QStringLiteral("param2"), QStringLiteral("Grün"), ArgSpec::Type::SignedInt, 0, -255, 255),
         A(QStringLiteral("param3"), QStringLiteral("Blau"), ArgSpec::Type::SignedInt, 0, -255, 255),
         A(QStringLiteral("a0"), QStringLiteral("Grauanteil"), ArgSpec::Type::Int, 0, 0, 255),
         A(QStringLiteral("a1"), QStringLiteral("Dauer (Sek.)"), ArgSpec::Type::Int, 1, 0, 60)});
    add(CC::ScreenFlash, QStringLiteral("Bildschirm blitzen..."), 2,
        QStringLiteral("Lässt den Bildschirm kurz aufblitzen."),
        {A(QStringLiteral("param1"), QStringLiteral("Rot"), ArgSpec::Type::Int, 255, 0, 255),
         A(QStringLiteral("param2"), QStringLiteral("Grün"), ArgSpec::Type::Int, 255, 0, 255),
         A(QStringLiteral("param3"), QStringLiteral("Blau"), ArgSpec::Type::Int, 255, 0, 255),
         A(QStringLiteral("a0"), QStringLiteral("Stärke (0-255)"), ArgSpec::Type::Int, 160, 0, 255),
         A(QStringLiteral("a1"), QStringLiteral("Dauer (Sek.)"), ArgSpec::Type::Int, 1, 0, 30)});
    add(CC::ScreenShake, QStringLiteral("Bildschirm beben..."), 2,
        QStringLiteral("Lässt den Bildschirm beben."),
        {A(QStringLiteral("param1"), QStringLiteral("Stärke (1-9)"), ArgSpec::Type::Int, 5, 1, 9),
         A(QStringLiteral("param2"), QStringLiteral("Tempo (1-9)"), ArgSpec::Type::Int, 5, 1, 9),
         A(QStringLiteral("a0"), QStringLiteral("Dauer (Sek.)"), ArgSpec::Type::Int, 1, 0, 30),
         A(QStringLiteral("param3"), QStringLiteral("Warten bis fertig"), ArgSpec::Type::Bool, 1, 0, 1)});
    add(CC::ShowPicture, QStringLiteral("Bild zeigen..."), 2,
        QStringLiteral("Zeigt ein Bild an (Position x/y 0-640/0-480)."),
        {A(QStringLiteral("text"), QStringLiteral("Bild-Datei"), ArgSpec::Type::FileName),
         A(QStringLiteral("param1"), QStringLiteral("X"), ArgSpec::Type::Int, 320, 0, 640),
         A(QStringLiteral("param2"), QStringLiteral("Y"), ArgSpec::Type::Int, 240, 0, 480)});
    add(CC::MovePicture, QStringLiteral("Bild bewegen..."), 2,
        QStringLiteral("Bewegt ein angezeigtes Bild."),
        {A(QStringLiteral("param1"), QStringLiteral("Bild-Nr."), ArgSpec::Type::Int, 1, 1, 99),
         A(QStringLiteral("param2"), QStringLiteral("X"), ArgSpec::Type::Int, 320, 0, 640),
         A(QStringLiteral("param3"), QStringLiteral("Y"), ArgSpec::Type::Int, 240, 0, 480)});
    add(CC::RotatePicture, QStringLiteral("Bild drehen..."), 2,
        QStringLiteral("Dreht ein Bild."),
        {A(QStringLiteral("param1"), QStringLiteral("Bild-Nr."), ArgSpec::Type::Int, 1, 1, 99),
         A(QStringLiteral("param2"), QStringLiteral("Winkel (Grad)"), ArgSpec::Type::SignedInt, 0, -360, 360)});
    add(CC::ChangePictureColorTone, QStringLiteral("Bild-Farbton ändern..."), 2,
        QStringLiteral("Deckkraft eines Bildes ändern (0.0 - 1.0)."),
        {A(QStringLiteral("param1"), QStringLiteral("Bild-Nr."), ArgSpec::Type::Int, 1, 1, 99),
         A(QStringLiteral("a0"), QStringLiteral("Deckkraft x100 (0-100)"), ArgSpec::Type::Int, 100, 0, 100)});
    add(CC::ErasePicture, QStringLiteral("Bild löschen..."), 2,
        QStringLiteral("Entfernt ein Bild."),
        {A(QStringLiteral("param1"), QStringLiteral("Bild-Nr."), ArgSpec::Type::Int, 1, 1, 99)});
    add(CC::SetWeatherEffects, QStringLiteral("Wettereffekte..."), 2,
        QStringLiteral("Stellt Wetter ein."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Typ"),
            {QStringLiteral("Keins"), QStringLiteral("Regen"), QStringLiteral("Sturm"), QStringLiteral("Schnee")}),
         A(QStringLiteral("param2"), QStringLiteral("Stärke (1-9)"), ArgSpec::Type::Int, 5, 1, 9)});
    add(CC::PlayBGM, QStringLiteral("BGM abspielen..."), 2,
        QStringLiteral("Spielt Hintergrundmusik (Schleife)."),
        {A(QStringLiteral("text"), QStringLiteral("Datei (z.B. bgm_title.ogg)"), ArgSpec::Type::FileName)});
    add(CC::FadeOutBGM, QStringLiteral("BGM ausblenden..."), 2,
        QStringLiteral("Blendet die Musik aus."),
        {A(QStringLiteral("param1"), QStringLiteral("Sekunden"), ArgSpec::Type::Int, 2, 0, 60)});
    add(CC::PlayBGS, QStringLiteral("BGS abspielen..."), 2,
        QStringLiteral("Spielt einen Hintergrund-Sound (Regen etc.)."),
        {A(QStringLiteral("text"), QStringLiteral("Datei"), ArgSpec::Type::FileName)});
    add(CC::FadeOutBGS, QStringLiteral("BGS ausblenden..."), 2,
        QStringLiteral("Blendet den BGS aus."),
        {A(QStringLiteral("param1"), QStringLiteral("Sekunden"), ArgSpec::Type::Int, 2, 0, 60)});
    add(CC::MemorizeBGM, QStringLiteral("BGM/BGS merken"), 2,
        QStringLiteral("Merkt die aktuelle Musik zum späteren Wiederherstellen."));
    add(CC::RestoreBGM, QStringLiteral("BGM/BGS wiederherstellen"), 2,
        QStringLiteral("Stellt die gemerkte Musik wieder her."));
    add(CC::PlayME, QStringLiteral("ME abspielen..."), 2,
        QStringLiteral("Spielt einen Musikeffekt (Fanfare)."),
        {A(QStringLiteral("text"), QStringLiteral("Datei"), ArgSpec::Type::FileName)});
    add(CC::PlaySE, QStringLiteral("SE abspielen..."), 2,
        QStringLiteral("Spielt einen Soundeffekt."),
        {A(QStringLiteral("text"), QStringLiteral("Datei (z.B. se_door.wav)"), ArgSpec::Type::FileName)});
    add(CC::StopSE, QStringLiteral("SE stoppen"), 2,
        QStringLiteral("Stoppt alle Soundeffekte."));

    // ---- 3D-Erweiterungen (Seite 2 unten) ----
    add(CC::ShowScreenText, QStringLiteral("HUD-Text zeigen (3D)..."), 2,
        QStringLiteral("Zeigt Text frei auf dem Bildschirm (Engine-Erweiterung)."),
        {MTxt(QStringLiteral("text"), QStringLiteral("Text")),
         A(QStringLiteral("param1"), QStringLiteral("X in % (0-100)"), ArgSpec::Type::Int, 50, 0, 100),
         A(QStringLiteral("param2"), QStringLiteral("Y in % (0-100)"), ArgSpec::Type::Int, 20, 0, 100),
         A(QStringLiteral("param3"), QStringLiteral("Dauer (Zehntel-Sek.)"), ArgSpec::Type::Int, 30, 1, 600)});
    add(CC::ShowWorldText, QStringLiteral("Welt-Text zeigen (3D)..."), 2,
        QStringLiteral("Schwebender Text an einer Weltposition (Engine-Erweiterung)."),
        {MTxt(QStringLiteral("text"), QStringLiteral("Text")),
         A(QStringLiteral("param1"), QStringLiteral("X"), ArgSpec::Type::SignedInt, 0, -999, 999),
         A(QStringLiteral("param2"), QStringLiteral("Y (Höhe)"), ArgSpec::Type::SignedInt, 1, -99, 99),
         A(QStringLiteral("param3"), QStringLiteral("Z"), ArgSpec::Type::SignedInt, 0, -999, 999)});
    add(CC::ClearScreenTexts, QStringLiteral("HUD-Texte löschen (3D)"), 2,
        QStringLiteral("Entfernt alle HUD-Texte (Engine-Erweiterung)."));
    add(CC::SetTimeOfDay, QStringLiteral("Tageszeit setzen (3D)..."), 2,
        QStringLiteral("Setzt die Tageszeit für Licht/Schatten (Engine-Erweiterung)."),
        {A(QStringLiteral("param1"), QStringLiteral("Stunde (0-23)"), ArgSpec::Type::Int, 12, 0, 23),
         A(QStringLiteral("param2"), QStringLiteral("Minute"), ArgSpec::Type::Int, 0, 0, 59)});

    // ================= Seite 3 =================
    add(CC::BattleProcessing, QStringLiteral("Kampf verarbeiten..."), 3,
        QStringLiteral("Startet einen Kampf gegen eine Truppe. Ergebnis-Zweige: 'Wenn Sieg/Flucht/Niederlage' (601-603)."),
        {A(QStringLiteral("param1"), QStringLiteral("Truppe"), ArgSpec::Type::TroopId, 1, 1, 9999),
         A(QStringLiteral("a0"), QStringLiteral("Flucht erlaubt"), ArgSpec::Type::Bool, 1, 0, 1),
         A(QStringLiteral("a1"), QStringLiteral("Niederlage möglich (kein Game Over)"), ArgSpec::Type::Bool, 1, 0, 1)});
    add(CC::ShopProcessing, QStringLiteral("Laden verarbeiten..."), 3,
        QStringLiteral("Öffnet einen Laden mit den angegebenen Waren. Waren-Text: Zahl = Gegenstand, w<ID> = Waffe, a<ID> = Rüstung (z. B. '1,2,w1,a3')."),
        {Txt(QStringLiteral("text"), QStringLiteral("Waren (Items, w<Waffe>, a<Rüstung>)"), QStringLiteral("1,2,w1"))});
    add(CC::NameInputProcessing, QStringLiteral("Namenseingabe..."), 3,
        QStringLiteral("Öffnet die Namenseingabe für einen Akteur."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur"), ArgSpec::Type::ActorId, 1, 1, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Max. Zeichen"), ArgSpec::Type::Int, 8, 1, 16)});
    add(CC::ChangeHP, QStringLiteral("HP ändern..."), 3,
        QStringLiteral("HP eines Akteurs ändern (0 = ganze Gruppe)."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Änderung (+/-)"), ArgSpec::Type::SignedInt, -10, -9999, 9999)});
    add(CC::ChangeSP, QStringLiteral("SP ändern..."), 3,
        QStringLiteral("SP/MP eines Akteurs ändern (0 = ganze Gruppe)."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Änderung (+/-)"), ArgSpec::Type::SignedInt, -10, -9999, 9999)});
    add(CC::ChangeState, QStringLiteral("Status ändern..."), 3,
        QStringLiteral("Fügt einen Statuseffekt hinzu oder entfernt ihn."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Status-ID"), ArgSpec::Type::Int, 1, 1, 999),
         Ch(QStringLiteral("param3"), QStringLiteral("Aktion"),
            {QStringLiteral("Hinzufügen"), QStringLiteral("Entfernen")})});
    add(CC::RecoverAll, QStringLiteral("Alles wiederherstellen..."), 3,
        QStringLiteral("Stellt HP/MP komplett wieder her."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 0, 0, 9999)});
    add(CC::ChangeEXP, QStringLiteral("EXP ändern..."), 3,
        QStringLiteral("Erfahrungspunkte ändern."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Änderung (+/-)"), ArgSpec::Type::SignedInt, 10, -99999, 99999)});
    add(CC::ChangeLevel, QStringLiteral("Level ändern..."), 3,
        QStringLiteral("Setzt das Level eines Akteurs."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Level"), ArgSpec::Type::Int, 1, 1, 99)});
    add(CC::ChangeParameters, QStringLiteral("Parameter ändern..."), 3,
        QStringLiteral("Basiswerte eines Akteurs ändern."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         Ch(QStringLiteral("param2"), QStringLiteral("Wert"),
            {QStringLiteral("Max. HP"), QStringLiteral("Max. SP"), QStringLiteral("Angriff"), QStringLiteral("Abwehr"),
             QStringLiteral("Magie"), QStringLiteral("Magieabw."), QStringLiteral("Agilität"), QStringLiteral("Glück")}),
         A(QStringLiteral("param3"), QStringLiteral("Änderung (+/-)"), ArgSpec::Type::SignedInt, 1, -999, 999)});
    add(CC::ChangeSkills, QStringLiteral("Fertigkeiten ändern..."), 3,
        QStringLiteral("Fertigkeit lernen oder vergessen."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Fertigkeits-ID"), ArgSpec::Type::Int, 1, 1, 9999),
         Ch(QStringLiteral("param3"), QStringLiteral("Aktion"),
            {QStringLiteral("Lernen"), QStringLiteral("Vergessen")})});
    add(CC::ChangeEquipment, QStringLiteral("Ausrüstung ändern..."), 3,
        QStringLiteral("Rüstet einen Akteur aus (Ausrüstung muss im Inventar sein)."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         Ch(QStringLiteral("param2"), QStringLiteral("Slot"),
            {QStringLiteral("Waffe"), QStringLiteral("Rüstung")}),
         A(QStringLiteral("param3"), QStringLiteral("Ausrüstungs-ID"), ArgSpec::Type::Int, 1, 0, 9999)});
    add(CC::ChangeActorName, QStringLiteral("Akteurname ändern..."), 3,
        QStringLiteral("Ändert den Namen eines Akteurs."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         Txt(QStringLiteral("text"), QStringLiteral("Neuer Name"), QStringLiteral("Held"))});
    add(CC::ChangeActorClass, QStringLiteral("Akteurklasse ändern..."), 3,
        QStringLiteral("Ändert die Klasse eines Akteurs."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("param2"), QStringLiteral("Klassen-ID"), ArgSpec::Type::Int, 1, 1, 999)});
    add(CC::ChangeActorGraphic, QStringLiteral("Akteurgrafik ändern..."), 3,
        QStringLiteral("Ändert die Grafik (Modell/Sprite) eines Akteurs."),
        {A(QStringLiteral("param1"), QStringLiteral("Akteur (0=Gruppe)"), ArgSpec::Type::ActorId, 1, 0, 9999),
         A(QStringLiteral("text"), QStringLiteral("Grafik-Datei"), ArgSpec::Type::FileName)});
    add(CC::ChangeEnemyHP, QStringLiteral("Gegner-HP ändern..."), 3,
        QStringLiteral("HP eines Gegners im laufenden Kampf ändern."),
        {A(QStringLiteral("param1"), QStringLiteral("Gegner (0=alle)"), ArgSpec::Type::Int, 1, 0, 8),
         A(QStringLiteral("param2"), QStringLiteral("Änderung (+/-)"), ArgSpec::Type::SignedInt, -10, -9999, 9999)});
    add(CC::ChangeEnemySP, QStringLiteral("Gegner-SP ändern..."), 3,
        QStringLiteral("SP eines Gegners im laufenden Kampf ändern."),
        {A(QStringLiteral("param1"), QStringLiteral("Gegner (0=alle)"), ArgSpec::Type::Int, 1, 0, 8),
         A(QStringLiteral("param2"), QStringLiteral("Änderung (+/-)"), ArgSpec::Type::SignedInt, -10, -9999, 9999)});
    add(CC::ChangeEnemyState, QStringLiteral("Gegner-Status ändern..."), 3,
        QStringLiteral("Statuseffekt eines Gegners ändern."),
        {A(QStringLiteral("param1"), QStringLiteral("Gegner (0=alle)"), ArgSpec::Type::Int, 1, 0, 8),
         A(QStringLiteral("param2"), QStringLiteral("Status-ID"), ArgSpec::Type::Int, 1, 1, 999),
         Ch(QStringLiteral("param3"), QStringLiteral("Aktion"),
            {QStringLiteral("Hinzufügen"), QStringLiteral("Entfernen")})});
    add(CC::EnemyRecoverAll, QStringLiteral("Gegner: Alles wiederherstellen"), 3,
        QStringLiteral("Stellt HP/MP eines Gegners komplett wieder her."),
        {A(QStringLiteral("param1"), QStringLiteral("Gegner (0=alle)"), ArgSpec::Type::Int, 1, 0, 8)});
    add(CC::EnemyAppearance, QStringLiteral("Gegner erscheinen lassen..."), 3,
        QStringLiteral("Lässt einen versteckten/besiegten Gegner (wieder) erscheinen."),
        {A(QStringLiteral("param1"), QStringLiteral("Gegner"), ArgSpec::Type::Int, 1, 1, 8)});
    add(CC::EnemyTransform, QStringLiteral("Gegner verwandeln..."), 3,
        QStringLiteral("Verwandelt einen Gegner in einen anderen."),
        {A(QStringLiteral("param1"), QStringLiteral("Gegner"), ArgSpec::Type::Int, 1, 1, 8),
         A(QStringLiteral("param2"), QStringLiteral("Neue Gegner-ID"), ArgSpec::Type::Int, 1, 1, 9999)});
    add(CC::ShowBattleAnimation, QStringLiteral("Kampfanimation zeigen..."), 3,
        QStringLiteral("Spielt eine Animation im Kampf ab."),
        {Txt(QStringLiteral("text"), QStringLiteral("Animationsname"))});
    add(CC::DealDamage, QStringLiteral("Schaden zufügen..."), 3,
        QStringLiteral("Fügt Gegnern oder Akteuren direkt Schaden zu."),
        {Ch(QStringLiteral("param1"), QStringLiteral("Ziel"),
            {QStringLiteral("Gegner"), QStringLiteral("Akteur")}),
         A(QStringLiteral("param2"), QStringLiteral("Index (0=alle)"), ArgSpec::Type::Int, 0, 0, 8),
         A(QStringLiteral("param3"), QStringLiteral("Schaden"), ArgSpec::Type::Int, 20, 0, 99999)});
    add(CC::ForceAction, QStringLiteral("Aktion erzwingen..."), 3,
        QStringLiteral("Erzwingt eine sofortige Kampfaktion."),
        {Txt(QStringLiteral("text"), QStringLiteral("Notiz (Aktionsart)"))});
    add(CC::AbortBattle, QStringLiteral("Kampf abbrechen"), 3,
        QStringLiteral("Bricht den laufenden Kampf sofort ab."));
    add(CC::OpenMenuScreen, QStringLiteral("Menü aufrufen"), 3,
        QStringLiteral("Öffnet das Spielmenü (Pause)."));
    add(CC::OpenSaveScreen, QStringLiteral("Speicherbildschirm aufrufen"), 3,
        QStringLiteral("Öffnet den Speicherdialog (Slot 1)."));
    add(CC::GameOver, QStringLiteral("Game Over"), 3,
        QStringLiteral("Zeigt den Game-Over-Bildschirm."));
    add(CC::ReturnToTitle, QStringLiteral("Zum Titelbildschirm zurückkehren"), 3,
        QStringLiteral("Setzt das Spiel zurück zum Titel."));
    add(CC::Script, QStringLiteral("Skript..."), 3,
        QStringLiteral("Führt Ruby-Code aus (Engine-API: Game.*, UI.*, Engine.*). Folgebefehle 'Skriptzeile' (655) werden angehängt."),
        {MTxt(QStringLiteral("text"), QStringLiteral("Ruby-Code"))});
    add(CC::SpawnEntity, QStringLiteral("Objekt spawnen (3D)..."), 3,
        QStringLiteral("Engine-Erweiterung: spawnnt ein Objekt in der Szene (Ruby-Snippet)."),
        {MTxt(QStringLiteral("text"), QStringLiteral("Snippet (z.B. Engine.spawn_cube(0,0,0))"))});

    return c;
}

const std::vector<CommandSpec> gCatalog = BuildCatalog();

QString FirstLine(const QString& s) {
    int nl = s.indexOf('\n');
    QString l = nl >= 0 ? s.left(nl) : s;
    return l;
}

} // namespace

const std::vector<CommandSpec>& GetEventCommandCatalog() {
    return gCatalog;
}

const CommandSpec* FindCommandSpec(rpg::EventCommandCode code) {
    for (const auto& s : gCatalog)
        if (s.code == code) return &s;
    return nullptr;
}

QStringList ChoiceCancelLabels() {
    return {QStringLiteral("Abbruch nicht erlaubt"),
            QStringLiteral("Wähle Option 1"), QStringLiteral("Wähle Option 2"),
            QStringLiteral("Wähle Option 3"), QStringLiteral("Wähle Option 4"),
            QStringLiteral("Abbruch-Zweig")};
}

QStringList XpButtonLabels() {
    return {QStringLiteral("2: Unten"), QStringLiteral("4: Links"),
            QStringLiteral("6: Rechts"), QStringLiteral("8: Oben"),
            QStringLiteral("11: A (Shift)"), QStringLiteral("12: B (Esc)"),
            QStringLiteral("13: C (Enter)"), QStringLiteral("15: L (Q)"),
            QStringLiteral("16: R (Tab)")};
}
int XpButtonCodeAt(int index) {
    static const int codes[] = {2, 4, 6, 8, 11, 12, 13, 15, 16};
    if (index < 0 || index >= 9) return 2;
    return codes[index];
}

// ---------------------------------------------------------------------------
// Argument-Zugriff (param1..3 / text / parameters[a0..a5])
// ---------------------------------------------------------------------------
int GetCommandArgInt(const rpg::EventCommand& cmd, const QString& key, int def) {
    if (key == QLatin1String("param1")) return cmd.param1;
    if (key == QLatin1String("param2")) return cmd.param2;
    if (key == QLatin1String("param3")) return cmd.param3;
    if (key.startsWith(QLatin1Char('a'))) {
        bool ok = false;
        int idx = key.mid(1).toInt(&ok);
        if (ok && idx >= 0 && idx < (int)cmd.parameters.size()) {
            bool okV = false;
            int v = QString::fromStdString(cmd.parameters[(size_t)idx]).toInt(&okV);
            return okV ? v : def;
        }
        return def;
    }
    return def;
}

QString GetCommandArgText(const rpg::EventCommand& cmd, const QString& key) {
    if (key == QLatin1String("text")) return QString::fromStdString(cmd.text);
    if (key.startsWith(QLatin1Char('a'))) {
        bool ok = false;
        int idx = key.mid(1).toInt(&ok);
        if (ok && idx >= 0 && idx < (int)cmd.parameters.size())
            return QString::fromStdString(cmd.parameters[(size_t)idx]);
    }
    return QString();
}

void SetCommandArgInt(rpg::EventCommand& cmd, const QString& key, int value) {
    if (key == QLatin1String("param1")) { cmd.param1 = value; return; }
    if (key == QLatin1String("param2")) { cmd.param2 = value; return; }
    if (key == QLatin1String("param3")) { cmd.param3 = value; return; }
    if (key.startsWith(QLatin1Char('a'))) {
        bool ok = false;
        int idx = key.mid(1).toInt(&ok);
        if (!ok || idx < 0) return;
        while ((int)cmd.parameters.size() <= idx) cmd.parameters.push_back("0");
        cmd.parameters[(size_t)idx] = std::to_string(value);
    }
}

void SetCommandArgText(rpg::EventCommand& cmd, const QString& key, const QString& value) {
    if (key == QLatin1String("text")) { cmd.text = value.toStdString(); return; }
    if (key.startsWith(QLatin1Char('a'))) {
        bool ok = false;
        int idx = key.mid(1).toInt(&ok);
        if (!ok || idx < 0) return;
        while ((int)cmd.parameters.size() <= idx) cmd.parameters.push_back(std::string());
        cmd.parameters[(size_t)idx] = value.toStdString();
    }
}

// ---------------------------------------------------------------------------
// Finalisierung nach Dialog-OK (Flags packen / Optionen kodieren)
// ---------------------------------------------------------------------------
void FinalizeEventCommand(rpg::EventCommand& cmd) {
    if (cmd.code == CC::ShowChoices) {
        // Kanonisches Layout: a0..a3 = Optionen ("" = leer), a4 = Prompt.
        // Engine liest die gepackte Form aus text: "prompt|o1|o2|..."
        QStringList opts;
        for (int i = 0; i < 4; ++i) {
            QString o = GetCommandArgText(cmd, QStringLiteral("a%1").arg(i)).trimmed();
            if (!o.isEmpty()) opts << o;
        }
        if (opts.isEmpty()) opts << QStringLiteral("Ja") << QStringLiteral("Nein");
        cmd.param1 = (int)opts.size();
        QString prompt = GetCommandArgText(cmd, QStringLiteral("a4"));
        QString packed = prompt;
        for (const QString& o : opts) packed += QLatin1Char('|') + o;
        cmd.text = packed.toStdString();
        cmd.parameters.assign(5, std::string());
        for (int i = 0; i < (int)opts.size(); ++i)
            cmd.parameters[(size_t)i] = opts[(size_t)i].toStdString();
        cmd.parameters[4] = prompt.toStdString();
        return;
    }
    if (cmd.code == CC::SetMoveRoute) {
        // a0/a1/a2 (Bools) -> param2 Bit-Flags fuer die Engine
        int flags = 0;
        if (GetCommandArgInt(cmd, QStringLiteral("a0"), 1)) flags |= 1; // wiederholen
        if (GetCommandArgInt(cmd, QStringLiteral("a1"), 1)) flags |= 2; // ueberspringbar
        if (GetCommandArgInt(cmd, QStringLiteral("a2"), 0)) flags |= 4; // warten
        cmd.param2 = flags;
        return;
    }
    if (cmd.code == CC::BattleProcessing) {
        int flags = 0;
        if (GetCommandArgInt(cmd, QStringLiteral("a0"), 1)) flags |= 1; // Flucht erlaubt
        if (GetCommandArgInt(cmd, QStringLiteral("a1"), 1)) flags |= 2; // Niederlage moeglich
        cmd.param2 = flags;
        // a0/a1 als parameters entfernen (nur param2 ist kanonisch)
        cmd.parameters.clear();
        return;
    }
    if (cmd.code == CC::ChangePictureColorTone) {
        // Dialog liefert 0..100 -> Engine erwartet 0.0..1.0 in parameters[0]
        int v = GetCommandArgInt(cmd, QStringLiteral("a0"), 100);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.2f", v / 100.0);
        cmd.parameters.assign(1, buf);
        return;
    }
    if (cmd.code == CC::ChangeScreenColorTone) {
        // a0 = Grau, a1 = Dauer(s) -> parameters[0],[1]
        int grey = GetCommandArgInt(cmd, QStringLiteral("a0"), 0);
        int dur = GetCommandArgInt(cmd, QStringLiteral("a1"), 1);
        cmd.parameters.assign(2, std::string());
        cmd.parameters[0] = std::to_string(grey);
        cmd.parameters[1] = std::to_string(dur);
        return;
    }
    if (cmd.code == CC::ScreenFlash) {
        int pwr = GetCommandArgInt(cmd, QStringLiteral("a0"), 160);
        int dur = GetCommandArgInt(cmd, QStringLiteral("a1"), 1);
        cmd.parameters.assign(2, std::string());
        cmd.parameters[0] = std::to_string(pwr);
        cmd.parameters[1] = std::to_string(dur);
        return;
    }
    // ShowChoices Re-Edit: Wenn ein alter Befehl nur gepackten text hat,
    // einmalig in a0..a4 aufschluesseln:
    // (wird vom Dialog vor dem Befuellen gerufen - siehe DecodeCommandForEdit in der Doku)
}

void DecodeCommandForEdit(rpg::EventCommand& cmd) {
    if (cmd.code == CC::ShowChoices && cmd.parameters.size() < 4) {
        QString packed = QString::fromStdString(cmd.text);
        QStringList parts = packed.split(QLatin1Char('|'));
        QString prompt = parts.isEmpty() ? QString() : parts.takeFirst();
        cmd.parameters.assign(5, std::string());
        for (int i = 0; i < (int)parts.size() && i < 4; ++i)
            cmd.parameters[(size_t)i] = parts[(size_t)i].toStdString();
        cmd.parameters[4] = prompt.toStdString();
        return;
    }
    if (cmd.code == CC::SetMoveRoute && cmd.parameters.size() < 3) {
        cmd.parameters.assign(3, "0");
        cmd.parameters[0] = (cmd.param2 & 1) ? "1" : "0";
        cmd.parameters[1] = (cmd.param2 & 2) ? "1" : "0";
        cmd.parameters[2] = (cmd.param2 & 4) ? "1" : "0";
        return;
    }
    if (cmd.code == CC::BattleProcessing && cmd.parameters.size() < 2) {
        cmd.parameters.assign(2, "0");
        cmd.parameters[0] = (cmd.param2 & 1) ? "1" : "0";
        cmd.parameters[1] = (cmd.param2 & 2) ? "1" : "0";
        return;
    }
}

// ---------------------------------------------------------------------------
// Fortsetzungszeilen (401/408/655)
// ---------------------------------------------------------------------------
namespace {
rpg::EventCommandCode ContinuationCodeFor(rpg::EventCommandCode code) {
    switch (code) {
        case CC::ShowText: return CC::TextLine;
        case CC::Comment:  return CC::CommentLine;
        case CC::Script:   return CC::ScriptLine;
        default:           return CC::None;
    }
}
/// Zerlegt mehrzeiligen Text: erste Zeile in den Kopfbefehl, Rest als
/// Fortsetzungsbefehle (401/408/655) mit gleichem Einzug.
void AppendWithContinuation(std::vector<rpg::EventCommand>& out, rpg::EventCommand& head) {
    const rpg::EventCommandCode cont = ContinuationCodeFor(head.code);
    if (cont == CC::None) { out.push_back(head); return; }
    QString full = QString::fromStdString(head.text);
    QStringList lines = full.split(QLatin1Char('\n'));
    head.text = lines.isEmpty() ? std::string() : lines.takeFirst().toStdString();
    out.push_back(head);
    for (const QString& l : lines) {
        rpg::EventCommand c;
        c.code = cont;
        c.indent = head.indent;
        c.text = l.toStdString();
        out.push_back(c);
    }
}
} // namespace

int ContinuationLineCount(const std::vector<rpg::EventCommand>& list, int headerIndex) {
    if (headerIndex < 0 || headerIndex >= (int)list.size()) return 0;
    const rpg::EventCommandCode cont = ContinuationCodeFor(list[(size_t)headerIndex].code);
    if (cont == CC::None) return 0;
    int n = 0;
    for (size_t i = (size_t)headerIndex + 1; i < list.size() && list[i].code == cont; ++i) ++n;
    return n;
}

QString JoinContinuationLines(const std::vector<rpg::EventCommand>& list, int headerIndex) {
    if (headerIndex < 0 || headerIndex >= (int)list.size()) return QString();
    QString full = QString::fromStdString(list[(size_t)headerIndex].text);
    const rpg::EventCommandCode cont = ContinuationCodeFor(list[(size_t)headerIndex].code);
    if (cont == CC::None) return full;
    for (size_t i = (size_t)headerIndex + 1; i < list.size() && list[i].code == cont; ++i) {
        full += QLatin1Char('\n');
        full += QString::fromStdString(list[i].text);
    }
    return full;
}

// ---------------------------------------------------------------------------
// Block-Aufbau (Choices/Bedingung/Schleife)
// ---------------------------------------------------------------------------
std::vector<rpg::EventCommand> BuildCommandBlock(const rpg::EventCommand& edited, int indent) {
    std::vector<rpg::EventCommand> out;
    rpg::EventCommand c = edited;
    c.indent = indent;

    FinalizeEventCommand(c);

    if (ContinuationCodeFor(c.code) != CC::None) {
        // Text zeigen / Kommentar / Skript: mehrzeilig -> Kopf + Folgezeilen
        AppendWithContinuation(out, c);
        return out;
    }

    if (c.code == CC::ShowChoices) {
        out.push_back(c);
        QStringList opts;
        for (int i = 0; i < 4; ++i) {
            QString o = GetCommandArgText(c, QStringLiteral("a%1").arg(i)).trimmed();
            if (!o.isEmpty()) opts << o;
        }
        if (opts.isEmpty()) opts << QStringLiteral("Ja") << QStringLiteral("Nein");
        // XP-Konvention: "Wenn"-Koepfe haben denselben Einzug wie der
        // ShowChoices-Kopf (Interpreter vergleicht mBranch[indent]!).
        for (int i = 0; i < (int)opts.size(); ++i) {
            rpg::EventCommand w;
            w.code = CC::WhenChoice;
            w.indent = indent;
            w.param1 = i;
            w.text = opts[(size_t)i].toStdString();
            out.push_back(w);
        }
        if (c.param2 == 5) { // Abbruch-Zweig
            rpg::EventCommand wc;
            wc.code = CC::WhenCancel;
            wc.indent = indent;
            out.push_back(wc);
        }
        rpg::EventCommand end;
        end.code = CC::ChoicesEnd;
        end.indent = indent;
        out.push_back(end);
        return out;
    }

    if (c.code == CC::ConditionalBranch) {
        out.push_back(c);
        rpg::EventCommand e;
        // Sonst-Zweig (a5 transient-Feld des Spezialdialogs)
        if (GetCommandArgInt(c, QStringLiteral("a5"), 0) == 1) {
            e.code = CC::Else;
            e.indent = indent;
            out.push_back(e);
        }
        rpg::EventCommand end;
        end.code = CC::BranchEnd;
        end.indent = indent;
        out.push_back(end);
        return out;
    }

    if (c.code == CC::Loop) {
        out.push_back(c);
        rpg::EventCommand rep;
        rep.code = CC::RepeatAbove;
        rep.indent = indent;
        out.push_back(rep);
        return out;
    }

    if (c.code == CC::BattleProcessing) {
        // XP legt automatisch "Wenn Sieg/Flucht/Niederlage"-Zweige an
        // (gleicher Einzug wie der Kopf, kein explizites Ende).
        out.push_back(c);
        // Finalize() hat a0/a1 bereits nach param2 gebitpackt
        const bool canEscape = (c.param2 & 1) != 0;
        const bool canLose = (c.param2 & 2) != 0;
        rpg::EventCommand win; win.code = CC::IfWin; win.indent = indent;
        out.push_back(win);
        if (canEscape) {
            rpg::EventCommand esc; esc.code = CC::IfEscape; esc.indent = indent;
            out.push_back(esc);
        }
        if (canLose) {
            rpg::EventCommand lose; lose.code = CC::IfLose; lose.indent = indent;
            out.push_back(lose);
        }
        return out;
    }

    out.push_back(c);
    return out;
}

// ---------------------------------------------------------------------------
// Anzeigeformatierung (Befehlsliste, XP-Stil)
// ---------------------------------------------------------------------------
QString FormatEventCommand(const rpg::EventCommand& cmd, int eventContext) {
    (void)eventContext;
    switch (cmd.code) {
    case CC::ShowText:
        return QL("Text: %1").arg(FirstLine(QString::fromStdString(cmd.text)));
    case CC::TextLine:
        return QL("      %1").arg(QString::fromStdString(cmd.text));
    case CC::ShowChoices: {
        QString packed = QString::fromStdString(cmd.text);
        QStringList parts = packed.split(QLatin1Char('|'));
        QString prompt = parts.isEmpty() ? QString() : parts.takeFirst();
        QString t = QL("Auswahl zeigen: ");
        for (const QString& p : parts) t += QL("[%1] ").arg(p);
        if (!prompt.isEmpty()) t += QL(" (%1)").arg(prompt);
        return t.trimmed();
    }
    case CC::WhenChoice:
        return QL("Wenn [%1]").arg(QString::fromStdString(cmd.text));
    case CC::WhenCancel:
        return QL("Wenn Abbruch");
    case CC::ChoicesEnd:
        return QL("Ende der Auswahl");
    case CC::InputNumber:
        return QL("Zahlen eingeben: Var %1, %2 Stellen").arg(cmd.param1).arg(cmd.param2);
    case CC::ChangeTextOptions:
        return QL("Text-Optionen: Position %1, Fenster %2").arg(cmd.param1).arg(cmd.param2);
    case CC::ButtonInputProcessing:
        return QL("Tastenabfrage: Var %1").arg(cmd.param1);
    case CC::Wait:
        return QL("Warten: %1 Frames").arg(cmd.param1);
    case CC::Comment:
        return QL("Kommentar: %1").arg(FirstLine(QString::fromStdString(cmd.text)));
    case CC::CommentLine:
        return QL("      : %1").arg(QString::fromStdString(cmd.text));
    case CC::ConditionalBranch: {
        switch (cmd.param1) {
        // XP-Kodierung der Bedingungen: param3 == 0 -> "ist AN", 1 -> "ist AUS"
        case 0: return QL("Bedingung: Schalter %1 ist %2").arg(cmd.param2).arg(OnOff(cmd.param3 == 0));
        case 1: {
            static const char* ops[] = {"==", ">=", "<=", ">", "<", "!="};
            QString op = (cmd.param3 >= 0 && cmd.param3 < 6) ? QString::fromUtf8(ops[cmd.param3]) : QL("==");
            QString rhs;
            QString kind = GetCommandArgText(cmd, QL("a0"));
            if (kind == QLatin1String("1"))
                rhs = QL("Var %1").arg(GetCommandArgText(cmd, QL("a1")));
            else if (kind == QLatin1String("2"))
                rhs = QL("Zufall %1..%2").arg(GetCommandArgText(cmd, QL("a1"))).arg(GetCommandArgText(cmd, QL("a2")));
            else
                rhs = GetCommandArgText(cmd, QL("a1"));
            return QL("Bedingung: Var %1 %2 %3").arg(cmd.param2).arg(op, rhs);
        }
        case 2: return QL("Bedingung: Selbstschalter %1 ist %2")
                    .arg(QString::fromStdString(cmd.text)).arg(OnOff(cmd.param3 == 0));
        case 3: return QL("Bedingung: Timer %1 %2 s")
                    .arg(cmd.param3 == 0 ? QL(">=") : QL("<=")).arg(cmd.param2);
        case 4: {
            static const char* kinds[] = {"in der Gruppe", "Name ist", "hat Fertigkeit",
                                          "Waffe %1", "Rüstung %1", "hat Status %1"};
            QString k = (cmd.param3 >= 0 && cmd.param3 < 6) ? QString::fromUtf8(kinds[cmd.param3]) : QL("?");
            if (k.contains(QLatin1String("%1")))
                k = k.arg(GetCommandArgText(cmd, QL("a0")));
            else if (cmd.param3 == 1)
                k += QL(" \"%1\"").arg(GetCommandArgText(cmd, QL("a0")));
            return QL("Bedingung: Akteur %1 %2").arg(cmd.param2).arg(k);
        }
        case 5: return QL("Bedingung: Gegner %1 %2").arg(cmd.param2)
                    .arg(cmd.param3 == 0 ? QL("erschienen") : QL("Status"));
        case 6: {
            static const QMap<int, QString> dirs = {{2, QL("Unten")}, {4, QL("Links")},
                                                    {6, QL("Rechts")}, {8, QL("Oben")}};
            int evId = cmd.param2 > 0 ? cmd.param2 : 0;
            QString name = evId > 0 ? QL("Event %1").arg(evId) : QL("Dieses Event");
            return QL("Bedingung: %1 blickt nach %2").arg(name, dirs.value(cmd.param3, QL("?")));
        }
        case 7: return QL("Bedingung: Geld %1 %2").arg(cmd.param3 == 0 ? QL(">=") : QL("<=")).arg(cmd.param2);
        case 8: return QL("Bedingung: Gegenstand %1 vorhanden").arg(cmd.param2);
        case 9: return QL("Bedingung: Waffe %1 vorhanden").arg(cmd.param2);
        case 10: return QL("Bedingung: Rüstung %1 vorhanden").arg(cmd.param2);
        case 11: return QL("Bedingung: Taste %1 gedrückt").arg(cmd.param2);
        case 12: return QL("Bedingung: Skript: %1").arg(FirstLine(QString::fromStdString(cmd.text)));
        default: return QL("Bedingung: (unbekannt)");
        }
    }
    case CC::Else:
        return QL("Sonst");
    case CC::BranchEnd:
        return QL("Ende der Bedingung");
    case CC::Loop:
        return QL("Schleife");
    case CC::BreakLoop:
        return QL("Schleife verlassen");
    case CC::RepeatAbove:
        return QL("Wiederhole von oben");
    case CC::ExitEventProcessing:
        return QL("Event-Verarbeitung beenden");
    case CC::EraseEvent:
        return QL("Event löschen");
    case CC::CallCommonEvent:
        return QL("Gemeinsames Event %1 aufrufen").arg(cmd.param1);
    case CC::Label:
        return QL("Marke: %1").arg(QString::fromStdString(cmd.text));
    case CC::JumpToLabel:
        return QL("Springe zu Marke: %1").arg(QString::fromStdString(cmd.text));
    case CC::ControlSwitches:
        return cmd.param1 == cmd.param2
            ? QL("Schalter %1 = %2").arg(cmd.param1).arg(OnOff(cmd.param3 != 0))
            : QL("Schalter %1..%2 = %3").arg(cmd.param1).arg(cmd.param2).arg(OnOff(cmd.param3 != 0));
    case CC::ControlVariables: {
        static const char* ops[] = {"=", "+=", "-=", "*=", "/=", "%="};
        QString op = (cmd.param3 >= 0 && cmd.param3 < 6) ? QString::fromUtf8(ops[cmd.param3]) : QL("=");
        QString rhs;
        QString kind = GetCommandArgText(cmd, QL("a0"));
        if (kind == QLatin1String("1")) rhs = QL("Var %1").arg(GetCommandArgText(cmd, QL("a1")));
        else if (kind == QLatin1String("2")) rhs = QL("Zufall %1..%2")
            .arg(GetCommandArgText(cmd, QL("a1"))).arg(GetCommandArgText(cmd, QL("a2")));
        else rhs = GetCommandArgText(cmd, QL("a1"));
        return cmd.param1 == cmd.param2
            ? QL("Variable %1 %2 %3").arg(cmd.param1).arg(op, rhs)
            : QL("Variable %1..%2 %3 %4").arg(cmd.param1).arg(cmd.param2).arg(op, rhs);
    }
    case CC::ControlSelfSwitch:
        return QL("Selbstschalter %1 = %2").arg(QString::fromStdString(cmd.text)).arg(OnOff(cmd.param3 != 0));
    case CC::ControlTimer:
        return cmd.param1 == 0 ? QL("Timer starten: %1 s").arg(cmd.param2) : QL("Timer stoppen");
    case CC::ChangeGold:
        return QL("Geld: %1").arg(SignNum(cmd.param1));
    case CC::ChangeItems:
        return QL("Gegenstand %1: %2 Stk.").arg(cmd.param1).arg(SignNum(cmd.param2));
    case CC::ChangeWeapons:
        return QL("Waffe %1: %2 Stk.").arg(cmd.param1).arg(SignNum(cmd.param2));
    case CC::ChangeArmor:
        return QL("Rüstung %1: %2 Stk.").arg(cmd.param1).arg(SignNum(cmd.param2));
    case CC::ChangePartyMember:
        return cmd.param3 == 0 ? QL("Gruppenmitglied %1 hinzufügen").arg(cmd.param1)
                               : QL("Gruppenmitglied %1 entfernen").arg(cmd.param1);
    case CC::ChangeWindowskin:
        return QL("Windowskin: %1").arg(QString::fromStdString(cmd.text));
    case CC::ChangeBattleBGM:
        return QL("Kampf-BGM: %1").arg(QString::fromStdString(cmd.text));
    case CC::ChangeBattleEndME:
        return QL("Sieg-ME: %1").arg(QString::fromStdString(cmd.text));
    case CC::ChangeSaveAccess:
        return QL("Speichern: %1").arg(cmd.param1 != 0 ? QL("erlaubt") : QL("gesperrt"));
    case CC::ChangeMenuAccess:
        return QL("Menü: %1").arg(cmd.param1 != 0 ? QL("erlaubt") : QL("gesperrt"));
    case CC::ChangeEncounter:
        return QL("Zufallskämpfe: %1").arg(cmd.param1 != 0 ? QL("erlaubt") : QL("gesperrt"));
    case CC::TransferPlayer:
        return cmd.param3 > 0
            ? QL("Spieler transferieren: Map %1, (%2,%3)").arg(cmd.param3).arg(cmd.param1).arg(cmd.param2)
            : QL("Spieler transferieren: (%1,%2)").arg(cmd.param1).arg(cmd.param2);
    case CC::SetEventLocation:
        return QL("Event %1 nach (%2,%3)").arg(cmd.param1).arg(cmd.param2).arg(cmd.param3);
    case CC::ChangeTransparentFlag:
        return QL("Spieler-Transparenz: %1").arg(cmd.param1 != 0 ? QL("Transparent") : QL("Normal"));
    case CC::SetMoveRoute: {
        QString t = QL("Bewegungsroute setzen: %1").arg(QString::fromStdString(cmd.text));
        if (cmd.param2 & 4) t += QL(" (warten)");
        return t;
    }
    case CC::WaitForMoveCompletion:
        return QL("Auf Bewegung warten");
    case CC::ShowAnimation:
        return QL("Animation %1 zeigen").arg(cmd.param2);
    case CC::ShowPicture:
        return QL("Bild zeigen: %1 @(%2,%3)").arg(QString::fromStdString(cmd.text)).arg(cmd.param1).arg(cmd.param2);
    case CC::MovePicture:
        return QL("Bild %1 bewegen nach (%2,%3)").arg(cmd.param1).arg(cmd.param2).arg(cmd.param3);
    case CC::RotatePicture:
        return QL("Bild %1 drehen: %2°").arg(cmd.param1).arg(cmd.param2);
    case CC::ErasePicture:
        return QL("Bild %1 löschen").arg(cmd.param1);
    case CC::SetWeatherEffects:
        return QL("Wetter: Typ %1, Stärke %2").arg(cmd.param1).arg(cmd.param2);
    case CC::PlayBGM:
        return QL("BGM: %1").arg(QString::fromStdString(cmd.text));
    case CC::FadeOutBGM:
        return QL("BGM ausblenden (%1 s)").arg(cmd.param1);
    case CC::PlayBGS:
        return QL("BGS: %1").arg(QString::fromStdString(cmd.text));
    case CC::PlayME:
        return QL("ME: %1").arg(QString::fromStdString(cmd.text));
    case CC::PlaySE:
        return QL("SE: %1").arg(QString::fromStdString(cmd.text));
    case CC::StopSE:
        return QL("SE stoppen");
    case CC::BattleProcessing:
        return QL("Kampf: Truppe %1").arg(cmd.param1);
    case CC::IfWin:
        return QL("Wenn Sieg");
    case CC::IfEscape:
        return QL("Wenn Flucht");
    case CC::IfLose:
        return QL("Wenn Niederlage");
    case CC::ShopProcessing:
        return QL("Laden: %1").arg(QString::fromStdString(cmd.text));
    case CC::NameInputProcessing:
        return QL("Namenseingabe: Akteur %1 (%2 Zeichen)").arg(cmd.param1).arg(cmd.param2);
    case CC::ChangeHP:
        return QL("HP Akteur %1: %2").arg(cmd.param1).arg(SignNum(cmd.param2));
    case CC::ChangeSP:
        return QL("SP Akteur %1: %2").arg(cmd.param1).arg(SignNum(cmd.param2));
    case CC::ChangeState:
        return QL("Status %2 bei Akteur %1: %3").arg(cmd.param1).arg(cmd.param2)
            .arg(cmd.param3 == 0 ? QL("hinzufügen") : QL("entfernen"));
    case CC::RecoverAll:
        return cmd.param1 > 0 ? QL("Akteur %1 komplett heilen").arg(cmd.param1)
                              : QL("Gruppe komplett heilen");
    case CC::ChangeEXP:
        return QL("EXP Akteur %1: %2").arg(cmd.param1).arg(SignNum(cmd.param2));
    case CC::ChangeLevel:
        return QL("Level Akteur %1 = %2").arg(cmd.param1).arg(cmd.param2);
    case CC::ChangeParameters:
        return QL("Parameter Akteur %1: Wert %2 %+3").arg(cmd.param1).arg(cmd.param2).arg(cmd.param3);
    case CC::ChangeSkills:
        return QL("Fertigkeit %2, Akteur %1: %3").arg(cmd.param1).arg(cmd.param2)
            .arg(cmd.param3 == 0 ? QL("lernen") : QL("vergessen"));
    case CC::ChangeEquipment:
        return QL("Ausrüstung Akteur %1: Slot %2 -> %3").arg(cmd.param1).arg(cmd.param2).arg(cmd.param3);
    case CC::ChangeActorName:
        return QL("Akteur %1 heißt jetzt \"%2\"").arg(cmd.param1).arg(QString::fromStdString(cmd.text));
    case CC::ChangeActorClass:
        return QL("Klasse Akteur %1 = %2").arg(cmd.param1).arg(cmd.param2);
    case CC::ChangeActorGraphic:
        return QL("Grafik Akteur %1 = %2").arg(cmd.param1).arg(QString::fromStdString(cmd.text));
    case CC::ChangeEnemyHP:
        return QL("HP Gegner %1: %2").arg(cmd.param1).arg(SignNum(cmd.param2));
    case CC::AbortBattle:
        return QL("Kampf abbrechen");
    case CC::OpenMenuScreen:
        return QL("Menü aufrufen");
    case CC::OpenSaveScreen:
        return QL("Speicherbildschirm aufrufen");
    case CC::GameOver:
        return QL("Game Over");
    case CC::ReturnToTitle:
        return QL("Zum Titelbildschirm");
    case CC::Script:
        return QL("Skript: %1").arg(FirstLine(QString::fromStdString(cmd.text)));
    case CC::ScriptLine:
        return QL("      $ %1").arg(QString::fromStdString(cmd.text));
    case CC::ShowScreenText:
        return QL("HUD-Text: %1").arg(FirstLine(QString::fromStdString(cmd.text)));
    case CC::ShowWorldText:
        return QL("Welt-Text: %1").arg(FirstLine(QString::fromStdString(cmd.text)));
    case CC::ClearScreenTexts:
        return QL("HUD-Texte löschen");
    case CC::SetTimeOfDay:
        return QL("Tageszeit: %1:%2").arg(cmd.param1, 2, 10, QLatin1Char('0'))
            .arg(cmd.param2, 2, 10, QLatin1Char('0'));
    case CC::SpawnEntity:
        return QL("Objekt spawnen (3D)");
    default:
        if (const CommandSpec* spec = FindCommandSpec(cmd.code))
            return spec->label;
        return QL("Code %1").arg((int)cmd.code);
    }
}

} // namespace qt_editor
