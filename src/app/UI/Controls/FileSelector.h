#ifndef FILESELECTOR_H
#define FILESELECTOR_H

#include <QWidget>
#include <QSet>
#include <QString>

class QDragEnterEvent;
class QDropEvent;
class QPushButton;
class LineEdit;

class FileSelector final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath)
    Q_PROPERTY(QString filter READ filter WRITE setFilter)
    Q_PROPERTY(QSet<QString> fileDropExtensions READ fileDropExtensions WRITE setFileDropExtensions)
public:
    explicit FileSelector(QWidget *parent = nullptr);
    QString filePath() const;
    void setFilePath(const QString &filePath);
    QString filter() const;
    void setFilter(const QString &filter);
    QSet<QString> fileDropExtensions() const;
    void setFileDropExtensions(const QSet<QString> &fileDropExtensions);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void filePathChanged(const QString &newFilePath);

private Q_SLOTS:
    void onBrowseButtonClicked();
    void onTextChanged();

private:
    LineEdit *m_lineEdit;
    QPushButton *m_browseButton;
    QString m_filter;
    QSet<QString> m_fileDropExtensions;
};

inline QString FileSelector::filter() const {
    return m_filter;
}

inline void FileSelector::setFilter(const QString &filter) {
    m_filter = filter;
}

inline QSet<QString> FileSelector::fileDropExtensions() const {
    return m_fileDropExtensions;
}

inline void FileSelector::setFileDropExtensions(const QSet<QString> &fileDropExtensions) {
    m_fileDropExtensions.clear();
    m_fileDropExtensions.reserve(fileDropExtensions.size());
    for (const QString &str : std::as_const(fileDropExtensions)) {
        m_fileDropExtensions.insert(str.toLower());
    }
}

#endif // FILESELECTOR_H
