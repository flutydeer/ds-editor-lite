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
#include "UI/Dialogs/PackageManager/PackageListItemWidget.h"
#include "UI/Dialogs/PackageManager/PackageListModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLocale>

PackageManagerDialog::PackageManagerDialog(QWidget *parent) {
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

void PackageManagerDialog::updatePackageList(const QList<srt::PackageRef> &packages) {
    // 初始化模型和代理
    auto *model = new PackageListModel(this);
    model->setPackages(packages);

    auto *proxyModel = new PackageFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    m_listView->setModel(proxyModel);

    // 为每个条目设置自定义 Widget
    for (int i = 0; i < proxyModel->rowCount(); ++i) {
        const auto index = proxyModel->index(i, 0);
        const auto package = index.data(Qt::UserRole).value<srt::PackageRef>();

        auto *widget = new PackageListItemWidget;
        widget->setContent(
            QString::fromStdString(package.id()),
            QString::fromStdString(package.vendor().text(locale))
            );
        m_listView->setIndexWidget(index, widget);
    }

    // 连接搜索框信号
    connect(findChild<QLineEdit *>(), &QLineEdit::textChanged,
            proxyModel, &PackageFilterProxyModel::setFilterString);
}

void PackageManagerDialog::initUi() {
    locale = QLocale::system().name().toStdString();
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

    // 新增搜索框
    auto *searchEdit = new LineEdit;
    searchEdit->setPlaceholderText(tr("Search by ID..."));
    searchEdit->setClearButtonEnabled(true);

    auto actionBar = new QHBoxLayout;
    actionBar->addWidget(m_btnInstall);
    actionBar->addStretch();
    actionBar->addWidget(m_lbPackageCount);

    // 改用 QListView
    m_listView = new QListView; // 替换原来的 m_listPackages
    m_listView->setObjectName("PackageManagerDialogPackageListView");
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setStyleSheet(
        "QListView { background: transparent; } QListView::item {height: 48px;}");

    auto layout = new QVBoxLayout;
    layout->addLayout(actionBar);
    layout->addWidget(searchEdit); // 添加搜索框
    layout->addWidget(m_listView);
    layout->setContentsMargins({});

    QWidget *panel = new QWidget;
    panel->setLayout(layout);
    panel->setContentsMargins({});
    return panel;
}