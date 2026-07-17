#include "QtMapTab.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Database.h"

#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRect>
#include <QScrollArea>
#include <QSize>
#include <QSpinBox>
#include <QVBoxLayout>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

// ---------------------------------------------------------------------------
// Canvas: zeichnet das Kartenraster und verarbeitet Maus-Malen
// ---------------------------------------------------------------------------

class QtMapTabCanvas : public QWidget {
public:
    QtMapTabCanvas(rpg::Engine* engine, QWidget* parent = nullptr)
        : QWidget(parent), mEngine(engine) {
        setMouseTracking(true);
    }

    int layer = 0;
    int tileId = 0;
    int cell = 20;

    std::function<void(int, int)> onPaint;      // (x,z) gemalt
    std::function<void(int, int)> onHover;      // (x,z) Mausposition

    QSize sizeHint() const override {
        if (!mEngine || !mEngine->IsInitialized()) return {640, 480};
        auto& map = mEngine->GetMap();
        return {map.GetWidth() * cell + 1, map.GetHeight() * cell + 1};
    }

    void refreshSize() {
        const QSize s = sizeHint();
        setMinimumSize(s);
        resize(s);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(24, 26, 30));
        if (!mEngine || !mEngine->IsInitialized()) {
            p.setPen(Qt::gray);
            p.drawText(rect(), Qt::AlignCenter,
                       QL("Keine Karte geladen.\nProjekt öffnen oder Karte anlegen (Map-Dock)."));
            return;
        }
        auto& map = mEngine->GetMap();
        const int w = map.GetWidth();
        const int h = map.GetHeight();

        // einfache Füllfarben je Tile (stabil gehasht)
        auto tileColor = [](int id) -> QColor {
            if (id <= 0) return QColor(32, 34, 38);
            const int r = 64 + (id * 73) % 160;
            const int g = 70 + (id * 149) % 150;
            const int b = 72 + (id * 199) % 140;
            return QColor(r, g, b);
        };

        const int visibleLayer = layer;
        for (int z = 0; z < h; ++z) {
            for (int x = 0; x < w; ++x) {
                int id = map.GetTile(visibleLayer, x, z);
                if (id <= 0 && visibleLayer > 0) id = map.GetTile(0, x, z); // Grund schimmert durch
                const QRect rc(x * cell, z * cell, cell, cell);
                p.fillRect(rc, tileColor(id));
                if (id > 0 && cell >= 14) {
                    p.setPen(QColor(0, 0, 0, 90));
                    p.drawText(rc, Qt::AlignCenter, QString::number(id));
                }
            }
        }
        // Raster
        p.setPen(QColor(255, 255, 255, 28));
        for (int x = 0; x <= w; ++x)
            p.drawLine(x * cell, 0, x * cell, h * cell);
        for (int z = 0; z <= h; ++z)
            p.drawLine(0, z * cell, w * cell, z * cell);

        // Startposition markieren
        const int sx = mStartX, sz = mStartZ;
        if (sx >= 0 && sz >= 0 && sx < w && sz < h) {
            p.setPen(QPen(QColor(80, 200, 255), 2));
            p.drawEllipse(QPointF(sx * cell + cell / 2.0, sz * cell + cell / 2.0),
                          cell * 0.3, cell * 0.3);
        }

        // Hover-Feld
        if (mHoverX >= 0 && mHoverX < w && mHoverZ >= 0 && mHoverZ < h) {
            p.setPen(QPen(QColor(255, 220, 90), 1));
            p.drawRect(mHoverX * cell, mHoverZ * cell, cell - 1, cell - 1);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        mPainting = true;
        applyAt(e->pos());
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        const int x = e->pos().x() / cell;
        const int z = e->pos().y() / cell;
        mHoverX = x;
        mHoverZ = z;
        if (onHover) onHover(x, z);
        if (mPainting) applyAt(e->pos());
        update();
    }
    void mouseReleaseEvent(QMouseEvent*) override { mPainting = false; }
    void leaveEvent(QEvent*) override {
        mHoverX = mHoverZ = -1;
        update();
    }

public:
    int mStartX = -1, mStartZ = -1;
    void setStartPos(int x, int z) { mStartX = x; mStartZ = z; }

private:
    void applyAt(const QPoint& pos) {
        if (!mEngine || !mEngine->IsInitialized()) return;
        auto& map = mEngine->GetMap();
        const int x = pos.x() / cell;
        const int z = pos.y() / cell;
        if (x < 0 || z < 0 || x >= map.GetWidth() || z >= map.GetHeight()) return;
        map.SetTile(layer, x, z, tileId < 0 ? 0 : tileId);
        if (onPaint) onPaint(x, z);
        update();
    }

    rpg::Engine* mEngine = nullptr;
    bool mPainting = false;
    int mHoverX = -1, mHoverZ = -1;
};

// ---------------------------------------------------------------------------
// Tab mit Werkzeugzeile + ScrollArea
// ---------------------------------------------------------------------------

QtMapTab::QtMapTab(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    auto* tb = new QHBoxLayout();
    tb->addWidget(new QLabel(QL("Ebene:"), this));
    mLayerCombo = new QComboBox(this);
    for (int i = 0; i < 4; ++i)
        mLayerCombo->addItem(QL("Ebene %1").arg(i), i);
    tb->addWidget(mLayerCombo);

    tb->addSpacing(12);
    tb->addWidget(new QLabel(QL("Zoom:"), this));
    mZoomSpin = new QSpinBox(this);
    mZoomSpin->setRange(4, 64);
    mZoomSpin->setValue(20);
    mZoomSpin->setSuffix(QL(" px"));
    tb->addWidget(mZoomSpin);

    tb->addStretch(1);
    mPosLabel = new QLabel(QL("Feld: -"), this);
    tb->addWidget(mPosLabel);
    root->addLayout(tb);

    mCanvas = new QtMapTabCanvas(engine);
    mCanvas->setStartPos(0, 0);
    mScroll = new QScrollArea(this);
    mScroll->setWidget(mCanvas);
    mScroll->setWidgetResizable(false);
    mScroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    root->addWidget(mScroll, 1);

    connect(mLayerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        mCanvas->layer = idx;
        mCanvas->update();
    });
    connect(mZoomSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int px) {
        mCanvas->cell = px;
        mCanvas->refreshSize();
    });

    mCanvas->onPaint = [this](int x, int z) {
        emit tilesChanged();
        mPosLabel->setText(QL("Feld: %1, %2  (gemalt)").arg(x).arg(z));
    };
    mCanvas->onHover = [this](int x, int z) {
        if (mCanvas->isVisible())
            mPosLabel->setText(QL("Feld: %1, %2").arg(x).arg(z));
    };
}

void QtMapTab::refresh() {
    if (!mEngine || !mEngine->IsInitialized()) return;
    const auto& sys = rpg::Database::Get().System();
    mCanvas->setStartPos(sys.startX, sys.startY);
    mCanvas->refreshSize();
    mCanvas->update();
}

void QtMapTab::setPaintTile(int tileId) {
    mTileId = tileId;
    mCanvas->tileId = tileId;
    mCanvas->update();
}

void QtMapTab::setPaintLayer(int layer) {
    const int idx = mLayerCombo->findData(layer);
    if (idx >= 0) mLayerCombo->setCurrentIndex(idx);
    mCanvas->layer = layer;
}

int QtMapTab::paintLayer() const {
    return mLayerCombo ? mLayerCombo->currentData().toInt() : 0;
}

} // namespace qt_editor
