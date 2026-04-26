#include "Modules/FillLyric/Widgets/LyricExtWidget.h"

#include <QFile>
#include <utility>

namespace FillLyric
{
    LyricExtWidget::LyricExtWidget(int *notesCount, const LyricTabConfig &config,
                                   const std::vector<std::string> &priorityG2pIds,
                                   QMap<std::string, std::string> langToG2pId, QWidget *parent) :
        QWidget(parent), m_notesCount(notesCount), m_priorityG2pIds(priorityG2pIds),
        m_langToG2pId(std::move(langToG2pId)) {
        this->setContentsMargins(0, 0, 0, 0);

        QStringList priorityG2pIdList;
        for (const auto &id : m_priorityG2pIds)
            priorityG2pIdList.append(QString::fromStdString(id));

        m_wrapView = new LyricWrapView(":/theme/lyricwarpview-dark.qss", priorityG2pIdList,
                                       m_langToG2pId);
        m_wrapView->setContentsMargins(0, 0, 0, 0);

        m_tableTopLayout = new QHBoxLayout();
        m_tableTopLayout->setContentsMargins(0, 0, 0, 0);
        m_btnFoldLeft = new QPushButton(tr("Fold Left"));

        m_btnInsertText = new QPushButton(tr("Test"));
        m_tableTopLayout->addWidget(m_btnFoldLeft);
        m_tableTopLayout->addWidget(m_btnInsertText);
        m_tableTopLayout->addStretch(1);

        m_tableCountLayout = new QHBoxLayout();
        m_noteCountLabel = new QLabel("0/0");

        m_tableCountLayout->addStretch(1);
        m_tableCountLayout->addWidget(m_noteCountLabel);

        m_tableLayout = new QVBoxLayout();
        m_tableLayout->setContentsMargins(0, 0, 0, 0);
        m_tableLayout->addLayout(m_tableTopLayout);
        m_tableLayout->addWidget(m_wrapView);
        m_tableLayout->addLayout(m_tableCountLayout);

        m_mainLayout = new QHBoxLayout();
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->addLayout(m_tableLayout);
        this->setLayout(m_mainLayout);

        QFont font = m_wrapView->font();
        font.setPointSizeF(std::max(9.0, config.lyricExtFontSize));
        m_wrapView->setFont(font);

        connect(m_wrapView, &LyricWrapView::noteCountChanged, this, &LyricExtWidget::onNotesCountChanged);
        connect(m_wrapView, &LyricWrapView::fontSizeChanged, this, &LyricExtWidget::modifyOption);
        connect(m_btnFoldLeft, &QAbstractButton::clicked, this, &LyricExtWidget::foldLeftRequested);
        connect(m_btnInsertText, &QAbstractButton::clicked, this, &LyricExtWidget::insertTextRequested);
    }

    LyricExtWidget::~LyricExtWidget() = default;

    LyricWrapView *LyricExtWidget::wrapView() const { return m_wrapView; }

    double LyricExtWidget::fontSize() const { return m_wrapView->font().pointSizeF(); }

    void LyricExtWidget::setFoldLeftText(const QString &text) { m_btnFoldLeft->setText(text); }

    void LyricExtWidget::onNotesCountChanged(const int &count) const {
        m_noteCountLabel->setText(QString::number(count) + "/" + QString::number(*m_notesCount));
    }

} // namespace FillLyric
