#ifndef LYRICCELL_H
#define LYRICCELL_H

#include <QObject>
#include <QApplication>

#include <QGraphicsView>
#include <QGraphicsObject>

#include <LangCommon.h>

namespace FillLyric {
    class LyricCell final : public QGraphicsObject {
        Q_OBJECT
    public:
        explicit LyricCell(const qreal &x, const qreal &y, LangNote *note, QGraphicsView *view,
                           QGraphicsItem *parent = nullptr);
        ~LyricCell() override;

        [[nodiscard]] qreal width() const;
        [[nodiscard]] qreal height() const;

        [[nodiscard]] QRectF lyricRect() const;
        [[nodiscard]] QRectF syllableRect() const;

        [[nodiscard]] LangNote *note() const;
        void setNote(LangNote *note);

        [[nodiscard]] QString lyric() const;
        void setLyric(const QString &lyric);

        [[nodiscard]] QString syllable() const;
        void setSyllable(const QString &syllable);

        void setFont(const QFont &font);
        void setLyricRect(const QRect &rect);
        void setSyllableRect(const QRect &rect);

    Q_SIGNALS:
        void updateLyric(LyricCell *cell, const QString &lyric) const;
        void changeSyllable(LyricCell *cell, const QString &syllable) const;

        void clearCell(LyricCell *cell) const;
        void deleteCell(LyricCell *cell) const;
        void addPrevCell(LyricCell *cell) const;
        void addNextCell(LyricCell *cell) const;
        void linebreak(LyricCell *cell) const;

        void updateWidth(const qreal &w) const;

    protected:
        [[nodiscard]] int type() const override;
        [[nodiscard]] QPainterPath shape() const override;
        [[nodiscard]] QRectF boundingRect() const override;

        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

    private:
        void updateLyricRect();

        [[nodiscard]] qreal lyricWidth() const;
        [[nodiscard]] qreal syllableWidth() const;

        [[nodiscard]] QPointF lyricPos() const;
        [[nodiscard]] QPointF rectPos() const;
        [[nodiscard]] QPointF syllablePos() const;

        void changePhonicMenu(QMenu *menu);
        void changeSyllableMenu(QMenu *menu);

        QRect m_lRect;
        QRect m_sRect;

        LangNote *m_note;
        QGraphicsView *m_view;

        qreal m_lsPadding = 5;
        qreal m_rectPadding = 3;
        qreal m_padding = 3;
        qreal m_reckBorder = 2.5;

        QFont m_font = QApplication::font();

    private:
        enum State {
            Normal = 0,
            Hovered = 1,
            Selected = 2,
        };

        QBrush m_backgroundBrush[3] = {QColor(155, 186, 255), QColor(169, 196, 255),
                                       QColor(169, 196, 255)};
        QPen m_borderPen[3] = {QPen(QColor(112, 156, 255), 2), QPen(QColor(112, 156, 255), 2),
                               QPen(QColor(Qt::white), 2)};

        enum PenType { MultiTone = 1, Revised, G2pError };
        QPen m_lyricPen[4] = {QColor(Qt::black), QColor(Qt::green), QColor(Qt::yellow),
                              QColor(Qt::red)};
        QPen m_syllablePen[4] = {QColor(Qt::white), QColor(Qt::green), QColor(Qt::yellow),
                                 QColor(Qt::red)};
    };
}

#endif // LYRICCELL_H
