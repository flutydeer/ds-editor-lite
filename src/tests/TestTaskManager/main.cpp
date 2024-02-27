//
// Created by fluty on 24-2-26.
//

#include <QApplication>
#include <QStyleFactory>
#include <QListWidget>

#include "ProgressIndicator.h"
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(f);


    MainWindow w;
    w.resize(360, 120);
    w.show();

    return QApplication::exec();
}