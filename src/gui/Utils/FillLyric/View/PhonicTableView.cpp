#include "PhonicTableView.h"

namespace FillLyric {

    PhonicTableView::PhonicTableView(QWidget *parent) : QTableView(parent) {
        // 隐藏行头
        QFont font = this->font();
        font.setPointSize(12);
        this->setFont(font);

        // 打开右键菜单
        this->setContextMenuPolicy(Qt::CustomContextMenu);
    }

    PhonicTableView::~PhonicTableView() = default;

} // FillLyric