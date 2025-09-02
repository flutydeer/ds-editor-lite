//
// Created by FlutyDeer on 2025/8/31.
//

#ifndef DS_EDITOR_LITE_PACKAGEDETAILSHEADER_H
#define DS_EDITOR_LITE_PACKAGEDETAILSHEADER_H

#include <QWidget>

class PackageInfo;
class Button;
class QLabel;

class PackageDetailsHeader : public QWidget {
    Q_OBJECT

public:
    explicit PackageDetailsHeader(QWidget *parent = nullptr);

public slots:
    void onPackageChanged(const PackageInfo *package);

private slots:
    void onOpenWebsiteClicked() const;

private:
    void moveToNullPackageState() const;
    void moveToPackageState(const PackageInfo &package) const;

    QLabel *lbPackageId = nullptr;
    QLabel *lbVendor = nullptr;
    QLabel *lbVersion = nullptr;
    QLabel *lbCopyright = nullptr;

    Button *btnOpenWebsite = nullptr;
    Button *btnVerify = nullptr;
    Button *btnUninstall = nullptr;

    const PackageInfo *currentPackage = nullptr;
};


#endif //DS_EDITOR_LITE_PACKAGEDETAILSHEADER_H