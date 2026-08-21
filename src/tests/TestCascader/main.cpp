#include "Cascader.h"

#include <QLabel>
#include <QList>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QFile>

#include <memory>

namespace {

    // A small demonstration option tree.
    QList<CascaderNode> buildDemoOptions() {
        QList<CascaderNode> regions;
        // Region -> Province -> City
        const auto makeCity = [](const QString &name, const QString &cityValue) {
            CascaderNode city;
            city.value = cityValue;
            city.label = name;
            return city;
        };
        const auto makeProvince = [&](const QString &name, const QString &provinceValue,
                                      const QList<CascaderNode> &cities) {
            CascaderNode province;
            province.value = provinceValue;
            province.label = name;
            province.children = cities;
            return province;
        };

        // East China
        CascaderNode east;
        east.value = QStringLiteral("east");
        east.label = QStringLiteral("华东");
        east.children = {
            makeProvince(QStringLiteral("江苏省"), QStringLiteral("js"),
                         {makeCity(QStringLiteral("南京"), QStringLiteral("nj")),
                          makeCity(QStringLiteral("苏州"), QStringLiteral("sz")),
                          makeCity(QStringLiteral("无锡"), QStringLiteral("wx"))}),
            makeProvince(QStringLiteral("浙江省"), QStringLiteral("zj"),
                         {makeCity(QStringLiteral("杭州"), QStringLiteral("hz")),
                          makeCity(QStringLiteral("宁波"), QStringLiteral("nb")),
                          makeCity(QStringLiteral("温州"), QStringLiteral("wz"))}),
            makeProvince(QStringLiteral("上海市"), QStringLiteral("sh"),
                         {makeCity(QStringLiteral("上海市"), QStringLiteral("sh")),
                          makeCity(QStringLiteral("浦东新区"), QStringLiteral("pd"))}),
        };

        // South China
        CascaderNode south;
        south.value = QStringLiteral("south");
        south.label = QStringLiteral("华南");
        south.children = {
            makeProvince(QStringLiteral("广东省"), QStringLiteral("gd"),
                         {makeCity(QStringLiteral("广州"), QStringLiteral("gz")),
                          makeCity(QStringLiteral("深圳"), QStringLiteral("sg"))}),
            makeProvince(QStringLiteral("海南省"), QStringLiteral("hn"),
                         {makeCity(QStringLiteral("海口"), QStringLiteral("hk"))}),
        };

        regions = {east, south};
        return regions;
    }

}

int main(int argc, char *argv[]) {
    // Always record argv first (plain C-style write, in case QApplication
    // construction blocks before we reach the Qt-owned logging).
    {
        QFile argLog(QStringLiteral("argv-log.txt"));
        if (argLog.open(QIODevice::WriteOnly)) {
            for (int i = 0; i < argc; ++i)
                argLog.write(QByteArray("[" + QByteArray::number(i) + "]=" + argv[i] + "\n"));
        }
    }

    QApplication app(argc, argv);

    // Headless logic self-test: no event loop, exits non-zero on failure.
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--selftest")) {
        {
            QFile marker(QStringLiteral("selftest-entered.txt"));
            if (marker.open(QIODevice::WriteOnly))
                marker.write(QByteArray::number(argc) + "/" + argv[1]);
        }
        qDebug() << "[selftest] entered, argc =" << argc << "argv[1] =" << argv[1];
        bool ok = true;
        qDebug() << "[selftest] constructing Cascader...";
        auto cascade2 = std::make_unique<Cascader>();
        qDebug() << "[selftest] constructed";
        cascade2->setOptions(buildDemoOptions());
        qDebug() << "[selftest] options set";
        ok &= cascade2->currentValue().isEmpty();
        qDebug() << "[selftest] initial empty check ok =" << ok;

        // Select a leaf via value path.
        cascade2->setCurrentValue(
            {QStringLiteral("east"), QStringLiteral("zj"), QStringLiteral("hz")});
        ok &= cascade2->currentText() == QStringLiteral("华东 / 浙江省 / 杭州");

        bool signalFired = false;
        QObject::connect(cascade2.get(), &Cascader::currentValueChanged,
                         [&signalFired] { signalFired = true; });
        cascade2->setCurrentValue(
            {QStringLiteral("east"), QStringLiteral("gd"), QStringLiteral("gz")});
        ok &= signalFired && cascade2->currentText() == QStringLiteral("华南 / 广东省 / 广州");

        // Invalid path clears the selection.
        QList<QVariant> emitted;
        QObject::connect(cascade2.get(), &Cascader::currentValueChanged,
                         [&emitted](const QList<QVariant> &value) { emitted = value; });
        cascade2->setCurrentValue({QStringLiteral("nope")});
        ok &= emitted.isEmpty() ? (cascade2->currentValue().isEmpty()) : false;

        if (!ok) {
            qCritical() << "Cascader self-test failed";
            return 1;
        }
        qDebug() << "Cascader self-test passed";
        return 0;
    }

    // A minimal demo stylesheet approximating Element Plus's light appearance so
    // the demo is readable standalone (the real integration will reuse theme QSS).
    app.setStyleSheet(QStringLiteral(R"(
            #cascaderTrigger {
                background: #ffffff;
                border: 1px solid #dcdfe6;
                border-radius: 4px;
            }
            #cascaderTrigger:hover { border-color: #c0c4cc; }
            #cascaderTrigger QToolTip { border: 1px solid #e4e7ed; background: #ffffff; }
            #cascaderPopup { background: transparent; }
            #cascaderPanelColumn {
                background: #ffffff;
                border: 1px solid #e4e7ed;
                border-right: 1px solid #e4e7ed;
                outline: none;
            }
            #cascaderPanelColumn::item {
                padding: 0 20px;
                color: #303133;
            }
            #cascaderPanelColumn::item:hover {
                background: #f5f7fa;
            }
            #cascaderPanelColumn::item:selected {
                color: #409eff;
                background: #ecf5ff;
            }
        )"));

    auto *window = new QFrame;
    window->setWindowTitle(QStringLiteral("Cascader Demo (Element Plus style)"));
    window->resize(420, 260);

    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *label = new QLabel(QStringLiteral("区域选择(Region)"));
    layout->addWidget(label);

    auto *cascader = new Cascader(window);
    cascader->setMinimumHeight(32);
    layout->addWidget(cascader);

    auto *resultLabel = new QLabel(window);
    resultLabel->setWordWrap(true);
    layout->addWidget(resultLabel);
    layout->addStretch();

    cascader->setOptions(buildDemoOptions());
    cascader->setPlaceholderText(QStringLiteral("请选择区域(Select a region)"));

    QObject::connect(cascader, &Cascader::currentValueChanged, [cascader, resultLabel] {
        resultLabel->setText(QStringLiteral("当前选择(Current): %1").arg(cascader->currentText()));
    });

    window->show();
    return QApplication::exec();
}
