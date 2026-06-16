#ifndef DS_EDITOR_LITE_SPEAKERMIXLIST_H
#define DS_EDITOR_LITE_SPEAKERMIXLIST_H

#include <QListWidget>
#include <QVector>
#include <QString>

class QHBoxLayout;
class QComboBox;
class QPushButton;
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

    SpeakerMixBar *getMixBar() const {
        return m_mixBar;
    }

public slots:
    void addRow();

private slots:
    void removeRow();
    void onItemOrderChanged();
    void onSpeakerTypeChanged();
    void syncRowsFromBar(const QVector<int> &values);

private:
    struct RowComponents {
        QWidget *container;
        QHBoxLayout *layout;
        QLabel *dragHandle;
        QWidget *colorDot;
        QComboBox *speakerComboBox;
        QLabel *positionLabel;
        QPushButton *deleteButton;
        QColor color;
    };

    void createRow(const QString &speakerType = "default");
    QWidget *createRowWidget(const QString &speakerType);
    void setRowsValues(const QVector<int> &values);
    void syncRowsWithListItems();
    int findRowIndexBySender(const QObject *object) const;
    void updateRowColor(RowComponents &row);
    void updateBarLabelsAndColors();
    QVector<QColor> getColors() const;
    static QVector<QColor> defaultColors();

    QString m_packageName;
    QStringList m_speakerTypes;

    QVector<RowComponents> m_rows;
    SpeakerMixBar *m_mixBar;
    bool m_sourceEditingEnabled;
};

#endif
