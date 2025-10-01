//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGEMANAGERDIALOG_H
#define PACKAGEMANAGERDIALOG_H

#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "Modules/PackageManager/Models/PackageInfo.h"

class QScrollArea;
class QStackedWidget;
class PackageDetailsContent;
class PackageFilterProxyModel;
class PackageListModel;
class PackageDetailsHeader;
class LineEdit;
class QListView;

namespace srt {
    class PackageRef;
}

class PackageManagerDialog : public Dialog {
    Q_OBJECT

public:
    explicit PackageManagerDialog(QWidget *parent = nullptr);
    ~PackageManagerDialog() override = default;

private slots:
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void updatePackageCount(int count);
    void updatePackageList(QList<PackageInfo> packages);
    void onSelectionChanged(const QModelIndex &current, const QModelIndex &previous) const;

private:
    // enum State {
    //     Loading,
    //     Success,
    //     Failed
    // };

    enum DetailsPanelState {
        PackageUnselected,
        PackageSelected
    };

    void initUi();
    void onInferenceModuleReady();
    void loadPackageList();
    QWidget *buildPackagePanel();
    QWidget *buildDetailsPanel();
    QWidget *buildDetailsPanelPlaceholder();

    Button *btnInstall = nullptr;
    QLabel *lbPackageCount = nullptr;
    LineEdit *leSearch = nullptr;
    QListView *listView = nullptr;

    QStackedWidget *detailsPanel = nullptr;
    QWidget *detailsPanelPlaceholder = nullptr;
    QScrollArea *detailsPanelContent = nullptr;

    PackageDetailsHeader *detailsHeader = nullptr;
    PackageDetailsContent *detailsContent = nullptr;

    // QList<PackageInfo> successfulPackages;

    PackageListModel *listModel = nullptr;
    PackageFilterProxyModel *proxyModel = nullptr;

    // State m_state = Loading;
};


#endif // PACKAGEMANAGERDIALOG_H