#include <QApplication>
#include <QStyleFactory>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>

#include "InsertTableWidget.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(f);

    auto centralWidget = new QWidget;
    auto layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto titleLabel = new QLabel("InsertTableWidget Test");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    layout->addWidget(titleLabel);

    auto hintLabel = new QLabel("Move cursor to the left edge between rows to see insert button");
    hintLabel->setStyleSheet("color: gray;");
    layout->addWidget(hintLabel);

    auto table = new InsertTableWidget;
    table->setColumnCount(4);
    table->setRowCount(5);
    table->setHorizontalHeaderLabels({"Column 1", "Column 2", "Column 3", "Column 4"});
    table->setEdgeDetectionWidth(30);
    table->tableWidget()->horizontalHeader()->setStretchLastSection(true);
    table->tableWidget()->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 4; ++col) {
            auto item = new QTableWidgetItem(QString("Row %1, Col %2").arg(row + 1).arg(col + 1));
            table->setItem(row, col, item);
        }
    }

    table->setMinimumHeight(400);
    layout->addWidget(table);

    QMainWindow w;
    w.setCentralWidget(centralWidget);
    w.resize(800, 600);
    w.setWindowTitle("Test Insert Table");
    w.show();

    return QApplication::exec();
}
