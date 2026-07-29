#include "WindowPlacement.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QScreen>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace {
    constexpr QSize largeDefaultSize{1536, 816};
    constexpr QSize compactDefaultSize{1366, 768};

    QRect availableGeometry(const QScreen *screen) {
        if (!screen)
            return {};
        const auto available = screen->availableGeometry();
        return available.isValid() ? available : screen->geometry();
    }

    qint64 intersectionArea(const QRect &first, const QRect &second) {
        const auto intersection = first.intersected(second);
        if (intersection.isEmpty())
            return 0;
        return static_cast<qint64>(intersection.width()) * intersection.height();
    }
}

WindowPlacement::WindowPlacement(QWidget &window) : m_window(window) {
    m_correctionTimer.setSingleShot(true);
    connect(&m_correctionTimer, &QTimer::timeout, this, &WindowPlacement::ensureVisible);

    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        connectScreen(screen);
        scheduleCorrection();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        QObject::disconnect(screen, nullptr, this, nullptr);
        m_connectedScreens.remove(screen);
        syncScreens();
        scheduleCorrection();
    });

    syncScreens();
    m_window.installEventFilter(this);
}

WindowPlacement::~WindowPlacement() {
    m_window.removeEventFilter(this);
}

void WindowPlacement::restoreOrPlace(const QByteArray &geometry) {
    const bool restored = !geometry.isEmpty() && m_window.restoreGeometry(geometry);
    m_window.setWindowState(m_window.windowState() & ~Qt::WindowMinimized);

    if (!restored)
        placeDefault();
    ensureVisible();
    scheduleCorrection();
}

QByteArray WindowPlacement::saveGeometry() const {
    return m_window.saveGeometry();
}

bool WindowPlacement::eventFilter(QObject *watched, QEvent *event) {
    if (watched == &m_window &&
        (event->type() == QEvent::Show || event->type() == QEvent::WindowStateChange)) {
        scheduleCorrection();
    }
    return QObject::eventFilter(watched, event);
}

void WindowPlacement::connectScreen(QScreen *screen) {
    if (!screen || m_connectedScreens.contains(screen))
        return;

    m_connectedScreens.insert(screen);
    connect(screen, &QScreen::geometryChanged, this,
            [this] { scheduleCorrection(); });
    connect(screen, &QScreen::availableGeometryChanged, this,
            [this] { scheduleCorrection(); });
    connect(screen, &QObject::destroyed, this, [this, screen] {
        m_connectedScreens.remove(screen);
        scheduleCorrection();
    });
}

void WindowPlacement::syncScreens() {
    const auto screens = QApplication::screens();
    const QSet<QScreen *> currentScreens(screens.cbegin(), screens.cend());
    const auto connectedScreens = m_connectedScreens;

    for (auto *screen : connectedScreens) {
        if (currentScreens.contains(screen))
            continue;
        QObject::disconnect(screen, nullptr, this, nullptr);
        m_connectedScreens.remove(screen);
    }
    for (auto *screen : screens)
        connectScreen(screen);
}

void WindowPlacement::scheduleCorrection() {
    m_correctionTimer.start(0);
}

void WindowPlacement::ensureVisible() {
    syncScreens();
    if (m_window.isMinimized()) {
        return;
    }

    const auto geometry = placementGeometry();
    if (!geometry.isValid())
        return;

    for (auto *screen : QApplication::screens()) {
        if (availableGeometry(screen).contains(geometry))
            return;
    }

    auto *screen = screenForGeometry(geometry);
    const auto available = availableGeometry(screen);
    if (!available.isValid())
        return;

    const auto constrained = constrainedGeometry(geometry, available);
    const auto state = m_window.windowState();
    const bool wasFullScreen = state.testFlag(Qt::WindowFullScreen);
    const bool wasMaximized = state.testFlag(Qt::WindowMaximized);
    const bool wasVisible = m_window.isVisible();

    if (wasFullScreen || wasMaximized)
        m_window.setWindowState(state & ~(Qt::WindowFullScreen | Qt::WindowMaximized));
    m_window.setGeometry(constrained);

    if (wasVisible && wasFullScreen)
        m_window.showFullScreen();
    else if (wasVisible && wasMaximized)
        m_window.showMaximized();
    else if (wasFullScreen)
        m_window.setWindowState(m_window.windowState() | Qt::WindowFullScreen);
    else if (wasMaximized)
        m_window.setWindowState(m_window.windowState() | Qt::WindowMaximized);
}

void WindowPlacement::placeDefault() {
    auto *screen = QApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QApplication::primaryScreen();

    const auto available = availableGeometry(screen);
    if (!available.isValid()) {
        m_window.resize(compactDefaultSize);
        return;
    }

    const auto preferred =
        available.width() > largeDefaultSize.width() &&
                available.height() > largeDefaultSize.height()
            ? largeDefaultSize
            : compactDefaultSize;
    const auto size = preferred.boundedTo(available.size());
    const auto topLeft =
        available.topLeft() +
        QPoint((available.width() - size.width()) / 2, (available.height() - size.height()) / 2);
    m_window.setGeometry(QRect(topLeft, size));
}

QRect WindowPlacement::placementGeometry() const {
    const auto state = m_window.windowState();
    if (state.testFlag(Qt::WindowMaximized) || state.testFlag(Qt::WindowFullScreen)) {
        const auto normal = m_window.normalGeometry();
        if (normal.isValid())
            return normal;
    }
    return m_window.geometry();
}

QScreen *WindowPlacement::screenForGeometry(const QRect &geometry) const {
    QScreen *bestScreen = nullptr;
    qint64 bestArea = 0;
    for (auto *screen : QApplication::screens()) {
        const auto area = intersectionArea(geometry, availableGeometry(screen));
        if (area > bestArea) {
            bestArea = area;
            bestScreen = screen;
        }
    }
    if (bestScreen)
        return bestScreen;

    if (auto *screen = QApplication::screenAt(QCursor::pos()))
        return screen;
    return QApplication::primaryScreen();
}

QRect WindowPlacement::constrainedGeometry(const QRect &geometry,
                                           const QRect &availableGeometry) {
    const auto size = geometry.size().boundedTo(availableGeometry.size());
    const auto maxLeft = availableGeometry.right() - size.width() + 1;
    const auto maxTop = availableGeometry.bottom() - size.height() + 1;
    const auto left = std::clamp(geometry.left(), availableGeometry.left(), maxLeft);
    const auto top = std::clamp(geometry.top(), availableGeometry.top(), maxTop);
    return {QPoint(left, top), size};
}
