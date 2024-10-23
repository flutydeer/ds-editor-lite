//
// Created by fluty on 24-9-16.
//

#ifndef PARAMEDITORTOOLBARVIEW_H
#define PARAMEDITORTOOLBARVIEW_H

#include "Model/AppModel/Params.h"

#include <QWidget>

class ComboBox;
class QLabel;
class SingingClip;

class ParamEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorToolBarView(QWidget *parent = nullptr);

signals:
    void foregroundChanged(ParamInfo::Name name);
    void backgroundChanged(ParamInfo::Name name);

private slots:
    void onForegroundSelectionChanged(int index);
    void onBackgroundSelectionChanged(int index);
    void onSwap() const;

private:
    SingingClip *m_clip = nullptr;
    QLabel *lbForegroundParam;
    ComboBox *cbForegroundParam;
    QLabel *lbBackgroundParam;
    ComboBox *cbBackgroundParam;
};



#endif // PARAMEDITORTOOLBARVIEW_H
