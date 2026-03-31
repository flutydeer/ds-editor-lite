//
// Created by FlutyDeer on 2026/4/1.
//

#ifndef DS_EDITOR_LITE_PHONEMENAMELISTWIDGET_H
#define DS_EDITOR_LITE_PHONEMENAMELISTWIDGET_H

#include <QListWidget>

class PhonemeNameListModel;

class PhonemeNameListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit PhonemeNameListWidget(QWidget *parent = nullptr);

    void setModel(PhonemeNameListModel *model);
    PhonemeNameListModel *model() const;

private:
    void refreshItems();
    void updateItemWidget(int row);
    PhonemeNameListModel *m_model = nullptr;
};



#endif //DS_EDITOR_LITE_PHONEMENAMELISTWIDGET_H
