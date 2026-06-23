#ifndef TWOLEVELCOMBOBOX_H
#define TWOLEVELCOMBOBOX_H

#include "Modules/PackageManager/Models/PackageInfo.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

#include "UI/Controls/Menu.h"
#include <QToolButton>

class QAction;

struct ComboBoxItemData {
    QString text;
    SingerInfo singer;
    SpeakerInfo speaker;
    QAction *action = nullptr;
    bool isInheritItem = false;
};
Q_DECLARE_METATYPE(ComboBoxItemData)

class TwoLevelComboBox : public QToolButton {
    Q_OBJECT

public:
    explicit TwoLevelComboBox(QWidget *parent = nullptr);
    ~TwoLevelComboBox() override;

    QString currentText() const;
    SingerInfo currentSinger() const;
    SpeakerInfo currentSpeaker() const;

    void setCurrentData(const SingerInfo &singer, const SpeakerInfo &speaker,
                        bool preferInherit = false);
    void setDisplayTextOverride(const QString &text);
    void clearDisplayTextOverride();
    void setCheckedInjectedAction(QAction *action);
    bool isInheritSelected() const;
    void setShowInheritItem(bool show);

    void addItem(const QString &itemText, const SingerInfo &singer, const SpeakerInfo &spk);

    void addGroup(const QString &groupName) const;
    void addItemToGroup(const QString &groupName, const QString &itemText, const SingerInfo &singer,
                        const SpeakerInfo &spk);

    Menu *mainMenu() const;
    Menu *groupMenuForSinger(const SingerInfo &singer) const;
    void clearInjectedActions() const;
    QAction *addInjectedActionToSinger(const SingerInfo &singer, const QString &text) const;
    QAction *addInjectedSeparatorToSinger(const SingerInfo &singer) const;

    void clear();

    void setLoadingText(const QString &text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setItems(const QList<PackageInfo> &packages);

signals:
    void currentDataChanged();
    void currentTextChanged(const QString &text);
    void itemsPopulated();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onActionTriggered(const QAction *action);

private:
    void updateDisplayText();
    void updateActionCheckStates() const;
    Menu *createGroupMenu(const QString &groupName) const;
    Menu *createGroupMenu(const QString &groupName, const SingerIdentifier &identifier) const;
    void addItemInternal(const QString &itemText, const SingerInfo &singer, const SpeakerInfo &spk,
                         QMenu *parentMenu);

    Menu *m_mainMenu = nullptr;
    QList<ComboBoxItemData> m_itemDataList;
    ComboBoxItemData m_currentItem;
    QString m_propertyName = "itemData";
    QString m_singerIdentifierPropertyName = "singerIdentifier";
    QString m_injectedPropertyName = "speakerMixInjected";
    bool m_showInheritItem = false;
    bool m_inheritWasSelected = false;
    QAction *m_checkedInjectedAction = nullptr;
    bool m_suppressCurrentActionCheck = false;
    SingerInfo m_prevSinger;
    SpeakerInfo m_prevSpeaker;
    QString m_loadingText;
    QString m_displayTextOverride;
};

#endif // TWOLEVELCOMBOBOX_H
