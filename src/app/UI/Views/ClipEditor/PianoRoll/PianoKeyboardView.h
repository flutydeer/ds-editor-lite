//
// Created by fluty on 24-8-19.
//

#ifndef PIANOKEYBOARDVIEW_H
#define PIANOKEYBOARDVIEW_H

#include <QWidget>
#include <QEnterEvent>
#include <QMouseEvent>
#include <memory>

namespace talcs {
    class NoteSynthesizer;
    class MixerAudioSource;
}

class PianoKeyPreviewHelper;

class PianoKeyboardView : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor whiteKeyColor READ whiteKeyColor WRITE setWhiteKeyColor)
    Q_PROPERTY(QColor blackKeyColor READ blackKeyColor WRITE setBlackKeyColor)
    Q_PROPERTY(QColor dividerColor READ dividerColor WRITE setDividerColor)

public:
    enum KeyboardStyle { Uniform, Classic };

    explicit PianoKeyboardView(QWidget *parent = nullptr);
    ~PianoKeyboardView();
    void setHoveredKeyIndex(int keyIndex);

public slots:
    void setKeyRange(double top, double bottom);

signals:
    void wheelScroll(QWheelEvent *event);

private:
    QColor whiteKeyColor() const;
    void setWhiteKeyColor(const QColor &whiteKeyColor);
    QColor blackKeyColor() const;
    void setBlackKeyColor(const QColor &blackKeyColor);
    QColor dividerColor() const;
    void setDividerColor(const QColor &dividerColor);

    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *e) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void drawUniformKeyboard(QPainter &painter) const;
    void drawClassicKeyboard(QPainter &painter);
    void drawHoverOverlay(QPainter &painter) const;
    int sceneYToKeyIndex(double y) const;
    int posToKeyIndex(double x, double y) const;
    int calculateVelocity(double x, double y, int keyIndex) const;

    double m_top = 0;
    double m_bottom = 127;
    KeyboardStyle m_style = Classic;

    const double penWidth = 1;
    QColor m_whiteKeyColor = {218, 219, 224};
    QColor m_blackKeyColor = {59, 63, 71};
    QColor m_dividerColor = {170, 172, 181};
    QColor m_primaryColor = {155, 186, 255};
    int m_hoveredKeyIndex = -1;
    int m_pressedKeyIndex = -1;
    QPointF m_pressPosition;
    qint64 m_pressTime = 0;

    std::unique_ptr<PianoKeyPreviewHelper> m_previewHelper;
    std::unique_ptr<talcs::NoteSynthesizer> m_previewSynthesizer;
    std::unique_ptr<talcs::MixerAudioSource> m_previewMixer;
    bool m_previewInitialized = false;

    void initializePreview();
    void cleanupPreview();
};



#endif // PIANOKEYBOARDVIEW_H
