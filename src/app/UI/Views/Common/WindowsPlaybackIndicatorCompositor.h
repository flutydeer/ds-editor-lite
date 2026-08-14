#ifndef WINDOWSPLAYBACKINDICATORCOMPOSITOR_H
#define WINDOWSPLAYBACKINDICATORCOMPOSITOR_H

#include <QColor>
#include <QString>
#include <QtGlobal>

#include <memory>

class QRhi;
class QRhiCommandBuffer;

class WindowsPlaybackIndicatorCompositor final {
public:
    WindowsPlaybackIndicatorCompositor();
    ~WindowsPlaybackIndicatorCompositor();

    bool initialize(QRhi *rhi, QRhiCommandBuffer *commandBuffer, quintptr windowId,
                    const QColor &indicatorColor, QString *error);
    void release();

    [[nodiscard]] bool isReady() const;
    [[nodiscard]] bool hasPendingContentUpdate() const;
    bool setColor(const QColor &color);
    bool prepare(QRhiCommandBuffer *commandBuffer, QString *error);
    bool setGeometry(qreal physicalX, qreal physicalWidth, qreal physicalHeight, bool visible,
                     QString *error);

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // WINDOWSPLAYBACKINDICATORCOMPOSITOR_H
