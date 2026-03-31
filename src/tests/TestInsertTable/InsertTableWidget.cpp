#include "InsertTableWidget.h"
#include <QMouseEvent>
#include <QHeaderView>
#include <QResizeEvent>
#include <QPainter>
#include <QPaintEvent>

InsertTableWidget::InsertTableWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    
    m_insertButton = new InsertButton(this);
    m_insertButton->setVisible(false);
    
    connect(m_insertButton, &InsertButton::insertClicked,
            this, &InsertTableWidget::insertRowAtIndex);
    
    m_table = new QTableWidget(this);
}

InsertTableWidget::~InsertTableWidget()
{
}

void InsertTableWidget::setColumnCount(int count)
{
    m_table->setColumnCount(count);
}

void InsertTableWidget::setRowCount(int count)
{
    m_table->setRowCount(count);
}

void InsertTableWidget::setHorizontalHeaderLabels(const QStringList &labels)
{
    m_table->setHorizontalHeaderLabels(labels);
}

void InsertTableWidget::setItem(int row, int column, QTableWidgetItem *item)
{
    m_table->setItem(row, column, item);
}

int InsertTableWidget::rowCount() const
{
    return m_table->rowCount();
}

int InsertTableWidget::columnCount() const
{
    return m_table->columnCount();
}

void InsertTableWidget::setEdgeDetectionWidth(int width)
{
    m_edgeDetectionWidth = width;
}

int InsertTableWidget::edgeDetectionWidth() const
{
    return m_edgeDetectionWidth;
}

void InsertTableWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    m_table->setGeometry(m_leftMargin, 0, width() - m_leftMargin, height());
    
    if (m_buttonVisible) {
        updateButtonPosition(m_currentInsertRow);
    }
}

void InsertTableWidget::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
    
    if (isInLeftArea(event->pos())) {
        bool isBetweenRows = false;
        int row = getRowAtPosition(event->pos(), isBetweenRows);
        
        if (row >= 0 && isBetweenRows) {
            showInsertButton(row);
            return;
        }
    }
    
    hideInsertButton();
}

void InsertTableWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    hideInsertButton();
}

void InsertTableWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor hotSpotColor(255, 200, 200, 100);
    
    int rowCount = m_table->rowCount();
    if (rowCount == 0) return;
    
    int headerHeight = m_table->horizontalHeader()->height();
    int threshold = 8;
    
    for (int row = 0; row <= rowCount; ++row) {
        int rowTop;
        int rowBottom;
        
        if (row == 0) {
            rowTop = 0;
        } else {
            rowTop = m_table->rowViewportPosition(row - 1) + m_table->rowHeight(row - 1);
        }
        
        if (row == rowCount) {
            rowBottom = rowTop;
        } else {
            rowBottom = m_table->rowViewportPosition(row);
        }
        
        int midPoint = (rowTop + rowBottom) / 2;
        int y = headerHeight + midPoint - threshold;
        int height = threshold * 2;
        
        painter.fillRect(0, y, m_leftMargin, height, hotSpotColor);
    }
}

void InsertTableWidget::insertRowAtIndex(int index)
{
    m_table->insertRow(index);
    
    int colCount = m_table->columnCount();
    for (int col = 0; col < colCount; ++col) {
        m_table->setItem(index, col, new QTableWidgetItem(QString("New Row %1").arg(index)));
    }
    
    hideInsertButton();
}

void InsertTableWidget::showInsertButton(int row)
{
    if (m_currentInsertRow != row || !m_buttonVisible) {
        m_currentInsertRow = row;
        m_buttonVisible = true;
        updateButtonPosition(row);
        m_insertButton->setInsertRow(row);
        m_insertButton->setVisible(true);
        m_insertButton->raise();
    }
}

void InsertTableWidget::hideInsertButton()
{
    m_buttonVisible = false;
    m_currentInsertRow = -1;
    m_insertButton->setVisible(false);
}

int InsertTableWidget::getRowAtPosition(const QPoint &pos, bool &isBetweenRows)
{
    int rowCount = m_table->rowCount();
    if (rowCount == 0) {
        isBetweenRows = false;
        return -1;
    }
    
    int headerHeight = m_table->horizontalHeader()->height();
    int y = pos.y() - headerHeight;
    
    if (y < 0) {
        isBetweenRows = false;
        return -1;
    }
    
    for (int row = 0; row <= rowCount; ++row) {
        int rowTop;
        int rowBottom;
        
        if (row == 0) {
            rowTop = 0;
        } else {
            rowTop = m_table->rowViewportPosition(row - 1) + m_table->rowHeight(row - 1);
        }
        
        if (row == rowCount) {
            rowBottom = rowTop;
        } else {
            rowBottom = m_table->rowViewportPosition(row);
        }
        
        int midPoint = (rowTop + rowBottom) / 2;
        int threshold = 8;
        
        if (y >= midPoint - threshold && y <= midPoint + threshold) {
            isBetweenRows = true;
            return row;
        }
    }
    
    isBetweenRows = false;
    return m_table->rowAt(pos.y());
}

void InsertTableWidget::updateButtonPosition(int row)
{
    int headerHeight = m_table->horizontalHeader()->height();
    int y;
    
    if (row == 0) {
        y = headerHeight - m_insertButton->height() / 2;
    } else if (row >= m_table->rowCount()) {
        y = m_table->rowViewportPosition(m_table->rowCount() - 1) + m_table->rowHeight(m_table->rowCount() - 1) + headerHeight 
            - m_insertButton->height() / 2;
    } else {
        int prevRowBottom = m_table->rowViewportPosition(row - 1) + m_table->rowHeight(row - 1) + headerHeight;
        int currRowTop = m_table->rowViewportPosition(row) + headerHeight;
        y = (prevRowBottom + currRowTop) / 2 - m_insertButton->height() / 2;
    }
    
    int buttonX = (m_leftMargin - m_insertButton->width()) / 2;
    m_insertButton->move(buttonX, y);
}

bool InsertTableWidget::isInLeftArea(const QPoint &pos) const
{
    return pos.x() >= 0 && pos.x() < m_leftMargin;
}

InsertButton::InsertButton(QWidget *parent)
    : QPushButton(parent)
{
    setFixedSize(24, 24);
    setText("+");
    setCursor(Qt::PointingHandCursor);
    
    setStyleSheet(R"(
        QPushButton {
            background-color: #4a9eff;
            color: white;
            border: none;
            border-radius: 12px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2d7dd2;
        }
        QPushButton:pressed {
            background-color: #1a5fa8;
        }
    )");
    
    connect(this, &QPushButton::clicked, this, &InsertButton::onClicked);
}

void InsertButton::setInsertRow(int row)
{
    m_insertRow = row;
}

int InsertButton::insertRow() const
{
    return m_insertRow;
}

void InsertButton::onClicked()
{
    emit insertClicked(m_insertRow);
}
