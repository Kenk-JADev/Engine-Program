#pragma once
// "Landkarte"-Tab (XP-Stil): 2D-Draufsicht auf dieselbe Karte, aus der das
// 3D-Mapsystem seine Geometrie baut (Engine::GetMap) — Malen hier wirkt
// also direkt auf die 3D-Welt. Oben Ebenen-Buttons 1/2/3 + Ereignis-Modus
// (EV) wie in RPG Maker XP.

#include <QWidget>
#include <functional>

class QScrollArea;
class QSpinBox;
class QLabel;
class QToolButton;
class QButtonGroup;

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

signals:
    void logMessage(const QString& msg);
    /// Tiles wurden in der 2D-Ansicht geändert (3D aktualisieren)
    void tilesChanged();
    /// Ereignisse wurden erstellt/geändert/gelöscht (Events-Dock neu laden)
    void eventsChanged();

private:
    void onModeButton(int id);              // 0..2 = Ebene, 3 = EV-Modus
    void createOrEditEventAt(int x, int z); // Doppelklick im EV-Modus
    void deleteSelectedEvent();             // Entf im EV-Modus
    void ensureLayers(int n);               // Karte auf mind. n Ebenen bringen
    int currentMapId() const;
    void saveMapEvents();

    rpg::Engine* mEngine = nullptr;
    QtMapTabCanvas* mCanvas = nullptr;
    QScrollArea* mScroll = nullptr;
    QButtonGroup* mModeGroup = nullptr;
    QToolButton* mModeBtns[4] = {};         // 1, 2, 3, EV
    QSpinBox* mZoomSpin = nullptr;
    QLabel* mPosLabel = nullptr;
    int mTileId = 0;
    std::function<int()> mMapIdFn;
};

} // namespace qt_editor
