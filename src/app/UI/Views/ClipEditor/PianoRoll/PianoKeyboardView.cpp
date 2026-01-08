//
// Created by fluty on 24-8-19.
//

#include "PianoKeyboardView.h"

#include "PianoPaintUtils.h"
#include "PianoKeyPreviewHelper.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/Log.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Controller/PlaybackController.h"

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/AudioDevice.h>

#include <QElapsedTimer>
#include <QPainter>
#include <QWheelEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QDateTime>

PianoKeyboardView::PianoKeyboardView(QWidget *parent) : QWidget(parent) {
    setFixedWidth(ClipEditorGlobal::pianoKeyboardWidth);
    setMouseTracking(true);
    initializePreview();
}

PianoKeyboardView::~PianoKeyboardView() {
    cleanupPreview();
}

void PianoKeyboardView::setKeyRange(const double top, const double bottom) {
    m_top = top;
    m_bottom = bottom;
    update();
}

QColor PianoKeyboardView::whiteKeyColor() const {
    return m_whiteKeyColor;
}

void PianoKeyboardView::setWhiteKeyColor(const QColor &whiteKeyColor) {
    m_whiteKeyColor = whiteKeyColor;
    update();
}

QColor PianoKeyboardView::blackKeyColor() const {
    return m_blackKeyColor;
}

void PianoKeyboardView::setBlackKeyColor(const QColor &blackKeyColor) {
    m_blackKeyColor = blackKeyColor;
    update();
}

QColor PianoKeyboardView::dividerColor() const {
    return m_dividerColor;
}

void PianoKeyboardView::setDividerColor(const QColor &dividerColor) {
    m_dividerColor = dividerColor;
    update();
}

void PianoKeyboardView::paintEvent(QPaintEvent *event) {
    QElapsedTimer mstimer;
    mstimer.start();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (m_style == Uniform) {
        drawUniformKeyboard(painter);
        if (m_hoveredKeyIndex >= 0 || m_pressedKeyIndex >= 0) {
            drawHoverOverlay(painter);
        }
    } else {
        drawClassicKeyboard(painter);

        // Draw hover/pressed overlay for black keys only (white keys are drawn in drawClassicKeyboard)
        if ((m_hoveredKeyIndex >= 0 && !PianoPaintUtils::isWhiteKey(m_hoveredKeyIndex)) ||
            (m_pressedKeyIndex >= 0 && !PianoPaintUtils::isWhiteKey(m_pressedKeyIndex))) {
            drawHoverOverlay(painter);
        }
    }

    // const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    // Logger::d("PianoKeyboardView", "paint time: " + QString::number(time));
}

void PianoKeyboardView::wheelEvent(QWheelEvent *e) {
    const auto angleDelta = e->angleDelta().transposed();
    const auto event =
        new QWheelEvent(e->position(), e->globalPosition(), e->pixelDelta(), angleDelta,
                        e->buttons(), e->modifiers(), e->phase(), e->inverted());
    emit wheelScroll(event);
    // QWidget::wheelEvent(event);
}

void PianoKeyboardView::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
}

void PianoKeyboardView::setHoveredKeyIndex(int keyIndex) {
    if (keyIndex != m_hoveredKeyIndex) {
        m_hoveredKeyIndex = keyIndex;
        update();
    }
}

void PianoKeyboardView::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    if (m_hoveredKeyIndex != -1) {
        m_hoveredKeyIndex = -1;
        update();
    }
}

void PianoKeyboardView::mouseMoveEvent(QMouseEvent *event) {
    QWidget::mouseMoveEvent(event);
    const auto x = event->position().x();
    const auto y = event->position().y();
    const int keyIndex = posToKeyIndex(x, y);

    // Handle hover effect
    if (keyIndex != m_hoveredKeyIndex) {
        m_hoveredKeyIndex = keyIndex;
        update();
    }

    // Handle slide/glissando effect when mouse is pressed
    if (event->buttons() & Qt::LeftButton) {
        // Only trigger sound when not playing
        if (playbackController->playbackStatus() != PlaybackGlobal::Stopped) {
            return;
        }

        if (!m_previewInitialized) {
            return;
        }

        // Check if we moved to a different key
        if (keyIndex >= 0 && keyIndex != m_pressedKeyIndex) {
            // Release the old key
            if (m_pressedKeyIndex >= 0) {
                m_previewHelper->noteOff(m_pressedKeyIndex);
            }

            // Press the new key with velocity based on position
            const int velocityInt = calculateVelocity(x, y, keyIndex);
            const double velocity = velocityInt / 127.0;

            m_pressedKeyIndex = keyIndex;
            m_pressPosition = event->position();
            m_pressTime = QDateTime::currentMSecsSinceEpoch();

            m_previewHelper->noteOn(keyIndex, velocity);
            update();
        }
    }
}

int PianoKeyboardView::sceneYToKeyIndex(double y) const {
    if (y < 0 || y > height())
        return -1;

    if (m_style == Uniform) {
        const auto ratio = y / height();
        const auto keyIndex = m_top - ratio * (m_top - m_bottom);
        return static_cast<int>(keyIndex);
    } else {
        const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
        const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;

        auto blackKeyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        auto closestBKeyY = [=](const int key) {
            const int remain = key % 12;
            const int bIndex = key + 12 - remain - 1;
            return blackKeyToY(bIndex);
        };

        auto whiteKeyToY = [=](const int key) {
            const int remain = key % 12;
            int index = 0;
            if (remain == 0)
                index = 0;
            else if (remain == 2)
                index = 1;
            else if (remain == 4)
                index = 2;
            else if (remain == 5)
                index = 3;
            else if (remain == 7)
                index = 4;
            else if (remain == 9)
                index = 5;
            else if (remain == 11)
                index = 6;

            const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
            return y;
        };

        const auto prevKeyIndex = static_cast<int>(m_top) + 1;

        for (int i = prevKeyIndex; i > m_bottom; i--) {
            if (PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto keyY = blackKeyToY(i);
            if (y >= keyY && y < keyY + pixelsPerBlackKey) {
                return i;
            }
        }

        const auto prevWhiteKeyIndex =
            PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;
        for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
            if (!PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto keyY = whiteKeyToY(i);
            if (y >= keyY && y < keyY + pixelsPerWhiteKey) {
                return i;
            }
        }

        const auto ratio = y / height();
        const auto approximateKey = m_top - ratio * (m_top - m_bottom);
        return static_cast<int>(approximateKey);
    }
}

int PianoKeyboardView::posToKeyIndex(double x, double y) const {
    if (y < 0 || y > height())
        return -1;

    if (m_style == Uniform) {
        // Uniform style doesn't need x coordinate
        return sceneYToKeyIndex(y);
    } else {
        // Classic style: first check x coordinate, then y coordinate
        const auto blackKeyWidth = width() / 5.0 * 3;
        const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
        const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;

        auto blackKeyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        auto closestBKeyY = [=](const int key) {
            const int remain = key % 12;
            const int bIndex = key + 12 - remain - 1;
            return blackKeyToY(bIndex);
        };

        auto whiteKeyToY = [=](const int key) {
            const int remain = key % 12;
            int index = 0;
            if (remain == 0)
                index = 0;
            else if (remain == 2)
                index = 1;
            else if (remain == 4)
                index = 2;
            else if (remain == 5)
                index = 3;
            else if (remain == 7)
                index = 4;
            else if (remain == 9)
                index = 5;
            else if (remain == 11)
                index = 6;

            const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
            return y;
        };

        const auto prevKeyIndex = static_cast<int>(m_top) + 1;

        // First check if x is within black key width range
        // If yes, check if y falls on a black key
        if (x >= 0 && x < blackKeyWidth) {
            for (int i = prevKeyIndex; i > m_bottom; i--) {
                if (PianoPaintUtils::isWhiteKey(i))
                    continue;

                const auto keyY = blackKeyToY(i);
                if (y >= keyY && y < keyY + pixelsPerBlackKey) {
                    return i;
                }
            }
        }

        // If x is outside black key width, or y doesn't fall on any black key,
        // check white keys
        const auto prevWhiteKeyIndex =
            PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;
        for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
            if (!PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto keyY = whiteKeyToY(i);
            if (y >= keyY && y < keyY + pixelsPerWhiteKey) {
                return i;
            }
        }

        // Fallback: approximate based on y coordinate
        const auto ratio = y / height();
        const auto approximateKey = m_top - ratio * (m_top - m_bottom);
        return static_cast<int>(approximateKey);
    }
}

void PianoKeyboardView::drawUniformKeyboard(QPainter &painter) const {
    QPen pen;
    pen.setWidthF(penWidth);

    const auto pixelsPerKey = height() / (m_top - m_bottom);
    const auto prevKeyIndex = static_cast<int>(m_top) + 1;
    bool prevIsWhiteKey = false;

    auto keyToY = [this](const int key) {
        const auto ratio = (key - m_top) / (m_bottom - m_top);
        const auto y = height() * ratio;
        return y;
    };

    for (int i = prevKeyIndex; i > m_bottom; i--) {
        constexpr auto radius = 4.0;
        const auto y = keyToY(i);

        const auto isWhiteKey = PianoPaintUtils::isWhiteKey(i);
        painter.setBrush(isWhiteKey ? m_whiteKeyColor : m_blackKeyColor);
        auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerKey);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(keyRect, radius, radius);

        if (i % 12 == 0) {
            auto textRect = QRectF(6, y, width(), pixelsPerKey);
            auto fontMetrics = painter.fontMetrics();
            const auto textHeight = fontMetrics.height();
            if (textRect.height() > textHeight) {
                pen.setColor(isWhiteKey ? m_blackKeyColor : m_whiteKeyColor);
                painter.setPen(pen);
                painter.drawText(textRect, PianoPaintUtils::noteName(i),
                                 QTextOption(Qt::AlignVCenter));
            }
        }

        if (prevIsWhiteKey && isWhiteKey) {
            pen.setColor(m_dividerColor);
            painter.setPen(pen);
            auto line = QLineF(0, y, width() - radius + 1, y);
            painter.drawLine(line);
        }

        prevIsWhiteKey = isWhiteKey;
    }
}

void PianoKeyboardView::drawClassicKeyboard(QPainter &painter) {
    constexpr auto radius = 4.0;
    const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
    const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;
    const auto prevKeyIndex = static_cast<int>(m_top) + 1;
    const auto prevWhiteKeyIndex =
        PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;

    auto blackKeyToY = [this](const int key) {
        const auto ratio = (key - m_top) / (m_bottom - m_top);
        const auto y = height() * ratio;
        return y;
    };

    auto closestBKeyY = [=](const int key) {
        const int remain = key % 12;
        const int bIndex = key + 12 - remain - 1;
        // qDebug() << "bIndex" << bIndex << "Note name: " << PianoPaintUtils::noteName(bIndex);
        return blackKeyToY(bIndex);
    };

    auto whiteKeyToY = [=](const int key) {
        const int remain = key % 12;
        int index = 0;
        if (remain == 0)
            index = 0;
        else if (remain == 2)
            index = 1;
        else if (remain == 4)
            index = 2;
        else if (remain == 5)
            index = 3;
        else if (remain == 7)
            index = 4;
        else if (remain == 9)
            index = 5;
        else if (remain == 11)
            index = 6;

        const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
        // qDebug() << "closestBKeyY: " << y;
        return y;
    };

    QPen pen;
    pen.setWidthF(penWidth);

    auto drawWhiteKeys = [&] {
        for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
            if (!PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto y = whiteKeyToY(i);
            painter.setBrush(m_whiteKeyColor);
            auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerWhiteKey);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(keyRect, radius, radius);

            pen.setColor(m_dividerColor);
            painter.setPen(pen);
            auto line = QLineF(0, y, width() - radius + 1, y);
            painter.drawLine(line);

            if (i % 12 == 0) {
                auto textRect = QRectF(6, y, width() - radius - 6, pixelsPerWhiteKey);
                auto fontMetrics = painter.fontMetrics();
                const auto textHeight = fontMetrics.height();
                if (textRect.height() > textHeight) {
                    pen.setColor(m_blackKeyColor);
                    painter.setPen(pen);
                    painter.drawText(textRect, PianoPaintUtils::noteName(i),
                                     QTextOption(Qt::AlignVCenter | Qt::AlignRight));
                }
            }
        }
    };

    auto drawBlackKeys = [&] {
        for (int i = prevKeyIndex; i > m_bottom; i--) {
            if (PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto y = blackKeyToY(i);
            painter.setBrush(m_blackKeyColor);
            auto keyRect = QRectF(-radius, y, width() / 5.0 * 3 + radius, pixelsPerBlackKey);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(keyRect, radius, radius);

            // auto textRect = QRectF(6, y, width(), pixelsPerBlackKey);
            // auto fontMetrics = painter.fontMetrics();
            // auto textHeight = fontMetrics.height();
            // if (textRect.height() > textHeight) {
            //     pen.setColor(m_whiteKeyColor);
            //     painter.setPen(pen);
            //     painter.drawText(textRect, PianoPaintUtils::noteName(i),
            //                      QTextOption(Qt::AlignVCenter));
            // }
        }
    };

    drawWhiteKeys();

    // Draw hover/pressed overlay for white keys
    const int activeKeyIndex = m_pressedKeyIndex >= 0 ? m_pressedKeyIndex : m_hoveredKeyIndex;
    if (activeKeyIndex >= 0 && PianoPaintUtils::isWhiteKey(activeKeyIndex)) {
        constexpr auto radius = 4.0;
        // Use darker color for pressed, lighter for hover
        const bool isPressed = (m_pressedKeyIndex >= 0 && m_pressedKeyIndex == activeKeyIndex);
        constexpr auto overlayAlpha = 128;
        QColor overlayColor = isPressed ? m_primaryColor.darker(130) : m_primaryColor;
        overlayColor.setAlpha(overlayAlpha);
        painter.setBrush(overlayColor);
        painter.setPen(Qt::NoPen);

        const auto y = whiteKeyToY(activeKeyIndex);
        auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerWhiteKey);
        painter.drawRoundedRect(keyRect, radius, radius);
    }

    drawBlackKeys();
}

void PianoKeyboardView::drawHoverOverlay(QPainter &painter) const {
    // Determine which key to highlight (pressed takes priority over hovered)
    const int activeKeyIndex = m_pressedKeyIndex >= 0 ? m_pressedKeyIndex : m_hoveredKeyIndex;

    if (activeKeyIndex < 0)
        return;

    constexpr auto radius = 4.0;
    // Use darker color for pressed, lighter for hover
    const bool isPressed = (m_pressedKeyIndex >= 0 && m_pressedKeyIndex == activeKeyIndex);
    constexpr auto overlayAlpha = 128;
    QColor overlayColor = isPressed ? m_primaryColor.darker(130) : m_primaryColor;
    overlayColor.setAlpha(overlayAlpha);
    painter.setBrush(overlayColor);
    painter.setPen(Qt::NoPen);

    if (m_style == Uniform) {
        const auto pixelsPerKey = height() / (m_top - m_bottom);
        auto keyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        const auto y = keyToY(activeKeyIndex);
        auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerKey);
        painter.drawRoundedRect(keyRect, radius, radius);
    } else {
        const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
        const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;

        auto blackKeyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        auto closestBKeyY = [=](const int key) {
            const int remain = key % 12;
            const int bIndex = key + 12 - remain - 1;
            return blackKeyToY(bIndex);
        };

        auto whiteKeyToY = [=](const int key) {
            const int remain = key % 12;
            int index = 0;
            if (remain == 0)
                index = 0;
            else if (remain == 2)
                index = 1;
            else if (remain == 4)
                index = 2;
            else if (remain == 5)
                index = 3;
            else if (remain == 7)
                index = 4;
            else if (remain == 9)
                index = 5;
            else if (remain == 11)
                index = 6;

            const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
            return y;
        };

        const bool isWhiteKey = PianoPaintUtils::isWhiteKey(activeKeyIndex);
        if (isWhiteKey) {
            const auto y = whiteKeyToY(activeKeyIndex);
            auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerWhiteKey);
            painter.drawRoundedRect(keyRect, radius, radius);
        } else {
            const auto y = blackKeyToY(activeKeyIndex);
            auto keyRect = QRectF(-radius, y, width() / 5.0 * 3 + radius, pixelsPerBlackKey);
            painter.drawRoundedRect(keyRect, radius, radius);
        }
    }
}

int PianoKeyboardView::calculateVelocity(const double x, const double y, const int keyIndex) const {
    // Calculate velocity based on click position
    // Click on the left/top part (front of key) = stronger velocity
    // Click on the right/bottom part (back of key) = weaker velocity

    if (m_style == Uniform) {
        // For uniform style, use x position (left = strong, right = weak)
        const double ratio = x / width();
        // Map ratio to velocity: left (0.0) -> 100, right (1.0) -> 40
        const int velocity = static_cast<int>(100 - ratio * 60);
        return qBound(40, velocity, 100);
    } else {
        // For classic style, check if it's a black or white key
        const bool isBlackKey = !PianoPaintUtils::isWhiteKey(keyIndex);
        const double blackKeyWidth = width() / 5.0 * 3;

        if (isBlackKey) {
            // For black keys, use x position within black key area
            const double ratio = x / blackKeyWidth;
            const int velocity = static_cast<int>(100 - ratio * 60);
            return qBound(40, velocity, 100);
        } else {
            // For white keys, use full width
            const double ratio = x / width();
            const int velocity = static_cast<int>(100 - ratio * 60);
            return qBound(40, velocity, 100);
        }
    }
}

void PianoKeyboardView::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);

    // Only trigger sound when not playing
    if (playbackController->playbackStatus() != PlaybackGlobal::Stopped) {
        return;
    }

    if (!m_previewInitialized) {
        return;
    }

    const auto x = event->position().x();
    const auto y = event->position().y();
    const int keyIndex = posToKeyIndex(x, y);

    if (keyIndex < 0) {
        return;
    }

    // Calculate velocity based on click position
    const int velocityInt = calculateVelocity(x, y, keyIndex);
    const double velocity = velocityInt / 127.0; // Convert to 0.0-1.0 range

    // Store pressed key info
    m_pressedKeyIndex = keyIndex;
    m_pressPosition = event->position();
    m_pressTime = QDateTime::currentMSecsSinceEpoch();

    // Send note on message through detector
    m_previewHelper->noteOn(keyIndex, velocity);

    update();
}

void PianoKeyboardView::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);

    if (m_pressedKeyIndex < 0) {
        return;
    }

    if (!m_previewInitialized) {
        return;
    }

    // Send note off message through detector
    m_previewHelper->noteOff(m_pressedKeyIndex);

    m_pressedKeyIndex = -1;
    update();
}

void PianoKeyboardView::initializePreview() {
    try {
        // Create preview synthesizer and helper
        m_previewHelper = std::make_unique<PianoKeyPreviewHelper>();
        m_previewSynthesizer = std::make_unique<talcs::NoteSynthesizer>();
        m_previewMixer = std::make_unique<talcs::MixerAudioSource>();

        // Set up synthesizer
        m_previewSynthesizer->setDetector(m_previewHelper.get());

        // Get audio device info
        auto outputSystem = AudioSystem::outputSystem();
        if (!outputSystem || !outputSystem->outputContext()) {
            return;
        }

        auto device = outputSystem->outputContext()->device();
        if (!device || !device->isOpen()) {
            return;
        }

        const auto bufferSize = device->bufferSize();
        const auto sampleRate = device->sampleRate();

        // Open synthesizer
        if (!m_previewSynthesizer->open(bufferSize, sampleRate)) {
            return;
        }

        // Set piano-like envelope to avoid clicks and pops
        // Attack: 5ms for quick response but smooth start
        // Decay: 200ms with 0.7 ratio for piano-like sustain decay
        // Release: 80ms for natural piano release
        const qint64 attackSamples = static_cast<qint64>(sampleRate * 0.005);   // 5ms
        const qint64 decaySamples = static_cast<qint64>(sampleRate * 0.2);      // 200ms
        const qint64 releaseSamples = static_cast<qint64>(sampleRate * 0.08);   // 80ms

        m_previewSynthesizer->setAttackTime(attackSamples);
        m_previewSynthesizer->setDecayTime(decaySamples);
        m_previewSynthesizer->setDecayRatio(0.8);  // Decay to 80% for sustain level
        m_previewSynthesizer->setReleaseTime(releaseSamples);

        // Use Triangle wave for a softer, more piano-like tone
        // Triangle has fewer harsh harmonics compared to Square or Sawtooth
        m_previewSynthesizer->setGenerator(talcs::NoteSynthesizer::Triangle);

        // Set lower amplitude for comfortable listening (0.3 = 30% of max)
        m_previewSynthesizer->setAmplitude(0.3f);

        // Add to mixer with reduced gain
        m_previewMixer->addSource(m_previewSynthesizer.get());
        m_previewMixer->setGain(0.4f);  // Further reduce volume to 40%

        // Add to output system's preMixer
        outputSystem->outputContext()->preMixer()->addSource(m_previewMixer.get());

        m_previewInitialized = true;
    } catch (...) {
        // Failed to initialize, cleanup
        cleanupPreview();
    }
}

void PianoKeyboardView::cleanupPreview() {
    if (!m_previewInitialized) {
        return;
    }

    // Remove from output system
    auto outputSystem = AudioSystem::outputSystem();
    if (outputSystem && outputSystem->outputContext()) {
        outputSystem->outputContext()->preMixer()->removeSource(m_previewMixer.get());
    }

    // Close synthesizer
    if (m_previewSynthesizer) {
        m_previewSynthesizer->close();
    }

    // Clear mixer
    if (m_previewMixer) {
        m_previewMixer->removeAllSources();
    }

    // Reset pointers will be done automatically by unique_ptr

    m_previewInitialized = false;
}