#ifndef FILESELECTOR_H
#define FILESELECTOR_H

#include <QPushButton>
#include <QFileDialog>
#include <QVBoxLayout>

#include "LineEdit.h"

class FileSelector final : public QWidget {
    Q_OBJECT
public:
    explicit FileSelector(QWidget *parent = nullptr);
    QString filePath() const;

signals:
    void filePathChanged(const QString &newFilePath);

private slots:
    void onBrowseButtonClicked();
    void onTextChanged();

private:
    LineEdit *m_lineEdit;
    QPushButton *m_browseButton;
};

#endif // FILESELECTOR_H
