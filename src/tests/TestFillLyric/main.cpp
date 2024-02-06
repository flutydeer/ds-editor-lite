#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QAbstractButton>

#include "g2pglobal.h"

#include "PhonicWidget.h"

using namespace FillLyric;

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    IKg2p::setDictionaryPath(QApplication::applicationDirPath() + "/dict");

    QWidget *syllableTableView = new PhonicWidget();

    QMainWindow window;
    window.setCentralWidget(syllableTableView);
    // 调整大小
    window.resize(800, 600);
    window.show();

    return QApplication::exec();
}
