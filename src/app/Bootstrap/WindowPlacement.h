#ifndef WINDOWPLACEMENT_H
#define WINDOWPLACEMENT_H

#include <QByteArray>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QTimer>

class QEvent;
class QScreen;
class QWidget;

class WindowPlacement final : public QObject {
public:
    explicit WindowPlacement(QWidget &window);
    ~WindowPlacement() override;

    // Restores geometry when available, otherwise places the window on the
    // screen under the cursor. preferredSize overrides the built-in default
    // window size for the "no saved geometry" case.
    void restoreOrPlace(const QByteArray &geometry, const QSize &preferredSize = {});
    [[nodiscard]] QByteArray saveGeometry() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void connectScreen(QScreen *screen);
    void syncScreens();
    void scheduleCorrection();
    void ensureVisible();
    void placeDefault(const QSize &preferredSize);
    [[nodiscard]] QRect placementGeometry() const;
    [[nodiscard]] QScreen *screenForGeometry(const QRect &geometry) const;
    [[nodiscard]] static QRect constrainedGeometry(const QRect &geometry,
                                                   const QRect &availableGeometry);

    QWidget &m_window;
    QSet<QScreen *> m_connectedScreens;
    QTimer m_correctionTimer;
};

#endif // WINDOWPLACEMENT_H
