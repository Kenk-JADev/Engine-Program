#pragma once
// Tileset-Palette des Qt-Editors (Port des ImGui-Tileset-Fensters):
//  - Werkzeug-Auswahl: Auswahl (3D-Picking) / Malen / Radierer (exclusive)
//  - Layer-Auswahl (ComboBox aus den Map-Layern)
//  - Tile-Gitter (QTableWidget, Icons per NEAREST aus der Tileset-Textur)
// Klick auf ein Tile waehlt es UND aktiviert automatisch den Mal-Modus
// (RPG-Maker-Feel). Das Panel ist bewusst engine-frei - es bekommt
// Bildpfad + Tilegroesse von aussen und meldet nur Auswahl-Aenderungen.

#include <QWidget>

class QTableWidget;
class QToolButton;
class QComboBox;
class QButtonGroup;
class QLabel;

namespace qt_editor {

enum class ViewMode; // aus QtGameViewWidget.h

class QtTilesetPanel : public QWidget {
    Q_OBJECT
public:
    explicit QtTilesetPanel(QWidget* parent = nullptr);

    // Laedt die Palette neu (Texturpfad + Tilegroesse). false bei Fehler.
    bool Reload(const QString& imagePath, int tileWidth, int tileHeight);
    void SetLayers(const QStringList& names);

    int SelectedTileId() const { return mSelectedTile; }
    int SelectedLayer() const;
    ViewMode CurrentMode() const;

signals:
    void modeChanged();   // Werkzeug gewechselt -> View-Modus umstellen
    void tileChanged();   // Kachel gewechselt (impliziert Paint-Modus)

private:
    QToolButton* mBtnSelect = nullptr;
    QToolButton* mBtnPaint = nullptr;
    QToolButton* mBtnErase = nullptr;
    QButtonGroup* mToolGroup = nullptr;
    QComboBox* mLayerCombo = nullptr;
    QLabel* mStatusLabel = nullptr;
    QTableWidget* mGrid = nullptr;

    int mSelectedTile = 0;
    int mColumns = 0;
    int mTileWidth = 32;
    int mTileHeight = 32;
};

} // namespace qt_editor
