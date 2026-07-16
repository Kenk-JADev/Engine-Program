#pragma once
// Map-Editor als natives Qt-Dock: Kartenliste, Eigenschaften, Tile-Palette.
// Malen im Game-View ueber paintMode/selectedTile (QtGameViewWidget).

#include <QWidget>
#include <QString>

class QListWidget;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QScrollArea;
class QGridLayout;

namespace rpg { class Engine; }

namespace qt_editor {

class QtMapEditorDock : public QWidget {
    Q_OBJECT
public:
    explicit QtMapEditorDock(rpg::Engine* engine, QWidget* parent = nullptr);

    void refresh();

    // Paint-State fuer den Game-View
    int selectedTile() const { return mSelectedTile; } // -1 = Eraser
    int selectedLayer() const { return mSelectedLayer; }
    bool paintEnabled() const { return mPaintEnabled; }
    // 0=paint drag, 1=rect fill (2 Klicks)
    int brushMode() const { return mBrushMode; }
    int selectedMapIndex() const { return mSelectedMapIndex; }

signals:
    void logMessage(const QString& msg);
    void mapLoaded();
    void paintStateChanged();

public slots:
    void setPaintEnabled(bool on);

private slots:
    void onMapSelected();
    void onNewMap();
    void onDeleteMap();
    void onSaveMap();
    void onLoadMap();
    void onResizeMap();
    void onApplyProps();
    void onLayerChanged(int index);
    void onEraser();
    void onClearLayer();
    void onFillLayer();
    void onTileClicked(int tileId);
    void onBrushModeChanged(int index);

private:
    void buildUi();
    void rebuildMapList();
    void rebuildTilePalette();
    void loadSelectedMap();
    void syncPropsFromMapInfo();

    rpg::Engine* mEngine = nullptr;

    QListWidget* mMapList = nullptr;
    QLineEdit* mNameEdit = nullptr;
    QSpinBox* mWidthSpin = nullptr;
    QSpinBox* mHeightSpin = nullptr;
    QSpinBox* mTilesetIdSpin = nullptr;
    QComboBox* mLayerCombo = nullptr;
    QLabel* mTileInfo = nullptr;
    QLabel* mTilesetLabel = nullptr;
    QWidget* mTileGridHost = nullptr;
    QGridLayout* mTileGrid = nullptr;
    QScrollArea* mTileScroll = nullptr;
    QCheckBox* mPaintCheck = nullptr;
    QPushButton* mEraserBtn = nullptr;

    int mSelectedMapIndex = -1;
    int mSelectedTile = 0;
    int mSelectedLayer = 0;
    bool mPaintEnabled = true;
    int mBrushMode = 0; // 0 paint, 1 rect
    QComboBox* mBrushCombo = nullptr;
    bool mSyncing = false;
};

} // namespace qt_editor
