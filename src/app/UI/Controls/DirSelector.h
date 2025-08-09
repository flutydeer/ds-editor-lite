#ifndef DIRSELECTOR_H
#define DIRSELECTOR_H

#include <QWidget>
#include <QString>

class QDragEnterEvent;
class QDropEvent;
class QPushButton;
class LineEdit;

class DirSelector final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath)
public:
    explicit DirSelector(QWidget *parent = nullptr);
    QString path() const;
    void setPath(const QString &path) const;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void pathChanged(const QString &newPath);

private Q_SLOTS:
    void onBrowseButtonClicked();
    void onTextChanged();

private:
    LineEdit *m_lineEdit;
    QPushButton *m_browseButton;
};

#endif // DIRSELECTOR_H
