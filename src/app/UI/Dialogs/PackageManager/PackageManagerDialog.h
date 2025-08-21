//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGEMANAGERDIALOG_H
#define PACKAGEMANAGERDIALOG_H

#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "Modules/PackageManager/Models/PackageInfo.h"

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

    Button *m_btnInstall = nullptr;
    QLabel *m_lbPackageCount = nullptr;
    LineEdit *m_leSearch = nullptr;
    QListView *m_listView = nullptr;;

    QList<PackageInfo> successfulPackages;

    State m_state = Loading;
};


#endif //PACKAGEMANAGERDIALOG_H