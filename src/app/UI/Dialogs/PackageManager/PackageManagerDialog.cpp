//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageManagerDialog.h"

#include "Modules/PackageManager/Tasks/GetInstalledPackagesTask.h"
#include "Modules/Task/TaskManager.h"
#include "synthrt/Core/PackageRef.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Dialogs/PackageManager/PackageFilterProxyModel.h"
#include "UI/Dialogs/PackageManager/PackageItemDelegate.h"
#include "UI/Dialogs/PackageManager/PackageListModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLocale>

PackageManagerDialog::PackageManagerDialog(QWidget *parent) : Dialog(parent) {
    initUi();
}

void PackageManagerDialog::onModuleStatusChanged(AppStatus::ModuleType module,
                                                 AppStatus::ModuleStatus status) {
    if (module != AppStatus::ModuleType::Inference)
        return;
    if (status == AppStatus::ModuleStatus::Ready) {
        m_btnInstall->setEnabled(true);
        loadPackageList();
    }
}

void PackageManagerDialog::updatePackageCount(int count) {
    m_lbPackageCount->setText(tr("Installed (%1)").arg(count));
}

void PackageManagerDialog::updatePackageList(QList<PackageInfo> packages) {
    // 初始化模型和代理
    auto *model = new PackageListModel(this);
    model->setPackages(std::move(packages));

    auto *proxyModel = new PackageFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    m_listView->setModel(proxyModel);

    connect(m_leSearch, &QLineEdit::textChanged,
            proxyModel, &PackageFilterProxyModel::setFilterString);
}

void PackageManagerDialog::initUi() {
    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(buildPackagePanel());
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    body()->setLayout(mainLayout);

    if (appStatus->inferEngineEnvStatus != AppStatus::ModuleStatus::Ready) {
        m_btnInstall->setEnabled(false);
        connect(appStatus, &AppStatus::moduleStatusChanged, this,
                &PackageManagerDialog::onModuleStatusChanged);
    }
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
                        m_listView->setModel(nullptr);
                        m_lbPackageCount->setText(tr("Installed (0)"));
                        return;
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
    m_btnInstall = new Button(tr("&Install..."));
    m_lbPackageCount = new QLabel();

    m_leSearch = new LineEdit;
    m_leSearch->setPlaceholderText(tr("Search..."));
    m_leSearch->setClearButtonEnabled(true);

    auto actionBar = new QHBoxLayout;
    actionBar->addWidget(m_btnInstall);
    actionBar->addStretch();
    actionBar->addWidget(m_lbPackageCount);

    m_listView = new QListView;
    m_listView->setObjectName("PackageManagerDialogPackageListView");
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setItemDelegate(new PackageItemDelegate(m_listView));
    m_listView->setContentsMargins({});
    m_listView->setStyleSheet(
        "QListView { background: transparent; border: none; padding: 0px; } "
        "QListView::item { background: transparent; border-radius: 4px; margin-top: 2px; margin-bottom: 2px } "
        "QListView::item:hover { background: #1BC7D8FF; }"
        "QListView::item:selected { background: #409BBAFF; }"
        );

    auto layout = new QVBoxLayout;
    layout->addLayout(actionBar);
    layout->addWidget(m_leSearch);
    layout->addWidget(m_listView);
    layout->setContentsMargins({});

    auto *panel = new QWidget;
    panel->setLayout(layout);
    panel->setContentsMargins({});
    return panel;
}