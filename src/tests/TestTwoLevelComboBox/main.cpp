#include "UI/Controls/TwoLevelComboBox.h"

#include <QApplication>
#include <QDebug>
#include <QVersionNumber>

namespace {

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        qCritical() << message;
        return false;
    }

}

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);

    TwoLevelComboBox comboBox;
    comboBox.setShowInheritItem(true);
    comboBox.setItems({});

    const SpeakerInfo speaker(QStringLiteral("speaker"), QStringLiteral("Speaker"));
    const SingerInfo singer(
        {QStringLiteral("singer"), QStringLiteral("package"), QVersionNumber(1, 0)},
        QStringLiteral("Singer"), {speaker});
    comboBox.addItem(QStringLiteral("Speaker"), singer, speaker);

    bool success = true;
    comboBox.setCurrentData(singer, speaker, true);
    success &= expect(comboBox.isInheritSelected(), "inherit item should be selected explicitly");

    comboBox.setCurrentData(singer, speaker, false);
    const auto dynamicMixText = QStringLiteral("Singer / Dynamic Mix");
    comboBox.setDisplayTextOverride(dynamicMixText);
    success &= expect(!comboBox.isInheritSelected(),
                      "concrete selection should leave the inherit item");
    success &= expect(comboBox.currentText() == dynamicMixText,
                      "dynamic mix text should not retain the Follow Track prefix");

    return success ? 0 : 1;
}
