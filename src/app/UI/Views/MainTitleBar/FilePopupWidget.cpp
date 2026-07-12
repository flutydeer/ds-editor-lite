//
// Created by FlutyDeer on 2026/7/13.
//

#include "FilePopupWidget.h"

#include "Controller/AppController.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/Menu.h"
#include "UI/Utils/IconUtils.h"
#include "Utils/SystemUtils.h"

#include <QMCore/qmsystem.h>

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <dwmapi.h>
#endif

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QCoreApplication>
#include <QDir>
#include <QEnterEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace {

    constexpr int kPopupWidth = 360;
    constexpr int kItemHeight = 46;

    QColor normalColor() {
        return QColor(0xC8, 0xC9, 0xCC);
    }

    QString elidePath(const QString &path, int width, const QFont &font) {
        return QFontMetrics(font).elidedText(path, Qt::ElideMiddle, width);
    }

    QString normalizedProjectPath(const QString &path) {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    bool projectPathsEqual(const QString &lhs, const QString &rhs) {
#ifdef Q_OS_WIN
        return QString::compare(lhs, rhs, Qt::CaseInsensitive) == 0;
#else
        return lhs == rhs;
#endif
    }

    class RecentFileMoreButton : public QToolButton {
    public:
        explicit RecentFileMoreButton(QWidget *parent = nullptr) : QToolButton(parent) {
            setProperty("hovered", false);
        }

        void syncHoveredWithCursor() {
            setHovered(isVisible() && rect().contains(mapFromGlobal(QCursor::pos())));
        }

    protected:
        void enterEvent(QEnterEvent *event) override {
            QToolButton::enterEvent(event);
            setHovered(true);
        }

        void leaveEvent(QEvent *event) override {
            QToolButton::leaveEvent(event);
            setHovered(false);
        }

    private:
        void setHovered(bool hovered) {
            if (property("hovered").toBool() == hovered)
                return;
            setProperty("hovered", hovered);
            style()->unpolish(this);
            style()->polish(this);
            update();
        }
    };

    class RecentFileItemWidget : public QFrame {
    public:
        RecentFileItemWidget(QString filePath, bool isCurrent, QWidget *parent = nullptr)
            : QFrame(parent), m_filePath(std::move(filePath)), m_isCurrent(isCurrent) {
            setObjectName("filePopupRecentItem");
            setProperty("current", m_isCurrent);
            setProperty("hovered", false);
            setAttribute(Qt::WA_StyledBackground);
            setFixedHeight(kItemHeight);

            auto *itemLayout = new QHBoxLayout;
            itemLayout->setContentsMargins(8, 4, 6, 4);
            itemLayout->setSpacing(6);
            itemLayout->setAlignment(Qt::AlignVCenter);

            // TODO: 替换为工程文件图标后恢复左侧图标槽。

            auto *infoFrame = new QFrame;
            infoFrame->setAttribute(Qt::WA_TransparentForMouseEvents);
            infoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            auto *infoLayout = new QVBoxLayout;
            infoLayout->setContentsMargins(0, 0, 0, 0);
            infoLayout->setSpacing(0);

            m_nameLabel = new QLabel;
            m_nameLabel->setObjectName("filePopupRecentName");
            m_nameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_nameLabel->setToolTip(m_filePath);
            infoLayout->addWidget(m_nameLabel);

            m_pathLabel = new QLabel;
            m_pathLabel->setObjectName("filePopupRecentPath");
            m_pathLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            m_pathLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_pathLabel->setToolTip(m_filePath);
            infoLayout->addWidget(m_pathLabel);

            infoFrame->setLayout(infoLayout);
            itemLayout->addWidget(infoFrame, 1);

            m_btnMore = new RecentFileMoreButton;
            m_btnMore->setObjectName("filePopupMoreButton");
            m_btnMore->setFixedSize(28, 28);
            m_btnMore->setIcon(IconUtils::createTintedSvgIcon(
                QStringLiteral(":/svg/icons/more_vertical_16_regular.svg"), QSize(16, 16),
                normalColor()));
            m_btnMore->setIconSize(QSize(16, 16));
            m_btnMore->setToolButtonStyle(Qt::ToolButtonIconOnly);
            m_btnMore->setAutoRaise(true);
            m_btnMore->setAttribute(Qt::WA_NoMousePropagation);
            connect(m_btnMore, &QToolButton::clicked, this, [this] {
                auto *menu = new Menu(this);
                auto *actionReveal = menu->addAction(
                    IconUtils::menuIcon(QStringLiteral(":/svg/icons/folder_open_16_regular.svg")),
                    QCoreApplication::translate("FilePopupWidget", "Show in Folder"));
                connect(actionReveal, &QAction::triggered, this, [this] {
                    QM::reveal(QFileInfo(m_filePath).absoluteFilePath());
                });
                menu->addSeparator();
                auto *actionRemove = menu->addAction(
                    IconUtils::menuIcon(QStringLiteral(":/svg/icons/subtract_16_regular.svg")),
                    QCoreApplication::translate("FilePopupWidget", "Remove"));
                actionRemove->setEnabled(!m_isCurrent);
                connect(actionRemove, &QAction::triggered, this,
                        [this] {
                            if (m_removeHandler)
                                m_removeHandler(m_filePath);
                        });
                connect(menu, &Menu::aboutToHide, this, [this] {
                    QTimer::singleShot(0, this, [this] { syncHoveredWithCursor(); });
                });
                menu->setAttribute(Qt::WA_DeleteOnClose);
                menu->popup(m_btnMore->mapToGlobal(QPoint(0, m_btnMore->height())));
            });
            itemLayout->addWidget(m_btnMore);

            setLayout(itemLayout);
            updateTexts();
        }

        void setCurrent(bool current) {
            if (m_isCurrent == current)
                return;
            m_isCurrent = current;
            setProperty("current", m_isCurrent);
            update();
            updateTexts();
        }

        void setOpenHandler(std::function<void()> handler) {
            m_openHandler = std::move(handler);
        }

        void setRemoveHandler(std::function<void(const QString &)> handler) {
            m_removeHandler = std::move(handler);
        }

        void syncHoveredWithCursor() {
            setHovered(isVisible() && rect().contains(mapFromGlobal(QCursor::pos())));
            m_btnMore->syncHoveredWithCursor();
        }

        void setHovered(bool hovered) {
            if (property("hovered").toBool() == hovered)
                return;
            setProperty("hovered", hovered);
            style()->unpolish(this);
            style()->polish(this);
            update();
        }

    protected:
        void enterEvent(QEnterEvent *event) override {
            QFrame::enterEvent(event);
            setHovered(true);
        }

        void leaveEvent(QEvent *event) override {
            QFrame::leaveEvent(event);
            setHovered(false);
        }

        void mouseReleaseEvent(QMouseEvent *event) override {
            if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
                if (m_openHandler)
                    m_openHandler();
                event->accept();
                return;
            }
            QFrame::mouseReleaseEvent(event);
        }

        void resizeEvent(QResizeEvent *event) override {
            QFrame::resizeEvent(event);
            updateTexts();
        }

    private:
        void updateTexts() {
            const QFileInfo fileInfo(m_filePath);
            m_nameLabel->setText(fileInfo.completeBaseName());
            const int textWidth =
                qMax(0, m_pathLabel->width() > 0 ? m_pathLabel->width() : width() - 32);
            m_pathLabel->setText(elidePath(m_filePath, textWidth, m_pathLabel->font()));
        }

        QString m_filePath;
        bool m_isCurrent = false;
        QLabel *m_nameLabel = nullptr;
        QLabel *m_pathLabel = nullptr;
        RecentFileMoreButton *m_btnMore = nullptr;
        std::function<void()> m_openHandler;
        std::function<void(const QString &)> m_removeHandler;
    };

}

FilePopupWidget::FilePopupWidget(QWidget *parent) : QFrame(parent) {
    setObjectName("filePopupWidget");
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_WindowPropagation);
    setProperty("dwmBorder", false);
    setFixedWidth(kPopupWidth);

#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11()) {
        setProperty("dwmBorder", true);
    }
#endif

    auto *outerLayout = new QVBoxLayout;
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_surface = new QFrame;
    m_surface->setObjectName("filePopupSurface");
    m_surface->setAttribute(Qt::WA_StyledBackground);

    auto *containerLayout = new QVBoxLayout;
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto *buttonSection = new QFrame;
    buttonSection->setObjectName("filePopupButtonSection");
    auto *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(12, 12, 12, 12);
    btnLayout->setSpacing(6);

    auto *btnNew = new Button(tr("New"));
    btnNew->setObjectName("filePopupActionButton");
    btnNew->setProperty("filePopupAction", true);
    btnNew->setIcon(IconUtils::createTintedSvgIcon(
        QStringLiteral(":/svg/icons/document_add_16_regular.svg"), QSize(16, 16),
        IconUtils::defaultActionPalette()));
    btnNew->setToolTip(tr("New"));
    connect(btnNew, &Button::clicked, this, [this] {
        close();
        emit newProjectClicked();
    });

    auto *btnOpen = new Button(tr("Open..."));
    btnOpen->setObjectName("filePopupActionButton");
    btnOpen->setProperty("filePopupAction", true);
    btnOpen->setIcon(IconUtils::createTintedSvgIcon(
        QStringLiteral(":/svg/icons/folder_open_16_regular.svg"), QSize(16, 16),
        IconUtils::defaultActionPalette()));
    btnOpen->setToolTip(tr("Open..."));
    connect(btnOpen, &Button::clicked, this, [this] {
        close();
        emit openProjectClicked();
    });

    btnLayout->addWidget(btnNew);
    btnLayout->addWidget(btnOpen);
    buttonSection->setLayout(btnLayout);
    containerLayout->addWidget(buttonSection);

    auto *recentSection = new QFrame;
    recentSection->setObjectName("filePopupRecentSection");
    auto *recentLayout = new QVBoxLayout;
    recentLayout->setContentsMargins(6, 0, 6, 6);
    recentLayout->setSpacing(0);

    auto *lbRecentTitle = new QLabel(tr("Recent Projects"));
    lbRecentTitle->setObjectName("filePopupRecentTitle");
    lbRecentTitle->setContentsMargins(6, 0, 6, 6);
    recentLayout->addWidget(lbRecentTitle);

    m_listLayout = new QVBoxLayout;
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(0);
    recentLayout->addLayout(m_listLayout);

    m_lbEmpty = new QLabel(tr("(No Recent Projects)"));
    m_lbEmpty->setObjectName("filePopupEmptyLabel");
    m_lbEmpty->setAlignment(Qt::AlignCenter);
    m_lbEmpty->setFixedHeight(kItemHeight);
    m_lbEmpty->setVisible(false);
    recentLayout->addWidget(m_lbEmpty);

    recentSection->setLayout(recentLayout);
    containerLayout->addWidget(recentSection);

    m_surface->setLayout(containerLayout);
    outerLayout->addWidget(m_surface);
    setLayout(outerLayout);

    refreshRecentFiles();
    connect(appController, &AppController::recentProjectFilesChanged, this,
            &FilePopupWidget::refreshRecentFiles);
}

void FilePopupWidget::showAt(const QPoint &globalPos) {
    refreshRecentFiles();
    clearRecentItemHoverState();
    syncPopupGeometry();

    QPoint topLeft = globalPos;
    if (const auto screen = QApplication::screenAt(globalPos)) {
        const auto screenRect = screen->availableGeometry();
        const QRect popupRect(topLeft, size());
        if (popupRect.right() > screenRect.right())
            topLeft.setX(screenRect.right() - width());
        if (popupRect.left() < screenRect.left())
            topLeft.setX(screenRect.left());
        if (popupRect.bottom() > screenRect.bottom())
            topLeft.setY(screenRect.bottom() - height());
        if (popupRect.top() < screenRect.top())
            topLeft.setY(screenRect.top());
    }

    setGeometry(QRect(topLeft, size()));
    show();
    raise();
    applyWindowEffects();
}

void FilePopupWidget::hideEvent(QHideEvent *event) {
    clearRecentItemHoverState();
    QFrame::hideEvent(event);
}

void FilePopupWidget::refreshRecentFiles() {
    QLayoutItem *item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    m_recentItems.clear();

    const auto recentFiles = appController->recentProjectFiles();
    const auto currentPath = appController->projectPath();

    if (recentFiles.isEmpty()) {
        m_lbEmpty->setVisible(true);
    } else {
        m_lbEmpty->setVisible(false);

        constexpr int maxItems = 5;
        const auto count = std::min(recentFiles.size(), qsizetype{maxItems});
        for (qsizetype i = 0; i < count; ++i) {
            const auto &filePath = recentFiles.at(i);
            const bool isCurrent =
                !currentPath.isEmpty() &&
                projectPathsEqual(normalizedProjectPath(filePath),
                                  normalizedProjectPath(currentPath));
            auto *itemWidget = createRecentFileItem(filePath, isCurrent);
            m_recentItems.append(itemWidget);
            m_listLayout->addWidget(itemWidget);
        }
    }

    syncPopupGeometry();
}

QWidget *FilePopupWidget::createRecentFileItem(const QString &filePath, bool isCurrent) {
    auto *itemWidget = new RecentFileItemWidget(filePath, isCurrent);
    itemWidget->setOpenHandler([this, filePath] {
        close();
        emit openRecentProject(filePath);
    });
    itemWidget->setRemoveHandler([this](const QString &filePath) {
        appController->removeRecentProjectFile(filePath);
    });
    return itemWidget;
}

void FilePopupWidget::clearRecentItemHoverState() {
    for (auto *item : std::as_const(m_recentItems)) {
        if (!item)
            continue;
        static_cast<RecentFileItemWidget *>(item)->setHovered(false);
    }
}

void FilePopupWidget::syncPopupGeometry() {
    ensurePolished();
    m_surface->ensurePolished();
    if (m_surface->layout())
        m_surface->layout()->activate();
    if (layout())
        layout()->activate();

    const int height = m_surface->layout() ? m_surface->layout()->sizeHint().height()
                                           : m_surface->sizeHint().height();
    const QSize targetSize(kPopupWidth, height);
    setFixedSize(targetSize);
    m_surface->setFixedSize(targetSize);
    m_surface->setGeometry(QRect(QPoint(0, 0), targetSize));
}

void FilePopupWidget::applyWindowEffects() {
#ifdef Q_OS_WIN
    if (!SystemUtils::isWindows11())
        return;

    if (!winId())
        return;

    HWND hwnd = reinterpret_cast<HWND>(winId());
    constexpr int DWMWA_USE_IMMERSIVE_DARK_MODE_ = 20;
    constexpr int DWMWA_WINDOW_CORNER_PREFERENCE_ = 33;
    constexpr int DWMWA_NCRENDERING_POLICY_ = 2;
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
    INT dwcp = 2;
    UINT dark = 1;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY_, &ncrp, sizeof(ncrp));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE_, &dwcp, sizeof(dwcp));
#endif
}
