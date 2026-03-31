#ifndef INSERTTABLEWIDGET_H
#define INSERTTABLEWIDGET_H

#include <QTableWidget>
#include <QPushButton>

class InsertButton;

class InsertTableWidget : public QWidget {
    Q_OBJECT

public:
    explicit InsertTableWidget(QWidget *parent = nullptr);
    ~InsertTableWidget() override;

    QTableWidget *tableWidget() const {
        return m_table;
    }

    void setColumnCount(int count);
    void setRowCount(int count);
    void setHorizontalHeaderLabels(const QStringList &labels);
    void setItem(int row, int column, QTableWidgetItem *item);
    int rowCount() const;
    int columnCount() const;

    void setEdgeDetectionWidth(int width);
    int edgeDetectionWidth() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void insertRowAtIndex(int index);

private:
    void showInsertButton(int row);
    void hideInsertButton();
    int getRowAtPosition(const QPoint &pos, bool &isBetweenRows);
    void updateButtonPosition(int row);
    bool isInLeftArea(const QPoint &pos) const;

    QTableWidget *m_table = nullptr;
    InsertButton *m_insertButton = nullptr;
    int m_edgeDetectionWidth = 30;
    int m_leftMargin = 30;
    int m_currentInsertRow = -1;
    bool m_buttonVisible = false;
};

class InsertButton : public QPushButton {
    Q_OBJECT

public:
    explicit InsertButton(QWidget *parent = nullptr);

    void setInsertRow(int row);
    int insertRow() const;

signals:
    void insertClicked(int row);

private slots:
    void onClicked();

private:
    int m_insertRow = -1;
};

#endif // INSERTTABLEWIDGET_H
