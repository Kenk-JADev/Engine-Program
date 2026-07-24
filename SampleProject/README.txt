SampleProject für RPG Maker 3D Engine
=====================================

Sofort spielbare Demo (PAKET 25): Titel -> Neues Spiel -> erkunden!

Testparcours:
- WASD bewegen, Shift = sprinten, E/Enter = ansprechen/oeffnen, Esc = Menue
- Dorfaeltester ansprechen (gibt 50 Gold), Wegweiser lesen
- Truhe am Haus (Nordost): Gegengift + Potion
- Heilkristall am Teich (Westen): volle Heilung ( beliebig oft)
- Arena-Trainer am Dorfplatz: Uebungskampf (Flucht erlaubt)
- Sueden runter: Transfer zum Waldweg (Karte 2)
  - Zufallskaempfe: Slime/Bat-Trupps, im hohen Gras (dunkelgruen)
    doppelt so haeufig
  - versteckte Truhe: Phoenixeder (Wiederbelebung)
- Menue (Esc): Items/Skills/Ausruestung, Speichern & Laden

Ordner:
- assets/textures: PNG Tilesets (tileset_demo.png)
- assets/models: OBJ Modelle
- assets/audio: OGG/MP3/WAV Sounds
- maps: Karten + Events
    map1.map / map2.map            binaere Kartendaten (Editor-speicherbar)
    Map001_events.json             Events gruppiertes Dorf (v2-Format)
    Map002_events.json             Events Waldweg
- scripts: Ruby Scripts (.rb) + plugins/
- prefabs: Prefab Dateien (.prefab)
- database: JSON Datenbank
    Actors/Classes/Items/Skills/Weapons/Armors/Enemies/Troops/States/
    Tilesets (Passagen/Bush-Flags), MapInfos (Namen + Encounter-Listen)
- saves: Spielstaende

Karten regenerieren (Original-Generator, PAKET 25):
  python3 scripts/make_sample_maps.py   (Repository-Wurzel, scripts/-Ordner)

Starte die Engine mit:
RPGMaker3D.exe --project ./SampleProject --editor

Oder direkt testen:
RPGMaker3D_Player.exe ./SampleProject
Kampftest direkt:
RPGMaker3D_Player.exe ./SampleProject --battletest 1
