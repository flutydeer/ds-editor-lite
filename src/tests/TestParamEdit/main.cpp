//
// Created by fluty on 24-2-20.
//

#include <QApplication>
#include <QStyleFactory>
#include <QMainWindow>

#include "ParamEditArea.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(f);

    auto area = new ParamEditArea;

    QMainWindow w;
    w.setCentralWidget(area);
    w.resize(1280, 360);
    w.show();

    return QApplication::exec();
}