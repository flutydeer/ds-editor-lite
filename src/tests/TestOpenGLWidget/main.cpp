//
// Created by fluty on 24-7-4.
//

#include <QApplication>
#include <QStyleFactory>
#include <QMainWindow>

#include "TestGLWidget.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setPointSize(11);
    QApplication::setFont(f);

    auto mainWidget = new TestGLWidget;

    QMainWindow w;
    w.setCentralWidget(mainWidget);
    w.resize(1280, 720);
    w.show();

    return QApplication::exec();
}