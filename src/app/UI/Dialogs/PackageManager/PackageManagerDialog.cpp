//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageManagerDialog.h"

#include "Modules/PackageManager/PackageManager.h"
#include "Modules/PackageManager/Tasks/GetInstalledPackagesTask.h"
#include "Modules/Task/TaskManager.h"
#include "synthrt/Core/PackageRef.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Dialogs/PackageManager/PackageDetailsContent.h"
#include "UI/Dialogs/PackageManager/PackageDetailsHeader.h"
#include "UI/Dialogs/PackageManager/PackageFilterProxyModel.h"
#include "UI/Dialogs/PackageManager/PackageItemDelegate.h"
#include "UI/Dialogs/PackageManager/PackageListModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>

PackageManagerDialog::PackageManagerDialog(QWidget *parent) : Dialog(parent) {
    initUi();
}

void PackageManagerDialog::onModuleStatusChanged(AppStatus::ModuleType module,
                                                 AppStatus::ModuleStatus status) {
    if (module != AppStatus::ModuleType::Inference)
        return;
    if (status == AppStatus::ModuleStatus::Ready)
        onInferenceModuleReady();
}

void PackageManagerDialog::updatePackageCount(int count) {
    lbPackageCount->setText(tr("Installed (%1)").arg(count));
}

void PackageManagerDialog::updatePackageList(QList<PackageInfo> packages) {
    listModel->setPackages(std::move(packages));

    proxyModel->setSourceModel(listModel);
    listView->setModel(proxyModel);
}

void PackageManagerDialog::onSelectionChanged(const QModelIndex &current,
                                              const QModelIndex &previous) const {
    if (!current.isValid()) {
        detailsHeader->onPackageChanged(nullptr);
        detailsContent->onPackageChanged(nullptr);
        detailsPanel->setCurrentIndex(PackageUnselected);
        return;
    }
    const QModelIndex sourceIndex = proxyModel->mapToSource(current);
    if (auto package = &listModel->getPackage(sourceIndex))
        detailsPanel->setCurrentIndex(PackageSelected);
    else
        detailsPanel->setCurrentIndex(PackageUnselected);

    detailsHeader->onPackageChanged(&listModel->getPackage(sourceIndex));
    detailsContent->onPackageChanged(&listModel->getPackage(sourceIndex));
}

void PackageManagerDialog::initUi() {
    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(buildPackagePanel());
    mainLayout->addWidget(buildDetailsPanel());
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    body()->setContentsMargins({});
    body()->setLayout(mainLayout);

    listModel = new PackageListModel(this);
    proxyModel = new PackageFilterProxyModel(this);
    proxyModel->setSourceModel(listModel);
    listView->setModel(proxyModel);

    connect(listView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &PackageManagerDialog::onSelectionChanged);

    connect(leSearch, &QLineEdit::textChanged,
            proxyModel, &PackageFilterProxyModel::setFilterString);

    if (appStatus->inferEngineEnvStatus != AppStatus::ModuleStatus::Ready) {
        btnInstall->setEnabled(false);
        connect(appStatus, &AppStatus::moduleStatusChanged, this,
                &PackageManagerDialog::onModuleStatusChanged);
    } else
        onInferenceModuleReady();

    resize(1280, 768);
    setWindowTitle(tr("Package Manager"));
}

void PackageManagerDialog::onInferenceModuleReady() {
    btnInstall->setEnabled(true);
    loadPackageList();
}

void PackageManagerDialog::loadPackageList() {
    auto packages = packageManager->installedPackages();
    if (packages.successfulPackages.empty()) {
        listView->setModel(nullptr);
        lbPackageCount->setText(tr("Installed (0)"));
    }
    updatePackageCount(packages.successfulPackages.size());
    updatePackageList(packages.successfulPackages);
}

QWidget *PackageManagerDialog::buildPackagePanel() {
    btnInstall = new Button(tr("&Install..."));
    lbPackageCount = new QLabel();
    lbPackageCount->setObjectName("lbPackageCount");

    leSearch = new LineEdit;
    leSearch->setPlaceholderText(tr("Search..."));
    leSearch->setClearButtonEnabled(true);

    auto actionBar = new QHBoxLayout;
    actionBar->addWidget(btnInstall);
    actionBar->addStretch();
    actionBar->addWidget(lbPackageCount);

    listView = new QListView;
    listView->setObjectName("PackageManagerDialogPackageListView");
    listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listView->setItemDelegate(new PackageItemDelegate(listView));
    listView->setContentsMargins({});
    listView->setStyleSheet(
        "QListView { background: transparent; border: none; padding: 0px; } "
        "QListView::item { background: transparent; border-radius: 4px; margin-top: 2px; margin-bottom: 2px } "
        "QListView::item:hover { background: #1BC7D8FF; }"
        "QListView::item:selected { background: #409BBAFF; }"
        );

    auto layout = new QVBoxLayout;
    layout->addLayout(actionBar);
    layout->addWidget(leSearch);
    layout->addWidget(listView);
    layout->setContentsMargins({});

    auto panel = new QWidget;
    panel->setObjectName("PackageManagerDialogPackagePanel");
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setStyleSheet(
        "QWidget#PackageManagerDialogPackagePanel { border-right: 1px solid #1D1F26 }"
        "QLabel#lbPackageCount { color: rgba(182, 183, 186, 140); }");
    panel->setLayout(layout);
    panel->setContentsMargins({12, 12, 12, 0});
    panel->setFixedWidth(280);
    return panel;
}

QWidget *PackageManagerDialog::buildDetailsPanel() {
    detailsHeader = new PackageDetailsHeader;
    detailsContent = new PackageDetailsContent;

    auto layout = new QVBoxLayout;
    layout->addWidget(detailsHeader);
    layout->addWidget(detailsContent);
    layout->addStretch(); //TODO: 重构
    layout->setContentsMargins({12, 0, 12, 0});
    layout->setSpacing(12);

    auto detailsWidget = new QWidget;
    detailsWidget->setObjectName("PackageManagerDialogDetailsWidget");
    detailsWidget->setLayout(layout);
    detailsWidget->setContentsMargins({});
    detailsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    detailsWidget->setStyleSheet(
        "QWidget#PackageManagerDialogDetailsWidget { background: transparent; }"
        "PackageDetailsHeader { border-bottom: 1px solid #1D1F26; } "
        "PackageDetailsHeader>QLabel#lbPackageId { font-size: 24px; color: rgb(182, 183, 186); } "
        "PackageDetailsHeader>QLabel#lbVendor { font-size: 13px; color: rgba(182, 183, 186, 140); } "
        "PackageDetailsHeader>QLabel#lbVersion { font-size: 13px; color: rgba(182, 183, 186, 140); } "
        "PackageDetailsHeader>QLabel#lbCopyright { font-size: 13px; color: rgba(182, 183, 186, 140); } ");

    detailsPanelContent = new QScrollArea;
    detailsPanelContent->setObjectName("PackageManagerDialogDetailsScrollArea");
    detailsPanelContent->setStyleSheet(
        "QScrollArea#PackageManagerDialogDetailsScrollArea { padding: 0px; border: none }");
    detailsPanelContent->setWidgetResizable(true);
    detailsPanelContent->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailsPanelContent->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detailsPanelContent->setWidget(detailsWidget);
    detailsPanelContent->viewport()->setContentsMargins({});

    detailsPanelPlaceholder = buildDetailsPanelPlaceholder();

    detailsPanel = new QStackedWidget;
    detailsPanel->addWidget(detailsPanelPlaceholder);
    detailsPanel->addWidget(detailsPanelContent);
    detailsPanel->setCurrentWidget(detailsPanelPlaceholder);

    return detailsPanel;
}

QWidget *PackageManagerDialog::buildDetailsPanelPlaceholder() {
    auto label = new QLabel(tr("Select a package to view details"));
    label->setAlignment(Qt::AlignCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    label->setStyleSheet("QLabel { color: rgba(182, 183, 186, 140); }");

    auto layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->setContentsMargins({12, 0, 12, 0});

    detailsPanelPlaceholder = new QWidget;
    detailsPanelPlaceholder->setLayout(layout);
    detailsPanelPlaceholder->setContentsMargins({});
    return detailsPanelPlaceholder;
}