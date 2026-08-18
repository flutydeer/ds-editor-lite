#include "UI/Controls/TwoLevelComboBox.h"

#include <QAction>
#include <QApplication>
#include <QTextStream>

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }
}

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    const SpeakerInfo speaker(QStringLiteral("internal_emb"),
                              QStringLiteral("Configured Emb Name"));
    const SingerInfo singer(SingerIdentifier{QStringLiteral("singer-id"),
                                             QStringLiteral("package-id"), QVersionNumber(1, 0)},
                            QStringLiteral("Configured Role Name"), {speaker});
    const PackageInfo package(QStringLiteral("package-id"), QVersionNumber(1, 0), {}, {}, {}, {},
                              {}, {}, {singer});

    TwoLevelComboBox comboBox;
    comboBox.setItems({package});

    const auto actions = comboBox.mainMenu()->actions();
    expect(actions.size() == 2, "a single-speaker singer must be a direct menu item");
    if (actions.size() == 2) {
        expect(actions.at(1)->text() == singer.name(),
               "a single-speaker menu item must use the configured singer name");
        expect(actions.at(1)->text() != speaker.id(),
               "a single-speaker menu item must not expose the internal emb id");
        actions.at(1)->trigger();
        expect(comboBox.currentSinger() == singer && comboBox.currentSpeaker() == speaker,
               "the direct menu item must retain its singer and speaker data");
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All SingerMenuDisplay tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
