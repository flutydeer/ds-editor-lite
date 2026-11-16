#ifndef TIMEPOPUPWIDGET_H
#define TIMEPOPUPWIDGET_H

#include "Model/AppOptions/Options/AppearanceOption.h"

#include <QFrame>
#include <QPointer>

class IconLabel;
class LineEdit;
class QLabel;
class SegmentedTabBar;

// Companion popup of the transport timecode: the playhead position as
// bar:beat:tick and as minutes:seconds.milliseconds side by side (both edit the
// same position), plus the readout format applied to the title bar field. The
// popup owns no state of its own: it reports edits to its owner and is then
// refreshed from the authoritative playhead.
class TimePopupWidget final : public QFrame {
    Q_OBJECT

public:
    explicit TimePopupWidget(QWidget *parent = nullptr);

    void setPosition(int tick);
    void setFormat(AppearanceOption::TimecodeFormat format);
    [[nodiscard]] AppearanceOption::TimecodeFormat format() const;
    void showAt(const QPoint &globalPos);

signals:
    void positionSubmitted(int tick);
    void formatChanged(AppearanceOption::TimecodeFormat format);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshEditors(bool keepActiveEditor);
    void commitEditor(LineEdit *editor, AppearanceOption::TimecodeFormat format);
    void applyWindowEffects();
    void retranslateUi();

    QLabel *m_titleLabel = nullptr;
    QLabel *m_formatLabel = nullptr;
    LineEdit *m_edBarBeatTick = nullptr;
    LineEdit *m_edMinSecMs = nullptr;
    SegmentedTabBar *m_formatBar = nullptr;

    int m_tick = 0;
    AppearanceOption::TimecodeFormat m_format = AppearanceOption::TimecodeFormat::BarBeatTick;
    // Field the user is currently typing into; playhead updates leave it alone
    // until the edit is committed.
    QPointer<LineEdit> m_activeEditor;
    bool m_syncing = false;
};

#endif // TIMEPOPUPWIDGET_H
