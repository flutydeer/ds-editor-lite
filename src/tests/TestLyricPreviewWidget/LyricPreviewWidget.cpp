#include "LyricPreviewWidget.h"
#include "LyricPreviewWidget_p.h"

#include <QScrollBar>
#include <QTimer>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsProxyWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QAbstractItemView>

CellGraphicsItem::CellGraphicsItem(LyricPreviewWidgetCell *cell, QGraphicsItem *parent) : m_cell(cell), QGraphicsItem(parent) {
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsFocusable, true);
    setAcceptHoverEvents(true);
    m_lyric.setTextFormat(Qt::PlainText);
    m_pronunciation.setTextFormat(Qt::PlainText);
    updateRect();
}
CellGraphicsItem::~CellGraphicsItem() {
    Q_UNUSED(m_cell->d_func()->cellItem.release())
}
int CellGraphicsItem::type() const {
    return UserType + 114514;
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
QPainterPath CellGraphicsItem::shape() const {
    QPainterPath path;
    auto pronunciationRect = QRectF(0.5 * (m_rect.width() - m_pronunciation.size().width()), 0, m_pronunciation.size().width(), QFontMetrics(m_font).height());
    auto boxRect = QRectF(0.5 * (m_rect.width() - m_lyric.size().width()) - m_padding, QFontMetrics(m_font).height() + 2, m_lyric.size().width() + m_padding * 2, QFontMetrics(m_font).height() + m_padding * 2);
    path.addRect(pronunciationRect);
    path.addRect(boxRect);
    return path;
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
    auto boxRect = QRectF(0.5 * (m_rect.width() - m_lyric.size().width()) - m_padding, QFontMetrics(m_font).height() + 2, m_lyric.size().width() + m_padding * 2, factor + m_padding * 2);

    painter->setBrush(m_backgroundBrush[flag]);
    painter->setPen(m_borderPen[flag]);
    painter->drawRoundedRect(boxRect, m_padding * 0.5, m_padding * 0.5);

    painter->setFont(m_font);
    painter->setPen(m_lyricPen[flag]);
    painter->drawStaticText(QPointF(0.5 * (m_rect.width() - m_lyric.size().width()), QFontMetrics(m_font).height() + m_padding + 2), m_lyric);
}
void CellGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    auto pronunciationRect = mapToScene(QRectF(0.5 * (m_rect.width() - m_pronunciation.size().width()), 0, m_pronunciation.size().width(), QFontMetrics(m_font).height())).boundingRect();
    auto boxRect = mapToScene(QRectF(0.5 * (m_rect.width() - m_lyric.size().width()) - m_padding, QFontMetrics(m_font).height() + 2, m_lyric.size().width() + m_padding * 2, QFontMetrics(m_font).height() + m_padding * 2)).boundingRect();
    auto pos = event->scenePos();
    if (pronunciationRect.contains(pos)) {
        if (m_cell->d_func()->lyricPreviewWidget())
            m_cell->d_func()->lyricPreviewWidget()->d_func()->editCell(m_cell, pronunciationRect, LyricPreviewWidgetCell::Pronunciation);
        event->accept();
    } else if (boxRect.contains(pos)) {
        if (m_cell->d_func()->lyricPreviewWidget())
            m_cell->d_func()->lyricPreviewWidget()->d_func()->editCell(m_cell, boxRect, LyricPreviewWidgetCell::Lyric);
        event->accept();
    }
    return QGraphicsItem::mouseDoubleClickEvent(event);
}
QVariant CellGraphicsItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) {
    return QGraphicsItem::itemChange(change, value);
}
void CellGraphicsItem::updateRect() {
    m_pronunciation.prepare({}, m_font);
    m_lyric.prepare({}, m_font);
    qreal boundWidth = qMax(m_pronunciation.size().width(), m_lyric.size().width() + m_padding * 2);
    qreal boundHeight = m_padding * 2 + QFontMetrics(m_font).height() * 2 + 2;
    auto newRect = QRectF(0, 0, boundWidth, boundHeight);
    if (m_rect != newRect) {
        m_rect = newRect;
        if (cell()->d_func()->lyricPreviewWidget())
            cell()->d_func()->lyricPreviewWidget()->d_func()->updateLayout();
    }
    update();
}



SplitterGraphicsItem::SplitterGraphicsItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
}
SplitterGraphicsItem::~SplitterGraphicsItem() = default;
void SplitterGraphicsItem::setPen(const QPen &pen) {
    if (m_pen != pen) {
        m_pen = pen;
        update();
    }
}
QPen SplitterGraphicsItem::pen() const {
    return m_pen;
}
void SplitterGraphicsItem::setBoundingRect(const QRectF &rect) {
    if (m_rect != rect) {
        m_rect = rect;
        update();
    }
}
QRectF SplitterGraphicsItem::boundingRect() const {
    return m_rect;
}
void SplitterGraphicsItem::setLineHeight(qreal height) {
    if (m_lineHeight != height) {
        m_lineHeight = height;
        update();
    }
}
qreal SplitterGraphicsItem::lineHeight() const {
    return m_lineHeight;
}
void SplitterGraphicsItem::setMargin(qreal margin) {
    if (m_margin != margin) {
        m_margin = margin;
        update();
    }
}
qreal SplitterGraphicsItem::margin() const {
    return m_margin;
}
void SplitterGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    if (m_rect.width() < m_margin * 2)
        return;
    for (int i = 1;; i++) {
        QPointF pos = {0, i * m_lineHeight};
        if (pos.y() > m_rect.height())
            break;
        painter->setPen(m_pen);
        painter->drawLine(pos, {m_rect.width(), pos.y()});
    }
}


LyricPreviewWidget *LyricPreviewWidgetCellPrivate::lyricPreviewWidget() const {
    Q_Q(const LyricPreviewWidgetCell);
    return qobject_cast<LyricPreviewWidget *>(q->parent());
}


qreal LyricPreviewWidgetPrivate::cellHeight() const {
    Q_Q(const LyricPreviewWidget);
    return cellPadding * 2 + QFontMetrics(q->font()).height() * 2 + 2;
}
void LyricPreviewWidgetPrivate::updateLayout() {
    for (int row = 0; row < items.size(); row++) {
        auto pos = QPointF(horizontalMargin, (cellHeight() + 2 * verticalMargin) * row + verticalMargin);
        for (auto cell : items[row]) {
            cell->d_func()->cellItem->setPadding(cellPadding);
            cell->d_func()->cellItem->setPos(pos);
            pos.setX(pos.x() + cell->d_func()->cellItem->boundingRect().width() + horizontalMargin);
        }
    }
    splitterItem->setMargin(horizontalMargin);
    splitterItem->setLineHeight(cellHeight() + 2 * verticalMargin);

}
void LyricPreviewWidgetPrivate::updateSplitterRect() {
    Q_Q(LyricPreviewWidget);
    auto rect = q->mapToScene(q->viewport()->rect()).boundingRect();
    qDebug() << rect << scene.sceneRect();
    splitterItem->setX(rect.x() + horizontalMargin);
    QTimer::singleShot(0, [=] {
        splitterItem->setBoundingRect({0, 0, rect.width() - 2 * horizontalMargin, scene.height()});
    });
}
void LyricPreviewWidgetPrivate::editCell(LyricPreviewWidgetCell *cell, QRectF rect, LyricPreviewWidgetCell::Role role) {
    Q_Q(LyricPreviewWidget);
    if (editingItem) {
        editingItem->clearFocus();
    }
    if (role == LyricPreviewWidgetCell::Pronunciation) {
        comboBox->setEditable(true);
        comboBox->addItems(cell->pronunciationCandidates());
        comboBox->setCurrentText(cell->pronunciation());
        for (int i = 0; i < cell->pronunciationCandidates().size(); i++) {
            if (cell->pronunciationCandidates()[i] == cell->pronunciation())
                comboBox->setCurrentIndex(i);
        }
        connect(comboBox, &QComboBox::currentIndexChanged, q, [=] {
            cell->setPronunciation(comboBox->currentText());
            scene.removeItem(editingItem);
            editingCell = nullptr;
            disconnect(comboBox, nullptr, q, nullptr);
            comboBox->clear();
        });
        editingItem->setWidget(comboBox);
    } else {
        lineEdit->setText(cell->lyric());
        connect(lineEdit, &QLineEdit::editingFinished, q, [=] {
            cell->setLyric(lineEdit->text());
            scene.removeItem(editingItem);
            editingCell = nullptr;
            disconnect(lineEdit, nullptr, q, nullptr);
        });
        editingItem->setWidget(lineEdit);
    }
    editingItem->setPos(rect.topLeft());
    editingItem->resize(rect.size().toSize());
    scene.addItem(editingItem);
    comboBox->installEventFilter(this);
    editingItem->widget()->setFocus();
    editingCell = cell;
}
bool LyricPreviewWidgetPrivate::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() != QEvent::FocusOut || static_cast<QFocusEvent *>(event)->reason() == Qt::PopupFocusReason || static_cast<QFocusEvent *>(event)->reason() == Qt::ActiveWindowFocusReason)
        return false;
    if (watched != comboBox)
        return false;
    if (qobject_cast<QComboBox *>(editingItem->widget())) {
        qDebug() << "Focus out" << static_cast<QFocusEvent *>(event)->reason();
        emit comboBox->currentIndexChanged(comboBox->currentIndex());
        return true;
    }
    return false;
}



LyricPreviewWidgetCell::LyricPreviewWidgetCell(LyricPreviewWidget *parent) : LyricPreviewWidgetCell(parent, *new LyricPreviewWidgetCellPrivate) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem = std::make_unique<CellGraphicsItem>(this);
}
LyricPreviewWidgetCell::LyricPreviewWidgetCell(const QString &lyric, const QString &pronunciation, const QStringList &pronunciationCandidates, LyricPreviewWidget *parent) : LyricPreviewWidgetCell(parent) {
    setLyric(lyric);
    setPronunciation(pronunciation);
    setPronunciationCandidates(pronunciationCandidates);
}
LyricPreviewWidgetCell::~LyricPreviewWidgetCell() = default;
void LyricPreviewWidgetCell::setLyric(const QString &lyric) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setLyric(lyric);
    if (d->lyricPreviewWidget())
        emit d->lyricPreviewWidget()->cellChanged(this);
}
QString LyricPreviewWidgetCell::lyric() const {
    Q_D(const LyricPreviewWidgetCell);
    return d->cellItem->lyric();
}
void LyricPreviewWidgetCell::setPronunciation(const QString &pronunciation) {
    Q_D(LyricPreviewWidgetCell);
    d->cellItem->setPronunciation(pronunciation);
    if (d->lyricPreviewWidget())
        emit d->lyricPreviewWidget()->cellChanged(this);
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
    this->installEventFilter(this);
    d->scene.addRect(0, 0, 0, 0);
    d->splitterItem = new SplitterGraphicsItem;
    d->splitterItem->setPos(0, 0);
    d->scene.addItem(d->splitterItem);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [=] { d->updateSplitterRect(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [=] { d->updateSplitterRect(); });
    d->updateLayout();

    d->editingItem = new QGraphicsProxyWidget;
    d->lineEdit = new QLineEdit;
    d->comboBox = new QComboBox;
}
LyricPreviewWidget::~LyricPreviewWidget() {
    Q_D(LyricPreviewWidget);
    disconnect(horizontalScrollBar(), nullptr, this, nullptr);
    disconnect(verticalScrollBar(), nullptr, this, nullptr);
    if (d->editingItem)
        d->editingItem->deleteLater();
    if (d->lineEdit)
        d->lineEdit->deleteLater();
    if (d->comboBox)
        d->comboBox->deleteLater();
}
void LyricPreviewWidget::insertCell(int row, int column, LyricPreviewWidgetCell *cell) {
    Q_D(LyricPreviewWidget);
    if (cell->d_func()->row != -1) {
        qWarning() << "Cell has already been inserted to a LyricPreviewWidget.";
        return;
    }
    cell->setParent(this);
    cell->d_func()->cellItem->setFont(font());
    if (row >= 0 && row < rowCount()) { // inserting to an existing row
        int columnCnt = columnCount(row);
        if (column < 0 || column >= columnCnt) { // appending to the end of the row
            column = columnCnt;
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
                nextCell->d_func()->column++;
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
    cell->d_func()->row = row;
    cell->d_func()->column = column;
}
void LyricPreviewWidget::insertRow(int row, const QList<LyricPreviewWidgetCell *> &cells) {
    Q_D(LyricPreviewWidget);
    if (row < 0 || row >= rowCount()) { // appending a new row
        row = rowCount();
        d->items.append(QList<LyricPreviewWidgetCell *>{});

    } else { // inserting between existing rows
        for (int i = row; i < rowCount(); i++) {
            for (auto cell : d->items[i]) {
                cell->d_func()->cellItem->setY(cell->d_func()->cellItem->y() + d->cellHeight() + 2 * d->verticalMargin);
                cell->d_func()->row++;
            }
        }
        d->items.insert(row, QList<LyricPreviewWidgetCell *>{});
    }
    QPointF thisCellPoint = QPointF(d->horizontalMargin, (d->cellHeight() + 2 * d->verticalMargin) * row + d->verticalMargin);
    for (auto cell : cells) {
        if (cell->d_func()->row != -1) {
            qWarning() << "Cell has already been inserted to a LyricPreviewWidget.";
            continue;
        }
        cell->d_func()->cellItem->setPos(thisCellPoint);
        d->scene.addItem(cell->d_func()->cellItem.get());
        d->items[row].append(cell);
        thisCellPoint.setX(thisCellPoint.x() + cell->d_func()->cellItem->boundingRect().width() + d->horizontalMargin);
        cell->setParent(this);
        cell->d_func()->row = row;
        cell->d_func()->column = d->items[row].size() - 1;
    }
}
void LyricPreviewWidget::removeCell(LyricPreviewWidgetCell *cell) {
    Q_D(LyricPreviewWidget);
    if (cell->parent() != this || cell->d_func()->row == -1) {
        qWarning() << "Cell is not in this LyricPreviewWidget.";
        return;
    }
    int row = cell->d_func()->row;
    int column = cell->d_func()->column;
    qreal deltaX = cell->d_func()->cellItem->boundingRect().width() + d->horizontalMargin;
    d->scene.removeItem(cell->d_func()->cellItem.get());
    d->items[row].removeAt(column);
    cell->d_func()->row = -1;
    cell->d_func()->column = -1;
    cell->setParent(nullptr);
    for (int i = column; i < d->items[row].size(); i++) {
        auto nextCell = d->items[row][i];
        nextCell->d_func()->cellItem->setX(nextCell->d_func()->cellItem->x() - deltaX);
        nextCell->d_func()->column--;
    }
}
void LyricPreviewWidget::removeRow(int row) {
    Q_D(LyricPreviewWidget);
    if (row < 0 || row >= rowCount())
        return;
    for (auto cell : d->items[row]) {
        d->scene.removeItem(cell->d_func()->cellItem.get());
        cell->d_func()->row = -1;
        cell->d_func()->column = -1;
        cell->setParent(nullptr);
    }
    d->items.removeAt(row);
    for (int i = row; i < rowCount(); i++) {
        for (auto cell : d->items[i]) {
            cell->d_func()->cellItem->setY(cell->d_func()->cellItem->y() - d->cellHeight() - 2 * d->verticalMargin);
            cell->d_func()->row--;
        }
    }
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
    Q_D(const LyricPreviewWidget);
    auto item = qgraphicsitem_cast<CellGraphicsItem *>(d->scene.itemAt(mapToScene(point), {}));
    if (item) {
        return item->cell();
    }
    return nullptr;
}
QList<LyricPreviewWidgetCell *> LyricPreviewWidget::cells() const {
    Q_D(const LyricPreviewWidget);
    QList<LyricPreviewWidgetCell *> ret;
    for (const auto &l : d->items) {
        for(auto c : l) {
            ret.append(c);
        }
    }
    return ret;
}
LyricPreviewWidgetCell *LyricPreviewWidget::currentCell() const {
    return nullptr;
}
void LyricPreviewWidget::setCurrentCell(LyricPreviewWidgetCell *cell) {
}
QList<LyricPreviewWidgetCell *> LyricPreviewWidget::selectedCells() const {
    return QList<LyricPreviewWidgetCell *>();
}
void LyricPreviewWidget::setSplitter(const QPen &splitter) {
}
QPen LyricPreviewWidget::splitter() const {
    return QPen();
}
void LyricPreviewWidget::setVerticalMargin(qreal margin) {
    Q_D(LyricPreviewWidget);
    d->verticalMargin = margin;
    d->updateLayout();
}
qreal LyricPreviewWidget::verticalMargin() const {
    Q_D(const LyricPreviewWidget);
    return d->verticalMargin;
}
void LyricPreviewWidget::setHorizontalMargin(qreal margin) {
    Q_D(LyricPreviewWidget);
    d->horizontalMargin = margin;
    d->updateLayout();
}
qreal LyricPreviewWidget::horizontalMargin() const {
    Q_D(const LyricPreviewWidget);
    return d->horizontalMargin;
}
void LyricPreviewWidget::setCellPadding(qreal padding) {
    Q_D(LyricPreviewWidget);
    d->cellPadding = padding;
    d->updateLayout();
}
qreal LyricPreviewWidget::cellPadding() const {
    Q_D(const LyricPreviewWidget);
    return d->cellPadding;
}
LyricPreviewWidget::LyricPreviewWidget(QWidget *parent, LyricPreviewWidgetPrivate &d) : QGraphicsView(&d.scene, parent), d_ptr(&d) {
    d.q_ptr = this;
}
void LyricPreviewWidget::resizeEvent(QResizeEvent *event) {
    Q_D(LyricPreviewWidget);
    QGraphicsView::resizeEvent(event);
    d->updateSplitterRect();
}
void LyricPreviewWidget::wheelEvent(QWheelEvent *event) {
    Q_D(LyricPreviewWidget);
    QGraphicsView::wheelEvent(event);
    d->updateSplitterRect();
}
