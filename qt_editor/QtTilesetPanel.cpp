#include "QtTilesetPanel.h"
#include "QtGameViewWidget.h" // ViewMode

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

namespace qt_editor {

namespace {
constexpr int kIconSize = 48; // Anzeigegroesse in der Palette (NEAREST skaliert)
}

QtTilesetPanel::QtTilesetPanel(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    // --- Werkzeug-Zeile: Auswahl / Malen / Radierer (exclusive) ---
    auto* toolRow = new QHBoxLayout();
    mToolGroup = new QButtonGroup(this);
    mToolGroup->setExclusive(true);
    const auto mkTool = [this, toolRow](const QString& text) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setCheckable(true);
        b->setAutoRaise(true);
        mToolGroup->addButton(b);
        toolRow->addWidget(b);
        return b;
    };
    mBtnSelect = mkTool("Auswahl");
    mBtnPaint = mkTool("Malen");
    mBtnErase = mkTool("Radierer");
    mToolGroup->setId(mBtnSelect, 0);
    mToolGroup->setId(mBtnPaint, 1);
    mToolGroup->setId(mBtnErase, 2);
    mBtnSelect->setChecked(true);
    toolRow->addStretch(1);
    layout->addLayout(toolRow);

    connect(mBtnSelect, &QToolButton::toggled, this, [this](bool on) { if (on) emit modeChanged(); });
    connect(mBtnPaint, &QToolButton::toggled, this, [this](bool on) { if (on) emit modeChanged(); });
    connect(mBtnErase, &QToolButton::toggled, this, [this](bool on) { if (on) emit modeChanged(); });

    // --- Layer-Auswahl ---
    auto* layerRow = new QHBoxLayout();
    layerRow->addWidget(new QLabel("Ebene:", this));
    mLayerCombo = new QComboBox(this);
    layerRow->addWidget(mLayerCombo, 1);
    layout->addLayout(layerRow);

    // --- Tile-Gitter ---
    mGrid = new QTableWidget(this);
    mGrid->setIconSize(QSize(kIconSize, kIconSize));
    mGrid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(mGrid, 1);

    mStatusLabel = new QLabel("Kein Tileset geladen.", this);
    layout->addWidget(mStatusLabel);

    connect(mGrid, &QTableWidget::cellClicked, this, [this](int row, int col) {
        if (mColumns <= 0) return;
        mSelectedTile = row * mColumns + col;
        // Kachel-Klick aktiviert automatisch den Mal-Modus (RPG-Maker-Feel)
        if (!mBtnPaint->isChecked()) mBtnPaint->setChecked(true); // -> toggled -> modeChanged
        emit tileChanged();
    });
}

ViewMode QtTilesetPanel::CurrentMode() const {
    if (mBtnErase && mBtnErase->isChecked()) return ViewMode::Erase;
    if (mBtnPaint && mBtnPaint->isChecked()) return ViewMode::Paint;
    return ViewMode::Select;
}

int QtTilesetPanel::SelectedLayer() const {
    return mLayerCombo ? mLayerCombo->currentIndex() : 0;
}

void QtTilesetPanel::SetLayers(const QStringList& names) {
    mLayerCombo->blockSignals(true);
    mLayerCombo->clear();
    mLayerCombo->addItems(names);
    if (mLayerCombo->count() == 0) mLayerCombo->addItem("Ebene 1");
    mLayerCombo->setCurrentIndex(0);
    mLayerCombo->blockSignals(false);
}

bool QtTilesetPanel::Reload(const QString& imagePath, int tileWidth, int tileHeight) {
    QImage img(imagePath);
    if (img.isNull() || tileWidth <= 0 || tileHeight <= 0) {
        mGrid->setRowCount(0);
        mGrid->setColumnCount(0);
        mStatusLabel->setText("Tileset konnte nicht geladen werden:\n" + imagePath);
        return false;
    }

    mTileWidth = tileWidth;
    mTileHeight = tileHeight;
    mColumns = img.width() / tileWidth;
    const int rows = img.height() / tileHeight;
    const int count = mColumns * rows;

    mGrid->clearContents();
    mGrid->setRowCount(rows);
    mGrid->setColumnCount(mColumns);
    for (int r = 0; r < rows; ++r) {
        mGrid->setRowHeight(r, kIconSize + 4);
        for (int c = 0; c < mColumns; ++c) {
            if (r == 0) mGrid->setColumnWidth(c, kIconSize + 4);
            const QImage tile = img.copy(c * tileWidth, r * tileHeight, tileWidth, tileHeight);
            // NEAREST-Skalierung (wie der Engine-Renderer: Tileset NEAREST fix)
            const QPixmap pm = QPixmap::fromImage(tile).scaled(
                kIconSize, kIconSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            auto* item = new QTableWidgetItem(QIcon(pm), QString());
            mGrid->setItem(r, c, item);
        }
    }
    mSelectedTile = 0;
    mStatusLabel->setText(QString("%1 Tiles (%2x%3) - %4")
        .arg(count).arg(mColumns).arg(rows).arg(imagePath));
    return true;
}

} // namespace qt_editor
