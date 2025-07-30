//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGELISTITEMWIDGET_H
#define PACKAGELISTITEMWIDGET_H

#include <QWidget>

class QLabel;

class PackageListItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit PackageListItemWidget(QWidget *parent = nullptr);

    void setContent(const QString &id, const QString &vendor);

    QSize sizeHint() const override;

private:
    QLabel *m_idLabel;
    QLabel *m_vendorLabel;
};

#endif //PACKAGELISTITEMWIDGET_H
