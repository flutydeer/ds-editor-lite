#ifndef EDITORMENUPREVIEWGUARD_H
#define EDITORMENUPREVIEWGUARD_H

#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QObject>

#include <functional>
#include <utility>

class EditorMenuPreviewGuard final : public QObject {
public:
    EditorMenuPreviewGuard(QMenu *menu, QAction *previewAction, std::function<void()> clearCallback)
        : QObject(menu), m_previewAction(previewAction), m_clearPreview(std::move(clearCallback)) {
        Q_ASSERT(menu);
        menu->installEventFilter(this);
        connect(menu, &QMenu::hovered, this, [this](QAction *action) {
            if (action != m_previewAction)
                this->clearPreview();
        });
        connect(menu, &QMenu::aboutToHide, this, [this] { this->clearPreview(); });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Leave)
            clearPreview();
        return QObject::eventFilter(watched, event);
    }

private:
    void clearPreview() const {
        if (m_clearPreview)
            m_clearPreview();
    }

    QAction *m_previewAction = nullptr;
    std::function<void()> m_clearPreview;
};

#endif // EDITORMENUPREVIEWGUARD_H
