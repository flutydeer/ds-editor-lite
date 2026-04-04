//
// Created by FlutyDeer on 2026/4/1.
//

#ifndef DS_EDITOR_LITE_PHONEMENAMELISTWIDGET_H
#define DS_EDITOR_LITE_PHONEMENAMELISTWIDGET_H

#include <QListWidget>

class CMenu;
class PhonemeNameListModel;

class PhonemeNameListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit PhonemeNameListWidget(QWidget *parent = nullptr);

    void setModel(PhonemeNameListModel *model);
    PhonemeNameListModel *model() const;

private slots:
    void onCustomContextMenuRequested(const QPoint &pos);
    void insertAbove();
    void insertBelow();
    void deleteItem();

private:
    void refreshItems();
    void updateItemWidget(int row);
    PhonemeNameListModel *m_model = nullptr;
    CMenu *m_contextMenu = nullptr;
    int m_contextMenuRow = -1;
};



#endif //DS_EDITOR_LITE_PHONEMENAMELISTWIDGET_H
