#ifndef LYRICPREVIEWWIDGET_P_H
#define LYRICPREVIEWWIDGET_P_H

#include <LyricPreviewWidget.h>

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QStaticText>
#include <QPointer>

class CellGraphicsItem : public QGraphicsItem {
public:
    explicit CellGraphicsItem(LyricPreviewWidgetCell *cell, QGraphicsItem *parent = nullptr);
    ~CellGraphicsItem() override;

    int type() const override;

    void setFont(const QFont &font);
    QFont font() const;

    void setPadding(qreal padding);
    qreal padding() const;

    void setLyric(const QString &lyric);
    QString lyric() const;

    void setPronunciation(const QString &pronunciation);
    QString pronunciation() const;

    void setBackground(LyricPreviewWidgetCell::State state, const QBrush &brush);
    QBrush background(LyricPreviewWidgetCell::State state) const;

    void setBorder(LyricPreviewWidgetCell::State state, const QPen &border);
    QPen border(LyricPreviewWidgetCell::State state) const;

    void setForeground(LyricPreviewWidgetCell::State state, LyricPreviewWidgetCell::Role role, const QPen &pen);
    QPen foreground(LyricPreviewWidgetCell::State state, LyricPreviewWidgetCell::Role role);

    QPainterPath shape() const override;
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    LyricPreviewWidgetCell *cell() const {
        return m_cell;
    }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;


private:
    QFont m_font = QApplication::font();
    QStaticText m_lyric;
    QStaticText m_pronunciation;

    QBrush m_backgroundBrush[3] = {QColor(155, 186, 255), QColor(169, 196, 255), QColor(169, 196, 255)};
    QPen m_borderPen[3] = {QPen(QColor(112, 156, 255), 2), QPen(QColor(112, 156, 255), 2), QPen(QColor(Qt::white), 2)};
    QPen m_lyricPen[3] = {QColor(Qt::black), QColor(Qt::black), QColor(Qt::black)};
    QPen m_pronunciationPen[3] = {QColor(Qt::white), QColor(Qt::white), QColor(Qt::white)};

    QRectF m_rect;
    qreal m_padding = 4;

    LyricPreviewWidgetCell *m_cell;

    void updateRect();
};

class SplitterGraphicsItem : public QGraphicsItem {
public:
    explicit SplitterGraphicsItem(QGraphicsItem *parent = nullptr);
    ~SplitterGraphicsItem() override;

    void setPen(const QPen &pen);
    QPen pen() const;

    void setBoundingRect(const QRectF &rect);
    QRectF boundingRect() const override;

    void setLineHeight(qreal height);
    qreal lineHeight() const;

    void setMargin(qreal margin);
    qreal margin() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPen m_pen = QPen(Qt::gray, 1);
    QRectF m_rect;
    qreal m_lineHeight = 4;
    qreal m_margin = 4;
};

class LyricPreviewWidgetPrivate : public QObject {
    Q_DECLARE_PUBLIC(LyricPreviewWidget)
public:
    LyricPreviewWidget *q_ptr;
    QGraphicsScene scene;
    QList<int> columnCountOfRow = {0};
    SplitterGraphicsItem *splitterItem;
    QList<QList<LyricPreviewWidgetCell *>> items = {{}};
    qreal verticalMargin = 8;
    qreal horizontalMargin = 8;
    qreal cellPadding = 4;

    qreal cellHeight() const;

    void updateLayout();
    void updateSplitterRect();

    LyricPreviewWidgetCell *editingCell = nullptr;
    QPointer<QGraphicsProxyWidget> editingItem;
    QPointer<QLineEdit> lineEdit;
    QPointer<QComboBox> comboBox;
    void editCell(LyricPreviewWidgetCell *cell, QRectF rect, LyricPreviewWidgetCell::Role role);

    bool eventFilter(QObject *watched, QEvent *event) override;
};

class LyricPreviewWidgetCellPrivate {
    Q_DECLARE_PUBLIC(LyricPreviewWidgetCell)
public:
    LyricPreviewWidgetCell *q_ptr;
    std::unique_ptr<CellGraphicsItem> cellItem;
    int row = -1;
    int column = -1;
    QStringList pronunciationCandidate;
    bool inScene = false;

    LyricPreviewWidget *lyricPreviewWidget() const;

};

#endif // LYRICPREVIEWWIDGET_P_H
