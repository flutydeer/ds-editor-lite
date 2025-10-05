#include <QApplication>
#include <QStyleFactory>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>

#include "SpeakerMixBar.h"
#include "SpeakerMixList.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    const auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto font = QFont();
    font.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(font);

    const QStringList speakerTypes = {"1", "2", "3"};
    const auto mixList = new SpeakerMixList("test_package", speakerTypes);
    const auto mixBar = mixList->getMixBar();
    const auto addButton = new QPushButton("Add Speaker");

    QObject::connect(addButton, &QPushButton::clicked, mixList, &SpeakerMixList::addRow);

    const auto centralWidget = new QWidget;
    const auto mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(mixList);
    mainLayout->addWidget(addButton);
    mainLayout->addWidget(mixBar);

    QMainWindow mainWindow;
    mainWindow.setCentralWidget(centralWidget);
    mainWindow.resize(640, 360);
    mainWindow.show();

    return QApplication::exec();
}