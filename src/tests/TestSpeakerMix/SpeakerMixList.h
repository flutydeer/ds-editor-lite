#ifndef DS_EDITOR_LITE_SPEAKERMIXLIST_H
#define DS_EDITOR_LITE_SPEAKERMIXLIST_H

#include <QListWidget>
#include <QVector>
#include <QString>

class QHBoxLayout;
class QComboBox;
class QPushButton;
class SpeakerMixBar;

class SpeakerMixList : public QListWidget {
    Q_OBJECT

public:
    explicit SpeakerMixList(const QString &packageName, const QStringList &speakerTypes,
                            QWidget *parent = nullptr);
    void setSpeakerTypes(const QStringList &speakerTypes);
    QVector<int> getValues() const;
    QVector<QString> getLabels() const;

    SpeakerMixBar *getMixBar() const {
        return m_mixBar;
    }

public slots:
    void addRow();

private slots:
    void removeRow();
    void onItemOrderChanged();
    void onSpeakerTypeChanged();

private:
    struct RowComponents {
        QWidget *container;
        QHBoxLayout *layout;
        QComboBox *speakerComboBox;
        QPushButton *deleteButton;
    };

    void createRow(const QString &speakerType = "default");
    QWidget *createRowWidget(const QString &speakerType);
    void updateBarValues(const QVector<int> &previousValues = QVector<int>(),
                         bool isAddOperation = false, int removedIndex = -1);
    void syncRowsWithListItems();
    int findRowIndexByWidget(const QWidget *widget) const;
    void updateBarLabels();

    QString m_packageName;
    QStringList m_speakerTypes;

    QVector<RowComponents> m_rows;
    SpeakerMixBar *m_mixBar;
};

#endif