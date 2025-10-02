//
// Created by FlutyDeer on 2025/9/3.
//

#include "PackageDetailsContent.h"

#include "Modules/PackageManager/Models/PackageInfo.h"
#include "UI/Dialogs/PackageManager/Cards/DescriptionCard.h"

#include <QVBoxLayout>

PackageDetailsContent::PackageDetailsContent(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    descriptionCard = new DescriptionCard;
    readMeCard = new ReadMeCard;

    auto layout = new QVBoxLayout;
    layout->addWidget(descriptionCard);
    layout->addWidget(readMeCard);
    setLayout(layout);
}

void PackageDetailsContent::onPackageChanged(const PackageInfo *package) {
    currentPackage = package;
    if (package)
        moveToPackageState(*package);
    else
        moveToNullPackageState();
}

void PackageDetailsContent::moveToNullPackageState() const {
    descriptionCard->onDataContextChanged({});
    readMeCard->onDataContextChanged({});
}

void PackageDetailsContent::moveToPackageState(const PackageInfo &package) const {
    descriptionCard->onDataContextChanged(package.description());
    // readMeCard->onDataContextChanged(
    //     "F:\\Sound libraries\\DiffSinger\\packages\\zhibin-5.2-dsinfer\\inferences\\vocoder\\NOTICE.zh-CN.txt");
    if (package.readme().isEmpty()) {
        readMeCard->onDataContextChanged({});
    } else {
        QFileInfo readmeFileInfo(package.path(), package.readme());
        auto readmePath = readmeFileInfo.absoluteFilePath();
        readMeCard->onDataContextChanged(readmePath);
    }
}
