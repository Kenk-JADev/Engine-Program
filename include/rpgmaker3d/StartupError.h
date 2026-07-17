#pragma once
// StartupError.h – Gemeinsamer, leichtgewichtiger Fehler-Helfer für die
// Einstiegspunkte (SDL main.cpp und Qt QtMain.cpp).
//
// Problem, das dieser Helfer löst:
//   Eine einzige ungefangene Exception während der Engine-/Fenster-
//   Initialisierung führt zu std::terminate -> das Programm schließt
//   *ohne* sichtbare Fehlermeldung („Konsole blinkt kurz, dann zu").
//
// Dieser Helfer sorgt dafür, dass solche Fehler
//   1) in engine.log geschrieben werden (auch wenn der Logger noch nicht
//      initialisiert ist) und
//   2) dem Benutzer über ein Meldungsfenster angezeigt werden.
//
// Damit wird aus einem lautlosen Absturz ein reproduzierbarer, meldender
// Fehler – und der echte Grund ist im engine.log nachlesbar.

#include <string>
#include <exception>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdlib>

#include "rpgmaker3d/Platform.h"

namespace rpg {

// Schreibt eine Startup-Fehlermeldung nach engine.log (append) und zeigt
// gleichzeitig ein Meldungsfenster an. Funktionert auch *vor* der
// Engine-Logger-Initialisierung.
inline void ReportStartupError(const std::string& where, const std::string& what) {
    try {
        std::time_t t = std::time(nullptr);
        char buf[64] = {0};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

        std::ofstream log("engine.log", std::ios::out | std::ios::app);
        if (log.good()) {
            log << "[" << buf << "] [FATAL-STARTUP] " << where << ": " << what << "\n";
            log << "  (Engine wurde vor der Initialisierung oder beim Start abgebrochen.)\n";
        }
    } catch (...) {
        // Niemals selbst crashien.
    }

    std::string msg = where + ":\n\n" + what +
                      "\n\nDetails stehen in engine.log.";
    Platform::ShowMessageBox("RPG Maker 3D – Start fehlgeschlagen", msg, true);
}

// Installiert einen globalen std::terminate-Handler, der die aktuelle
// Exception abfängt, loggt und meldet, bevor das Programm endet.
// Muss *früh* (ganz am Anfang von main) aufgerufen werden.
inline void InstallStartupTerminateHandler() {
    std::set_terminate([]() {
        std::string what = "Unbekannter Fehler (std::terminate, keine Exception-Info).";
        if (auto ex = std::current_exception()) {
            try {
                std::rethrow_exception(ex);
            } catch (const std::exception& e) {
                what = std::string("Exception: ") + e.what();
            } catch (const std::string& s) {
                what = "Exception (string): " + s;
            } catch (...) {
                what = "Unbekannter Exception-Typ.";
            }
        }
        ReportStartupError("Schwerwiegender Startfehler (std::terminate)", what);
        // std::terminate muss terminieren – wir aborten sauber nach Meldung.
        std::abort();
    });
}

} // namespace rpg
