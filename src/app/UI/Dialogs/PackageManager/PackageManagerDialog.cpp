#include "PackageManagerDialog.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppStatus/AppStatus.h"

#include <lite/PackageManager/PackageManager.h>
#include <lite/PackageManager/Tasks/GetInstalledPackagesTask.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ItemViewTouchFilter.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>
#include <lite/GUI/Controls/OverlaySplitter.h>
#include <lite/GUI/Controls/SmoothScroller.h>
#include "UI/Dialogs/PackageManager/PackageDetailsContent.h"
#include "UI/Dialogs/PackageManager/PackageDetailsHeader.h"
#include "UI/Dialogs/PackageManager/PackageFilterProxyModel.h"
#include "UI/Dialogs/PackageManager/PackageItemDelegate.h"
#include "UI/Dialogs/PackageManager/PackageListModel.h"
#include <lite/GUI/Theme/ThemeManager.h>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QScrollArea>
#include <QStackedWidget>

namespace {
    PackageInfo fromAutomationDto(const Automation::PackageDto &package) {
        QList<SingerInfo> singers;
        for (const auto &singer : package.singers) {
            singers.append(singer.info);
        }
        PackageInfo result(package.id, package.version, package.vendor, package.description,
                           package.license, package.readme, package.url, package.path, singers);
        result.setLocalizedVendor(package.localizedVendor);
        result.setLocalizedDescription(package.localizedDescription);
        result.setLocalizedLicense(package.localizedLicense);
        return result;
    }
}

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
    lbPackageCount->setText(tr("Installed (%L1)").arg(count));
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
    // Suppress repaints while the cards change content: setText posts its
    // repaint request ahead of the layout-invalidation chain, so the label
    // would otherwise be painted with its previous geometry (clipped when the
    // new text is taller, vertically centered with gaps when it is shorter)
    // for a frame before the chain unwinds.
    detailsPanel->setUpdatesEnabled(false);
    detailsHeader->onPackageChanged(&listModel->getPackage(sourceIndex));
    detailsContent->onPackageChanged(&listModel->getPackage(sourceIndex));
    // The card updates above only post their layout-invalidation chain: each
    // activation posts the next level's LayoutRequest, and sendPostedEvents
    // does not deliver events posted during its own run, so one pass only
    // unwinds a single layout level. Drain the whole chain: the stack switch
    // below lays the details panel out synchronously and must read the new
    // package's size hints, not the previous package's.
    for (int i = 0; i < 8; ++i)
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    if (auto package = &listModel->getPackage(sourceIndex))
        detailsPanel->setCurrentIndex(PackageSelected);
    else
        detailsPanel->setCurrentIndex(PackageUnselected);
    detailsPanel->setUpdatesEnabled(true);
}

void PackageManagerDialog::onVerifyPackageRequested(const PackageInfo &package) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    const auto validation = runtime->packages().validatePackage(package.path());
    if (!validation) {
        QMessageBox::critical(this, tr("Verify Package"), validation.getError().message);
        return;
    }
    const auto &report = validation.get();

    if (report.items.isEmpty()) {
        QMessageBox::information(this, tr("Verify Package"),
                                 tr("No issues found in package:\n%1").arg(package.path()));
        return;
    }

    QStringList lines;
    for (const auto &item : report.items) {
        QString severity;
        switch (item.severity) {
            case Automation::PackageValidationSeverity::Error:
                severity = tr("Error");
                break;
            case Automation::PackageValidationSeverity::Warning:
                severity = tr("Warning");
                break;
            case Automation::PackageValidationSeverity::Info:
            default:
                severity = tr("Info");
                break;
        }
        QString line = QStringLiteral("[%1] ").arg(severity);
        if (!item.path.isEmpty()) {
            line += item.path + QStringLiteral(": ");
        }
        line += item.message;
        if (!item.actualValue.isEmpty()) {
            line += tr("\n  Actual: %1").arg(item.actualValue);
        }
        if (!item.recommendation.isEmpty()) {
            line += tr("\n  Recommendation: %1").arg(item.recommendation);
        }
        lines.append(line);
    }

    QMessageBox messageBox(report.hasErrors ? QMessageBox::Critical : QMessageBox::Warning,
                           tr("Verify Package"),
                           report.hasErrors ? tr("Package verification failed.")
                                            : tr("Package verification completed with warnings."),
                           QMessageBox::Ok, this);
    messageBox.setDetailedText(lines.join(QStringLiteral("\n\n")));
    messageBox.exec();
}

void PackageManagerDialog::initUi() {
    auto mainLayout = new QHBoxLayout;
    auto splitter = new OverlaySplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(buildPackagePanel());
    splitter->addWidget(buildDetailsPanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 1});
    mainLayout->addWidget(splitter);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    body()->setContentsMargins({});
    body()->setLayout(mainLayout);

    listModel = new PackageListModel(this);
    proxyModel = new PackageFilterProxyModel(this);
    proxyModel->setSourceModel(listModel);
    listView->setModel(proxyModel);

    connect(listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &PackageManagerDialog::onSelectionChanged);

    connect(leSearch, &QLineEdit::textChanged, proxyModel,
            &PackageFilterProxyModel::setFilterString);

    // Repaint existing items so the delegate picks up the new theme colors
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, listView,
            [this] { listView->viewport()->update(); });

    if (appStatus->inferEngineEnvStatus != AppStatus::ModuleStatus::Ready) {
        btnInstall->setEnabled(false);
        connect(appStatus, &AppStatus::moduleStatusChanged, this,
                &PackageManagerDialog::onModuleStatusChanged);
    } else
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
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    const auto packageResult = runtime->packages().getInstalledPackages();
    if (!packageResult)
        return;
    QList<PackageInfo> packages;
    for (const auto &package : packageResult.get())
        packages.append(fromAutomationDto(package));
    if (packages.isEmpty()) {
        listView->setModel(nullptr);
        lbPackageCount->setText(tr("Installed (%L1)").arg(0));
    }
    updatePackageCount(packages.size());
    updatePackageList(std::move(packages));
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
    // Package names are elided by the delegate instead of scrolling horizontally
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setItemDelegate(new PackageItemDelegate(listView));
    listView->setContentsMargins({});
    OverlayScrollBar *listBar = nullptr;
    {
        // Overlay scrollbar: the native bar is disabled (no space reserved)
        listBar = OverlayScrollBar::install(listView, Qt::Vertical);
        // Animate mouse-wheel scrollbar movement with OutCubic; touchpad passes through
        auto *smoothScroller = new SmoothScroller(this);
        smoothScroller->attachTo(listView);
        // Touch scrolling must not select the item under the finger; taps still select
        ItemViewTouchFilter::install(listView);
    }

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
    panel->setMinimumWidth(280);
    // Pin the bar to the panel's right wall so it rests in the gutter between the
    // item edge and the divider instead of on top of the items
    if (listBar)
        listBar->setGeometryHost(panel);
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
    // The scroll viewport spans the details column edge to edge: the column's
    // 12px side margins live here so the whole area around the cards (gutters
    // included) is part of the scrollable page, and the overlay scrollbar pins
    // to the window's right edge. The margins are scrollable content - a gap
    // on the root layout would stay visible under the header while scrolled.
    contentLayout->setContentsMargins({12, 12, 12, 12});
    contentLayout->setSpacing(0);

    auto contentWidget = new QWidget;
    contentWidget->setObjectName("PackageManagerDialogDetailsContentWidget");
    contentWidget->setLayout(contentLayout);
    contentWidget->setContentsMargins({});
    // A wordWrap QLabel's sizeHint wraps its text at a heuristic ("golden ratio")
    // width, so the page's minimumSizeHint height ends up taller than its real
    // heightForWidth at the viewport width. QScrollArea (widgetResizable) starts
    // sizing the page from that minimum (updateScrollBars: p.expandedTo(min)),
    // which would leave a permanent gap below the last card. Pin the explicit
    // minimum down so the page height comes from heightForWidth alone.
    contentWidget->setMinimumHeight(1);

    detailsPanelContent = new QScrollArea;
    detailsPanelContent->setObjectName("PackageManagerDialogDetailsScrollArea");
    detailsPanelContent->setWidgetResizable(true);
    detailsPanelContent->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailsPanelContent->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detailsPanelContent->setWidget(contentWidget);
    detailsPanelContent->viewport()->setContentsMargins({});
    {
        // Overlay scrollbar: the native bar is disabled (no space reserved)
        OverlayScrollBar::install(detailsPanelContent, Qt::Vertical);
        // Animate mouse-wheel scrollbar movement with OutCubic; touchpad passes through
        auto *smoothScroller = new SmoothScroller(this);
        smoothScroller->attachTo(detailsPanelContent);
    }

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(detailsHeader);
    mainLayout->addWidget(detailsPanelContent);
    // No side margins and no spacing here: the scroll area must span the
    // column edge to edge and sit flush under the header (see contentLayout
    // above); the header carries its own side margins.
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);

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
