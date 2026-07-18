//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageManagerDialog.h"

#include "Modules/PackageManager/PackageManager.h"
#include "Modules/PackageManager/Tasks/GetInstalledPackagesTask.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Dialogs/PackageManager/PackageDetailsContent.h"
#include "UI/Dialogs/PackageManager/PackageDetailsHeader.h"
#include "UI/Dialogs/PackageManager/PackageFilterProxyModel.h"
#include "UI/Dialogs/PackageManager/PackageItemDelegate.h"
#include "UI/Dialogs/PackageManager/PackageListModel.h"
#include "Utils/StringUtils.h"

#include <diffsinger/Bank/PackageValidator.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
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

void PackageManagerDialog::onVerifyPackageRequested(const PackageInfo &package) {
    ds::bank::PackageValidator validator;
    const auto report = validator.validatePackage(StringUtils::qstr_to_path(package.path()),
                                                  ds::bank::PackageValidator::SchemaVersion::V10);

    if (report.items().empty()) {
        QMessageBox::information(this, tr("Verify Package"),
                                 tr("No issues found in package:\n%1").arg(package.path()));
        return;
    }

    QStringList lines;
    for (const auto &item : report.items()) {
        QString severity;
        switch (item.severity) {
            case ds::bank::ValidationItem::Error:
                severity = tr("Error");
                break;
            case ds::bank::ValidationItem::Warning:
                severity = tr("Warning");
                break;
            case ds::bank::ValidationItem::Info:
            default:
                severity = tr("Info");
                break;
        }
        QString line = QStringLiteral("[%1] ").arg(severity);
        if (!item.path.empty()) {
            line += QString::fromStdString(item.path) + QStringLiteral(": ");
        }
        line += QString::fromStdString(item.message);
        if (!item.actualValue.empty()) {
            line += tr("\n  Actual: %1").arg(QString::fromStdString(item.actualValue));
        }
        if (!item.recommendation.empty()) {
            line += tr("\n  Recommendation: %1")
                        .arg(QString::fromStdString(item.recommendation));
        }
        lines.append(line);
    }

    QMessageBox messageBox(report.hasErrors() ? QMessageBox::Critical : QMessageBox::Warning,
                           tr("Verify Package"),
                           report.hasErrors() ? tr("Package verification failed.")
                                              : tr("Package verification completed with warnings."),
                           QMessageBox::Ok, this);
    messageBox.setDetailedText(lines.join(QStringLiteral("\n\n")));
    messageBox.exec();
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
    }  else
        onInferenceModuleReady();

    resize(1280, 768);
    setMinimumWidth(960);
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

    auto layout = new QVBoxLayout;
    layout->addLayout(actionBar);
    layout->addWidget(leSearch);
    layout->addWidget(listView);
    layout->setContentsMargins({});

    auto panel = new QWidget;
    panel->setObjectName("PackageManagerDialogPackagePanel");
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setLayout(layout);
    panel->setContentsMargins({12, 12, 12, 0});
    panel->setFixedWidth(280);
    return panel;
}

QWidget *PackageManagerDialog::buildDetailsPanel() {
    detailsHeader = new PackageDetailsHeader;
    detailsContent = new PackageDetailsContent;
    connect(detailsHeader, &PackageDetailsHeader::verifyRequested, this,
            &PackageManagerDialog::onVerifyPackageRequested);

    auto contentLayout = new QVBoxLayout;
    contentLayout->addWidget(detailsContent);
    contentLayout->addStretch();
    contentLayout->setContentsMargins({});
    contentLayout->setSpacing(0);

    auto contentWidget = new QWidget;
    contentWidget->setObjectName("PackageManagerDialogDetailsContentWidget");
    contentWidget->setLayout(contentLayout);
    contentWidget->setContentsMargins({});

    detailsPanelContent = new QScrollArea;
    detailsPanelContent->setObjectName("PackageManagerDialogDetailsScrollArea");
    detailsPanelContent->setWidgetResizable(true);
    detailsPanelContent->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailsPanelContent->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detailsPanelContent->setWidget(contentWidget);
    detailsPanelContent->viewport()->setContentsMargins({});

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(detailsHeader);
    mainLayout->addWidget(detailsPanelContent);
    mainLayout->setContentsMargins({12, 0, 12, 0});
    mainLayout->setSpacing(12);

    auto detailsWidget = new QWidget;
    detailsWidget->setObjectName("PackageManagerDialogDetailsWidget");
    detailsWidget->setLayout(mainLayout);
    detailsWidget->setContentsMargins({});
    detailsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    detailsPanelPlaceholder = buildDetailsPanelPlaceholder();

    detailsPanel = new QStackedWidget;
    detailsPanel->addWidget(detailsPanelPlaceholder);
    detailsPanel->addWidget(detailsWidget);
    detailsPanel->setCurrentWidget(detailsPanelPlaceholder);

    return detailsPanel;
}

QWidget *PackageManagerDialog::buildDetailsPanelPlaceholder() {
    auto label = new QLabel(tr("Select a package to view details"));
    label->setObjectName("lbPackageDetailsPlaceholder");
    label->setAlignment(Qt::AlignCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->setContentsMargins({12, 0, 12, 0});

    detailsPanelPlaceholder = new QWidget;
    detailsPanelPlaceholder->setLayout(layout);
    detailsPanelPlaceholder->setContentsMargins({});
    return detailsPanelPlaceholder;
}
