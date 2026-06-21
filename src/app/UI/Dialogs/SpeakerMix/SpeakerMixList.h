#ifndef DS_EDITOR_LITE_SPEAKERMIXLIST_H
#define DS_EDITOR_LITE_SPEAKERMIXLIST_H

#include <QListWidget>
#include <QMap>
#include <QVector>
#include <QString>

#include "Modules/PackageManager/Models/SpeakerInfo.h"

class QHBoxLayout;
class ComboBox;
class QLabel;
class SpeakerMixBar;

class SpeakerMixList : public QListWidget {
    Q_OBJECT

public:
    explicit SpeakerMixList(const QString &packageName, const QStringList &speakerTypes,
                            const QList<SpeakerInfo> &referenceSpeakers,
                            QWidget *parent = nullptr);
    void setSpeakerTypes(const QStringList &speakerTypes);
    void setSpeakerDisplayNames(const QMap<QString, QString> &displayNames);
    void setSourceEditingEnabled(bool enabled);
    void setDoubleValues(const QVector<double> &values);
    QVector<int> getValues() const;
    QVector<QString> getLabels() const;

    void addSpeaker(const QString &speakerName);
    void removeSpeaker(const QString &speakerName);

    SpeakerMixBar *getMixBar() const {
        return m_mixBar;
    }

signals:
    void speakerChanged(const QString &oldName, const QString &newName);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onItemOrderChanged();
    void onSpeakerTypeChanged(int index);
    void syncRowsFromBar(const QVector<double> &values);

private:
    struct RowComponents {
        QWidget *container;
        QHBoxLayout *layout;
        QLabel *dragHandle;
        QWidget *colorDot;
        ComboBox *speakerComboBox;
        QLabel *positionLabel;
        QColor color;
        QString speakerName;
    };

    void createRow(const QString &speakerType = "default");
    QWidget *createRowWidget(const QString &speakerType);
    void setRowsValues(const QVector<double> &values);
    void redistributeValues();
    void syncRowsWithListItems();
    int findRowIndexBySender(const QObject *object) const;
    int findRowIndexBySpeaker(const QString &speakerName) const;
    void refreshComboBoxItems();
    void updateRowColor(RowComponents &row);
    void updateBarLabelsAndColors();
    QString speakerDisplayName(const QString &speakerName) const;
    QVector<QColor> getColors() const;

private:
    QString m_packageName;
    QStringList m_speakerTypes;
    QList<SpeakerInfo> m_referenceSpeakers;
    QMap<QString, QString> m_speakerDisplayNames;

    QVector<RowComponents> m_rows;
    SpeakerMixBar *m_mixBar;
    bool m_sourceEditingEnabled;
};

#endif
