#ifndef TIMECOMBOBOX_H
#define TIMECOMBOBOX_H

#include "Model/AppOptions/Options/AppearanceOption.h"
#include "UI/Views/Common/PopupInlineEditLabel.h"

class TimePopupWidget;

// Transport timecode field. The text is the current position in the configured
// readout; a single click opens the time popup, a double click edits the field
// in place with the same parser the popup uses.
class TimeComboBox : public PopupInlineEditLabel {
    Q_OBJECT

public:
    explicit TimeComboBox(QWidget *parent = nullptr);
    ~TimeComboBox() override = default;

    void setPosition(int tick);
    void setFormat(AppearanceOption::TimecodeFormat format);
    [[nodiscard]] AppearanceOption::TimecodeFormat format() const;

signals:
    void positionSubmitted(int tick);
    void formatChanged(AppearanceOption::TimecodeFormat format);

protected:
    void preparePopup() override;
    void presentPopup(const QPoint &globalPos) override;

private:
    void updateText();

    TimePopupWidget *m_popup = nullptr;
    int m_tick = 0;
    // Last tick announced through positionSubmitted, so re-committing the same
    // text does not move the playhead again.
    int m_reportedTick = 0;
    AppearanceOption::TimecodeFormat m_format = AppearanceOption::TimecodeFormat::BarBeatTick;
};

#endif // TIMECOMBOBOX_H
