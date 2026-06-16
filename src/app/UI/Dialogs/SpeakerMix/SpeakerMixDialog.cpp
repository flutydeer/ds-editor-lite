#include "SpeakerMixDialog.h"
#include "SpeakerMixBar.h"
#include "SpeakerMixList.h"

#include <QVBoxLayout>

SpeakerMixDialog::SpeakerMixDialog(QWidget *parent) : Dialog(parent) {
    setWindowTitle("Speaker Mix");
    setTitle("Speaker Mix");
    setMinimumWidth(454);
    setMinimumHeight(312);

    const QStringList speakerTypes = {"朱", "樱", "琪", "梨", "珏"};
    const auto mixList = new SpeakerMixList("test_package", speakerTypes, this);

    const auto layout = new QVBoxLayout(body());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(mixList);
    layout->addWidget(mixList->getMixBar());
}
