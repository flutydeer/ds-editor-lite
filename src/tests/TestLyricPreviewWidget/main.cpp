//
// Created by Crs_1 on 2024/3/11.
//

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>

#include "LyricPreviewWidget.h"


int main(int argc, char **argv) {
    QApplication a(argc, argv);
    QMainWindow win;
    qDebug() << QFontMetrics(a.font()).height();
    auto mainWidget = new QWidget;
    auto mainLayout = new QVBoxLayout;
    mainWidget->setLayout(mainLayout);
    win.setCentralWidget(mainWidget);

    auto lyricPreviewWidget = new LyricPreviewWidget;
    mainLayout->addWidget(lyricPreviewWidget);

    lyricPreviewWidget->setBackgroundBrush(QColor(0x2a, 0x2b, 0x2c));

    auto cell2 = new LyricPreviewWidgetCell;
    cell2->setLyric("テ");
    cell2->setPronunciation("te");

    auto cell3 = new LyricPreviewWidgetCell;
    cell3->setLyric("ス");
    cell3->setPronunciation("su");

    auto cell1 = new LyricPreviewWidgetCell;
    cell1->setLyric("ト");
    cell1->setPronunciation("to");

    lyricPreviewWidget->insertCell(0, -1, cell2);
    lyricPreviewWidget->insertCell(0, -1, cell1);
    lyricPreviewWidget->insertCell(0, 1, cell3);

    lyricPreviewWidget->insertRow(-1, {});

    auto cell4 = new LyricPreviewWidgetCell;
    cell4->setLyric("测");
    cell4->setPronunciation("ce");

    auto cell5 = new LyricPreviewWidgetCell;
    cell5->setLyric("试");
    cell5->setPronunciation("shi");

    lyricPreviewWidget->insertRow(-1, {cell4, cell5});

    qDebug() << (lyricPreviewWidget->cell(2,0) == cell4);

    auto cell6 = new LyricPreviewWidgetCell;
    cell6->setLyric("TEST");
    cell6->setPronunciation("test");

    lyricPreviewWidget->insertRow(0, {cell6});

    qDebug() << (lyricPreviewWidget->cell(3,0) == cell4);

    lyricPreviewWidget->removeCell(cell4);

    lyricPreviewWidget->removeRow(0);

    lyricPreviewWidget->setHorizontalMargin(16);

    lyricPreviewWidget->insertRow(-1, {new LyricPreviewWidgetCell("长", "chang", {"chang", "zhang"})});

    win.show();
    return a.exec();
}