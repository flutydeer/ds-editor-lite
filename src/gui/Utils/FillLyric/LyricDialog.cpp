#include "LyricDialog.h"

namespace FillLyric {

    LyricDialog::LyricDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Fill Lyric");
        setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
        setWindowModality(Qt::WindowModal);
        setBaseSize(600, 800);
        m_phonicWidget = new PhonicWidget(this);
        setLayout(new QVBoxLayout);
        layout()->addWidget(m_phonicWidget);
    }

    LyricDialog::~LyricDialog() = default;
} // FillLyric