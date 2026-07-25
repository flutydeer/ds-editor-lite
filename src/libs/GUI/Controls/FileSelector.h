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
    Q_PROPERTY(QString path READ path WRITE setPath)
    Q_PROPERTY(QString filter READ filter WRITE setFilter)
    Q_PROPERTY(bool dirMode READ isDirMode WRITE setDirMode)
    Q_PROPERTY(QSet<QString> fileDropExtensions READ fileDropExtensions WRITE setFileDropExtensions)
public:
    explicit FileSelector(QWidget *parent = nullptr);
    QString path() const;
    void setPath(const QString &filePath) const;
    QString filter() const;
    void setFilter(const QString &filter);
    bool isDirMode() const;
    void setDirMode(bool dirMode);
    QSet<QString> fileDropExtensions() const;
    void setFileDropExtensions(const QSet<QString> &fileDropExtensions);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void pathChanged(const QString &newFilePath);

private Q_SLOTS:
    void onBrowseButtonClicked();
    void onTextChanged();

private:
    LineEdit *m_lineEdit;
    QPushButton *m_browseButton;
    QString m_filter;
    bool m_dirMode = false;
    QSet<QString> m_fileDropExtensions;
};

inline QString FileSelector::filter() const {
    return m_filter;
}

inline void FileSelector::setFilter(const QString &filter) {
    m_filter = filter;
}

inline bool FileSelector::isDirMode() const {
    return m_dirMode;
}

inline void FileSelector::setDirMode(const bool dirMode) {
    m_dirMode = dirMode;
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