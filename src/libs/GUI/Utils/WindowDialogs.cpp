#include "WindowDialogs.h"

#include <QApplication>
#include <lite/Support/ObjectContext.h>

namespace WindowDialogs {
    static thread_local QList<SingletonRegistry::Context> executingDialogs;

    DialogExecution::DialogExecution(const QWidget *dialog) {
        const auto owner = ObjectContext::of(dialog);
        executingDialogs.append(owner ? owner : SingletonRegistry::current());
    }

    DialogExecution::~DialogExecution() {
        executingDialogs.removeLast();
    }

    bool DialogExecution::activeFor(const SingletonRegistry::Context &context) {
        return executingDialogs.contains(context);
    }

    static bool owns(const QWidget *owner, const QWidget *child) {
        for (; child; child = child->parentWidget())
            if (child == owner)
                return true;
        return false;
    }

    QWidget *blockingDialog(const QWidget *window) {
        auto *active = QApplication::activeModalWidget();
        QWidget *blocker = nullptr;
        for (auto *dialog : QApplication::topLevelWidgets()) {
            if (!dialog->isVisible() || !dialog->isModal() || owns(dialog, window))
                continue;
            if (dialog->windowModality() != Qt::ApplicationModal && !owns(window, dialog))
                continue;
            if (owns(dialog, active))
                return active;
            if (!blocker || owns(blocker, dialog))
                blocker = dialog;
        }
        return blocker;
    }

    bool blocks(const QWidget *window) {
        return !window || !window->isEnabled() || blockingDialog(window);
    }

    int exec(QDialog &dialog) {
        const DialogExecution execution(&dialog);
        return dialog.exec();
    }

    static QStringList choose(QWidget *parent, const QString &caption, const QString &dir,
                              const QString &filter, QFileDialog::FileMode mode,
                              QFileDialog::AcceptMode accept, QString *selectedFilter = nullptr,
                              Qt::WindowModality modality = Qt::WindowModal) {
        QFileDialog dialog(parent, caption, dir, filter);
        dialog.setFileMode(mode);
        dialog.setAcceptMode(accept);
        if (mode == QFileDialog::Directory)
            dialog.setOption(QFileDialog::ShowDirsOnly);
        if (selectedFilter)
            dialog.selectNameFilter(*selectedFilter);
        dialog.setWindowModality(modality);
        if (exec(dialog) != QDialog::Accepted)
            return {};
        if (selectedFilter)
            *selectedFilter = dialog.selectedNameFilter();
        return dialog.selectedFiles();
    }

    QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                            const QString &filter) {
        return choose(parent, caption, dir, filter, QFileDialog::ExistingFile,
                      QFileDialog::AcceptOpen)
            .value(0);
    }

    QStringList getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir,
                                 const QString &filter) {
        return choose(parent, caption, dir, filter, QFileDialog::ExistingFiles,
                      QFileDialog::AcceptOpen, nullptr,
                      parent ? Qt::WindowModal : Qt::ApplicationModal);
    }

    QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                            const QString &filter, QString *selectedFilter,
                            Qt::WindowModality modality) {
        return choose(parent, caption, dir, filter, QFileDialog::AnyFile, QFileDialog::AcceptSave,
                      selectedFilter, modality)
            .value(0);
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
        const bool accepted = exec(dialog) == QDialog::Accepted;
        if (ok)
            *ok = accepted;
        return accepted ? dialog.textValue() : QString();
    }

    static QMessageBox::StandardButton
        message(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text,
                QMessageBox::StandardButtons buttons,
                QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) {
        QMessageBox dialog(icon, title, text, buttons, parent);
        dialog.setDefaultButton(defaultButton);
        dialog.setWindowModality(Qt::WindowModal);
        exec(dialog);
        return dialog.standardButton(dialog.clickedButton());
    }

    QMessageBox::StandardButton question(QWidget *parent, const QString &title, const QString &text,
                                         QMessageBox::StandardButtons buttons,
                                         QMessageBox::StandardButton defaultButton) {
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
