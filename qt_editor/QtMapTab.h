#pragma once
// "Landkarte"-Tab: 2D-Draufsicht auf die Karte mit Zoom + Klick-Malen.
// Ergänzt den 3D-View: schnelle Kartenübersicht wie im klassischen Maker.

#include <QWidget>

class QScrollArea;
class QComboBox;
class QSpinBox;
class QLabel;

namespace rpg { class Engine; }

namespace qt_editor {

class QtMapTabCanvas;

class QtMapTab : public QWidget {
    Q_OBJECT
public:
    explicit QtMapTab(rpg::Engine* engine, QWidget* parent = nullptr);

    /// Nach Projekt-/Kartenwechsel aufrufen
    void refresh();

    /// Vom Map-Dock übergeben: aktives Tile (-1 = Radierer) + Ebene
    void setPaintTile(int tileId);
    void setPaintLayer(int layer);
    int paintTile() const { return mTileId; }
    int paintLayer() const;

signals:
    void logMessage(const QString& msg);
    /// Karte wurde in der 2D-Ansicht geändert (3D aktualisieren)
    void tilesChanged();

private:
    rpg::Engine* mEngine = nullptr;
    QtMapTabCanvas* mCanvas = nullptr;
    QScrollArea* mScroll = nullptr;
    QComboBox* mLayerCombo = nullptr;
    QSpinBox* mZoomSpin = nullptr;
    QLabel* mPosLabel = nullptr;
    int mTileId = 0;
};

} // namespace qt_editor
