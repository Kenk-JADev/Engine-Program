#pragma once
// XP-Tileset-Flag-Raster (Paket 2, TODO_XP_PARITY.md)
// Zeigt die Tileset-Grafik als 8-Spalten-Raster (wie beim RPG Maker XP)
// und erlaubt das Bearbeiten der XP-Flag-Tabellen per Mausklick:
//   - Durchgang        : gruener Kreis = frei, rotes X = blockiert
//   - Durchgang 4-Dir  : Pfeile = freie Richtungen, X = alle gesperrt
//   - Prioritaet       : Zahl 0..5
//   - Busch-Flag       : "B" (Durchwiese)
//   - Tresen-Flag      : "C" (Counter)
//   - Terrain-Tag      : Zahl 0..7
// ES WERDEN KEINE Qt-EDITOR-KLASSEN VERWENDET (Zirkel-Include vermeiden).

#include <functional>
#include <QImage>
#include <QWidget>

#include "rpgmaker3d/Database.h"

namespace qt_editor {

class QtTilesetGridWidget : public QWidget {
    Q_OBJECT
public:
    enum Mode {
        ModePassage,   // Durchgang
        ModePassage4,  // Durchgang 4-Dir
        ModePriority,  // Prioritaet
        ModeBush,      // Busch-Flag
        ModeCounter,   // Tresen-Flag
        ModeTerrain    // Terrain-Tag
    };

    explicit QtTilesetGridWidget(QWidget* parent = nullptr);

    /// Verknuepft das Widget mit den Flag-Tabellen eines Tilesets (Zeiger-Alias,
    /// Besitz bleibt beim Aufrufer). Aufruf NACH jeder Tab-Auswahl.
    void setData(rpg::TilesetData* data);

    /// Laedt die Tileset-Grafik, baut das Raster (8 Spalten wie XP), legt
    /// die Flag-Vektoren auf die Tileanzahl an und zeichnet neu.
    void loadImage(const QString& absPath);

    /// Aktiver Bearbeitungsmodus (Klick-Verhalten + Overlay).
    void setMode(Mode m);

    int tileCount() const { return (int)mTiles.size(); }

signals:
    /// Ein Flag wurde per Klick geaendert -> Parent kann "geaendert" markieren.
    void flagsChanged();

private:
    // Klick-Verhalten pro Modus (Rechtsklick = rueckgaengig/Standard)
    void applyLeftClick(int tileId);
    void applyRightClick(int tileId);

    // zieht die Flag-Vektoren auf die aktuelle Tileanzahl hoch
    void ensureSizesIfPossible();

    // Overlay-Malerei pro Modus
    void paintPassage(QPainter& p, const QRectF& rc, int tileId);
    void paintPassage4(QPainter& p, const QRectF& rc, int tileId);
    void paintPriority(QPainter& p, const QRectF& rc, int tileId);
    void paintBush(QPainter& p, const QRectF& rc, int tileId);
    void paintCounter(QPainter& p, const QRectF& rc, int tileId);
    void paintTerrain(QPainter& p, const QRectF& rc, int tileId);

    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

    rpg::TilesetData* mData = nullptr;
    QImage mImage;          // skalierte Tileset-Grafik
    int mTileW = 32;        // logische Tile-Groesse in der Originalgrafik
    int mTilesX = 8;        // XP: immer 8 Tiles pro Zeile
    int mTilesY = 0;
    Mode mMode = ModePassage;

    // Wiederholt benutzte Farben/Pens werden lokal in den paint*-Methoden
    // angelegt (QPainter-Benutzung ist damit headerarm).
};

} // namespace qt_editor
