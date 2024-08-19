#ifndef LYRICCELL_H
#define LYRICCELL_H

#include <QObject>
#include <QApplication>

#include <QGraphicsView>
#include <QGraphicsObject>

#include <language-manager/LangCommon.h>

namespace FillLyric {
    class LyricCell final : public QGraphicsObject {
        Q_OBJECT

    public:
        explicit LyricCell(const qreal &x, const qreal &y, LangNote *note, QGraphicsView *view,
                           QGraphicsItem *parent = nullptr);
        ~LyricCell() override;

        [[nodiscard]] qreal width() const;
        [[nodiscard]] qreal height() const;

        [[nodiscard]] QPainterPath shape() const override;
        [[nodiscard]] QRectF boundingRect() const override;

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
        void updateLyric(FillLyric::LyricCell *cell, const QString &lyric) const;
        void changeSyllable(FillLyric::LyricCell *cell, const QString &syllable) const;

        void clearCell(FillLyric::LyricCell *cell) const;
        void deleteCell(FillLyric::LyricCell *cell) const;
        void addPrevCell(FillLyric::LyricCell *cell) const;
        void addNextCell(FillLyric::LyricCell *cell) const;
        void linebreak(FillLyric::LyricCell *cell) const;

        void updateWidth(const qreal &w) const;

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

    private:
        void updateLyricRect();
        void setQss();
        QVector<QPen> qssPens(const QString &property) const;

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

        QVector<QBrush> m_backgroundBrush = {Qt::NoBrush, QColor(255, 255, 255, 15),
                                             QColor(255, 255, 255, 30)};
        QVector<QPen> m_borderPen = {QPen(QColor(83, 83, 85), 2), QPen(QColor(137, 137, 139), 2),
                                     QPen(QColor(155, 186, 255), 2)};

        enum PenType { MultiTone = 1, Revised, G2pError };

        QVector<QPen> m_lyricPen = {QColor(240, 240, 240), QColor(240, 240, 240),
                                    QColor(240, 240, 240), QColor(240, 240, 240)};
        QVector<QPen> m_syllablePen = {QColor(240, 240, 240), QColor(155, 186, 255),
                                       QColor(255, 204, 153), QColor(255, 155, 157)};
    };
}

#endif // LYRICCELL_H