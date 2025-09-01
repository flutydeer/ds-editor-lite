//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageManagerDialog.h"

#include "Modules/PackageManager/Tasks/GetInstalledPackagesTask.h"
#include "Modules/Task/TaskManager.h"
#include "synthrt/Core/PackageRef.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Dialogs/PackageManager/PackageDetailsHeader.h"
#include "UI/Dialogs/PackageManager/PackageFilterProxyModel.h"
#include "UI/Dialogs/PackageManager/PackageItemDelegate.h"
#include "UI/Dialogs/PackageManager/PackageListModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QSplitter>

PackageManagerDialog::PackageManagerDialog(QWidget *parent) : Dialog(parent) {
    initUi();
}

void PackageManagerDialog::onModuleStatusChanged(AppStatus::ModuleType module,
                                                 AppStatus::ModuleStatus status) {
    if (module != AppStatus::ModuleType::Inference)
        return;
    if (status == AppStatus::ModuleStatus::Ready) {
        btnInstall->setEnabled(true);
        loadPackageList();
    }
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
                                              const QModelIndex &previous) {
    if (!current.isValid()) {
        detailsHeader->onPackageChanged(nullptr);
        return;
    }

    auto proxyModel = dynamic_cast<PackageFilterProxyModel *>(listView->model());
    if (!proxyModel) {
        detailsHeader->onPackageChanged(nullptr);
        return;
    }

    QModelIndex sourceIndex = proxyModel->mapToSource(current);

    auto sourceModel = dynamic_cast<PackageListModel *>(proxyModel->sourceModel());
    if (!sourceModel) {
        detailsHeader->onPackageChanged(nullptr);
        return;
    }

    detailsHeader->onPackageChanged(&sourceModel->getPackage(sourceIndex));
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
    }

    resize(1280, 768);
}

void PackageManagerDialog::loadPackageList() {
    auto task = new GetInstalledPackagesTask;
    connect(task, &GetInstalledPackagesTask::finished, this,
            [this, task] {
                auto result = task->result;
                if (!result)
                    qCritical() << "加载错误：" << result.getError().message;
                else {
                    if (result.get().successfulPackages.empty()) {
                        listView->setModel(nullptr);
                        lbPackageCount->setText(tr("Installed (0)"));
                    }
                    updatePackageCount(result.get().successfulPackages.size());
                    updatePackageList(result.get().successfulPackages);
                }

                taskManager->removeTask(task);
                delete task;
            });
    taskManager->addAndStartTask(task);
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
    auto layout = new QVBoxLayout;
    layout->addWidget(detailsHeader);
    layout->addStretch();
    layout->setContentsMargins({12, 0, 12, 0});
    layout->setSpacing(12);

    auto panel = new QWidget;
    panel->setLayout(layout);
    panel->setContentsMargins({});
    panel->setStyleSheet("PackageDetailsHeader { border-bottom: 1px solid #1D1F26; } "
        "PackageDetailsHeader>QLabel#lbPackageId { font-size: 24px; color: rgb(182, 183, 186); } "
        "PackageDetailsHeader>QLabel#lbVendor { font-size: 13px; color: rgba(182, 183, 186, 140); } "
        "PackageDetailsHeader>QLabel#lbVersion { font-size: 13px; color: rgba(182, 183, 186, 140); } "
        "PackageDetailsHeader>QLabel#lbCopyright { font-size: 13px; color: rgba(182, 183, 186, 140); } ");
    return panel;
}