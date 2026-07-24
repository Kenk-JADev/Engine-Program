#include "QtMapTab.h"
#include "QtTilesetGridWidget.h"

#include "QtEventEditorDialog.h"

#include <cstdio>
#include <queue>
#include <unordered_set>
#include <utility>

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Texture.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Project.h"

#include <QButtonGroup>
#include <QColor>
#include <QContextMenuEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRect>
#include <QScrollArea>
#include <QShortcut>
#include <QSize>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstring>
#include <vector>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

// ---------------------------------------------------------------------------
// Canvas: Kartenraster + XP-Ebenen/Ereignis-Modus
// ---------------------------------------------------------------------------

class QtMapTabCanvas : public QWidget {
public:
    QtMapTabCanvas(rpg::Engine* engine, QWidget* parent = nullptr)
        : QWidget(parent), mEngine(engine) {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus); // Entf-Taste im EV-Modus
    }

    int layer = 0;        // aktive Tile-Ebene (0..2)
    int tileId = 0;       // -1 = Radierer
    int cell = 20;
    bool eventMode = false;
    // Zeichenwerkzeug (XP): 0 = Stift, 1 = Rechteck, 2 = Ellipse, 3 = Flutfuellung
    int tool = 0;

    std::function<void(int, int)> onPaint;           // (x,z) gemalt
    std::function<void(int, int)> onHover;           // (x,z) Mausposition
    std::function<void(int, int)> onEventActivated;  // Doppelklick im EV-Modus
    std::function<void()> onDeleteEvent;             // Entf im EV-Modus
    std::function<void()> onStrokeBegin;             // Mal-Schritt beginnt (Maus drücken)
    std::function<void()> onStrokeEnd;               // Mal-Schritt endet (Maus loslassen)
    std::function<void(int, int, int, int, int)> onTileEdited; // (x,z,layer,vorher,nachher)
    std::function<void(int, int, const QPoint&)> onContextMenu; // Rechtsklick: (x,z,globalPos)

    int selectedEventId = -1;

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
        // PAKET 40: Hintergrund/Leer-Farben an das App-Theme koppeln
        // (XP-Classic = helle Arbeitsflaeche) statt hart verdrahteten
        // Dunkeltoenen; Raster-/Markenfarben folgen der Kontrastlage.
        const QColor bg = palette().color(QPalette::Base).darker(104);
        p.fillRect(rect(), bg);
        if (!mEngine || !mEngine->IsInitialized()) {
            p.setPen(palette().color(QPalette::Text));
            p.drawText(rect(), Qt::AlignCenter,
                       QL("Keine Karte geladen.\nProjekt öffnen oder Karte anlegen (Map-Dock)."));
            return;
        }
        auto& map = mEngine->GetMap();
        const int w = map.GetWidth();
        const int h = map.GetHeight();

        // Leere Felder: Theme-Untergrund, etwas dunkler als die Flaeche.
        const QColor emptyCol = bg.darker(115);

        // einfache Füllfarben je Tile (stabil gehasht)
        auto tileColor = [emptyCol](int id) -> QColor {
            if (id <= 0) return emptyCol;
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
        // Raster (PAKET 40: auf hellem Untergrund dunkel, sonst hell)
        const bool lightTheme = bg.value() > 140;
        p.setPen(lightTheme ? QColor(30, 45, 70, 30) : QColor(255, 255, 255, 28));
        for (int x = 0; x <= w; ++x)
            p.drawLine(x * cell, 0, x * cell, h * cell);
        for (int z = 0; z <= h; ++z)
            p.drawLine(0, z * cell, w * cell, z * cell);

        // Ereignisse einzeichnen (nur im EV-Modus sichtbar, wie bei XP)
        if (eventMode) {
            auto& events = rpg::EventSystem::Get().GetEvents();
            for (const auto& ev : events) {
                if (ev.erased || ev.x < 0 || ev.z < 0 || ev.x >= w || ev.z >= h) continue;
                const QRect rc(ev.x * cell, ev.z * cell, cell, cell);
                // XP: dunkles Kästchen mit Name/Grafik-Platzhalter
                p.fillRect(rc, QColor(40, 120, 130, 190));
                p.setPen(QPen(QColor(200, 240, 250, 220), 1));
                p.drawRect(rc.adjusted(0, 0, -1, -1));
                QString label = QString::fromStdString(ev.name);
                if (label.isEmpty()) label = QL("EV%1").arg(ev.id, 3, 10, QLatin1Char('0'));
                if (cell >= 14) {
                    p.setPen(Qt::white);
                    p.drawText(rc, Qt::AlignCenter, p.fontMetrics().elidedText(
                        label, Qt::ElideRight, cell - 2));
                }
                if (ev.id == selectedEventId) {
                    p.setPen(QPen(QColor(255, 220, 90), 2));
                    p.drawRect(rc.adjusted(1, 1, -2, -2));
                }
            }
        }

        // Startposition markieren
        if (mStartX >= 0 && mStartZ >= 0 && mStartX < w && mStartZ < h) {
            p.setPen(QPen(QColor(80, 200, 255), 2));
            p.drawEllipse(QPointF(mStartX * cell + cell / 2.0, mStartZ * cell + cell / 2.0),
                          cell * 0.3, cell * 0.3);
        }

        // Form-Vorschau waehrend Drag (Rechteck/Ellipse)
        if (mDragging && mDragX0 >= 0) {
            int x0 = mDragX0, z0 = mDragZ0, x1 = mDragX1, z1 = mDragZ1;
            if (x0 > x1) std::swap(x0, x1);
            if (z0 > z1) std::swap(z0, z1);
            const QRect rc(x0 * cell, z0 * cell, (x1 - x0 + 1) * cell, (z1 - z0 + 1) * cell);
            QColor fill = tileColor(tileId < 0 ? 0 : tileId);
            fill.setAlpha(110);
            p.setBrush(fill);
            p.setPen(QPen(QColor(255, 220, 90), 2));
            if (tool == 2) p.drawEllipse(rc);   // Ellipse
            else p.drawRect(rc);                // Rechteck
            p.setBrush(Qt::NoBrush);
        }

        // Hover-Feld
        if (mHoverX >= 0 && mHoverX < w && mHoverZ >= 0 && mHoverZ < h) {
            p.setPen(QPen(QColor(255, 220, 90), 1));
            p.drawRect(mHoverX * cell, mHoverZ * cell, cell - 1, cell - 1);
        }
    }

    int eventAt(int x, int z) const {
        auto& events = rpg::EventSystem::Get().GetEvents();
        for (const auto& ev : events)
            if (!ev.erased && ev.x == x && ev.z == z) return ev.id;
        return -1;
    }

    void mousePressEvent(QMouseEvent* e) override {
        setFocus();
        if (e->button() == Qt::LeftButton) {
            if (eventMode) {
                const int x = e->pos().x() / cell;
                const int z = e->pos().y() / cell;
                selectedEventId = eventAt(x, z);
                if (onHover) onHover(x, z);
                update();
                return;
            }
            const int x = e->pos().x() / cell;
            const int z = e->pos().y() / cell;
            if (tool == 1 || tool == 2) {
                // Form-Werkzeuge (Rechteck/Ellipse): Drag beginnen,
                // Stroke beginnt erst bei Release (abbruchbar via Rechtsklick)
                mDragging = true;
                mDragX0 = mDragX1 = x;
                mDragZ0 = mDragZ1 = z;
                update();
            } else if (tool == 3) {
                // Flutfuellung: sofort, als ein Verlaufs-Eintrag
                if (onStrokeBegin) onStrokeBegin();
                applyFloodAt(x, z);
                if (onStrokeEnd) onStrokeEnd();
                update();
            } else {
                mPainting = true;
                if (onStrokeBegin) onStrokeBegin();
                applyAt(e->pos());
            }
        } else if (e->button() == Qt::RightButton && mDragging) {
            // Form-Zug abbrechen (vor Stroke-Beginn -> kein Verlauf)
            mDragging = false;
            update();
        }
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        if (onContextMenu)
            onContextMenu(e->pos().x() / cell, e->pos().y() / cell, e->globalPos());
        e->accept();
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (eventMode && e->button() == Qt::LeftButton) {
            const int x = e->pos().x() / cell;
            const int z = e->pos().y() / cell;
            selectedEventId = eventAt(x, z);
            if (onEventActivated) onEventActivated(x, z);
            update();
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        const int x = e->pos().x() / cell;
        const int z = e->pos().y() / cell;
        mHoverX = x;
        mHoverZ = z;
        if (onHover) onHover(x, z);
        if (mPainting) applyAt(e->pos());
        if (mDragging) { mDragX1 = x; mDragZ1 = z; }
        update();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (mPainting && e->button() == Qt::LeftButton) {
            mPainting = false;
            if (onStrokeEnd) onStrokeEnd();
        }
        if (mDragging && e->button() == Qt::LeftButton) {
            mDragging = false;
            if (onStrokeBegin) onStrokeBegin();
            applyShape(tool == 2, mDragX0, mDragZ0, mDragX1, mDragZ1);
            if (onStrokeEnd) onStrokeEnd();
            update();
        }
    }
    void leaveEvent(QEvent*) override {
        mHoverX = mHoverZ = -1;
        update();
    }
    void keyPressEvent(QKeyEvent* e) override {
        if (eventMode && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
            if (onDeleteEvent) onDeleteEvent();
            return;
        }
        QWidget::keyPressEvent(e);
    }

public:
    int mStartX = -1, mStartZ = -1;
    void setStartPos(int x, int z) { mStartX = x; mStartZ = z; }

private:
    // einzelnes Feld bemalen (gibt true bei echter Aenderung)
    bool paintCell(int x, int z) {
        if (!mEngine || !mEngine->IsInitialized()) return false;
        auto& map = mEngine->GetMap();
        if (x < 0 || z < 0 || x >= map.GetWidth() || z >= map.GetHeight()) return false;
        // PAKET 35 (Bugfix): -1 = Radierer -> LEER (-1) schreiben. Vorher
        // wurde <0 auf 0 gemappt und der Radierer malte damit Tile 0
        // (das linke obere Tile des Sets) statt zu loeschen.
        const int nv = tileId;
        const int before = map.GetTile(layer, x, z);
        if (before == nv) return false; // keine echte Änderung -> auch kein Verlauf
        map.SetTile(layer, x, z, nv); // setzt mDirty -> 3D baut neu
        if (onTileEdited) onTileEdited(x, z, layer, before, nv);
        if (onPaint) onPaint(x, z);
        return true;
    }

    void applyAt(const QPoint& pos) {
        const int x = pos.x() / cell;
        const int z = pos.y() / cell;
        if (paintCell(x, z)) update();
    }

    // Rechteck/Ellipse-Fuellung zwischen zwei Drag-Ecken (inkl. 1-Zelle)
    void applyShape(bool ellipse, int x0, int z0, int x1, int z1) {
        if (!mEngine || !mEngine->IsInitialized()) return;
        if (x0 > x1) std::swap(x0, x1);
        if (z0 > z1) std::swap(z0, z1);
        auto& map = mEngine->GetMap();
        // Drag komplett ausserhalb -> nichts zu tun
        if (x1 < 0 || z1 < 0 || x0 >= map.GetWidth() || z0 >= map.GetHeight()) return;

        const int sw = x1 - x0 + 1;
        const int sh = z1 - z0 + 1;
        const double cx = (x0 + x1) * 0.5;
        const double cz = (z0 + z1) * 0.5;
        const double rx = sw * 0.5;
        const double rz = sh * 0.5;
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (ellipse && sw > 1 && sh > 1) {
                    // normierte Ellipsengleichung (Zellzentren)
                    const double px = (x - cx) / rx;
                    const double pz = (z - cz) / rz;
                    if (px * px + pz * pz > 1.0001) continue;
                }
                paintCell(x, z); // Bounds-Schnitt passiert darin
            }
        }
    }

    // Flutfuellung (iterativ, 4-Nachbarschaft) auf der aktiven Ebene
    void applyFloodAt(int sx, int sz) {
        if (!mEngine || !mEngine->IsInitialized()) return;
        auto& map = mEngine->GetMap();
        if (sx < 0 || sz < 0 || sx >= map.GetWidth() || sz >= map.GetHeight()) return;
        // PAKET 35 (Bugfix): -1 = Radierer -> leere Felder fuellen, siehe
        // paintCell (vorher: <0 -> 0, Flufuellung mit Tile 0).
        const int nv = tileId;
        const int target = map.GetTile(layer, sx, sz);
        if (target == nv) return;

        std::queue<std::pair<int, int>> q;
        std::unordered_set<qint64> seen;
        q.push({sx, sz});
        seen.insert((qint64)sz * 1000000 + sx);
        while (!q.empty()) {
            auto [x, z] = q.front();
            q.pop();
            const int before = map.GetTile(layer, x, z);
            if (before != target) continue;
            map.SetTile(layer, x, z, nv);
            if (onTileEdited) onTileEdited(x, z, layer, before, nv);
            static const int dx[4] = {1, -1, 0, 0};
            static const int dz[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; ++k) {
                const int nx = x + dx[k], nz = z + dz[k];
                if (nx < 0 || nz < 0 || nx >= map.GetWidth() || nz >= map.GetHeight()) continue;
                const qint64 key = (qint64)nz * 1000000 + nx;
                if (seen.count(key)) continue;
                if (map.GetTile(layer, nx, nz) != target) continue;
                seen.insert(key);
                q.push({nx, nz});
            }
        }
        if (onPaint) onPaint(sx, sz); // 3D-Aktualisierung + Statuszeile
    }

    rpg::Engine* mEngine = nullptr;
    bool mPainting = false;
    bool mDragging = false;                     // Form-Werkzeug aktiv (Drag)
    int mDragX0 = -1, mDragZ0 = -1;             // Drag-Start (Zelle)
    int mDragX1 = -1, mDragZ1 = -1;             // Drag aktuell (Zelle)
    int mHoverX = -1, mHoverZ = -1;
};

// ---------------------------------------------------------------------------
// Tab mit XP-Werkzeugzeile (Ebenen 1/2/3/EV, Zoom) + ScrollArea
// ---------------------------------------------------------------------------

QtMapTab::QtMapTab(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    auto* tb = new QHBoxLayout();
    tb->addWidget(new QLabel(QL("Ebene:"), this));

    // XP: Buttons 1 / 2 / 3 / Ereignisse
    mModeGroup = new QButtonGroup(this);
    mModeGroup->setExclusive(true);
    static const char* labels[4] = {"1", "2", "3", "EV"};
    static const char* tips[4] = {
        "Ebene 1 (XP: untere Ebene)",
        "Ebene 2 (XP: mittlere Ebene)",
        "Ebene 3 (XP: obere Ebene)",
        "Ereignis-Modus (XP): Events ansehen/anlegen/bearbeiten (Doppelklick/Entf)"
    };
    for (int i = 0; i < 4; ++i) {
        mModeBtns[i] = new QToolButton(this);
        mModeBtns[i]->setText(QString::fromLatin1(labels[i])); // QStringLiteral braucht Literale!
        mModeBtns[i]->setToolTip(QString::fromLatin1(tips[i]));
        mModeBtns[i]->setCheckable(true);
        mModeBtns[i]->setAutoRaise(true);
        mModeGroup->addButton(mModeBtns[i], i);
        tb->addWidget(mModeBtns[i]);
    }
    mModeBtns[0]->setChecked(true);
    connect(mModeGroup, &QButtonGroup::idClicked, this, &QtMapTab::onModeButton);

    // XP-Zeichenwerkzeuge: Stift / Rechteck / Ellipse / Fuellen (Paket 3)
    tb->addSpacing(12);
    tb->addWidget(new QLabel(QL("Werkzeug:"), this));
    mToolGroup = new QButtonGroup(this);
    mToolGroup->setExclusive(true);
    static const char* toolNames[4] = {"Stift", "Rechteck", "Ellipse", "Füllen"};
    static const char* toolTips[4] = {
        "Stift (XP): freies Malen",
        "Rechteck (XP): Ziehen füllt das Rechteck mit dem gewählten Tile",
        "Ellipse (XP): Ziehen füllt die Ellipse mit dem gewählten Tile",
        "Füllen (XP): Flutfüllung ersetzt zusammenhängende gleiche Tiles"
    };
    for (int i = 0; i < 4; ++i) {
        mToolBtns[i] = new QToolButton(this);
        mToolBtns[i]->setText(QString::fromUtf8(toolNames[i]));
        mToolBtns[i]->setToolTip(QString::fromUtf8(toolTips[i]));
        mToolBtns[i]->setCheckable(true);
        mToolBtns[i]->setAutoRaise(true);
        mToolGroup->addButton(mToolBtns[i], i);
        tb->addWidget(mToolBtns[i]);
    }
    mToolBtns[0]->setChecked(true);
    connect(mToolGroup, &QButtonGroup::idClicked, this, [this](int id) {
        if (mCanvas) mCanvas->tool = id;
    });

    tb->addSpacing(12);
    tb->addWidget(new QLabel(QL("Zoom:"), this));
    mZoomSpin = new QSpinBox(this);
    mZoomSpin->setRange(4, 64);
    mZoomSpin->setValue(20);
    mZoomSpin->setSuffix(QL(" px"));
    mZoomSpin->setToolTip(QL("Kachelgröße der 2D-Ansicht in Pixeln"));
    tb->addWidget(mZoomSpin);

    // Easy-to-use: Rückgängig / Wiederholen für das 2D-Malen
    tb->addSpacing(12);
    auto* undoBtn = new QToolButton(this);
    undoBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    undoBtn->setToolTip(QL("Rückgängig [Strg+Z]"));
    undoBtn->setAutoRaise(true);
    connect(undoBtn, &QToolButton::clicked, this, [this]() { undo(); });
    tb->addWidget(undoBtn);
    auto* redoBtn = new QToolButton(this);
    redoBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    redoBtn->setToolTip(QL("Wiederholen [Strg+Y]"));
    redoBtn->setAutoRaise(true);
    connect(redoBtn, &QToolButton::clicked, this, [this]() { redo(); });
    tb->addWidget(redoBtn);

    auto* scUndo = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Z")), this);
    scUndo->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scUndo, &QShortcut::activated, this, [this]() { undo(); });
    auto* scRedo = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Y")), this);
    scRedo->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scRedo, &QShortcut::activated, this, [this]() { redo(); });

    tb->addStretch(1);
    mPosLabel = new QLabel(QL("Feld: -"), this);
    tb->addWidget(mPosLabel);
    root->addLayout(tb);

    // ---- XP-Hauptbereich: links Tileset-Palette, rechts Karte --------------
    // (PAKET 28: Palette = EIN Tileset-Bild mit 32x32-Klickstellen wie in XP,
    //  statt einzelner Tile-Bloecke/Buttons)
    auto* mainLay = new QHBoxLayout();
    auto* palScroll = new QScrollArea(this);
    palScroll->setWidgetResizable(true);
    palScroll->setMinimumWidth(276);   // 8 Kacheln a 32 px + Rand
    palScroll->setMaximumWidth(288);
    mPaletteHost = new QWidget();
    auto* palLay = new QVBoxLayout(mPaletteHost);
    palLay->setContentsMargins(4, 4, 4, 4);
    auto* palTitle = new QLabel(QL("Tileset"), mPaletteHost);
    palTitle->setStyleSheet(QL("font-weight: bold;"));
    palLay->addWidget(palTitle);

    mPaletteWidget = new QtTilesetGridWidget(mPaletteHost);
    mPaletteWidget->setInteraction(QtTilesetGridWidget::PickTile);
    mPaletteWidget->setZoom(1); // echte 32x32-Stellen wie in XP
    palLay->addWidget(mPaletteWidget, 0, Qt::AlignLeft | Qt::AlignTop);
    connect(mPaletteWidget, &QtTilesetGridWidget::tilePicked,
            this, [this](int tid) {
        setPaintTile(tid);
        emit paintTilePicked(tid); // Dock mitziehen (3D-Pinsel + dortige Palette)
    });

    auto* palBottom = new QHBoxLayout();
    palBottom->setContentsMargins(0, 0, 0, 0);
    mEraserBtn = new QToolButton(mPaletteHost);
    mEraserBtn->setText(QL("Radierer"));
    mEraserBtn->setToolTip(QL("Leeres Feld malen (Tile -1 / Rechtsklick-Malen)"));
    mEraserBtn->setCheckable(true);
    connect(mEraserBtn, &QToolButton::clicked, this, [this]() {
        setPaintTile(-1);
        emit paintTilePicked(-1);
    });
    palBottom->addWidget(mEraserBtn);
    mPaletteSel = new QLabel(QL("Tile: -"), mPaletteHost);
    palBottom->addWidget(mPaletteSel, 1, Qt::AlignRight);
    palLay->addLayout(palBottom);
    palScroll->setWidget(mPaletteHost);
    mainLay->addWidget(palScroll);

    mCanvas = new QtMapTabCanvas(engine);
    mCanvas->setStartPos(0, 0);
    mScroll = new QScrollArea(this);
    mScroll->setWidget(mCanvas);
    mScroll->setWidgetResizable(false);
    mScroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLay->addWidget(mScroll, 1);
    root->addLayout(mainLay, 1);

    connect(mZoomSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int px) {
        mCanvas->cell = px;
        mCanvas->refreshSize();
    });

    mCanvas->onPaint = [this](int x, int z) {
        emit tilesChanged(); // 3D-View benachrichtigen (Map baut Geometrie neu)
        const QString text = QL("Feld: %1, %2  (gemalt)").arg(x).arg(z);
        mPosLabel->setText(text);
        emit hoverInfo(text);
    };
    mCanvas->onHover = [this](int x, int z) {
        if (!mCanvas->isVisible()) return;
        QString text;
        if (mCanvas->eventMode) {
            const int eid = mCanvas->selectedEventId;
            if (eid > 0)
                text = QL("Feld: %1, %2  (Event %3 gewählt)").arg(x).arg(z).arg(eid, 3, 10, QLatin1Char('0'));
            else
                text = QL("Feld: %1, %2  (Doppelklick = neues Event)").arg(x).arg(z);
        } else {
            text = QL("Feld: %1, %2").arg(x).arg(z);
        }
        mPosLabel->setText(text);
        emit hoverInfo(text);
    };
    mCanvas->onEventActivated = [this](int x, int z) { createOrEditEventAt(x, z); };
    mCanvas->onDeleteEvent = [this]() { deleteSelectedEvent(); };
    mCanvas->onStrokeBegin = [this]() { beginStroke(); };
    mCanvas->onStrokeEnd = [this]() { endStroke(); };
    mCanvas->onTileEdited = [this](int x, int z, int layer, int before, int after) {
        recordEdit(x, z, layer, before, after);
    };
    mCanvas->onContextMenu = [this](int x, int z, const QPoint& gp) {
        showCanvasMenu(x, z, gp);
    };
}

void QtMapTab::onModeButton(int id) {
    if (!mCanvas) return;
    if (id >= 3) {
        mCanvas->eventMode = true;
    } else {
        if (mEngine && mEngine->IsInitialized()) ensureLayers(id + 1);
        mCanvas->eventMode = false;
        mCanvas->layer = id;
        mCanvas->selectedEventId = -1;
    }
    mCanvas->refreshSize();
    mCanvas->update();
}

void QtMapTab::ensureLayers(int n) {
    if (!mEngine || !mEngine->IsInitialized()) return;
    auto& map = mEngine->GetMap();
    while ((int)map.GetLayers().size() < n)
        map.AddLayer("Ebene " + std::to_string((int)map.GetLayers().size() + 1));
}

int QtMapTab::currentMapId() const {
    if (mMapIdFn) return mMapIdFn();
    return rpg::Database::Get().System().startMapId;
}

void QtMapTab::saveMapEvents() {
    if (!mEngine) return;
    std::string pp = mEngine->GetProject().GetProjectPath();
    rpg::EventSystem::Get().SaveMapEvents(currentMapId(), pp.empty() ? "." : pp);
}

namespace {

rpg::MapEvent* findEventById(int id) {
    auto& events = rpg::EventSystem::Get().GetEvents();
    for (auto& ev : events)
        if (ev.id == id && !ev.erased) return &ev;
    return nullptr;
}

rpg::MapEvent* findEventAt(int x, int z) {
    auto& events = rpg::EventSystem::Get().GetEvents();
    for (auto& ev : events)
        if (!ev.erased && ev.x == x && ev.z == z) return &ev;
    return nullptr;
}

} // namespace

void QtMapTab::createOrEditEventAt(int x, int z) {
    if (!mEngine || !mEngine->IsInitialized()) return;
    auto& map = mEngine->GetMap();
    if (x < 0 || z < 0 || x >= map.GetWidth() || z >= map.GetHeight()) return;

    auto& es = rpg::EventSystem::Get();
    rpg::MapEvent* ev = findEventAt(x, z);
    bool created = false;

    if (!ev) {
        // XP: Doppelklick auf leeres Feld legt ein neues Event an und öffnet den Dialog
        int nextId = 1;
        for (const auto& e : es.GetEvents()) nextId = qMax(nextId, e.id + 1);
        rpg::MapEvent neu;
        neu.id = nextId;
        char evName[8];
        std::snprintf(evName, sizeof(evName), "EV%03d", nextId);
        neu.name = evName;
        neu.x = x; neu.y = 0; neu.z = z;
        neu.worldPos = rpg::Vec3((float)x, 0.0f, (float)z);
        rpg::EventPage page;
        page.trigger = rpg::EventTrigger::ActionButton;
        neu.pages.push_back(page);
        es.GetEvents().push_back(neu);
        ev = &es.GetEvents().back();
        mCanvas->selectedEventId = ev->id;
        created = true;
    }

    const bool ok = QtEventEditorDialog::EditEvent(this, *ev);
    (void)ok; // XP: auch bei Abbrechen bleibt ein frisch angelegtes Event bestehen
    saveMapEvents();
    mCanvas->update();
    emit eventsChanged();
    emit logMessage(created
        ? QL("Event %1 angelegt @ (%2, %3)").arg(ev->id, 3, 10, QLatin1Char('0')).arg(x).arg(z)
        : QL("Event %1 bearbeitet.").arg(ev->id, 3, 10, QLatin1Char('0')));
}

void QtMapTab::deleteSelectedEvent() {
    if (mCanvas->selectedEventId <= 0) return;
    auto& es = rpg::EventSystem::Get();
    auto& events = es.GetEvents();
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].id == mCanvas->selectedEventId) {
            events.erase(events.begin() + (ptrdiff_t)i);
            mCanvas->selectedEventId = -1;
            saveMapEvents();
            mCanvas->update();
            emit eventsChanged();
            emit logMessage(QL("Event gelöscht."));
            return;
        }
    }
}

void QtMapTab::rebuildPalette() {
    if (!mPaletteWidget || !mEngine || !mEngine->IsInitialized()) return;
    auto tileset = mEngine->GetMap().GetTileset();
    // Cache: nur neu aufbauen, wenn das Tileset wechselt (oder noch leer)
    if (tileset.get() == mLastTileset && mPaletteWidget->tileCount() > 0) {
        updatePaletteSelection();
        return;
    }
    mLastTileset = tileset.get();

    if (!tileset) {
        mPaletteWidget->setImage(QImage(), 0, 0); // zeigt Hinweistext
        mPaletteWidget->setSolidBits({});
        updatePaletteSelection();
        return;
    }

    // Echte Pixel aus der GL-Textur (wie Map-Dock; braucht aktiven Qt-GL-Kontext)
    QImage atlas;
    if (auto* tex = tileset->GetTexture()) {
        std::vector<unsigned char> rgba;
        if (tex->ReadPixelsRGBA(rgba) && tex->GetWidth() > 0 && tex->GetHeight() > 0) {
            const int w = tex->GetWidth();
            const int h = tex->GetHeight();
            atlas = QImage(w, h, QImage::Format_RGBA8888);
            // stbi flippt vertikal beim Laden; glGetTexImage liefert origin bottom-left
            for (int y = 0; y < h; ++y) {
                const unsigned char* src = rgba.data() + (size_t)(h - 1 - y) * w * 4;
                std::memcpy(atlas.scanLine(y), src, (size_t)w * 4);
            }
        }
    }

    const int cols = tileset->GetColumns();
    const int rows = tileset->GetRows();

    // Sperr-Marken (rotes Dreieck) fuer die Palette
    std::vector<bool> solidBits;
    const int count = (cols > 0 && rows > 0) ? cols * rows : 0;
    solidBits.resize((size_t)count, false);
    for (int i = 0; i < count; ++i)
        if (const auto* ti = tileset->GetTileInfo(i))
            if (ti->solid) solidBits[(size_t)i] = true;
    mPaletteWidget->setSolidBits(solidBits);

    // EIN Bild mit 32x32-Klickstellen (XP-Stil); Quellkachelgroesse 32 px
    mPaletteWidget->setImage(atlas, cols > 0 ? cols : 0, rows > 0 ? rows : 0);
    updatePaletteSelection();
}

void QtMapTab::updatePaletteSelection() {
    if (mPaletteSel)
        mPaletteSel->setText(mTileId < 0 ? QL("Tile: Radierer") : QL("Tile: %1").arg(mTileId));
    if (mPaletteWidget)
        mPaletteWidget->setSelectedTile(mTileId >= 0 ? mTileId : -1);
    if (mEraserBtn)
        mEraserBtn->setChecked(mTileId < 0);
}

// ---------------------------------------------------------------------------
// Rückgängig / Wiederholen (2D-Malen) + Rechtsklick-Menü
// ---------------------------------------------------------------------------

void QtMapTab::beginStroke() {
    mStrokeActive = true;
    mStrokeAccum.clear();
}

void QtMapTab::recordEdit(int x, int z, int layer, int before, int after) {
    if (!mStrokeActive) beginStroke();
    const qint64 key = ((qint64)layer << 40) | ((qint64)x << 20) | (qint64)z;
    auto it = mStrokeAccum.find(key);
    if (it == mStrokeAccum.end())
        mStrokeAccum.emplace(key, TileEdit{layer, x, z, before, after});
    else
        it->second.after = after; // erster Startwert bleibt, Endwert wandert mit
}

void QtMapTab::endStroke() {
    if (!mStrokeActive) return;
    mStrokeActive = false;
    if (mStrokeAccum.empty() || !mEngine || !mEngine->IsInitialized()) {
        mStrokeAccum.clear();
        return;
    }
    StrokeEntry e;
    e.mapId = currentMapId();
    e.w = mEngine->GetMap().GetWidth();
    e.h = mEngine->GetMap().GetHeight();
    e.edits.reserve(mStrokeAccum.size());
    for (const auto& kv : mStrokeAccum)
        e.edits.push_back(kv.second);
    mStrokeAccum.clear();
    // Echte Gesamtänderung? (gemalt + wieder übermalt -> before == after)
    bool anyChange = false;
    for (const auto& ed : e.edits)
        if (ed.before != ed.after) { anyChange = true; break; }
    if (!anyChange) return;
    mUndoStrokes.push_back(std::move(e));
    if (mUndoStrokes.size() > 100)
        mUndoStrokes.erase(mUndoStrokes.begin()); // Verlauf begrenzen
    mRedoStrokes.clear(); // neuer Schritt verwirft die Wiederholen-Kette
}

void QtMapTab::undoRedoImpl(std::vector<StrokeEntry>& from,
                            std::vector<StrokeEntry>& to, bool reverse) {
    if (!mEngine || !mEngine->IsInitialized()) return;
    auto& map = mEngine->GetMap();
    while (!from.empty()) {
        StrokeEntry e = std::move(from.back());
        from.pop_back();
        // Strokes anderer Karten / alter Kartengrößen verworfen (Sicherheit)
        if (e.mapId != currentMapId() || e.w != map.GetWidth() || e.h != map.GetHeight())
            continue;
        for (auto it = e.edits.rbegin(); it != e.edits.rend(); ++it)
            map.SetTile(it->layer, it->x, it->z, reverse ? it->before : it->after);
        const int n = static_cast<int>(e.edits.size());
        to.push_back(std::move(e));
        mCanvas->update();
        emit tilesChanged(); // 3D-Geometrie neu bauen
        emit logMessage(reverse ? QL("Rückgängig: %1 Felder.").arg(n)
                                : QL("Wiederholt: %1 Felder.").arg(n));
        emit hoverInfo(reverse ? QL("Rückgängig (%1 Felder)").arg(n)
                               : QL("Wiederholt (%1 Felder)").arg(n));
        return;
    }
    emit hoverInfo(reverse ? QL("Nichts rückgängig zu machen.")
                           : QL("Nichts zu wiederholen."));
}

void QtMapTab::undo() { undoRedoImpl(mUndoStrokes, mRedoStrokes, true); }
void QtMapTab::redo() { undoRedoImpl(mRedoStrokes, mUndoStrokes, false); }

void QtMapTab::showCanvasMenu(int x, int z, const QPoint& globalPos) {
    if (!mEngine || !mEngine->IsInitialized()) return;
    auto& map = mEngine->GetMap();
    const bool inMap = x >= 0 && z >= 0 && x < map.GetWidth() && z < map.GetHeight();

    QMenu menu(this);
    QAction* actNewEv = nullptr;
    QAction* actEditEv = nullptr;
    QAction* actDelEv = nullptr;
    rpg::MapEvent* ev = inMap ? findEventAt(x, z) : nullptr;
    if (mCanvas->eventMode && inMap) {
        if (ev) {
            QString evName = QString::fromStdString(ev->name);
            if (evName.isEmpty()) evName = QL("EV%1").arg(ev->id, 3, 10, QLatin1Char('0'));
            actEditEv = menu.addAction(QL("Ereignis „%1“ bearbeiten …").arg(evName));
            actDelEv = menu.addAction(QL("Ereignis löschen"));
        } else {
            actNewEv = menu.addAction(QL("Neues Ereignis hier …"));
        }
        menu.addSeparator();
    }
    QAction* actStartPos = nullptr;
    if (inMap) actStartPos = menu.addAction(QL("Startposition hierher setzen"));
    QAction* actProps = menu.addAction(QL("Karteneigenschaften …"));

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (actNewEv && chosen == actNewEv) {
        createOrEditEventAt(x, z);
    } else if (actEditEv && chosen == actEditEv && ev) {
        mCanvas->selectedEventId = ev->id;
        const int evId = ev->id;
        const bool ok = QtEventEditorDialog::EditEvent(this, *ev);
        (void)ok;
        saveMapEvents();
        mCanvas->update();
        emit eventsChanged();
        emit logMessage(QL("Event %1 bearbeitet.").arg(evId, 3, 10, QLatin1Char('0')));
    } else if (actDelEv && chosen == actDelEv && ev) {
        mCanvas->selectedEventId = ev->id;
        deleteSelectedEvent();
    } else if (actStartPos && chosen == actStartPos) {
        auto& sys = rpg::Database::Get().System();
        sys.startMapId = currentMapId();
        sys.startX = x;
        sys.startY = z; // 2. Karten-Achse entspricht der Welt-Z-Achse
        mCanvas->setStartPos(x, z);
        mCanvas->update();
        emit logMessage(QL("Startposition gesetzt: (%1, %2) auf Karte %3 – "
                           "wird beim nächsten Speichern übernommen.")
                        .arg(x).arg(z).arg(sys.startMapId));
    } else if (chosen == actProps) {
        emit mapPropertiesRequested();
    }
}

void QtMapTab::refresh() {
    if (!mEngine || !mEngine->IsInitialized()) return;
    ensureLayers(3); // XP-Karten haben immer 3 Ebenen
    const auto& sys = rpg::Database::Get().System();
    mCanvas->setStartPos(sys.startX, sys.startY);
    // Ebene gültig halten
    if (mCanvas->layer >= (int)mEngine->GetMap().GetLayers().size())
        mCanvas->layer = 0;
    rebuildPalette(); // XP-Tileset-Palette (nur bei Wechsel neu)
    mCanvas->refreshSize();
    mCanvas->update();
}

void QtMapTab::setPaintTile(int tileId) {
    mTileId = tileId;
    mCanvas->tileId = tileId;
    mCanvas->update();
    updatePaletteSelection();
}

void QtMapTab::setPaintLayer(int layer) {
    const int idx = layer < 0 ? 0 : (layer > 3 ? 3 : layer);
    if (mModeBtns[idx] && !mModeBtns[idx]->isChecked())
        mModeBtns[idx]->setChecked(true);
    onModeButton(idx);
}

int QtMapTab::paintLayer() const {
    return (mCanvas && mCanvas->eventMode) ? 3 : (mCanvas ? mCanvas->layer : 0);
}

} // namespace qt_editor
