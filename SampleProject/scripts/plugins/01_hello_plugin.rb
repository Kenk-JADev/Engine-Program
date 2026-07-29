# @name    Hello Plugin
# @version 1.0
# @author  RPG Maker 3D
# @desc    Beispiel-Plugin: zeigt Plugin-Metadaten-Konvention + Lade-Log.
# Beispiel-Plugin – wird geladen falls plugins/ gescannt wird.
# Metadaten im Kopf (# @name/@version/@author/@desc) liest der ScriptManager
# ein (GetPluginInfos) — so listet ein Plugin-Manager sie spaeter im Editor.
Engine.log("[Plugin] Hello Plugin v1.0 geladen") if Object.const_defined?(:Engine)
