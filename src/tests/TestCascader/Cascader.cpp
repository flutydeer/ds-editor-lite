#include "Cascader.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStyleOption>
#include <QVBoxLayout>

#include <cmath>

namespace {
    // Column width inside the popup, close to Element Plus (~180px).
    constexpr int kColumnWidth = 180;
    // Item height in a column list.
    constexpr int kItemHeight = 34;
    // Max popup height before columns scroll individually.
    constexpr int kMaxPopupHeight = 360;
    // Horizontal padding for the trigger text.
    constexpr int kTextPadding = 10;
    // Width reserved for the trigger's arrow icon.
    constexpr int kArrowAreaWidth = 26;
}

// =============================================================================
// CascaderPopup
// =============================================================================
class CascaderPopup : public QWidget {
public:
    explicit CascaderPopup(Cascader *cascader) : QWidget(cascader), m_cascader(cascader) {
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_StyledBackground);
        setAttribute(Qt::WA_WindowPropagation);
        setObjectName(QStringLiteral("cascaderPopup"));
        setProperty("dwmBorder", false);

        m_layout = new QHBoxLayout(this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);
    }

    // (Re)build columns from scratch; currentPath pre-expands the panel to the
    // previous selection if it still resolves.
    void populate(const QList<CascaderNode> &rootNodes, const QList<QVariant> &currentPath) {
        while (QLayoutItem *item = m_layout->takeAt(0)) {
            if (item->widget())
                item->widget()->deleteLater(); // might outlive popup hiding; fine
            delete item;
        }
        m_columnNodes.clear();
        m_activeColumn.clear();
        appendColumn(rootNodes);
        // Expand up to but excluding the leaf value; the leaf node itself is
        // highlighted in its column, ready to click.
        for (qsizetype i = 1; i < currentPath.size(); ++i) {
            if (!advanceToValue(currentPath.at(i)))
                break;
        }
    }

    void showBelow(const QWidget *trigger, int triggerWidth) {
        adjustSize();
        const QPoint global = trigger->mapToGlobal(QPoint(0, trigger->height()));
        QRect rect(global, size());
        if (const auto screen = QApplication::screenAt(global)) {
            const auto avail = screen->availableGeometry();
            if (rect.bottom() > avail.bottom())
                rect.moveBottom(avail.bottom());
            if (rect.right() > avail.right())
                rect.moveRight(avail.right());
            if (rect.left() < avail.left())
                rect.moveLeft(avail.left());
            if (rect.top() < avail.top())
                rect.moveTop(avail.top());
        }
        setGeometry(rect);
        show();
        raise();
    }

private:
    static const CascaderNode *findNode(const QList<CascaderNode> &list, const QVariant &value) {
        for (const auto &node : list) {
            if (node.value == value)
                return &node;
        }
        return nullptr;
    }

    int columnIndexOf(const QListWidget *list) const {
        for (int c = 0; c < m_layout->count(); ++c) {
            if (m_layout->itemAt(c)->widget() == list)
                return c;
        }
        return -1;
    }

    // Select the row with the given value in the last column. If it has
    // children, truncate columns right of it and append those children.
    bool advanceToValue(const QVariant &value) {
        auto *list = qobject_cast<QListWidget *>(m_layout->itemAt(m_layout->count() - 1)->widget());
        if (!list)
            return false;
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            if (item->data(Qt::UserRole) != value)
                continue;
            item->setSelected(true);
            m_activeColumn.fill(-1);
            if (m_activeColumn.size() < m_layout->count())
                m_activeColumn.resize(m_layout->count());
            m_activeColumn[m_layout->count() - 1] = i;
            if (!hasChildren(item))
                return false; // leaf mid-path -> stop expanding
            const auto *node = findNode(m_columnNodes.last(), value);
            if (!node)
                return false;
            appendColumn(node->children);
            return true;
        }
        return false;
    }

    void appendColumn(const QList<CascaderNode> &nodes) {
        auto *list = new QListWidget(this);
        list->setObjectName(QStringLiteral("cascaderPanelColumn"));
        list->setFixedWidth(kColumnWidth);
        list->setUniformItemSizes(true);
        list->setFrameShape(QFrame::NoFrame);
        list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setCursor(Qt::PointingHandCursor);
        list->setMouseTracking(true);
        connect(list, &QListWidget::itemClicked, this, &CascaderPopup::onItemClicked);

        for (const auto &node : nodes) {
            auto *item = new QListWidgetItem;
            item->setText(node.label);
            item->setData(Qt::UserRole, node.value);
            item->setSizeHint(QSize(kColumnWidth, kItemHeight));
            if (!node.isLeaf())
                item->setData(Qt::UserRole + 1, true); // has children
            list->addItem(item);
        }
        m_layout->addWidget(list);
        m_columnNodes.append(nodes);
        m_activeColumn.append(-1);
    }

    bool hasChildren(const QListWidgetItem *item) const {
        return !item->data(Qt::UserRole + 1).isNull();
    }

    void onItemClicked(QListWidgetItem *row) {
        auto *list = qobject_cast<QListWidget *>(sender());
        if (!list)
            return;
        const int clicked = columnIndexOf(list);
        if (clicked < 0)
            return;
        row->setSelected(true);
        m_activeColumn[clicked] = list->row(row);

        const bool leaf = !hasChildren(row);
        if (leaf) {
            // Truncate any columns right of the clicked one, then commit.
            while (m_layout->count() > clicked + 1)
                removeLastColumn();
            commit();
            return;
        }

        // Node has children: show its child column to the right.
        const auto *node = findNode(m_columnNodes.at(clicked), row->data(Qt::UserRole));
        if (!node)
            return;
        while (m_layout->count() > clicked + 1)
            removeLastColumn();
        appendColumn(node->children);
    }

    void removeLastColumn() {
        if (m_layout->count() == 0)
            return;
        QLayoutItem *item = m_layout->takeAt(m_layout->count() - 1);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
        if (!m_columnNodes.isEmpty())
            m_columnNodes.removeLast();
        if (!m_activeColumn.isEmpty())
            m_activeColumn.removeLast();
    }

    void commit() {
        QList<QVariant> valuePath;
        for (int c = 0; c < m_layout->count(); ++c) {
            auto *list = qobject_cast<QListWidget *>(m_layout->itemAt(c)->widget());
            const int idx = m_activeColumn.value(c, -1);
            if (!list || idx < 0)
                continue;
            auto *item = list->item(idx);
            if (!item)
                break;
            valuePath.append(item->data(Qt::UserRole));
        }
        m_cascader->setCurrentValue(valuePath);
        hide();
    }

    QHBoxLayout *m_layout;
    Cascader *m_cascader;
    QList<QList<CascaderNode>> m_columnNodes;
    QVector<int> m_activeColumn;
};

// =============================================================================
// Cascader
// =============================================================================
Cascader::Cascader(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("cascaderTrigger"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMouseTracking(true);
    m_placeholder = QStringLiteral("Select...");
    m_popup = new CascaderPopup(this);
    connect(m_popup, &QWidget::destroyed, this, [this] { m_popup = nullptr; });
}

Cascader::~Cascader() = default;

void Cascader::setOptions(const QList<CascaderNode> &rootNodes) {
    m_options = rootNodes;
    // If the current path no longer resolves, clear it.
    if (!m_valuePath.isEmpty()) {
        QList<CascaderNode> breadcrumb;
        if (!resolvePath(m_valuePath, &breadcrumb))
            m_valuePath.clear();
    }
    update();
}

bool Cascader::resolvePath(const QList<QVariant> &valuePath,
                           QList<CascaderNode> *breadcrumb) const {
    QList<CascaderNode> level = m_options;
    for (const auto &value : valuePath) {
        bool found = false;
        for (qsizetype i = 0; i < level.size(); ++i) {
            if (level[i].value == value) {
                if (breadcrumb)
                    breadcrumb->append(level[i]);
                level = level[i].children;
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

void Cascader::setCurrentValue(const QList<QVariant> &path) {
    QList<CascaderNode> breadcrumb;
    if (!path.isEmpty() && resolvePath(path, &breadcrumb)) {
        if (m_valuePath == path)
            return;
        m_valuePath = path;
        update();
        emit currentValueChanged(m_valuePath);
        return;
    }
    // Invalid path clears the selection.
    if (!m_valuePath.isEmpty()) {
        m_valuePath.clear();
        update();
        emit currentValueChanged(m_valuePath);
    }
}

QString Cascader::currentText() const {
    if (m_valuePath.isEmpty())
        return {};
    QList<CascaderNode> breadcrumb;
    if (!resolvePath(m_valuePath, &breadcrumb))
        return {};
    QStringList labels;
    for (const auto &node : std::as_const(breadcrumb))
        labels << node.label;
    return labels.join(QStringLiteral(" / "));
}

void Cascader::setPlaceholderText(const QString &text) {
    m_placeholder = text;
    update();
}

void Cascader::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !rect().contains(event->pos()))
        return;
    if (m_popup && m_popup->isVisible()) {
        m_ignoreNextShow = false;
        m_popup->hide();
        return;
    }
    m_popup->populate(m_options, m_valuePath);
    m_popup->showBelow(this, width());
}

void Cascader::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter(this);

    const bool hasValue = !m_valuePath.isEmpty();
    const QRect textRect = contentsRect().adjusted(kTextPadding, 0, -kArrowAreaWidth, 0);

    painter.setPen(palette().color(hasValue ? QPalette::Text : QPalette::PlaceholderText));
    painter.setFont(font());
    const QString text = hasValue ? currentText() : m_placeholder;
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

    // Down-chevron arrow, drawn as today's toolbars do (no image dependency).
    const QRectF arrowRect(width() - kArrowAreaWidth, (height() - 16) / 2.0, 16, 16);
    drawArrow(&painter, arrowRect, palette().color(QPalette::Mid));
}

void Cascader::drawArrow(QPainter *painter, const QRectF &rect, const QColor &color) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    const double cx = rect.center().x();
    const double x0 = cx - 4.5;
    const double x1 = cx + 4.5;
    const double y = rect.center().y();
    painter->drawPolyline(QPolygonF()
                          << QPointF(x0, y - 3) << QPointF(cx, y + 2) << QPointF(x1, y - 3));
    painter->restore();
}
