#ifndef FILESELECTOR_H
#define FILESELECTOR_H

#include <QWidget>
#include <QString>

class QDragEnterEvent;
class QDropEvent;
class QPushButton;
class LineEdit;

class FileSelector final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath)
    Q_PROPERTY(QString filter READ filter WRITE setFilter)
public:
    explicit FileSelector(QWidget *parent = nullptr);
    QString filePath() const;
    void setFilePath(const QString &filePath);
    QString filter() const;
    void setFilter(const QString &filter);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    void filePathChanged(const QString &newFilePath);

private slots:
    void onBrowseButtonClicked();
    void onTextChanged();

private:
    LineEdit *m_lineEdit;
    QPushButton *m_browseButton;
    QString m_filter;
};

inline QString FileSelector::filter() const {
    return m_filter;
}

inline void FileSelector::setFilter(const QString &filter) {
    m_filter = filter;
}

#endif // FILESELECTOR_H
