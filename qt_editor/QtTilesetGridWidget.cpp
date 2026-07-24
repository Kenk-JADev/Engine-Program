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
    setMouseTracking(true);
    setMinimumSize(2 * 32, 2 * 32);
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

void QtTilesetGridWidget::setInteraction(Interaction it) {
    mInteraction = it;
    update();
}

void QtTilesetGridWidget::setZoom(int zoom) {
    mZoom = qBound(1, zoom, 3);
    relayout();
}

void QtTilesetGridWidget::setSelectedTile(int tileId) {
    if (mSelected == tileId) return;
    mSelected = tileId;
    update();
}

void QtTilesetGridWidget::setSolidBits(const std::vector<bool>& solid) {
    mSolid = solid;
    update();
}

void QtTilesetGridWidget::ensureSizesIfPossible() {
    if (!mData) return;
    const int count = tileCount();
    if (count <= 0) {
        // Noch kein Bild geladen: Vektoren mindestens auf 1 laengen,
        // damit die Getter/Setter nicht auf leere Vektoren zeigen.
        mData->EnsureFlagSizes(0);
        return;
    }
    mData->EnsureFlagSizes((size_t)count);
}

void QtTilesetGridWidget::relayout() {
    const int cell = cellSize();
    if (mDispCols > 0 && mDispRows > 0)
        setMinimumSize(mDispCols * cell, mDispRows * cell);
    updateGeometry();
    update();
}

void QtTilesetGridWidget::loadImage(const QString& absPath) {
    QImage src;
    if (!absPath.isEmpty()) {
        QImage tmp(absPath);
        if (!tmp.isNull()) src = tmp;
    }
    setImage(src, 0, 0);
}

void QtTilesetGridWidget::setImage(const QImage& img, int srcCols, int srcRows) {
    mImage = img;
    mSrcCols = 0;
    mSrcRows = 0;
    mDispRows = 0;

    if (!mImage.isNull()) {
        const int tw = 32; // logische Kachelgroesse der Quelle (wie XP)
        mSrcCols = srcCols > 0 ? srcCols : (mImage.width() / tw);
        mSrcRows = srcRows > 0 ? srcRows : (mImage.height() / tw);
        if (mSrcCols < 1) mSrcCols = 1;
        if (mSrcRows < 1) mSrcRows = 1;
        // Bildzuschnitt auf ganze Kacheln, damit die Ausschnitte sauber sind
        mImage = img.copy(0, 0, mSrcCols * tw, mSrcRows * tw);
        mDispCols = 8; // XP zeigt immer 8 Tiles pro Zeile
        const int total = mSrcCols * mSrcRows;
        mDispRows = (total + mDispCols - 1) / mDispCols;
        if (mDispRows < 1) mDispRows = 1;
    }

    if (mSelected >= tileCount()) mSelected = -1;
    ensureSizesIfPossible();
    relayout();
}

QPoint QtTilesetGridWidget::cellOf(int tileId) const {
    return QPoint(tileId % mDispCols, tileId / mDispCols);
}

int QtTilesetGridWidget::tileAt(const QPoint& pos) const {
    const int cell = cellSize();
    if (cell <= 0) return -1;
    const int col = pos.x() / cell;
    const int row = pos.y() / cell;
    if (row < 0 || col < 0 || col >= mDispCols || row >= mDispRows) return -1;
    const int tileId = row * mDispCols + col;
    if (tileId < 0 || tileId >= tileCount()) return -1;
    return tileId;
}

QSize QtTilesetGridWidget::sizeHint() const {
    const int cell = cellSize();
    return QSize(qMax(2, mDispCols) * cell, qMax(2, mDispRows) * cell);
}

// ---------------------------------------------------------------------
// Klick-Logik
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
    if (mInteraction == PickTile) {
        if (e->button() == Qt::LeftButton && id >= 0) {
            setSelectedTile(id);
            emit tilePicked(id);
        }
        QWidget::mousePressEvent(e);
        return;
    }
    if (id >= 0 && mData) {
        if (e->button() == Qt::RightButton) applyRightClick(id);
        else applyLeftClick(id);
    }
    QWidget::mousePressEvent(e);
}

void QtTilesetGridWidget::mouseMoveEvent(QMouseEvent* e) {
    const int id = tileAt(e->pos());
    if (id != mHover) {
        mHover = id;
        emit hoverTile(id);
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void QtTilesetGridWidget::leaveEvent(QEvent* e) {
    if (mHover != -1) {
        mHover = -1;
        emit hoverTile(-1);
        update();
    }
    QWidget::leaveEvent(e);
}

// ---------------------------------------------------------------------
// Flag-Overlay-Malerei (EditFlags)
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
    if (v <= 0) return;
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

// ---------------------------------------------------------------------
// Malerei: EIN Bild, kachelecht aus der Quelle (kein Verzerren!)
// ---------------------------------------------------------------------

void QtTilesetGridWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    // PAKET 40: Theme-bewusste Flaeche (AlternateBase des App-Themes),
    // statt hart verdrahtetem Dunkelgrau — die Palette wirkt jetzt im
    // XP-Classic-Theme wie eine MAKER-Tafel und bleibt zugleich
    // dark-theme-tauglich, falls das Theme spaeter umgeschaltet wird.
    p.fillRect(rect(), palette().color(QPalette::AlternateBase));

    if (mImage.isNull() || mSrcCols <= 0 || mSrcRows <= 0) {
        p.setPen(QPen(palette().color(QPalette::Text)));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("Keine Tileset-Grafik geladen"));
        return;
    }

    const int cell = cellSize();
    const int total = tileCount();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false); // kachelscharf wie XP

    for (int tid = 0; tid < total; ++tid) {
        const QPoint dc = cellOf(tid);
        const QRectF dst(dc.x() * cell, dc.y() * cell, cell, cell);
        // Quell-Ausschnitt der EXAKT passenden 32x32-Stelle
        const int sc = tid % mSrcCols;
        const int sr = tid / mSrcCols;
        const QRectF src(sc * 32, sr * 32, 32, 32);
        p.drawImage(dst, mImage, src);

        if (mInteraction == EditFlags && mData) {
            switch (mMode) {
            case ModePassage:  paintPassage(p, dst, tid);  break;
            case ModePassage4: paintPassage4(p, dst, tid); break;
            case ModePriority: paintPriority(p, dst, tid); break;
            case ModeBush:     paintBush(p, dst, tid);     break;
            case ModeCounter:  paintCounter(p, dst, tid);  break;
            case ModeTerrain:  paintTerrain(p, dst, tid);  break;
            }
        } else if (mInteraction == PickTile) {
            // Sperr-Marke: kleines rotes Dreieck rechts unten
            if (tid < (int)mSolid.size() && mSolid[(size_t)tid]) {
                QPolygonF tri;
                const qreal s = cell * 0.30;
                tri << dst.topRight() + QPointF(0, 1)
                    << dst.topRight() + QPointF(-s, 1)
                    << dst.topRight() + QPointF(1, s + 1);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(200, 60, 50, 210));
                p.drawPolygon(tri);
            }
        }
    }

    // Hover-Hinweis (dezent): helle Linie auf dunklen Themes, Akzent auf
    // hellen Themes (PAKET 40) — Kontrast in beide Richtungen.
    const bool lightTheme =
        palette().color(QPalette::AlternateBase).value() > 140;
    const QColor hoverCol = lightTheme
        ? QColor(palette().color(QPalette::Highlight))
        : QColor(255, 255, 255, 120);
    if (mHover >= 0 && mHover < total) {
        const QPoint dc = cellOf(mHover);
        p.setPen(QPen(hoverCol, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(dc.x() * cell + 0.5, dc.y() * cell + 0.5, cell - 1, cell - 1));
    }
    // Auswahlrahmen (PickTile): Theme-Akzent + weisser Innenrand fuer Saetze
    // mit aehnlicher Grundfarbe (PAKET 40).
    if (mInteraction == PickTile && mSelected >= 0 && mSelected < total) {
        const QPoint dc = cellOf(mSelected);
        p.setPen(QPen(QColor(255, 255, 255, 200), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(dc.x() * cell + 2.5, dc.y() * cell + 2.5, cell - 5, cell - 5));
        p.setPen(QPen(palette().color(QPalette::Highlight), 2));
        p.drawRect(QRectF(dc.x() * cell + 1, dc.y() * cell + 1, cell - 2, cell - 2));
    }
}

} // namespace qt_editor
