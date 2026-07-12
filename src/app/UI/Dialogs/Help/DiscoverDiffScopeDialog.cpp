#include "DiscoverDiffScopeDialog.h"

#include <QApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

#include "UI/Utils/ThemeManager.h"

DiscoverDiffScopeDialog::DiscoverDiffScopeDialog(QWidget *parent)
    : QDialog(parent),
      m_bannerLabel(new QLabel(this)),
      m_textLabel(new QLabel(this)),
      m_contentWidget(new QWidget(this)) {
    setObjectName("discover-diffscope-dialog");
    setWindowTitle(tr("Discover DiffScope"));
    resize(480, 360);

    setStyleSheet(QStringLiteral(R"(
        QDialog#discover-diffscope-dialog {
            background-color: #313235;
        }
        QWidget#discover-diffscope-content {
            background-color: #313235;
        }
        QLabel#discover-diffscope-text {
            background-color: transparent;
            color: #dadada;
            padding: 0;
        }
        QWidget#discover-diffscope-button-area {
            background-color: #232427;
        }
        QPushButton#discover-diffscope-github-button {
            background-color: #5566ff;
            border: 1px solid #4a4b4c;
            border-radius: 4px;
            color: #dadada;
            padding: 2px 4px;
        }
        QPushButton#discover-diffscope-github-button:hover {
            background-color: #6676ff;
        }
        QPushButton#discover-diffscope-github-button:pressed {
            background-color: #5566ff;
            color: rgba(218, 218, 218, 204);
        }
        QPushButton#discover-diffscope-github-button:focus {
            outline: none;
        }
    )"));

    m_bannerLabel->setObjectName("discover-diffscope-banner");
    m_bannerLabel->setPixmap(QPixmap(QStringLiteral(":/images/diffscope_banner.png")));
    m_bannerLabel->setScaledContents(true);
    m_bannerLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_textLabel->setObjectName("discover-diffscope-text");
    m_textLabel->setTextFormat(Qt::RichText);
    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    const auto textWidget = new QWidget(this);
    const auto textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(0);
    textLayout->addWidget(m_textLabel);
    textLayout->addStretch();

    m_contentWidget->setObjectName("discover-diffscope-content");
    const auto contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(12);
    contentLayout->addWidget(m_bannerLabel);
    contentLayout->addWidget(textWidget);

    const auto githubButton = new QPushButton(tr("View DiffScope on GitHub"), this);
    githubButton->setObjectName("discover-diffscope-github-button");
    githubButton->setCursor(Qt::PointingHandCursor);
    githubButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    githubButton->setMinimumWidth(64);
    githubButton->setFixedHeight(28);
    connect(githubButton, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/diffscope/diffscope-project")));
    });

    const auto buttonArea = new QWidget(this);
    buttonArea->setObjectName("discover-diffscope-button-area");
    buttonArea->setFixedHeight(52);
    const auto buttonLayout = new QHBoxLayout(buttonArea);
    buttonLayout->setContentsMargins(12, 12, 12, 12);
    buttonLayout->setSpacing(0);
    buttonLayout->addStretch();
    buttonLayout->addWidget(githubButton, 0, Qt::AlignVCenter);

    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_contentWidget);
    mainLayout->addWidget(buttonArea);

    updateBannerHeight();
    updateTextTitle();
    ThemeManager::instance()->addWindow(this);
}

DiscoverDiffScopeDialog::~DiscoverDiffScopeDialog() {
    ThemeManager::instance()->removeWindow(this);
}

DiscoverDiffScopeDialog::InfoType DiscoverDiffScopeDialog::infoType() const {
    return m_infoType;
}

void DiscoverDiffScopeDialog::setInfoType(InfoType infoType) {
    if (m_infoType == infoType)
        return;

    m_infoType = infoType;
    updateTextTitle();
}

void DiscoverDiffScopeDialog::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    updateBannerHeight();
}

void DiscoverDiffScopeDialog::updateBannerHeight() {
    const auto pixmap = m_bannerLabel->pixmap(Qt::ReturnByValue);
    if (pixmap.isNull())
        return;

    const auto bannerWidth = qMax(1, m_contentWidget->contentsRect().width() - 24);
    const auto bannerHeight = pixmap.height() * bannerWidth / qMax(1, pixmap.width());
    m_bannerLabel->setFixedWidth(bannerWidth);
    m_bannerLabel->setFixedHeight(bannerHeight);
}

void DiscoverDiffScopeDialog::updateTextTitle() {
    const auto title = m_infoType == InfoType::FeatureIntroduction
        ? tr("This Feature Is Planned for Introduction in DiffScope – The Next-Generation Editor Under Development")
        : tr("DiffScope – The Next-Generation Editor Under Development");
    m_textLabel->setText(tr(R"(
        <h2 style="color: #dadada; margin-top: 0;">%1</h2>
        <p style="color: #dadada;">
            DiffScope is an advanced DiffSinger editor designed for professional users, offering a
            more comprehensive feature set, a cleaner architecture, and powerful extensibility.
        </p>
        <p style="color: #dadada;">
            Compared to %2, DiffScope is better suited for complex projects and advanced
            customization needs, featuring a plugin system, customizable workflows, and a more
            refined editing experience.
        </p>
        <p style="color: #dadada;">
            The two editors are fully compatible: project files and voice libraries can be shared
            seamlessly between them.
        </p>
        <p style="color: #dadada;">
            DiffScope is currently still under development, and its features are continuously being
            refined. Like %2, DiffScope is free software driven by community
            contributions. We welcome you to contribute and join us in continuously improving this
            tool ecosystem.
        </p>
    )").arg(title).arg(QApplication::applicationName()));
}
