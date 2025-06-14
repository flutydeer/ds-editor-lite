//
// Created by fluty on 25-6-14.
//

#include "HSLGradientArea.h"

#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(f);

    HSLGradientArea area;
    area.resize(512, 48);
    area.show();

    return QApplication::exec();
}