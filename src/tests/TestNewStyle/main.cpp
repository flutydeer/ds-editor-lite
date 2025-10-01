//
// Created by fluty on 24-6-2.
//

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QStyleFactory>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLineEdit>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setPointSize(11);
    QApplication::setFont(f);

    auto btnOk = new QPushButton("确定");
    btnOk->setObjectName("btnOk");

    auto btnCancel = new QPushButton("取消");
    btnCancel->setObjectName("btnCancel");

    auto lineEdit = new QLineEdit;
    lineEdit->setPlaceholderText("搜索...");

    auto lineEditShadowEffect = new QGraphicsDropShadowEffect;
    lineEditShadowEffect->setBlurRadius(24);
    lineEditShadowEffect->setColor(QColor(6, 111, 255, 25));
    lineEditShadowEffect->setOffset(0, 4);
    lineEdit->setGraphicsEffect(lineEditShadowEffect);

    auto btnOkShadowEffect = new QGraphicsDropShadowEffect;
    btnOkShadowEffect->setBlurRadius(24);
    btnOkShadowEffect->setColor(QColor(6, 111, 255, 55));
    btnOkShadowEffect->setOffset(0, 4);
    btnOk->setGraphicsEffect(btnOkShadowEffect);

    auto btnCancelShadowEffect = new QGraphicsDropShadowEffect;
    btnCancelShadowEffect->setBlurRadius(24);
    btnCancelShadowEffect->setColor(QColor(6, 111, 255, 25));
    btnCancelShadowEffect->setOffset(0, 4);
    btnCancel->setGraphicsEffect(btnCancelShadowEffect);

    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(32, 32, 32, 32);
    mainLayout->setSpacing(16);
    mainLayout->addWidget(lineEdit);
    mainLayout->addWidget(btnOk);
    mainLayout->addWidget(btnCancel);

    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    auto styleSheet =
        QString("QMainWindow { background: #F0F8FD }"
                "QPushButton#btnOk { background: #066FFF; border: none; "
                "border-radius: 6px; color: #FFFFFF; padding: 6px 18px; }"
                "QPushButton#btnCancel { background: #CFFFFFFF; border: 1px solid #FFFFFF; "
                "border-radius: 6px; color: #066FFF; padding: 6px 18px; }"
                "QLineEdit { background: #BFFFFFFF; border: 1px solid #FFFFFF; "
                "border-bottom: 2px solid #066FFF;"
                "border-radius: 6px; color: #031546; padding: 4px 8px }"
                "QMenu { padding: 4px; background-color: #F7FAFF; border: 1px solid #20000000; "
                "border-radius: 6px; color: #031546; }"
                "QMenu::item { background: transparent; color: #031546; padding: 5px 20px; "
                "min-width: 96px }"
                "QMenu::item:selected { background-color: #D5E8FF; border-style: none; "
                "border-radius: 4px; color: #066FFF}");
    QMainWindow w;
    w.setCentralWidget(mainWidget);
    w.setStyleSheet(styleSheet);
    w.resize(500, 150);
    w.show();

    return QApplication::exec();
}