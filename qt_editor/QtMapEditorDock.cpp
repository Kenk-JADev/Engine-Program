#include "QtMapEditorDock.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Command.h"
#include "rpgmaker3d/CommandHistory.h"
#include "rpgmaker3d/Camera.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QMessageBox>
#include <QPainter>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QToolButton>
#include <filesystem>
#include <cstring>

namespace qt_editor {

namespace fs = std::filesystem;

QtMapEditorDock::QtMapEditorDock(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {
    buildUi();
    refresh();
}

void QtMapEditorDock::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    // Toolbar
    auto* tb = new QHBoxLayout();
    auto* btnNew = new QPushButton("Neu", this);
    auto* btnDel = new QPushButton("Loeschen", this);
    auto* btnSave = new QPushButton("Speichern", this);
    auto* btnLoad = new QPushButton("Laden", this);
    connect(btnNew, &QPushButton::clicked, this, &QtMapEditorDock::onNewMap);
    connect(btnDel, &QPushButton::clicked, this, &QtMapEditorDock::onDeleteMap);
    connect(btnSave, &QPushButton::clicked, this, &QtMapEditorDock::onSaveMap);
    connect(btnLoad, &QPushButton::clicked, this, &QtMapEditorDock::onLoadMap);
    tb->addWidget(btnNew);
    tb->addWidget(btnDel);
    tb->addWidget(btnSave);
    tb->addWidget(btnLoad);
    tb->addStretch(1);
    root->addLayout(tb);

    auto* split = new QSplitter(Qt::Vertical, this);

    // Map list
    mMapList = new QListWidget(split);
    mMapList->setMaximumHeight(140);
    connect(mMapList, &QListWidget::currentRowChanged, this, [this](int) { onMapSelected(); });

    // Props
    auto* props = new QGroupBox("Karten-Eigenschaften", split);
    auto* form = new QFormLayout(props);
    mNameEdit = new QLineEdit(props);
    mWidthSpin = new QSpinBox(props); mWidthSpin->setRange(1, 500); mWidthSpin->setValue(20);
    mHeightSpin = new QSpinBox(props); mHeightSpin->setRange(1, 500); mHeightSpin->setValue(15);
    mTilesetIdSpin = new QSpinBox(props); mTilesetIdSpin->setRange(1, 999); mTilesetIdSpin->setValue(1);
    auto* applyBtn = new QPushButton("Uebernehmen", props);
    auto* resizeBtn = new QPushButton("Groesse anwenden", props);
    connect(applyBtn, &QPushButton::clicked, this, &QtMapEditorDock::onApplyProps);
    connect(resizeBtn, &QPushButton::clicked, this, &QtMapEditorDock::onResizeMap);
    form->addRow("Name", mNameEdit);
    form->addRow("Breite", mWidthSpin);
    form->addRow("Hoehe", mHeightSpin);
    form->addRow("Tileset-ID", mTilesetIdSpin);
    auto* propBtns = new QHBoxLayout();
    propBtns->addWidget(applyBtn);
    propBtns->addWidget(resizeBtn);
    form->addRow(propBtns);

    // Paint controls
    auto* paintBox = new QGroupBox("Tile-Malen", split);
    auto* paintLay = new QVBoxLayout(paintBox);
    mPaintCheck = new QCheckBox("Malmodus (Linksklick im Game View)", paintBox);
    mPaintCheck->setChecked(true);
    connect(mPaintCheck, &QCheckBox::toggled, this, &QtMapEditorDock::setPaintEnabled);
    mLayerCombo = new QComboBox(paintBox);
    connect(mLayerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QtMapEditorDock::onLayerChanged);
    mEraserBtn = new QPushButton("Radiergummi", paintBox);
    connect(mEraserBtn, &QPushButton::clicked, this, &QtMapEditorDock::onEraser);
    auto* clearBtn = new QPushButton("Ebene leeren", paintBox);
    auto* fillBtn = new QPushButton("Ebene fuellen", paintBox);
    connect(clearBtn, &QPushButton::clicked, this, &QtMapEditorDock::onClearLayer);
    connect(fillBtn, &QPushButton::clicked, this, &QtMapEditorDock::onFillLayer);
    mTileInfo = new QLabel("Tile: 0", paintBox);
    mTilesetLabel = new QLabel("Tileset: -", paintBox);
    paintLay->addWidget(mPaintCheck);
    mBrushCombo = new QComboBox(paintBox);
    mBrushCombo->addItem("Pinsel (ziehen)");
    mBrushCombo->addItem("Rechteck (2 Klicks)");
    connect(mBrushCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QtMapEditorDock::onBrushModeChanged);
    paintLay->addWidget(new QLabel("Pinsel-Modus:", paintBox));
    paintLay->addWidget(mBrushCombo);
    paintLay->addWidget(new QLabel("Ebene:", paintBox));
    paintLay->addWidget(mLayerCombo);
    auto* paintBtns = new QHBoxLayout();
    paintBtns->addWidget(mEraserBtn);
    paintBtns->addWidget(clearBtn);
    paintBtns->addWidget(fillBtn);
    paintLay->addLayout(paintBtns);
    paintLay->addWidget(mTileInfo);
    paintLay->addWidget(mTilesetLabel);

    mTileScroll = new QScrollArea(paintBox);
    mTileScroll->setWidgetResizable(true);
    mTileScroll->setMinimumHeight(160);
    mTileGridHost = new QWidget(mTileScroll);
    mTileGrid = new QGridLayout(mTileGridHost);
    mTileGrid->setSpacing(2);
    mTileGrid->setContentsMargins(2, 2, 2, 2);
    mTileScroll->setWidget(mTileGridHost);
    paintLay->addWidget(mTileScroll, 1);

    split->addWidget(mMapList);
    split->addWidget(props);
    split->addWidget(paintBox);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 0);
    split->setStretchFactor(2, 1);

    root->addWidget(split, 1);
}

void QtMapEditorDock::refresh() {
    rebuildMapList();
    // Layers
    mLayerCombo->blockSignals(true);
    mLayerCombo->clear();
    if (mEngine) {
        auto& layers = mEngine->GetMap().GetLayers();
        for (size_t i = 0; i < layers.size(); ++i) {
            mLayerCombo->addItem(QString::fromStdString(layers[i].name), static_cast<int>(i));
        }
        if (layers.empty()) mLayerCombo->addItem("Ground", 0);
        if (mSelectedLayer >= mLayerCombo->count()) mSelectedLayer = 0;
        mLayerCombo->setCurrentIndex(mSelectedLayer);
    }
    mLayerCombo->blockSignals(false);
    rebuildTilePalette();
}

void QtMapEditorDock::setPaintEnabled(bool on) {
    mPaintEnabled = on;
    if (mPaintCheck && mPaintCheck->isChecked() != on) {
        mPaintCheck->blockSignals(true);
        mPaintCheck->setChecked(on);
        mPaintCheck->blockSignals(false);
    }
    emit paintStateChanged();
}

void QtMapEditorDock::rebuildMapList() {
    mMapList->blockSignals(true);
    mMapList->clear();
    auto& infos = rpg::Database::Get().MapInfos();
    for (const auto& info : infos) {
        mMapList->addItem(QString("%1: %2 (%3x%4)")
            .arg(info.id).arg(QString::fromStdString(info.name))
            .arg(info.width).arg(info.height));
    }
    if (infos.empty()) {
        mMapList->addItem("(keine Karten – Neu anlegen)");
        mSelectedMapIndex = -1;
    } else {
        if (mSelectedMapIndex < 0 || mSelectedMapIndex >= static_cast<int>(infos.size()))
            mSelectedMapIndex = 0;
        mMapList->setCurrentRow(mSelectedMapIndex);
        syncPropsFromMapInfo();
    }
    mMapList->blockSignals(false);
}

void QtMapEditorDock::syncPropsFromMapInfo() {
    auto& infos = rpg::Database::Get().MapInfos();
    if (mSelectedMapIndex < 0 || mSelectedMapIndex >= static_cast<int>(infos.size())) return;
    mSyncing = true;
    const auto& info = infos[static_cast<size_t>(mSelectedMapIndex)];
    mNameEdit->setText(QString::fromStdString(info.name));
    mWidthSpin->setValue(info.width);
    mHeightSpin->setValue(info.height);
    mTilesetIdSpin->setValue(info.tilesetId);
    mSyncing = false;
}

void QtMapEditorDock::onMapSelected() {
    const int row = mMapList->currentRow();
    auto& infos = rpg::Database::Get().MapInfos();
    if (row < 0 || row >= static_cast<int>(infos.size())) return;
    if (row == mSelectedMapIndex) {
        syncPropsFromMapInfo();
        return;
    }
    mSelectedMapIndex = row;
    syncPropsFromMapInfo();
    loadSelectedMap();
}

void QtMapEditorDock::onNewMap() {
    auto& infos = rpg::Database::Get().MapInfos();
    rpg::MapInfo info;
    info.id = infos.empty() ? 1 : infos.back().id + 1;
    info.name = "Karte " + std::to_string(info.id);
    info.width = 20;
    info.height = 15;
    info.tilesetId = 1;
    info.order = static_cast<int>(infos.size());
    infos.push_back(info);
    mSelectedMapIndex = static_cast<int>(infos.size()) - 1;
    rebuildMapList();
    loadSelectedMap();
    emit logMessage(QString("Neue Karte: %1").arg(info.id));
}

void QtMapEditorDock::onDeleteMap() {
    auto& infos = rpg::Database::Get().MapInfos();
    if (mSelectedMapIndex < 0 || mSelectedMapIndex >= static_cast<int>(infos.size())) return;
    if (infos.size() <= 1) {
        QMessageBox::information(this, "Loeschen", "Mindestens eine Karte muss bleiben.");
        return;
    }
    infos.erase(infos.begin() + mSelectedMapIndex);
    for (size_t i = 0; i < infos.size(); ++i) infos[i].order = static_cast<int>(i);
    mSelectedMapIndex = std::min(mSelectedMapIndex, static_cast<int>(infos.size()) - 1);
    rebuildMapList();
    loadSelectedMap();
    emit logMessage("Karte geloescht.");
}

void QtMapEditorDock::onSaveMap() {
    if (!mEngine) return;
    onApplyProps();
    auto& proj = mEngine->GetProject();
    auto& infos = rpg::Database::Get().MapInfos();
    int mapId = 1;
    if (mSelectedMapIndex >= 0 && mSelectedMapIndex < static_cast<int>(infos.size()))
        mapId = infos[static_cast<size_t>(mSelectedMapIndex)].id;
    const std::string pp = proj.GetProjectPath();
    if (pp.empty()) {
        QMessageBox::information(this, "Speichern", "Kein Projekt geladen.");
        return;
    }
    fs::create_directories(pp + "/maps");
    const std::string scenePath = pp + "/maps/map" + std::to_string(mapId) + "_scene.json";
    mEngine->SaveScene(scenePath);
    mEngine->GetMap().Save(proj.GetMapPath(mapId));
    rpg::Database::Get().Save(pp);
    emit logMessage(QString("Karte gespeichert (ID %1).").arg(mapId));
}

void QtMapEditorDock::onLoadMap() {
    loadSelectedMap();
}

void QtMapEditorDock::onApplyProps() {
    auto& infos = rpg::Database::Get().MapInfos();
    if (mSelectedMapIndex < 0 || mSelectedMapIndex >= static_cast<int>(infos.size())) return;
    auto& info = infos[static_cast<size_t>(mSelectedMapIndex)];
    info.name = mNameEdit->text().toStdString();
    info.tilesetId = mTilesetIdSpin->value();
    // width/height only via resize
    rebuildMapList();
    // Tileset neu laden
    if (mEngine) {
        auto& tilesets = rpg::Database::Get().Tilesets();
        for (const auto& ts : tilesets) {
            if (ts.id == info.tilesetId) {
                auto tileset = std::make_shared<rpg::Tileset>();
                std::string path = mEngine->GetProject().GetAssetPath("textures/" + ts.tilesetName);
                if (!tileset->Load(path, 32, 32))
                    tileset->Load("assets/textures/tileset_demo.png", 32, 32);
                mEngine->GetMap().SetTileset(tileset);
                break;
            }
        }
        rebuildTilePalette();
    }
    emit logMessage("Karten-Eigenschaften uebernommen.");
}

void QtMapEditorDock::onResizeMap() {
    auto& infos = rpg::Database::Get().MapInfos();
    if (mSelectedMapIndex < 0 || mSelectedMapIndex >= static_cast<int>(infos.size())) return;
    auto& info = infos[static_cast<size_t>(mSelectedMapIndex)];
    info.width = mWidthSpin->value();
    info.height = mHeightSpin->value();
    if (mEngine) {
        mEngine->GetMap().Resize(info.width, info.height);
    }
    rebuildMapList();
    emit logMessage(QString("Karte resized: %1x%2").arg(info.width).arg(info.height));
    emit mapLoaded();
}

void QtMapEditorDock::loadSelectedMap() {
    if (!mEngine) return;
    auto& infos = rpg::Database::Get().MapInfos();
    if (mSelectedMapIndex < 0 || mSelectedMapIndex >= static_cast<int>(infos.size())) return;
    auto& mapInfo = infos[static_cast<size_t>(mSelectedMapIndex)];

    auto& map = mEngine->GetMap();
    map.Resize(mapInfo.width, mapInfo.height);

    // Tileset
    auto& tilesets = rpg::Database::Get().Tilesets();
    bool loadedTs = false;
    for (const auto& ts : tilesets) {
        if (ts.id == mapInfo.tilesetId) {
            auto tileset = std::make_shared<rpg::Tileset>();
            std::string path = mEngine->GetProject().GetAssetPath("textures/" + ts.tilesetName);
            if (!tileset->Load(path, 32, 32))
                tileset->Load("assets/textures/tileset_demo.png", 32, 32);
            map.SetTileset(tileset);
            loadedTs = true;
            break;
        }
    }
    if (!loadedTs) {
        auto tileset = std::make_shared<rpg::Tileset>();
        tileset->Load("assets/textures/tileset_demo.png", 32, 32);
        map.SetTileset(tileset);
    }

    auto& fog = mEngine->GetRenderer().GetFog();
    fog.enabled = mapInfo.fogEnabled;
    fog.color = mapInfo.fogColor;

    auto& ambient = rpg::Lighting::Get().GetAmbient();
    ambient.color = mapInfo.backgroundColor;

    rpg::Camera& cam = mEngine->GetRenderer().GetCamera();
    cam.SetPosition(rpg::Vec3(0, 10, 10));
    cam.SetRotation(rpg::Vec3(-45, 0, 0));

    rpg::EventSystem::Get().LoadMapEvents(mapInfo.id, mEngine->GetProject().GetProjectPath());

    std::string scenePath = mEngine->GetProject().GetProjectPath() + "/maps/map"
        + std::to_string(mapInfo.id) + "_scene.json";
    if (!fs::exists(scenePath))
        scenePath = mEngine->GetProject().GetProjectPath() + "/scene.json";
    if (fs::exists(scenePath)) {
        mEngine->LoadScene(scenePath);
    } else {
        mEngine->GetMap().Load(mEngine->GetProject().GetMapPath(mapInfo.id));
    }

    refresh();
    emit logMessage(QString("Karte geladen: %1").arg(QString::fromStdString(mapInfo.name)));
    emit mapLoaded();
}

void QtMapEditorDock::onLayerChanged(int index) {
    if (index < 0) return;
    mSelectedLayer = index;
    emit paintStateChanged();
}

void QtMapEditorDock::onEraser() {
    mSelectedTile = -1;
    mTileInfo->setText("Tile: RADIERGUMMI");
    mEraserBtn->setStyleSheet("background:#803030; color:white;");
    emit paintStateChanged();
}

void QtMapEditorDock::onClearLayer() {
    if (!mEngine) return;
    auto& map = mEngine->GetMap();
    for (int z = 0; z < map.GetHeight(); ++z) {
        for (int x = 0; x < map.GetWidth(); ++x) {
            int oldTile = map.GetTile(mSelectedLayer, x, z);
            if (oldTile != -1) {
                auto cmd = std::make_shared<rpg::SetTileCommand>(mSelectedLayer, x, z, oldTile, -1);
                mEngine->GetCommandHistory().Execute(*mEngine, cmd);
            }
        }
    }
    emit logMessage(QString("Ebene %1 geleert.").arg(mSelectedLayer));
}

void QtMapEditorDock::onFillLayer() {
    if (!mEngine || mSelectedTile < 0) {
        QMessageBox::information(this, "Fuellen", "Bitte erst ein Tile waehlen (nicht Radiergummi).");
        return;
    }
    auto& map = mEngine->GetMap();
    for (int z = 0; z < map.GetHeight(); ++z) {
        for (int x = 0; x < map.GetWidth(); ++x) {
            int oldTile = map.GetTile(mSelectedLayer, x, z);
            if (oldTile != mSelectedTile) {
                auto cmd = std::make_shared<rpg::SetTileCommand>(mSelectedLayer, x, z, oldTile, mSelectedTile);
                mEngine->GetCommandHistory().Execute(*mEngine, cmd);
            }
        }
    }
    emit logMessage(QString("Ebene %1 mit Tile %2 gefuellt.").arg(mSelectedLayer).arg(mSelectedTile));
}

void QtMapEditorDock::onTileClicked(int tileId) {
    mSelectedTile = tileId;
    mTileInfo->setText(QString("Tile: %1").arg(tileId));
    mEraserBtn->setStyleSheet("");
    emit paintStateChanged();
}

void QtMapEditorDock::rebuildTilePalette() {
    if (mTileGrid) {
        QLayoutItem* child;
        while ((child = mTileGrid->takeAt(0)) != nullptr) {
            if (child->widget()) child->widget()->deleteLater();
            delete child;
        }
    }
    if (!mEngine) return;
    auto tileset = mEngine->GetMap().GetTileset();
    if (!tileset) {
        mTilesetLabel->setText("Tileset: (keins geladen)");
        return;
    }
    const int cols = tileset->GetColumns();
    const int rows = tileset->GetRows();
    const int tw = tileset->GetTileWidth();
    const int th = tileset->GetTileHeight();
    mTilesetLabel->setText(QString("Tileset: %1x%2 tiles (%3x%4 px)")
        .arg(cols).arg(rows).arg(tw).arg(th));

    // Versuche echte Pixel aus der GL-Textur (benoetigt current Context im Qt-Host)
    QImage atlas;
    if (auto* tex = tileset->GetTexture()) {
        std::vector<unsigned char> rgba;
        if (tex->ReadPixelsRGBA(rgba) && tex->GetWidth() > 0 && tex->GetHeight() > 0) {
            atlas = QImage(tex->GetWidth(), tex->GetHeight(), QImage::Format_RGBA8888);
            // stbi flippt vertikal beim Laden; glGetTexImage liefert origin bottom-left
            // -> vertical flip fuer Qt (top-left origin)
            const int w = tex->GetWidth();
            const int h = tex->GetHeight();
            for (int y = 0; y < h; ++y) {
                const unsigned char* src = rgba.data() + static_cast<size_t>(h - 1 - y) * w * 4;
                memcpy(atlas.scanLine(y), src, static_cast<size_t>(w) * 4);
            }
        }
    }

    const int tileCount = cols * rows;
    const int colsPerRow = std::max(4, std::min(8, cols > 0 ? cols : 8));
    const int btnSize = 40;
    for (int i = 0; i < tileCount; ++i) {
        auto* btn = new QToolButton(mTileGridHost);
        btn->setFixedSize(btnSize, btnSize);
        btn->setIconSize(QSize(btnSize - 4, btnSize - 4));
        btn->setToolTip(QString("Tile %1").arg(i));
        if (!atlas.isNull() && cols > 0) {
            const int tx = i % cols;
            const int ty = i / cols;
            QImage tile = atlas.copy(tx * tw, ty * th, tw, th);
            if (!tile.isNull()) {
                btn->setIcon(QIcon(QPixmap::fromImage(tile.scaled(btnSize - 4, btnSize - 4,
                    Qt::IgnoreAspectRatio, Qt::FastTransformation))));
                btn->setText(QString());
            } else {
                btn->setText(QString::number(i));
            }
        } else {
            btn->setText(QString::number(i));
        }
        if (i == mSelectedTile)
            btn->setStyleSheet("border: 2px solid #e0a020; background:#3a3020;");
        else
            btn->setStyleSheet("border: 1px solid #444;");
        connect(btn, &QToolButton::clicked, this, [this, i]() { onTileClicked(i); });
        mTileGrid->addWidget(btn, i / colsPerRow, i % colsPerRow);
    }
    if (mSelectedTile >= 0)
        mTileInfo->setText(QString("Tile: %1").arg(mSelectedTile));
}

void QtMapEditorDock::onBrushModeChanged(int index) {
    mBrushMode = index;
    emit paintStateChanged();
    emit logMessage(index == 0 ? "Pinsel: ziehen" : "Pinsel: Rechteck (2 Klicks im Game View)");
}

} // namespace qt_editor
