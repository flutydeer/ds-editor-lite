#include "PackageDetailsContent.h"

#include <lite/PackageManager/Models/PackageInfo.h>
#include "UI/Dialogs/PackageManager/Cards/DescriptionCard.h"

#include <QVBoxLayout>

PackageDetailsContent::PackageDetailsContent(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    descriptionCard = new DescriptionCard;
    readMeCard = new ReadMeCard;

    auto layout = new QVBoxLayout;
    layout->addWidget(descriptionCard);
    layout->addWidget(readMeCard);
    // Cards carry no bottom margin (see OptionsCard); spacing comes from here.
    layout->setSpacing(12);
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
    if (package.readme().isEmpty()) {
        readMeCard->onDataContextChanged({});
    } else {
        QFileInfo readmeFileInfo(package.path(), package.readme());
        auto readmePath = readmeFileInfo.absoluteFilePath();
        readMeCard->onDataContextChanged(readmePath);
    }
}
