#pragma once

#include <TalcsWidgets/AudioFileDialog.h>
#include <lite/GUI/Utils/WindowDialogs.h>
#include <QApplication>
#include <QDialog>
#include <QEvent>

namespace AudioFilePicker {
    // TALCS opens its own file, raw format and WavPack dialogs synchronously.
    // Apply the caller's modality before each of those dialogs is shown.
    class DialogScope final : public QObject {
    public:
        explicit DialogScope(QWidget *origin) : m_window(origin->window()) {
            qApp->installEventFilter(this);
        }

    protected:
        bool eventFilter(QObject *object, QEvent *event) override {
            if (event->type() == QEvent::Polish) {
                if (auto *dialog = qobject_cast<QDialog *>(object)) {
                    for (auto *parent = dialog->parentWidget(); parent;
                         parent = parent->parentWidget()) {
                        if (parent == m_window) {
                            dialog->setWindowModality(Qt::WindowModal);
                            break;
                        }
                    }
                }
            }
            return false;
        }

    private:
        QWidget *m_window;
    };

    inline talcs::AbstractAudioFormatIO *getOpenAudioFileIO(const talcs::FormatManager *formats,
                                                            QString &path, QVariant &data,
                                                            QString &entry, QWidget *parent,
                                                            const QString &caption,
                                                            const QString &directory) {
        const WindowDialogs::DialogExecution execution(parent);
        DialogScope scope(parent);
        return talcs::AudioFileDialog::getOpenAudioFileIO(formats, path, data, entry, parent,
                                                          caption, directory);
    }
}
