#ifndef LITE_GUI_WINDOWDIALOGS_H
#define LITE_GUI_WINDOWDIALOGS_H

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

namespace WindowDialogs {
    bool blocks(const QWidget *window);
    QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                            const QString &filter);
    QStringList getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir,
                                 const QString &filter);
    QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                            const QString &filter, QString *selectedFilter = nullptr,
                            Qt::WindowModality modality = Qt::WindowModal);
    QString getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir = {});
    QString getText(QWidget *parent, const QString &title, const QString &label,
                    QLineEdit::EchoMode mode, const QString &text, bool *ok = nullptr);
    QMessageBox::StandardButton
        question(QWidget *parent, const QString &title, const QString &text,
                 QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                 QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    void warning(QWidget *parent, const QString &title, const QString &text);
    void critical(QWidget *parent, const QString &title, const QString &text);
    void information(QWidget *parent, const QString &title, const QString &text);
}

#endif
