//
// Created by FlutyDeer on 2026/7/9.
//

#ifndef TOOLBUTTON_H
#define TOOLBUTTON_H

#include "UI/Utils/IconUtils.h"

#include <QPushButton>
#include <QSize>

class ToolButton : public QPushButton {
    Q_OBJECT
public:
    explicit ToolButton(QWidget *parent = nullptr);

    void setActionIcon(const QString &svgPath, const QSize &iconSize = QSize(16, 16));
    void setToggleIcon(const QString &svgPath, const QSize &iconSize, const QColor &checkedColor);
};

#endif // TOOLBUTTON_H