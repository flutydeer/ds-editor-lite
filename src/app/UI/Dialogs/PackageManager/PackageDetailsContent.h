//
// Created by FlutyDeer on 2025/9/3.
//

#ifndef DS_EDITOR_LITE_PACKAGEDETAILSCONTENT_H
#define DS_EDITOR_LITE_PACKAGEDETAILSCONTENT_H

#include <QWidget>

class DescriptionCard;
class PackageInfo;

class PackageDetailsContent final : public QWidget {
    Q_OBJECT

public:
    explicit PackageDetailsContent(QWidget *parent = nullptr);

public slots:
    void onPackageChanged(const PackageInfo *package);

private:
    void moveToNullPackageState() const;
    void moveToPackageState(const PackageInfo &package) const;

    DescriptionCard *descriptionCard = nullptr;

    const PackageInfo *currentPackage = nullptr;
};


#endif //DS_EDITOR_LITE_PACKAGEDETAILSCONTENT_H