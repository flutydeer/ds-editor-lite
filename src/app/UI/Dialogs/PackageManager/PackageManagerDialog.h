//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGEMANAGERDIALOG_H
#define PACKAGEMANAGERDIALOG_H

#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/Dialog.h"

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
    void updatePackageList(const QList<srt::PackageRef> &packages);

private:
    enum State {
        Loading,
        Success,
        Failed
    };
    void initUi();
    void loadPackageList();
    QWidget *buildPackagePanel();

    Button *m_btnInstall;
    QLabel *m_lbPackageCount;
    QListView *m_listView;

    QList<srt::PackageRef> successfulPackages;

    std::string locale;
    State m_state = Loading;
};


#endif //PACKAGEMANAGERDIALOG_H