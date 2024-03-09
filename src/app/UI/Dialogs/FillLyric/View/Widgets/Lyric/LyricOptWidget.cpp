#include "LyricOptWidget.h"

namespace FillLyric {
    LyricOptWidget::LyricOptWidget(QWidget *parent) : QWidget(parent) {
        // lyric option layout
        m_lyricOptLayout = new QVBoxLayout();
        m_lyricOptLayout->setContentsMargins(6, 0, 6, 0);
        btnInsertText = new Button("测试");
        btnToTable = new Button(">>");
        btnToText = new Button("<<");

        m_lyricOptLayout->addStretch(1);
        m_lyricOptLayout->addWidget(btnInsertText);
        m_lyricOptLayout->addWidget(btnToTable);
        m_lyricOptLayout->addWidget(btnToText);
        m_lyricOptLayout->addStretch(1);

        // bottom layout
        splitLabel = new QLabel("Split Mode :");
        splitComboBox = new ComboBox(true);
        splitComboBox->addItems({"Auto", "By Char", "Custom", "By Reg"});
        btnRegSetting = new Button("Setting");
        m_splitters = new LineEdit();
        m_splitters->setMaximumWidth(85);
        m_splitters->setToolTip("Custom delimiter, input with space intervals. If you want to use "
                                "spaces as separators, please check the checkbox above.");

        m_splitters->setVisible(false);
        btnRegSetting->setVisible(false);

        m_lyricOptLayout->addWidget(splitLabel);
        m_lyricOptLayout->addWidget(splitComboBox);
        m_lyricOptLayout->addWidget(btnRegSetting);
        m_lyricOptLayout->addWidget(m_splitters);
        m_lyricOptLayout->addStretch(1);

        this->setLayout(m_lyricOptLayout);
    }

    LyricOptWidget::~LyricOptWidget() = default;

} // FillLyric