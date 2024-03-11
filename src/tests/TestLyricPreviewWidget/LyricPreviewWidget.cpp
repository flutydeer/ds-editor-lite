#include "LyricPreviewWidget.h"
#include "LyricPreviewWidget_p.h"

#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

CellGraphicsItem::CellGraphicsItem(LyricPreviewWidgetCell *cell, QGraphicsItem *parent) : m_cell(cell), QGraphicsItem(parent) {
    setFlag(ItemIsSelectable, true);
    setAcceptHoverEvents(true);
    m_lyric.setTextFormat(Qt::PlainText);
    m_pronunciation.setTextFormat(Qt::PlainText);
    updateRect();
}
CellGraphicsItem::~CellGraphicsItem() {
    Q_UNUSED(m_cell->d_func()->cellItem.release())
}
void CellGraphicsItem::setFont(const QFont &font) {
    m_font = font;
    updateRect();
}
QFont CellGraphicsItem::font() const {
    return m_font;
}
void CellGraphicsItem::setPadding(qreal padding) {
    m_padding = padding;
    updateRect();
}
qreal CellGraphicsItem::padding() const {
    return m_padding;
}
void CellGraphicsItem::setLyric(const QString &lyric) {
    m_lyric.setText(lyric);
    updateRect();
}
QString CellGraphicsItem::lyric() const {
    return m_lyric.text();
}
void CellGraphicsItem::setPronunciation(const QString &pronunciation) {
    m_pronunciation.setText(pronunciation);
    updateRect();
}
QString CellGraphicsItem::pronunciation() const {
    return m_pronunciation.text();
}
void CellGraphicsItem::setBackground(LyricPreviewWidgetCell::State state, const QBrush &brush) {
    m_backgroundBrush[state] = brush;
    update();
}
QBrush CellGraphicsItem::background(LyricPreviewWidgetCell::State state) const {
    return m_backgroundBrush[state];
}
void CellGraphicsItem::setBorder(LyricPreviewWidgetCell::State state, const QPen &border) {
    m_borderPen[state] = border;
    updateRect();
}
QPen CellGraphicsItem::border(LyricPreviewWidgetCell::State state) const {
    return m_borderPen[state];
}
void CellGraphicsItem::setForeground(LyricPreviewWidgetCell::State state, LyricPreviewWidgetCell::Role role, const QPen &pen) {
    if (role == LyricPreviewWidgetCell::Lyric)
        m_lyricPen[state] = pen;
    else
        m_pronunciationPen[state] = pen;
    update();
}
QPen CellGraphicsItem::foreground(LyricPreviewWidgetCell::State state, LyricPreviewWidgetCell::Role role) {
    if (role == LyricPreviewWidgetCell::Lyric)
        return m_lyricPen[state];
    else
        return m_pronunciationPen[state];
}
QRectF CellGraphicsItem::boundingRect() const {
    return m_rect;
}
void CellGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    int flag = 0;
    if (option->state & QStyle::State_MouseOver)
        flag = LyricPreviewWidgetCell::Hovered;
    if (option->state & QStyle::State_Selected)
        flag = LyricPreviewWidgetCell::Selected;

    qreal factor = QFontMetrics(m_font).height();

    painter->setFont(m_font);
    painter->setPen(m_pronunciationPen[flag]);
    painter->drawStaticText(QPointF(0.5 * (m_rect.width() - m_pronunciation.size().width()), 0), m_pronunciation);
    auto boxRect = QRectF(0.5 * (m_rect.width() - m_lyric.size().width()) - m_padding, QFontMetrics(m_font).height() + m_borderPen[flag].widthF(), m_lyric.size().width() + m_padding * 2, factor + m_padding * 2);

    painter->setBrush(m_backgroundBrush[flag]);
    painter->setPen(m_borderPen[flag]);
    painter->drawRoundedRect(boxRect, m_padding * 0.5, m_padding * 0.5);

    painter->setFont(m_font);
    painter->setPen(m_lyricPen[flag]);
    painter->drawStaticText(QPointF(0.5 * (m_rect.width() - m_lyric.size().width()), QFontMetrics(m_font).height() + m_padding + m_borderPen[flag].widthF()), m_lyric);
}
void CellGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsItem::mouseDoubleClickEvent(event);
}
QVariant CellGraphicsItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) {
    return QGraphicsItem::itemChange(change, value);
}
void CellGraphicsItem::updateRect() {
    m_pronunciation.prepare({}, m_font);
    m_lyric.prepare({}, m_font);
    qreal boundWidth = qMax(m_pronunciation.size().width(), m_lyric.size().width() + m_padding * 2);
    qreal boundHeight = m_padding * 2 + QFontMetrics(m_font).height() * 2 + 2;
    m_rect = QRectF(0, 0, boundWidth, boundHeight);
    update();
}


qreal LyricPreviewWidgetPrivate::cellHeight() const {
    Q_Q(const LyricPreviewWidget);
    return cellPadding * 2 + QFontMetrics(q->font()).height() * 2 + 2;
}



LyricPreviewWidgetCell::LyricPreviewWidgetCell(LyricPreviewWidget *parent) : LyricPreviewWidgetCell(parent, *new LyricPreviewWidgetCellPrivate) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem = std::make_unique<CellGraphicsItem>(this);
}
LyricPreviewWidgetCell::~LyricPreviewWidgetCell() = default;
void LyricPreviewWidgetCell::setLyric(const QString &lyric) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setLyric(lyric);
}
QString LyricPreviewWidgetCell::lyric() const {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->lyric();
}
void LyricPreviewWidgetCell::setPronunciation(const QString &pronunciation) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setPronunciation(pronunciation);
}
QString LyricPreviewWidgetCell::pronunciation() const {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->pronunciation();
}
void LyricPreviewWidgetCell::setPronunciationCandidates(const QStringList &pronunciation) {
    Q_D(LyricPreviewWidgetCell);
    d->pronunciationCandidate = pronunciation;
}
QStringList LyricPreviewWidgetCell::pronunciationCandidates() const {
    Q_D(const LyricPreviewWidgetCell);
    return d->pronunciationCandidate;
}
void LyricPreviewWidgetCell::setBackground(State state, const QBrush &brush) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setBackground(state, brush);
}
QBrush LyricPreviewWidgetCell::background(State state) const {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->background(state);
}
void LyricPreviewWidgetCell::setBorder(State state, const QPen &border) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setBorder(state, border);
}
QPen LyricPreviewWidgetCell::border(State state) const {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->border(state);
}
void LyricPreviewWidgetCell::setForeground(State state, LyricPreviewWidgetCell::Role role, const QPen &pen) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setForeground(state, role, pen);
}
QPen LyricPreviewWidgetCell::foreground(State state, LyricPreviewWidgetCell::Role role) {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->foreground(state, role);
}
void LyricPreviewWidgetCell::setSelected(bool selected) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setSelected(selected);
}
bool LyricPreviewWidgetCell::isSelected() const {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->isSelected();
}
void LyricPreviewWidgetCell::setActivated(bool activated) {
}
bool LyricPreviewWidgetCell::isActivated() const {
    return false;
}
void LyricPreviewWidgetCell::setToolTip(const QString &toolTip) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setToolTip(toolTip);
}
QString LyricPreviewWidgetCell::toolTip() const {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->toolTip();
}
LyricPreviewWidgetCell::LyricPreviewWidgetCell(LyricPreviewWidget *parent, LyricPreviewWidgetCellPrivate &d) : QObject(parent), d_ptr(&d) {
    d.q_ptr = this;
}


LyricPreviewWidget::LyricPreviewWidget(QWidget *parent) : LyricPreviewWidget(parent, *new LyricPreviewWidgetPrivate) {
    Q_D(LyricPreviewWidget);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setRenderHint(QPainter::Antialiasing, true);
    d->scene.addRect(0, 0, 0, 0);
}
LyricPreviewWidget::~LyricPreviewWidget() {
}
void LyricPreviewWidget::insertCell(int row, int column, LyricPreviewWidgetCell *cell) {
    Q_D(LyricPreviewWidget);
    cell->setParent(this);
    cell->d_func()->cellItem->setFont(font());
    if (row >= 0 && row < rowCount()) { // inserting to an existing row
        int columnCnt = columnCount(row);
        if (column < 0 || column >= columnCnt) { // appending to the end of the row
            QPointF thisCellPoint;
            if (columnCnt > 0) {
                auto lastCellItem = this->cell(row, columnCnt - 1)->d_func()->cellItem.get();
                thisCellPoint = QPointF(lastCellItem->x() + lastCellItem->boundingRect().width() + d->horizontalMargin, lastCellItem->y());
            } else {
                thisCellPoint = QPointF(d->horizontalMargin, (d->cellHeight() + 2 * d->verticalMargin) * row + d->verticalMargin);
            }
            cell->d_func()->cellItem->setPos(thisCellPoint);
            d->scene.addItem(cell->d_func()->cellItem.get());
            d->items[row].append(cell);
        } else { // inserting within the row
            auto thisCellPoint = this->cell(row, column)->d_func()->cellItem->pos();
            cell->d_func()->cellItem->setPos(thisCellPoint);
            d->scene.addItem(cell->d_func()->cellItem.get());
            d->items[row].insert(column, cell);
            qreal deltaX = cell->d_func()->cellItem->boundingRect().width() + d->horizontalMargin;
            for (int i = column + 1; i < columnCnt + 1; i++) {
                auto nextCell = d->items[row][i];
                nextCell->d_func()->cellItem->setX(nextCell->d_func()->cellItem->x() + deltaX);
            }
        }
    } else { // appending a new row
        row = rowCount();
        d->items.append(QList<LyricPreviewWidgetCell *>{});
        QPointF thisCellPoint = QPointF(d->horizontalMargin, (d->cellHeight() + 2 * d->verticalMargin) * row + d->verticalMargin);
        cell->d_func()->cellItem->setPos(thisCellPoint);
        d->scene.addItem(cell->d_func()->cellItem.get());
        d->items[row].append(cell);
    }
}
void LyricPreviewWidget::insertRow(int row, const QList<LyricPreviewWidgetCell *> &cells) {
    Q_D(LyricPreviewWidget);
    if (row < 0 || row >= rowCount()) { // appending a new row
        row = rowCount();
        d->items.append(QList<LyricPreviewWidgetCell *>{});

    } else { // inserting between existing rows
        for (int i = row; i < rowCount(); i++) {
            for (auto cell : d->items[row]) {
                cell->d_func()->cellItem->setY(cell->d_func()->cellItem->y() + d->cellHeight() + 2 * d->verticalMargin);
            }
        }
        d->items.insert(row, QList<LyricPreviewWidgetCell *>{});
    }
    QPointF thisCellPoint = QPointF(d->horizontalMargin, (d->cellHeight() + 2 * d->verticalMargin) * row + d->verticalMargin);
    for (auto cell : cells) {
        cell->d_func()->cellItem->setPos(thisCellPoint);
        d->scene.addItem(cell->d_func()->cellItem.get());
        d->items[row].append(cell);
        thisCellPoint.setX(thisCellPoint.x() + cell->d_func()->cellItem->boundingRect().width() + d->horizontalMargin);
    }
}
void LyricPreviewWidget::removeCell(LyricPreviewWidgetCell *cell) {
}
void LyricPreviewWidget::removeRow(int row) {
}
int LyricPreviewWidget::rowCount() const {
    Q_D(const LyricPreviewWidget);
    return d->items.size();
}
int LyricPreviewWidget::columnCount(int row) const {
    Q_D(const LyricPreviewWidget);
    if (row < 0 || row >= d->items.size())
        return 0;
    return d->items[row].size();
}
LyricPreviewWidgetCell *LyricPreviewWidget::cell(int row, int column) const {
    Q_D(const LyricPreviewWidget);
    if (row < 0 || row >= d->items.size())
        return nullptr;
    if (column < 0 || column >= d->items[row].size())
        return nullptr;
    return d->items[row][column];
}
LyricPreviewWidgetCell *LyricPreviewWidget::cellAt(const QPoint &point) const {
    return nullptr;
}
QList<LyricPreviewWidgetCell *> LyricPreviewWidget::cells() const {
    return QList<LyricPreviewWidgetCell *>();
}
LyricPreviewWidgetCell *LyricPreviewWidget::currentCell() const {
    return nullptr;
}
void LyricPreviewWidget::setCurrentCell(LyricPreviewWidgetCell *cell) {
}
QList<LyricPreviewWidgetCell *> LyricPreviewWidget::selectedCells() const {
    return QList<LyricPreviewWidgetCell *>();
}
void LyricPreviewWidget::setLineWrap(bool enabled) {
}
bool LyricPreviewWidget::lineWrap() const {
    return false;
}
void LyricPreviewWidget::setSplitter(const QPen &splitter) {
}
QPen LyricPreviewWidget::splitter() const {
    return QPen();
}
void LyricPreviewWidget::setVerticalMargin(qreal margin) {
}
qreal LyricPreviewWidget::verticalMargin() const {
    return 0;
}
void LyricPreviewWidget::setHorizontalMargin(qreal margin) {
}
qreal LyricPreviewWidget::horizontalMargin() const {
    return 0;
}
LyricPreviewWidget::LyricPreviewWidget(QWidget *parent, LyricPreviewWidgetPrivate &d) : QGraphicsView(&d.scene, parent), d_ptr(&d) {
    d.q_ptr = this;
}
