//
// Created by FlutyDeer on 2025/9/3.
//

#include "PackageDetailsContent.h"

#include "Modules/PackageManager/Models/PackageInfo.h"
#include "UI/Dialogs/PackageManager/Cards/DescriptionCard.h"

#include <QVBoxLayout>

PackageDetailsContent::PackageDetailsContent(QWidget *parent)  : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    descriptionCard = new DescriptionCard;

    auto layout = new QVBoxLayout;
    layout->addWidget(descriptionCard);
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
}

void PackageDetailsContent::moveToPackageState(const PackageInfo &package) const {
    descriptionCard->onDataContextChanged(package.description());
}