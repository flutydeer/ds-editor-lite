#ifndef INFOLANEVIEW_H
#define INFOLANEVIEW_H

#include "UI/Utils/ITimelinePainter.h"

#include <QColor>
#include <QPixmap>
#include <QWidget>

class PlaybackIndicatorOverlay;

// A horizontally-synced strip below the timeline ruler that displays one kind
// of project-wide info (time signature, tempo, markers, ...) as small chips
// anchored at ticks. The strip never scrolls by itself: like TimelineView and
// PhonemeView, it maps the visible tick window received via setTimeRange
// linearly onto the widget width. The background shows the same bar/beat grid
// and playhead lines as the arrangement canvas. Subclasses supply the chip
// list and handle the editing interactions through the virtual hooks.
class InfoLaneView : public QWidget, public ITimelinePainter {
    Q_OBJECT
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QColor markerColor READ markerColor WRITE setMarkerColor)
    Q_PROPERTY(QColor hoverFillColor READ hoverFillColor WRITE setHoverFillColor)
    Q_PROPERTY(QColor barLineColor READ barLineColor WRITE setBarLineColor)
    Q_PROPERTY(QColor beatLineColor READ beatLineColor WRITE setBeatLineColor)
    Q_PROPERTY(QColor commonLineColor READ commonLineColor WRITE setCommonLineColor)
    Q_PROPERTY(QColor playheadColor READ playheadColor WRITE setPlayheadColor)
    Q_PROPERTY(QColor lastPlayheadColor READ lastPlayheadColor WRITE setLastPlayheadColor)

public:
    struct Chip {
        int id = 0;   // subclass-defined identity (e.g. bar index of the point)
        int tick = 0; // anchor position on the project timeline
        QString text;
    };

    explicit InfoLaneView(QWidget *parent = nullptr);

public slots:
    void setTimeRange(double startTick, double endTick);

signals:
    void wheelHorScale(QWheelEvent *event);
    void wheelHorScroll(QWheelEvent *event);
    void wheelVerScroll(QWheelEvent *event);

protected:
    // Chips must be sorted ascending by tick; the base class truncates a
    // chip's text at the next chip's anchor so neighbors never overlap.
    void setChips(QList<Chip> chips);
    [[nodiscard]] const QList<Chip> &chips() const;
    [[nodiscard]] int chipIndexAt(const QPoint &pos) const; // -1 when on blank
    [[nodiscard]] double tickToX(double tick) const;
    [[nodiscard]] double xToTick(double x) const;

    // Interaction hooks; base implementations do nothing
    virtual void chipDoubleClicked(const Chip &chip);
    virtual void blankDoubleClicked(const QPoint &pos);
    virtual void chipContextMenuRequested(const Chip &chip, const QPoint &globalPos);
    virtual void blankContextMenuRequested(const QPoint &globalPos);

    // Grid lines in the same style as the arrangement canvas background
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawSubdivision(QPainter *painter, int tick, int level, int levelCount) override;

    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    [[nodiscard]] QRectF chipRect(int index) const;
    void setHoveredChip(int index);
    void setPosition(double tick);
    void setLastPosition(double tick);
    void updatePlaybackIndicator();
    void updateContent();
    void renderContent(QPainter *painter);

    // Theme color accessors (QSS-overridable via qproperty-*)
    [[nodiscard]] QColor textColor() const;
    void setTextColor(const QColor &color);
    [[nodiscard]] QColor markerColor() const;
    void setMarkerColor(const QColor &color);
    [[nodiscard]] QColor hoverFillColor() const;
    void setHoverFillColor(const QColor &color);
    [[nodiscard]] QColor barLineColor() const;
    void setBarLineColor(const QColor &color);
    [[nodiscard]] QColor beatLineColor() const;
    void setBeatLineColor(const QColor &color);
    [[nodiscard]] QColor commonLineColor() const;
    void setCommonLineColor(const QColor &color);
    [[nodiscard]] QColor playheadColor() const;
    void setPlayheadColor(const QColor &color);
    [[nodiscard]] QColor lastPlayheadColor() const;
    void setLastPlayheadColor(const QColor &color);

    QColor m_textColor = {200, 201, 204};
    QColor m_markerColor = {200, 201, 204};
    QColor m_hoverFillColor = {255, 255, 255, 26};
    QColor m_barLineColor = {8, 9, 10};
    QColor m_beatLineColor = {22, 25, 28};
    QColor m_commonLineColor = {28, 32, 36};
    QColor m_playheadColor = {200, 200, 200};
    QColor m_lastPlayheadColor = {160, 160, 160};

    double m_startTick = 0;
    double m_endTick = 0;
    QList<Chip> m_chips;
    int m_hoveredChip = -1;
    double m_position = 0;
    double m_lastPosition = 0;
    PlaybackIndicatorOverlay *m_playbackIndicatorOverlay = nullptr;
    QPixmap m_contentCache;
    bool m_contentCacheDirty = true;
};

#endif // INFOLANEVIEW_H
