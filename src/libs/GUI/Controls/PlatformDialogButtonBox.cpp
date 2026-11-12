#include "PlatformDialogButtonBox.h"

#include <QLayout>

PlatformDialogButtonBox::PlatformDialogButtonBox(QWidget *parent)
    : PlatformDialogButtonBox(platformLayout(), parent) {
}

PlatformDialogButtonBox::PlatformDialogButtonBox(const ButtonLayout buttonLayout, QWidget *parent)
    : QDialogButtonBox(parent) {
    // Scope the override to this widget. Application/theme stylesheets still
    // control the existing Button and AccentButton instances added by callers.
    setStyleSheet(QStringLiteral("QDialogButtonBox { button-layout: %1; }")
                      .arg(static_cast<int>(buttonLayout)));
    setCenterButtons(false);
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSpacing(12);
}

QDialogButtonBox::ButtonLayout PlatformDialogButtonBox::platformLayout() {
#ifdef Q_OS_MAC
    return MacLayout;
#elif defined(Q_OS_LINUX)
    // Use GNOME's cancel-before-affirmative convention rather than inheriting
    // the Windows drawing style installed by AppEnvironment.
    return GnomeLayout;
#else
    return WinLayout;
#endif
}
