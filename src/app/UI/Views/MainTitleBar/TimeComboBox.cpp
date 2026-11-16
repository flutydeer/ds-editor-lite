#include "TimeComboBox.h"

#include "TimePopupWidget.h"
#include "UI/Utils/TimecodeFormatUtils.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

TimeComboBox::TimeComboBox(QWidget *parent) : PopupInlineEditLabel(parent) {
    m_popup = new TimePopupWidget(this);
    setPopupWidget(m_popup);

    connect(m_popup, &TimePopupWidget::positionSubmitted, this, [this](const int tick) {
        setPosition(tick);
        emit positionSubmitted(tick);
    });
    connect(m_popup, &TimePopupWidget::formatChanged, this,
            [this](const AppearanceOption::TimecodeFormat format) {
                // Adopt the format immediately so the field never disagrees with
                // the popup even if persisting the option fails later.
                m_format = format;
                updateText();
                emit formatChanged(format);
            });
    connect(this, &InlineEditLabel::editCompleted, this, [this](const QString &text) {
        int tick = m_tick;
        if (!TimecodeFormatUtils::parsePosition(appModel->timeline(), text, m_format, &tick))
            return;
        // The label already holds the typed text; rewrite it from the parsed
        // tick so what remains on screen is the canonical spelling.
        m_tick = tick;
        updateText();
        if (tick != m_reportedTick) {
            m_reportedTick = tick;
            emit positionSubmitted(tick);
        }
    });
    m_reportedTick = m_tick;
}

void TimeComboBox::setPosition(const int tick) {
    if (m_tick != tick)
        m_reportedTick = tick;
    m_tick = tick;
    updateText();
    // Keep the popup's two fields in step with the playhead while it is open,
    // including when playback moves it.
    m_popup->setPosition(tick);
}

void TimeComboBox::setFormat(const AppearanceOption::TimecodeFormat format) {
    m_format = format;
    m_popup->setFormat(format);
    updateText();
}

AppearanceOption::TimecodeFormat TimeComboBox::format() const {
    return m_format;
}

void TimeComboBox::preparePopup() {
    m_popup->setFormat(m_format);
    m_popup->setPosition(m_tick);
}

void TimeComboBox::presentPopup(const QPoint &globalPos) {
    m_popup->showAt(globalPos);
}

void TimeComboBox::updateText() {
    setText(TimecodeFormatUtils::formatPosition(appModel->timeline(), m_tick, m_format));
}
