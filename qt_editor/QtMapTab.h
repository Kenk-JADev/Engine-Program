#pragma once
// "Landkarte"-Tab (XP-Stil): 2D-Draufsicht auf dieselbe Karte, aus der das
// 3D-Mapsystem seine Geometrie baut (Engine::GetMap) — Malen hier wirkt
// also direkt auf die 3D-Welt. Oben Ebenen-Buttons 1/2/3 + Ereignis-Modus
// (EV) wie in RPG Maker XP.

#include <QWidget>
#include <functional>
#include <map>
#include <vector>
#include <QtGlobal>

class QScrollArea;
class QSpinBox;
class QLabel;
class QToolButton;
class QButtonGroup;
class QGridLayout;
class QPoint;

namespace rpg { class Engine; }

namespace qt_editor {

class QtMapTabCanvas;

class QtMapTab : public QWidget {
    Q_OBJECT
public:
    explicit QtMapTab(rpg::Engine* engine, QWidget* parent = nullptr);

    /// Nach Projekt-/Kartenwechsel aufrufen
    void refresh();

    /// Vom Map-Dock übergeben: aktives Tile (-1 = Radierer) + Ebene/Modus
    void setPaintTile(int tileId);
    void setPaintLayer(int layer);          // 0..2 = Ebene 1..3, >=3 = EV-Modus
    int paintTile() const { return mTileId; }
    int paintLayer() const;                 // 0..2 = Ebene, 3 = Ereignis-Modus

    /// Liefert die ID der Karte, deren Ereignisse bearbeitet/gespeichert werden
    void setCurrentMapIdFn(std::function<int()> fn) { mMapIdFn = std::move(fn); }

    /// Verlauf der 2D-Mal-Schritte (Strg+Z / Strg+Y)
    void undo();
    void redo();

signals:
    void logMessage(const QString& msg);
    /// Tiles wurden in der 2D-Ansicht geändert (3D aktualisieren)
    void tilesChanged();
    /// Ereignisse wurden erstellt/geändert/gelöscht (Events-Dock neu laden)
    void eventsChanged();
    /// In der XP-Palette des Tabs wurde ein Tile gewählt (Dock synchronisieren)
    void paintTilePicked(int tileId);
    /// Text für die Statuszeile (Feld-Koordinaten, Modus-Hinweise)
    void hoverInfo(const QString& text);
    /// Rechtsklick-Menü: Karteneigenschaften angefordert (EditorWindow öffnet den Dialog)
    void mapPropertiesRequested();

private:
    void onModeButton(int id);              // 0..2 = Ebene, 3 = EV-Modus
    void createOrEditEventAt(int x, int z); // Doppelklick im EV-Modus
    void deleteSelectedEvent();             // Entf im EV-Modus
    void ensureLayers(int n);               // Karte auf mind. n Ebenen bringen
    int currentMapId() const;
    void saveMapEvents();
    void rebuildPalette();                  // XP-Tileset-Palette links neu aufbauen
    void showCanvasMenu(int x, int z, const QPoint& globalPos); // Rechtsklick-Menü

    // --- Rückgängig/Wiederholen für das 2D-Malen --------------------------
    struct TileEdit { int layer, x, z, before, after; };
    struct StrokeEntry {
        int mapId = -1, w = 0, h = 0;       // nur gültig für diese Karte/Größe
        std::vector<TileEdit> edits;
    };
    void beginStroke();
    void recordEdit(int x, int z, int layer, int before, int after);
    void endStroke();
    void undoRedoImpl(std::vector<StrokeEntry>& from,
                      std::vector<StrokeEntry>& to, bool reverse);

    std::vector<StrokeEntry> mUndoStrokes;
    std::vector<StrokeEntry> mRedoStrokes;
    std::map<qint64, TileEdit> mStrokeAccum; // pro Zelle nur der erste Startwert
    bool mStrokeActive = false;

    rpg::Engine* mEngine = nullptr;
    QtMapTabCanvas* mCanvas = nullptr;
    QScrollArea* mScroll = nullptr;
    QButtonGroup* mModeGroup = nullptr;
    QToolButton* mModeBtns[4] = {};         // 1, 2, 3, EV
    QButtonGroup* mToolGroup = nullptr;     // XP-Zeichenwerkzeuge
    QToolButton* mToolBtns[4] = {};         // Stift, Rechteck, Ellipse, Fuellen
    QSpinBox* mZoomSpin = nullptr;
    QLabel* mPosLabel = nullptr;
    int mTileId = 0;
    std::function<int()> mMapIdFn;

    // XP-Palette (links vom Canvas)
    QWidget* mPaletteHost = nullptr;
    QGridLayout* mPaletteGrid = nullptr;
    QLabel* mPaletteSel = nullptr;
    void* mLastTileset = nullptr;           // Cache: Palette nur bei Wechsel neu
    void updatePaletteSelection();
};

} // namespace qt_editor
