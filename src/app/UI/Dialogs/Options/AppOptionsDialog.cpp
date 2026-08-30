#include "AppOptionsDialog.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPainter>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <lite/GUI/Utils/IconUtils.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include "Pages/AppearancePage.h"
#include "Pages/AudioPage.h"
#include "Pages/DeveloperPage.h"
#include "Pages/GeneralPage.h"
#include "Pages/InferencePage.h"
#include "Pages/MidiPage.h"
#include "UI/Dialogs/Base/Dialog.h"

namespace {

    // Re-tints the sidebar icon at paint time to match the item text color:
    // text.primary normally, text.accent while selected, mirroring the ::item QSS
    // color rules so icon and label always read as one element.
    class AppOptionsSidebarDelegate final : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override {
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);
            const bool selected = (opt.state & QStyle::State_Selected) != 0;
            const auto svgPath = index.data(Qt::UserRole).toString();
            if (!svgPath.isEmpty()) {
                auto color = ThemeManager::instance()->semanticColor(
                    selected ? QStringLiteral("text.accent") : QStringLiteral("text.primary"));
                if (!color.isValid())
                    color = QColor(240, 240, 240, 255);
                opt.icon = IconUtils::createTintedSvgIcon(svgPath, QSize(20, 20), color);
            }
            const auto *widget = opt.widget;
            QStyle *style = widget ? widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        }
    };

} // namespace

AppOptionsDialog::AppOptionsDialog(QWidget *parent, const bool standalone)
    : QWidget(parent), m_standalone(standalone) {
    setObjectName(QStringLiteral("AppOptionsDialog"));
    setFocusPolicy(Qt::ClickFocus);

    m_tabList = new QListWidget;
    m_tabList->setFixedWidth(160);
    m_tabList->setIconSize(QSize(20, 20));
    m_tabList->setObjectName("AppOptionsDialogTabListWidget");
    m_tabList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Icons follow the item text color (see delegate at the top of this file).
    m_tabList->setItemDelegate(new AppOptionsSidebarDelegate(m_tabList));

    // The sidebar is a dedicated container owning the sidebar background;
    // the list itself stays transparent and unpadded (box-model padding on a
    // scroll area caused a spurious scrollbar).
    m_sidebar = new QWidget;
    m_sidebar->setObjectName(QStringLiteral("AppOptionsSidebar"));
    m_sidebar->setAttribute(Qt::WA_StyledBackground);
    // Standalone mode: flush against the square window edge, so the QSS
    // drops the left rounding (QWidget#AppOptionsSidebar[standalone="true"]).
    m_sidebar->setProperty("standalone", m_standalone);
    const auto sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(8, 8, 8, 8);
    sidebarLayout->addWidget(m_tabList);
    // The list's raw sizeHint (content-based) can exceed its fixed width and
    // would stretch the sidebar; pin the sidebar to list width + margins so
    // the background hugs the list.
    m_sidebar->setFixedWidth(m_tabList->minimumWidth() + sidebarLayout->contentsMargins().left() +
                             sidebarLayout->contentsMargins().right());

    m_pageContent = new QStackedWidget;
    m_pageContent->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    m_pageContent->setMinimumWidth(600);

    retranslateUi();
    m_pages.resize(m_tabList->count());

    const auto body = new QWidget;
    body->setContentsMargins({});
    const auto bodyLayout = new QHBoxLayout;
    bodyLayout->addWidget(m_sidebar);
    bodyLayout->addWidget(m_pageContent);
    bodyLayout->setContentsMargins({});
    bodyLayout->setSpacing(0);
    body->setLayout(bodyLayout);

    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    mainLayout->addWidget(body);

    connect(m_tabList, &QListWidget::currentRowChanged, this,
            &AppOptionsDialog::onSelectionChanged);
}

int AppOptionsDialog::showStandaloneDialog(const AppOptionsGlobal::Option option, QWidget *parent) {
    auto *dialog = new Dialog(parent);
    auto *panel = new AppOptionsDialog(dialog, /*standalone=*/true);
    panel->selectOption(option);

    dialog->setModal(true);
    // The Dialog base leaves a 12px inset around the body; the options layout
    // is flush by design (sidebar owns its background, pages carry their own
    // 16px content margins), so drop it here.
    dialog->body()->setContentsMargins({});
    const auto bodyLayout = new QHBoxLayout(dialog->body());
    bodyLayout->setContentsMargins({});
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(panel);

    dialog->resize(900, 600);
    const auto result = dialog->exec();
    dialog->deleteLater();
    return result;
}

void AppOptionsDialog::onSelectionChanged(const int index) {
    if (auto *page = ensurePage(index))
        m_pageContent->setCurrentWidget(page);
}

IOptionPage *AppOptionsDialog::ensurePage(const int index) {
    if (index < 0 || index >= m_pages.size())
        return nullptr;
    if (m_pages.at(index))
        return m_pages.at(index);

    IOptionPage *page = nullptr;
    switch (static_cast<AppOptionsGlobal::Option>(index + 1)) {
        case AppOptionsGlobal::General:
            page = new GeneralPage;
            break;
        case AppOptionsGlobal::Audio:
            page = new AudioPage;
            break;
        case AppOptionsGlobal::Midi:
            page = new MidiPage;
            break;
        case AppOptionsGlobal::Appearance:
            page = new AppearancePage;
            break;
        case AppOptionsGlobal::Inference:
            page = new InferencePage;
            break;
        case AppOptionsGlobal::DeveloperOptions:
            page = new DeveloperPage;
            break;
        default:
            return nullptr;
    }

    m_pages[index] = page;
    m_pageContent->addWidget(page);
    return page;
}

void AppOptionsDialog::selectOption(const AppOptionsGlobal::Option option) {
    const auto pageIndex = option >= 1 ? option - 1 : 0; // Skip enum "All"
    m_tabList->setCurrentRow(pageIndex);
}

void AppOptionsDialog::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void AppOptionsDialog::retranslateUi() {
    // In standalone mode the wrapping Dialog (the panel's window) owns the
    // title bar; the title must be set on it so DialogTitleBar picks it up via
    // WindowTitleChange (skipped when embedded).
    if (m_standalone)
        window()->setWindowTitle(tr("Options"));

    // 16px regular Fluent glyphs, one per sidebar page (index-aligned with
    // pageNames). Icons keep the default action palette so they follow the
    // current theme tokens like every other menu icon.
    // 20px regular Fluent glyphs, one per sidebar page (index-aligned with
    // pageNames). Larger than the 16px menu versions so they read better in
    // the 160px sidebar; colors still follow theme tokens.
    static const char *const pageIconPaths[] = {
        ":/svg/icons/settings_20_regular.svg",  // General
        ":/svg/icons/speaker_2_20_regular.svg", // Audio
        ":/svg/icons/midi_20_regular.svg",      // MIDI
        ":/svg/icons/color_20_regular.svg",     // Appearance
        ":/svg/icons/sparkle_20_regular.svg",   // Inference
        ":/svg/icons/code_20_regular.svg",      // Developer Options
    };

    const QStringList pageNames = {tr("General"),    tr("Audio"),     tr("MIDI"),
                                   tr("Appearance"), tr("Inference"), tr("Developer Options")};
    const QSignalBlocker blocker(m_tabList);
    const auto currentRow = m_tabList->currentRow();
    if (m_tabList->count() != pageNames.size()) {
        m_tabList->clear();
        for (int i = 0; i < pageNames.size(); ++i) {
            auto *item = new QListWidgetItem(
                IconUtils::createTintedSvgIcon(QLatin1String(pageIconPaths[i]), QSize(20, 20),
                                               IconUtils::defaultActionPalette()),
                pageNames.at(i));
            item->setData(Qt::UserRole, QLatin1String(pageIconPaths[i]));
            m_tabList->addItem(item);
        }
    } else {
        for (int i = 0; i < pageNames.size(); ++i) {
            m_tabList->item(i)->setText(pageNames.at(i));
            m_tabList->item(i)->setIcon(IconUtils::createTintedSvgIcon(
                QLatin1String(pageIconPaths[i]), QSize(20, 20), IconUtils::defaultActionPalette()));
            m_tabList->item(i)->setData(Qt::UserRole, QLatin1String(pageIconPaths[i]));
        }
    }
    m_tabList->setCurrentRow(currentRow);
}
