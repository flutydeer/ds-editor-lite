#include "TimePopupWidget.h"

#include "UI/Utils/TimecodeFormatUtils.h"
#include "UI/Views/Common/SegmentedTabBar.h"
#include "Utils/FontManager.h"

#include <lite/GUI/Controls/IconLabel.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Utils/WindowFrameUtils.h>
#include <lite/MusicBase/MusicTimeConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/Support/SystemUtils.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <array>

namespace {
    constexpr int kPopupWidth = 320;
    constexpr int kPopupMargin = 12;
    constexpr int kFieldHeight = 28;
    constexpr int kLinkIconSize = 16;

    struct FormatTab {
        AppearanceOption::TimecodeFormat format;
        const char *source;
    };

    constexpr std::array kFormatTabs = {
        FormatTab{AppearanceOption::TimecodeFormat::BarBeatTick, QT_TRANSLATE_NOOP(
                                                                     "TimePopupWidget",
                                                                     "Bar:Beat:Tick")},
        FormatTab{AppearanceOption::TimecodeFormat::Time,
                  QT_TRANSLATE_NOOP("TimePopupWidget", "Min:Sec.Ms")           },
        FormatTab{AppearanceOption::TimecodeFormat::Both, QT_TRANSLATE_NOOP("TimePopupWidget", "Both")}
    };
} // namespace

TimePopupWidget::TimePopupWidget(QWidget *parent) : QFrame(parent) {
    setObjectName("timePopup");
    setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
    // Keep this popup opaque: WA_TranslucentBackground plus DWM frame effects on the same
    // window can freeze the compositor system-wide. Corner rounding comes from DWM on
    // Windows 11 and stays square elsewhere.
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_WindowPropagation);
    setProperty("dwmBorder", false);

#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11())
        setProperty("dwmBorder", true);
#endif

    const QFont musicFont = FontManager::instance().musicUIFont(13);

    m_titleLabel = new QLabel;
    m_titleLabel->setObjectName("popupTitle");
    m_titleLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const auto createEditor = [&musicFont](const QString &objectName) {
        auto *editor = new LineEdit;
        editor->setObjectName(objectName);
        editor->setFont(musicFont);
        editor->setAlignment(Qt::AlignCenter);
        editor->setFixedHeight(kFieldHeight);
        return editor;
    };
    m_edBarBeatTick = createEditor(QStringLiteral("edPosition"));
    m_edMinSecMs = createEditor(QStringLiteral("edPositionTime"));

    auto *linkIcon = new IconLabel;
    linkIcon->setObjectName("linkIcon");
    linkIcon->setIcon(QStringLiteral(":/svg/icons/link_16_filled.svg"));
    linkIcon->setSquareSize(kLinkIconSize);
    linkIcon->setColorToken(QStringLiteral("text.secondary"));
    linkIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *editorRow = new QHBoxLayout;
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(6);
    editorRow->addWidget(m_edBarBeatTick, 1);
    editorRow->addWidget(linkIcon);
    editorRow->addWidget(m_edMinSecMs, 1);

    m_formatLabel = new QLabel;
    m_formatLabel->setObjectName("popupSectionTitle");
    m_formatLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_formatBar = new SegmentedTabBar;
    m_formatBar->setObjectName("formatBar");
    // The popup is a fixed-width card; let the three segments share the row
    // instead of huddling at their natural size, and keep them at the standard
    // control height so they match the panel tab bar and the fields above.
    m_formatBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_formatBar->setFixedHeight(kFieldHeight);
    for (const auto &tab : kFormatTabs)
        m_formatBar->addTab(tr(tab.source));

    auto *surface = new QFrame;
    surface->setObjectName("timePopupSurface");
    surface->setAttribute(Qt::WA_StyledBackground);
    // The card has a fixed width so the two position fields and the three
    // format segments keep the proportions of the design.
    surface->setFixedWidth(kPopupWidth);
    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(kPopupMargin, kPopupMargin, kPopupMargin, kPopupMargin);
    surfaceLayout->setSpacing(8);
    surfaceLayout->addWidget(m_titleLabel);
    surfaceLayout->addLayout(editorRow);
    surfaceLayout->addWidget(m_formatLabel);
    surfaceLayout->addWidget(m_formatBar);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setSizeConstraint(QLayout::SetFixedSize);
    outerLayout->addWidget(surface);

    // Enter and focus-out both commit; the field is then rewritten from the
    // authoritative position, which also reverts unparseable input.
    connect(m_edBarBeatTick, &QLineEdit::textEdited, this,
            [this] { m_activeEditor = m_edBarBeatTick; });
    connect(m_edMinSecMs, &QLineEdit::textEdited, this,
            [this] { m_activeEditor = m_edMinSecMs; });
    connect(m_edBarBeatTick, &QLineEdit::editingFinished, this,
            [this] { commitEditor(m_edBarBeatTick, AppearanceOption::TimecodeFormat::BarBeatTick); });
    connect(m_edMinSecMs, &QLineEdit::editingFinished, this,
            [this] { commitEditor(m_edMinSecMs, AppearanceOption::TimecodeFormat::Time); });
    connect(m_formatBar, &SegmentedTabBar::currentChanged, this, [this](const int index) {
        if (m_syncing || index < 0 || index >= static_cast<int>(kFormatTabs.size()))
            return;
        const auto format = kFormatTabs.at(index).format;
        if (m_format == format)
            return;
        m_format = format;
        emit formatChanged(format);
    });

    retranslateUi();
    refreshEditors(false);
    setFormat(AppearanceOption::TimecodeFormat::BarBeatTick);
}

void TimePopupWidget::setPosition(const int tick) {
    if (m_tick == tick)
        return;
    m_tick = tick;
    refreshEditors(true);
}
void TimePopupWidget::setFormat(const AppearanceOption::TimecodeFormat format) {
    m_format = format;
    for (int i = 0; i < static_cast<int>(kFormatTabs.size()); ++i) {
        if (kFormatTabs.at(i).format != format)
            continue;
        m_formatBar->setCurrentIndex(i);
        break;
    }
}

AppearanceOption::TimecodeFormat TimePopupWidget::format() const {
    return m_format;
}

void TimePopupWidget::refreshEditors(const bool keepActiveEditor) {
    const auto &timeline = appModel->timeline();
    const QSignalBlocker barBeatBlocker(m_edBarBeatTick);
    const QSignalBlocker minSecBlocker(m_edMinSecMs);
    if (!keepActiveEditor || m_activeEditor != m_edBarBeatTick)
        m_edBarBeatTick->setText(timeline.getBarBeatTickTime(qMax(0, m_tick)));
    if (!keepActiveEditor || m_activeEditor != m_edMinSecMs)
        m_edMinSecMs->setText(MusicTimeConverter::formatTimecode(timeline.tickToMs(qMax(0, m_tick))));
}

void TimePopupWidget::commitEditor(LineEdit *editor,
                                   const AppearanceOption::TimecodeFormat format) {
    if (m_activeEditor == editor)
        m_activeEditor.clear();
    if (m_syncing)
        return;
    int tick = m_tick;
    if (TimecodeFormatUtils::parsePosition(appModel->timeline(), editor->text(), format, &tick) &&
        tick != m_tick) {
        m_tick = tick;
        m_syncing = true;
        emit positionSubmitted(tick);
        m_syncing = false;
    }
    refreshEditors(false);
}

void TimePopupWidget::showAt(const QPoint &globalPos) {
    ensurePolished();
    layout()->invalidate();
    layout()->activate();

    QPoint topLeft = globalPos;
    if (const auto screen = QApplication::screenAt(globalPos)) {
        const QRect available = screen->availableGeometry();
        const QRect popupRect(topLeft, sizeHint());
        if (popupRect.right() > available.right())
            topLeft.setX(available.right() - popupRect.width());
        if (popupRect.bottom() > available.bottom())
            topLeft.setY(globalPos.y() - popupRect.height());
        if (topLeft.x() < available.left())
            topLeft.setX(available.left());
        if (topLeft.y() < available.top())
            topLeft.setY(available.top());
    }

    move(topLeft);
    show();
    raise();
    applyWindowEffects();
}

void TimePopupWidget::applyWindowEffects() {
    WindowFrameUtils::applyPopupEffects(this);
}

void TimePopupWidget::changeEvent(QEvent *event) {
    QFrame::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void TimePopupWidget::retranslateUi() {
    m_titleLabel->setText(tr("Time"));
    m_formatLabel->setText(tr("Display Mode"));
    m_edBarBeatTick->setAccessibleName(tr("Bar Beat Tick"));
    m_edMinSecMs->setAccessibleName(tr("Minutes Seconds Milliseconds"));
    for (int i = 0; i < static_cast<int>(kFormatTabs.size()); ++i)
        m_formatBar->setTabText(i, tr(kFormatTabs.at(i).source));
}
