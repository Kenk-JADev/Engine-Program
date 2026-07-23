#include "QtTilesetGridWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QPolygonF>
#include <QtGlobal>
#include <cmath>

namespace qt_editor {

namespace {
// Zellengroesse im Widget (logische Tile-Groesse bleibt 32)
constexpr int kCell = 40;
// XP-4-Dir-Rotation fuer Linksklick: alle frei -> nur v -> nur < -> nur > -> nur ^ -> gesperrt
constexpr int kRotStates[6] = {
    0,                                        // 0 = Default: alle Richtungen frei
    rpg::TilesetData::DirDown,
    rpg::TilesetData::DirLeft,
    rpg::TilesetData::DirRight,
    rpg::TilesetData::DirUp,
    15                                        // Marker: alle gesperrt (Anzeige X)
};
constexpr int kRotCount = 6;
}

QtTilesetGridWidget::QtTilesetGridWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(kCell * 2, kCell * 2);
}

void QtTilesetGridWidget::setData(rpg::TilesetData* data) {
    mData = data;
    ensureSizesIfPossible();
    update();
}

void QtTilesetGridWidget::setMode(Mode m) {
    mMode = m;
    update();
}

void QtTilesetGridWidget::ensureSizesIfPossible() {
    if (!mData) return;
    if (mTilesY <= 0) {
        // Noch kein Bild geladen: Vektoren mindestens auf 1 laengen,
        // damit die Getter/Setter nicht auf leere Vektoren zeigen.
        mData->EnsureFlagSizes(0);
        return;
    }
    const int count = (mTilesX > 0 ? mTilesX : 1) * mTilesY;
    mData->EnsureFlagSizes((size_t)count);
}

void QtTilesetGridWidget::loadImage(const QString& absPath) {
    mImage = QImage();
    mTilesY = 0;

    if (!absPath.isEmpty()) {
        QImage src(absPath);
        if (!src.isNull()) {
            // Kachelung wie XP: Tilebreite aus Bildgroesse
            // (32px-Tiles, Minimal-Schutz gegen kaputte Bilder).
            int tw = mTileW;
            if (tw <= 0) tw = 32;
            int cols = src.width() > 0 ? src.width() / tw : 0;
            if (cols < 1) cols = 1;
            mTilesX = 8;              // XP zeigt immer 8 Spalten
            int rows = src.height() > 0 ? src.height() / tw : 0;
            if (rows * mTilesX < cols * rows && cols > 0) {
                // Wenn Bild schmaler als 8 Spalten ist, trotzdem sauber kacheln:
                // wir mappen Zeilen aus der Original-Spaltenzahl auf 8/ Zeile um.
            }
            // gesamte Tileanzahl aus Original-Raster
            mTilesY = (src.height() / tw);
            // Umrechnung: unser 8er-Raster zeigt (origCols*origRows) Tiles
            int total = cols * rows;
            mTilesY = (total + mTilesX - 1) / mTilesX;
            if (mTilesY < 1) mTilesY = 1;

            // Bild auf Zellengroesse skalieren (Cropping erfolgt beim Malen)
            mImage = src.scaled(mTilesX * kCell, mTilesY * kCell,
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    }

    ensureSizesIfPossible();
    update();
}

int QtTilesetGridWidget::tileAt(const QPoint& pos) const {
    const int col = pos.x() / kCell;
    const int row = pos.y() / kCell;
    if (row < 0 || col < 0 || col >= mTilesX || row >= mTilesY) return -1;
    const int tileId = row * mTilesX + col;
    if (!mData) return -1;
    if (tileId < 0 || tileId >= tileCount()) return -1;
    return tileId;
}

// ---------------------------------------------------------------------
// Klick-Logik
// (tileCount() ist inline im Header: mTilesX * mTilesY)
// ---------------------------------------------------------------------

void QtTilesetGridWidget::applyLeftClick(int tileId) {
    if (!mData || tileId < 0) return;
    switch (mMode) {
    case ModePassage: {
        int& v = mData->flags[(size_t)tileId];
        v = (v == 0) ? 1 : 0;
        break;
    }
    case ModePassage4: {
        int cur = mData->passage4dir[(size_t)tileId];
        // naechsten Rotations-Zustand suchen
        int next = kRotStates[0];
        for (int i = 0; i < kRotCount; ++i) {
            if (cur == kRotStates[i]) {
                next = kRotStates[(i + 1) % kRotCount];
                break;
            }
        }
        mData->passage4dir[(size_t)tileId] = next;
        break;
    }
    case ModePriority: {
        int& v = mData->priority[(size_t)tileId];
        v = (v + 1) % 6; // 0..5
        break;
    }
    case ModeBush: {
        int& v = mData->bushFlags[(size_t)tileId];
        v = (v == 0) ? 1 : 0;
        break;
    }
    case ModeCounter: {
        int& v = mData->counterFlags[(size_t)tileId];
        v = (v == 0) ? 1 : 0;
        break;
    }
    case ModeTerrain: {
        int& v = mData->terrainTags[(size_t)tileId];
        v = (v + 1) % 8; // 0..7
        break;
    }
    }
    emit flagsChanged();
    update();
}

void QtTilesetGridWidget::applyRightClick(int tileId) {
    if (!mData || tileId < 0) return;
    switch (mMode) {
    case ModePassage:
        mData->flags[(size_t)tileId] = 0;
        break;
    case ModePassage4:
        mData->passage4dir[(size_t)tileId] = 0; // alle frei
        break;
    case ModePriority:
        mData->priority[(size_t)tileId] = 0;
        break;
    case ModeBush:
        mData->bushFlags[(size_t)tileId] = 0;
        break;
    case ModeCounter:
        mData->counterFlags[(size_t)tileId] = 0;
        break;
    case ModeTerrain:
        mData->terrainTags[(size_t)tileId] = 0;
        break;
    }
    emit flagsChanged();
    update();
}

void QtTilesetGridWidget::mousePressEvent(QMouseEvent* e) {
    const int id = tileAt(e->pos());
    if (id >= 0 && mData) {
        if (e->button() == Qt::RightButton) {
            applyRightClick(id);
        } else {
            applyLeftClick(id);
        }
    }
    QWidget::mousePressEvent(e);
}

// ---------------------------------------------------------------------
// Malerei
// ---------------------------------------------------------------------

void QtTilesetGridWidget::paintPassage(QPainter& p, const QRectF& rc, int tileId) {
    const int v = mData ? mData->GetPassage(tileId) : 0;
    const QPointF c = rc.center();
    const qreal r = rc.width() * 0.28;
    if (v == 0) {
        // frei = gruener Kreis (wie XP)
        p.setPen(QPen(QColor(30, 170, 60), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, r, r);
    } else {
        // blockiert = rotes X
        p.setPen(QPen(QColor(220, 50, 40), 3));
        p.drawLine(QPointF(c.x() - r, c.y() - r), QPointF(c.x() + r, c.y() + r));
        p.drawLine(QPointF(c.x() - r, c.y() + r), QPointF(c.x() + r, c.y() - r));
    }
}

void QtTilesetGridWidget::paintPassage4(QPainter& p, const QRectF& rc, int tileId) {
    const int v = mData ? mData->GetPassage4Dir(tileId) : 0;
    const QPointF c = rc.center();
    const qreal w = rc.width();
    if (v == 15) {
        // alle gesperrt = X
        p.setPen(QPen(QColor(220, 50, 40), 3));
        p.drawLine(rc.topLeft(), rc.bottomRight());
        p.drawLine(rc.bottomLeft(), rc.topRight());
        return;
    }
    // Frei-Richtungen als kleine Pfeile
    const bool down  = (v == 0) || (v & rpg::TilesetData::DirDown);
    const bool left  = (v == 0) || (v & rpg::TilesetData::DirLeft);
    const bool right = (v == 0) || (v & rpg::TilesetData::DirRight);
    const bool up    = (v == 0) || (v & rpg::TilesetData::DirUp);
    p.setPen(QPen(QColor(250, 250, 250), 2));
    p.setBrush(Qt::NoBrush);
    const qreal a = w * 0.18; // Pfeillaenge
    auto arrow = [&](const QPointF& from, const QPointF& to) {
        p.drawLine(from, to);
        // Pfeilspitze
        QPointF d = to - from;
        qreal len = std::sqrt(d.x() * d.x() + d.y() * d.y());
        if (len < 1.0) return;
        d /= len;
        QPointF n(-d.y(), d.x());
        p.drawLine(to, to - d * 4.0 + n * 3.0);
        p.drawLine(to, to - d * 4.0 - n * 3.0);
    };
    if (down)  arrow(QPointF(c.x(), c.y() - a), QPointF(c.x(), c.y() + a));
    if (up)    arrow(QPointF(c.x(), c.y() + a), QPointF(c.x(), c.y() - a));
    if (left)  arrow(QPointF(c.x() + a, c.y()), QPointF(c.x() - a, c.y()));
    if (right) arrow(QPointF(c.x() - a, c.y()), QPointF(c.x() + a, c.y()));
}

void QtTilesetGridWidget::paintPriority(QPainter& p, const QRectF& rc, int tileId) {
    const int v = mData ? mData->GetPriority(tileId) : 0;
    if (v <= 0) return; // 0 wird in XP nicht besonders markiert
    p.setPen(QPen(QColor(250, 200, 40), 2));
    p.setBrush(QColor(0, 0, 0, 140));
    p.drawRect(QRectF(rc.topRight() + QPointF(-rc.width() * 0.42, 0),
                      QSizeF(rc.width() * 0.42, rc.height() * 0.42)));
    p.setPen(QPen(QColor(250, 220, 60), 2));
    p.drawText(rc, Qt::AlignTop | Qt::AlignRight, QString::number(v));
}

void QtTilesetGridWidget::paintBush(QPainter& p, const QRectF& rc, int tileId) {
    const int v = mData ? mData->GetBush(tileId) : 0;
    if (!v) return;
    p.setPen(QPen(QColor(90, 200, 90), 2));
    p.setBrush(QColor(30, 90, 30, 170));
    p.drawRect(QRectF(rc.bottomLeft() + QPointF(2, -rc.height() * 0.42),
                      QSizeF(rc.width() * 0.42, rc.height() * 0.42)));
    p.setPen(QPen(QColor(200, 255, 200), 2));
    p.drawText(rc.adjusted(2, 0, 0, -2), Qt::AlignBottom | Qt::AlignLeft, QStringLiteral("B"));
}

void QtTilesetGridWidget::paintCounter(QPainter& p, const QRectF& rc, int tileId) {
    const int v = mData ? mData->GetCounter(tileId) : 0;
    if (!v) return;
    p.setPen(QPen(QColor(120, 160, 255), 2));
    p.setBrush(QColor(30, 50, 110, 170));
    p.drawRect(QRectF(rc.bottomRight() + QPointF(-rc.width() * 0.42, -rc.height() * 0.42),
                      QSizeF(rc.width() * 0.42, rc.height() * 0.42)));
    p.setPen(QPen(QColor(200, 220, 255), 2));
    p.drawText(rc.adjusted(0, 0, -2, -2), Qt::AlignBottom | Qt::AlignRight, QStringLiteral("C"));
}

void QtTilesetGridWidget::paintTerrain(QPainter& p, const QRectF& rc, int tileId) {
    const int v = mData ? mData->GetTerrainTag(tileId) : 0;
    if (v <= 0) return;
    p.setPen(QPen(QColor(230, 230, 230), 2));
    p.drawText(rc, Qt::AlignBottom | Qt::AlignLeft, QString::number(v));
}

void QtTilesetGridWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 22));

    if (mImage.isNull()) {
        p.setPen(QPen(QColor(140, 140, 140)));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("Keine Tileset-Grafik geladen"));
        return;
    }

    // Kacheln: unser 8er-Raster -> Bildposition wird aus Tile-ID berechnet,
    // skaliertes Bild wird per Ausschnitt gemalt (Cropping).
    const int total = mTilesX * mTilesY;
    for (int tid = 0; tid < total; ++tid) {
        const int col = tid % mTilesX;
        const int row = tid / mTilesX;
        const QRectF cell(col * kCell, row * kCell, kCell, kCell);

        // Quell-Rect im (skalierten) Bild:
        // Bild ist auf mTilesX*kCell x mTilesY*kCell gestretcht,
        // jedes logische Tile = kCell x kCell
        const QRectF srcRect(col * kCell, row * kCell, kCell, kCell);
        p.drawImage(cell, mImage, srcRect);

        // dezente Trennlinie
        p.setPen(QPen(QColor(255, 255, 255, 26), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(cell);

        // Overlay je Modus
        switch (mMode) {
        case ModePassage:  paintPassage(p, cell, tid);  break;
        case ModePassage4: paintPassage4(p, cell, tid); break;
        case ModePriority: paintPriority(p, cell, tid); break;
        case ModeBush:     paintBush(p, cell, tid);     break;
        case ModeCounter:  paintCounter(p, cell, tid);  break;
        case ModeTerrain:  paintTerrain(p, cell, tid);  break;
        }
    }
}

} // namespace qt_editor
