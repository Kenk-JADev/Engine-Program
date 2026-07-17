# Beispiel-Plugin – wird geladen falls plugins/ gescannt wird
Engine.log("[Plugin] hello_plugin geladen") if Object.const_defined?(:Engine)
