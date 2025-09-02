//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGEMANAGERDIALOG_H
#define PACKAGEMANAGERDIALOG_H

#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "Modules/PackageManager/Models/PackageInfo.h"

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
    void onSelectionChanged(const QModelIndex &current, const QModelIndex &previous);

private:
    enum State {
        Loading,
        Success,
        Failed
    };
    void initUi();
    void loadPackageList();
    QWidget *buildPackagePanel();
    QWidget *buildDetailsPanel();

    Button *btnInstall = nullptr;
    QLabel *lbPackageCount = nullptr;
    LineEdit *leSearch = nullptr;
    QListView *listView = nullptr;;
    PackageDetailsHeader *detailsHeader = nullptr;
    PackageDetailsContent *detailsContent = nullptr;

    // QList<PackageInfo> successfulPackages;

    PackageListModel *listModel = nullptr;
    PackageFilterProxyModel *proxyModel = nullptr;

    State m_state = Loading;
};


#endif //PACKAGEMANAGERDIALOG_H