#pragma once
// XP-Tileset-Raster als EIN zusammenhaengendes Bild (wie im RPG Maker XP).
// Die Grafik wird kachelecht (jede 32x32-Quellstelle einzeln) in einem
// 8er-Raster gemalt — ein Klick auf eine Stelle nimmt genau dieses Tile.
// Zwei Betriebsarten:
//   - PickTile : reine Tile-Auswahl fuer die Karten-Palette (Auswahlrahmen,
//                blockierte Tiles bekommen eine kleine rote Eckmarke)
//   - EditFlags: XP-Flag-Tabellen der Datenbank bearbeiten:
//       Durchgang (gruener Kreis = frei / rotes X = blockiert)
//       Durchgang 4-Dir (Pfeile = freie Richtungen, X = alle gesperrt)
//       Prioritaet (Zahl 0..5), Busch (B), Tresen (C), Terrain-Tag (Zahl)
//       Linksklick = weiter, Rechtsklick = auf Standard zurueck.
// ES WERDEN KEINE Qt-EDITOR-KLASSEN VERWENDET (Zirkel-Include vermeiden).

#include <functional>
#include <vector>
#include <QImage>
#include <QPoint>
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

    enum Interaction {
        EditFlags,     // Datenbank-Tab: Flags per Klick bearbeiten
        PickTile       // Karten-Palette: nur ein Tile waehlen
    };

    explicit QtTilesetGridWidget(QWidget* parent = nullptr);

    /// Verknuepft das Widget mit den Flag-Tabellen eines Tilesets (Zeiger-Alias,
    /// Besitz bleibt beim Aufrufer). Nur fuer Interaction::EditFlags noetig.
    void setData(rpg::TilesetData* data);

    /// Laedt die Tileset-Grafik von Disk. Spalten/Zeilen der Quelle werden aus
    /// der Bildgroesse (32 px pro Kachel) bestimmt; angezeigt wird — wie XP —
    /// immer als 8-Spalten-Raster der einzelnen 32x32-Stellen.
    void loadImage(const QString& absPath);

    /// Setzt die Grafik direkt (z. B. GL-Textur-Readback des Map-Tilesets).
    /// srcCols/srcRows = Kachelzahl der Quelle in 32er-Einheit
    /// (0 = automatisch aus Bildgroesse).
    void setImage(const QImage& img, int srcCols = 0, int srcRows = 0);

    /// Aktiver Flag-Bearbeitungsmodus (nur Interaction::EditFlags).
    void setMode(Mode m);
    Mode mode() const { return mMode; }

    /// Betriebsart: Flags bearbeiten (Datenbank) oder Tile waehlen (Palette).
    void setInteraction(Interaction it);
    Interaction interaction() const { return mInteraction; }

    /// Zoomfaktor 1..3 (Zellgroesse = 32 * Zoom). Standard 1 (echte 32x32).
    void setZoom(int zoom);
    int zoom() const { return mZoom; }

    int tileCount() const { return mSrcCols * mSrcRows; }

    /// PickTile: aktuell gewaehltes Tile (-1 = keine Auswahl).
    int selectedTile() const { return mSelected; }
    void setSelectedTile(int tileId);

    /// PickTile: optionale Sperr-Marken (rotes Dreieck) pro Tile-ID.
    void setSolidBits(const std::vector<bool>& solid);

signals:
    /// EditFlags: ein Flag wurde per Klick geaendert.
    void flagsChanged();
    /// PickTile: Linksklick hat ein Tile gewaehlt.
    void tilePicked(int tileId);
    /// Maus steht ueber einem Tile (-1 = ausserhalb).
    void hoverTile(int tileId);

protected:
    QSize sizeHint() const override;
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    int cellSize() const { return 32 * mZoom; }
    /// Tile-ID an einer Widget-Position (oder -1 ausserhalb)
    int tileAt(const QPoint& pos) const;
    /// Anzeige-Zelle (Links/Oben) einer Tile-ID
    QPoint cellOf(int tileId) const;

    void applyLeftClick(int tileId);
    void applyRightClick(int tileId);

    // zieht die Flag-Vektoren auf die aktuelle Tileanzahl hoch
    void ensureSizesIfPossible();
    // neu bemassen (Minimum = Inhalt) + neu zeichnen
    void relayout();

    // Overlay-Malerei pro Flag-Modus
    void paintPassage(QPainter& p, const QRectF& rc, int tileId);
    void paintPassage4(QPainter& p, const QRectF& rc, int tileId);
    void paintPriority(QPainter& p, const QRectF& rc, int tileId);
    void paintBush(QPainter& p, const QRectF& rc, int tileId);
    void paintCounter(QPainter& p, const QRectF& rc, int tileId);
    void paintTerrain(QPainter& p, const QRectF& rc, int tileId);

    rpg::TilesetData* mData = nullptr;
    QImage mImage;              // Originalgrafik (unskaliert)
    int mSrcCols = 0;           // Kachel-Spalten der Quelle (32er-Raster)
    int mSrcRows = 0;
    int mDispCols = 8;          // XP: Anzeige immer 8 Spalten
    int mDispRows = 0;
    int mZoom = 1;              // 1..3
    Mode mMode = ModePassage;
    Interaction mInteraction = EditFlags;
    int mSelected = -1;         // PickTile-Auswahl
    int mHover = -1;            // Maus-Tile
    std::vector<bool> mSolid;   // PickTile: Sperr-Marken pro Tile-ID
};

} // namespace qt_editor
