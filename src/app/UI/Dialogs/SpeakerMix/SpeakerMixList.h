#ifndef DS_EDITOR_LITE_SPEAKERMIXLIST_H
#define DS_EDITOR_LITE_SPEAKERMIXLIST_H

#include <QListWidget>
#include <QVector>
#include <QString>

class QHBoxLayout;
class ComboBox;
class QLabel;
class SpeakerMixBar;

class SpeakerMixList : public QListWidget {
    Q_OBJECT

public:
    explicit SpeakerMixList(const QString &packageName, const QStringList &speakerTypes,
                            QWidget *parent = nullptr);
    void setSpeakerTypes(const QStringList &speakerTypes);
    void setSourceEditingEnabled(bool enabled);
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
    void updateRowColor(RowComponents &row);
    void updateBarLabelsAndColors();
    QVector<QColor> getColors() const;
public:
    static QVector<QColor> defaultColors();

private:

    QString m_packageName;
    QStringList m_speakerTypes;

    QVector<RowComponents> m_rows;
    SpeakerMixBar *m_mixBar;
    bool m_sourceEditingEnabled;
};

#endif
