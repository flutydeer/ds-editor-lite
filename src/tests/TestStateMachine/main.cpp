#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setPointSize(11);
    QApplication::setFont(f);

    MainWindow w;
    w.resize(400, 300);
    w.show();

    return QApplication::exec();
}