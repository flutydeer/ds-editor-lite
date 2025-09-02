//
// Created by FlutyDeer on 2025/8/31.
//

#include "PackageDetailsHeader.h"

#include "Modules/PackageManager/Models/PackageInfo.h"
#include "UI/Controls/Button.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>

PackageDetailsHeader::PackageDetailsHeader(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    lbPackageId = new QLabel;
    lbPackageId->setObjectName("lbPackageId");
    // lbPackageId->setText("packageId");

    lbVendor = new QLabel;
    lbVendor->setObjectName("lbVendor");
    // lbVendor->setText("Example Vendor");

    lbVersion = new QLabel;
    lbVersion->setObjectName("lbVersion");
    // lbVersion->setText("v1.0");

    lbCopyright = new QLabel;
    lbCopyright->setObjectName("lbCopyright");
    // lbCopyright->setText("Copyright (C) 2025 FlutyDeer. All rights reserved.");

    auto detailsSecondaryLayout = new QHBoxLayout;
    detailsSecondaryLayout->addWidget(lbVendor);
    detailsSecondaryLayout->addWidget(lbVersion);
    detailsSecondaryLayout->addWidget(lbCopyright);
    detailsSecondaryLayout->setContentsMargins({});
    detailsSecondaryLayout->setSpacing(16);

    auto detailsLayout = new QVBoxLayout;
    detailsLayout->addWidget(lbPackageId);
    detailsLayout->addLayout(detailsSecondaryLayout);
    detailsLayout->setContentsMargins({});
    detailsLayout->setSpacing(8);

    btnOpenWebsite = new Button;
    btnOpenWebsite->setText(tr("Open Website..."));

    btnVerify = new Button;
    btnVerify->setText(tr("Verify"));

    btnUninstall = new Button;
    btnUninstall->setText(tr("Uninstall"));

    auto actionsLayout = new QHBoxLayout;
    actionsLayout->addWidget(btnOpenWebsite);
    actionsLayout->addWidget(btnVerify);
    actionsLayout->addWidget(btnUninstall);
    actionsLayout->setContentsMargins({});
    actionsLayout->setSpacing(8);

    auto layout = new QHBoxLayout;
    layout->addLayout(detailsLayout);
    layout->addStretch();
    layout->addLayout(actionsLayout);
    layout->setContentsMargins({16, 16, 16, 16});
    layout->setSpacing(0);

    setContentsMargins({});
    setLayout(layout);

    moveToNullPackageState();

    connect(btnOpenWebsite, &Button::clicked, this, &PackageDetailsHeader::onOpenWebsiteClicked);
}

void PackageDetailsHeader::onPackageChanged(const PackageInfo *package) {
    currentPackage = package;
    if (package)
        moveToPackageState(*package);
    else
        moveToNullPackageState();
}

void PackageDetailsHeader::onOpenWebsiteClicked() const {
    if (currentPackage && !currentPackage->url().isEmpty())
        QDesktopServices::openUrl(currentPackage->url());
}

void PackageDetailsHeader::moveToNullPackageState() const {
    lbPackageId->setText(tr("No Package Selected"));
    lbVendor->setText({});
    lbVersion->setText({});
    lbCopyright->setText({});

    btnOpenWebsite->setEnabled(false);
    btnOpenWebsite->setVisible(false);

    btnVerify->setEnabled(false);
    btnVerify->setVisible(false);

    btnUninstall->setEnabled(false);
    btnUninstall->setVisible(false);
}

void PackageDetailsHeader::moveToPackageState(const PackageInfo &package) const {
    lbPackageId->setText(package.id());
    lbVendor->setText(package.vendor());
    lbVersion->setText("v" + package.version().toString());
    lbCopyright->setText(package.copyright());

    if (package.url().isEmpty()) {
        btnOpenWebsite->setEnabled(false);
        btnOpenWebsite->setVisible(false);
    } else {
        btnOpenWebsite->setEnabled(true);
        btnOpenWebsite->setVisible(true);
    }

    // TODO: 实现按钮逻辑
    btnVerify->setEnabled(true);
    btnVerify->setVisible(true);

    btnUninstall->setEnabled(true);
    btnUninstall->setVisible(true);
}