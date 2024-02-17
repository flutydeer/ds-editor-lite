#include "LyricDialog.h"

namespace FillLyric {

    LyricDialog::LyricDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Fill Lyric");
        setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
        setWindowModality(Qt::WindowModal);
        setBaseSize(800, 600);
        m_phonicWidget = new PhonicWidget(this);
        setLayout(new QVBoxLayout);
        layout()->addWidget(m_phonicWidget);

        m_btnOk = new QPushButton("OK", this);
        m_btnCancel = new QPushButton("Cancel", this);

        m_btnOk->setMaximumWidth(100);
        m_btnCancel->setMaximumWidth(100);

        auto buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch(10);
        buttonLayout->addWidget(m_btnOk);
        buttonLayout->addStretch(1);
        buttonLayout->addWidget(m_btnCancel);
        layout()->addItem(buttonLayout);

        connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
        connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    LyricDialog::~LyricDialog() = default;
} // FillLyric