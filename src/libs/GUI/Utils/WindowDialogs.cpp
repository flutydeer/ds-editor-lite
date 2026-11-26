#include "WindowDialogs.h"

#include <QApplication>

namespace WindowDialogs {
    bool blocks(const QWidget *window) {
        if (!window || !window->isEnabled())
            return true;
        for (auto *dialog : QApplication::topLevelWidgets()) {
            if (!dialog->isVisible() || !dialog->isModal() || dialog == window ||
                dialog->isAncestorOf(window))
                continue;
            if (dialog->windowModality() == Qt::ApplicationModal || window->isAncestorOf(dialog))
                return true;
        }
        return false;
    }

    static QStringList choose(QWidget *parent, const QString &caption, const QString &dir,
                              const QString &filter, QFileDialog::FileMode mode,
                              QFileDialog::AcceptMode accept, QString *selectedFilter = nullptr) {
        QFileDialog dialog(parent, caption, dir, filter);
        dialog.setFileMode(mode);
        dialog.setAcceptMode(accept);
        if (mode == QFileDialog::Directory)
            dialog.setOption(QFileDialog::ShowDirsOnly);
        if (selectedFilter)
            dialog.selectNameFilter(*selectedFilter);
        dialog.setWindowModality(Qt::WindowModal);
        if (dialog.exec() != QDialog::Accepted)
            return {};
        if (selectedFilter)
            *selectedFilter = dialog.selectedNameFilter();
        return dialog.selectedFiles();
    }

    QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                            const QString &filter) {
        return choose(parent, caption, dir, filter, QFileDialog::ExistingFile,
                      QFileDialog::AcceptOpen).value(0);
    }

    QStringList getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir,
                                 const QString &filter) {
        return choose(parent, caption, dir, filter, QFileDialog::ExistingFiles,
                      QFileDialog::AcceptOpen);
    }

    QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                            const QString &filter, QString *selectedFilter) {
        return choose(parent, caption, dir, filter, QFileDialog::AnyFile,
                      QFileDialog::AcceptSave, selectedFilter).value(0);
    }

    QString getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir) {
        return choose(parent, caption, dir, {}, QFileDialog::Directory, QFileDialog::AcceptOpen)
            .value(0);
    }

    QString getText(QWidget *parent, const QString &title, const QString &label,
                    QLineEdit::EchoMode mode, const QString &text, bool *ok) {
        QInputDialog dialog(parent);
        dialog.setWindowTitle(title);
        dialog.setLabelText(label);
        dialog.setTextEchoMode(mode);
        dialog.setTextValue(text);
        dialog.setWindowModality(Qt::WindowModal);
        const bool accepted = dialog.exec() == QDialog::Accepted;
        if (ok)
            *ok = accepted;
        return accepted ? dialog.textValue() : QString();
    }

    static QMessageBox::StandardButton message(QWidget *parent, QMessageBox::Icon icon,
        const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) {
        QMessageBox dialog(icon, title, text, buttons, parent);
        dialog.setDefaultButton(defaultButton);
        dialog.setWindowModality(Qt::WindowModal);
        dialog.exec();
        return dialog.standardButton(dialog.clickedButton());
    }

    QMessageBox::StandardButton question(QWidget *parent, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
        return message(parent, QMessageBox::Question, title, text, buttons, defaultButton);
    }

    void warning(QWidget *parent, const QString &title, const QString &text) {
        message(parent, QMessageBox::Warning, title, text, QMessageBox::Ok);
    }

    void critical(QWidget *parent, const QString &title, const QString &text) {
        message(parent, QMessageBox::Critical, title, text, QMessageBox::Ok);
    }

    void information(QWidget *parent, const QString &title, const QString &text) {
        message(parent, QMessageBox::Information, title, text, QMessageBox::Ok);
    }
}
