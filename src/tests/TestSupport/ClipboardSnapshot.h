#pragma once

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#include <memory>

namespace TestSupport {
    class ClipboardSnapshot final {
    public:
        ClipboardSnapshot() : previous(std::make_unique<QMimeData>()) {
            if (const auto *mime = QGuiApplication::clipboard()->mimeData()) {
                for (const auto &format : mime->formats())
                    previous->setData(format, mime->data(format));
            }
        }

        ~ClipboardSnapshot() {
            QGuiApplication::clipboard()->setMimeData(previous.release());
        }

    private:
        std::unique_ptr<QMimeData> previous;
    };
}
