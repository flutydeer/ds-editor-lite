#ifndef TWOLEVELCOMBOBOX_H
#define TWOLEVELCOMBOBOX_H

#include "Modules/PackageManager/Models/SingerInfo.h"

#include <QMWidgets/cmenu.h>
#include <QComboBox>

class QAction;

struct ComboBoxItemData {
    QString text;
    SingerInfo singer;
    SpeakerInfo speaker;
    QAction *action = nullptr;
};
Q_DECLARE_METATYPE(ComboBoxItemData)

class TwoLevelComboBox : public QComboBox {
    Q_OBJECT

public:
    explicit TwoLevelComboBox(QWidget *parent = nullptr);
    ~TwoLevelComboBox() override;

    QString currentText() const;
    SingerInfo currentSinger() const;
    SpeakerInfo currentSpeaker() const;

    void addItem(const QString &itemText, const SingerInfo &singer, const SpeakerInfo &spk);

    void addGroup(const QString &groupName) const;
    void addItemToGroup(const QString &groupName, const QString &itemText, const SingerInfo &singer,
                        const SpeakerInfo &spk);

    void clear();

signals:
    void currentDataChanged();

protected:
    void showPopup() override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onActionTriggered(const QAction *action);

private:
    void updateDisplayText();
    CMenu *createGroupMenu(const QString &groupName) const;
    void addItemInternal(const QString &itemText, const SingerInfo &singer, const SpeakerInfo &spk,
                         QMenu *parentMenu);

    CMenu *m_mainMenu = nullptr;
    QList<ComboBoxItemData> m_itemDataList;
    ComboBoxItemData m_currentItem;
    QString m_propertyName = "itemData";
};

#endif // TWOLEVELCOMBOBOX_H
