#include "PackageDetailsContent.h"

#include "Utils/UiLanguageManager.h"

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
    descriptionCard->setTitle(tr("Description"));
    descriptionCard->onDataContextChanged({});
    readMeCard->onDataContextChanged({});
}

void PackageDetailsContent::moveToPackageState(const PackageInfo &package) const {
    // A package that would not open has no description and no readme to read; the reason it was
    // refused takes the card instead, in full and unedited, because that is the whole of what
    // there is to say about it.
    if (package.isUnavailable()) {
        descriptionCard->setTitle(tr("Unavailable"));
        descriptionCard->onDataContextChanged(package.unavailableReason());
        readMeCard->onDataContextChanged({});
        return;
    }
    descriptionCard->setTitle(tr("Description"));
    descriptionCard->onDataContextChanged(
        package.displayDescription(UiLanguageManager::currentBcp47Candidates()));
    if (package.readme().isEmpty()) {
        readMeCard->onDataContextChanged({});
    } else {
        QFileInfo readmeFileInfo(package.path(), package.readme());
        auto readmePath = readmeFileInfo.absoluteFilePath();
        readMeCard->onDataContextChanged(readmePath);
    }
}
