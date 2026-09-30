#include "UI/Controls/TwoLevelComboBox.h"
#include "UI/Controls/SingerMenuUnavailablePackages.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

namespace {
    int g_failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }

    /// The tail of a singer menu once a refused package is listed: the disabled row, a separator,
    /// then the entry that opens the package manager.
    void checkMenuTail(const QList<QAction *> &tail, const QString &reason, int &managerOpened) {
        if (tail.size() != 4) {
            expect(false, "the menu tail must hold exactly one row per reportable failure");
            return;
        }
        const auto *row = tail.at(1);
        expect(!row->isEnabled(), "a package that would not open cannot be picked");
        expect(row->text() == QStringLiteral("⚠ 0913_wolf_club@1.0.0 — Unable to load"),
               "the refused package is titled after the directory that holds it");
        expect(row->toolTip() == reason, "the row tooltip is the loader's reason, verbatim");
        expect(tail.at(2)->isSeparator(), "the manager entry is set apart from the selection");
        const auto *manage = tail.at(3);
        expect(manage->isEnabled() && manage->text() == QStringLiteral("Manage voicebanks..."),
               "the tail ends with an entry that opens the package manager");
        tail.at(3)->trigger();
        expect(managerOpened == 1, "the manager entry opens the package manager");
    }

    /// The directory that holds the sources, found from wherever this test was started. A check
    /// that reads nothing is worse than no check, so a caller that gets an empty result fails.
    QString sourceRoot() {
        const QString marker = QStringLiteral("src/app/UI/Controls/TwoLevelComboBox.cpp");
        const QStringList starts{QCoreApplication::applicationDirPath(), QDir::currentPath()};
        for (const auto &start : starts) {
            QDir dir(start);
            for (int level = 0; level < 8; ++level) {
                if (dir.exists(marker))
                    return dir.absolutePath();
                if (!dir.cdUp())
                    break;
            }
        }
        return {};
    }

    /// The singer views cannot be built in this test -- their constructors read the running
    /// application through the same globals every other view uses -- so the order that broke is
    /// checked where it lives: in each view source, the tail wiring must come before the first
    /// setItems call. A view built once the package scan has finished is filled by that one call
    /// and is never populated again; wiring that arrives after it leaves the tail unwritten.
    void expectTailWiredBeforeFirstSetItems(const char *relativePath) {
        const auto root = sourceRoot();
        expect(!root.isEmpty(), "the source tree must be reachable, or the order check is vacuous");
        if (root.isEmpty())
            return;

        QFile file(QDir(root).filePath(QString::fromLatin1(relativePath)));
        expect(file.open(QIODevice::ReadOnly | QIODevice::Text),
               "a singer view source must be readable by the order check");
        if (!file.isOpen())
            return;

        const auto source = QString::fromUtf8(file.readAll());
        const auto firstSetItems = source.indexOf(QStringLiteral("setItems("));
        const auto tailWiring =
            source.indexOf(QStringLiteral("SingerMenuUnavailablePackages::append("));
        const auto message =
            QStringLiteral("%1: the tail wiring must precede the first setItems, otherwise a view "
                           "built after the package scan has finished never gets the tail")
                .arg(QString::fromLatin1(relativePath));
        expect(firstSetItems >= 0 && tailWiring >= 0 && tailWiring < firstSetItems,
               qPrintable(message));
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

    // A package that would not open is still listed, with the reason the loader gave, so that
    // someone who installed a voicebank and cannot use it is told why. It offers no singer, and
    // the reason it carries is the loader's own text rather than anything stated here.
    const auto reason = QStringLiteral("failed to resolve dependency of 0913_wolf_club: no installed "
                                       "Package satisfies dependency wolf/lang-ja");
    const PackageInfo unavailable =
        PackageInfo::unavailable(QStringLiteral("D:/voicebanks/0913_wolf_club@1.0.0"), reason);
    expect(unavailable.id() == QStringLiteral("0913_wolf_club@1.0.0"),
           "an unavailable entry is titled after the directory that holds it");
    expect(unavailable.path() == QStringLiteral("D:/voicebanks/0913_wolf_club@1.0.0"),
           "an unavailable entry keeps the path it was found at");
    expect(unavailable.unavailableReason() == reason,
           "an unavailable entry carries the loader's reason unchanged");
    expect(unavailable.isUnavailable() && !unavailable.isEmpty(),
           "an unavailable entry is an entry, not an empty one");
    expect(package.unavailableReason().isEmpty() && !package.isUnavailable(),
           "a package that loaded carries no reason");
    expect(unavailable != package, "an unavailable entry is not equal to a loaded one");

    TwoLevelComboBox unavailableBox;
    unavailableBox.setItems({unavailable});
    expect(unavailableBox.mainMenu()->actions().size() == 1,
           "a package that would not open still offers no singer of its own");

    // Where the singer menus do show it: one disabled row per refused package, carrying the
    // loader's reason as its tooltip, followed by the entry that opens the package manager. A
    // refusal with no reason is not listed at all, and the reason is never reworded.
    int managerOpened = 0;
    SingerMenuUnavailablePackages::append(
        unavailableBox.mainMenu(),
        {GetInstalledPackagesResult::FailedPackage(unavailable.path(), reason),
         GetInstalledPackagesResult::FailedPackage(QStringLiteral("D:/voicebanks/wordless"), {})},
        [&managerOpened] { ++managerOpened; });

    const auto tail = unavailableBox.mainMenu()->actions();
    expect(tail.size() == 4,
           "the menu tail lists the refused package and ends with the package manager entry");
    checkMenuTail(tail, reason, managerOpened);
    expect(unavailableBox.mainMenu()->toolTipsVisible(),
           "menu tooltips must be on, or the reason cannot be read");

    // The views do not fill the menu themselves: they wire the tail to itemsPopulated and let
    // setItems populate. A view built after the package scan is Ready is filled by the single
    // setItems in its constructor and is never populated again, so a tail wired up after that
    // call is never written at all. Replay the order the views must use: connect, then setItems.
    TwoLevelComboBox orderedBox;
    int orderedRefreshes = 0;
    int orderedManagerOpened = 0;
    const QList<GetInstalledPackagesResult::FailedPackage> orderedFailures{
        GetInstalledPackagesResult::FailedPackage(unavailable.path(), reason)};
    QObject::connect(&orderedBox, &TwoLevelComboBox::itemsPopulated, &orderedBox,
                     [&orderedRefreshes] { ++orderedRefreshes; });
    QObject::connect(&orderedBox, &TwoLevelComboBox::itemsPopulated, &orderedBox, [&] {
        SingerMenuUnavailablePackages::append(orderedBox.mainMenu(), orderedFailures,
                                              [&orderedManagerOpened] { ++orderedManagerOpened; });
    });
    orderedBox.setItems({package});

    // (No singer), the loaded singer, then the tail: the refused package, a separator, then the
    // entry that opens the package manager.
    expect(orderedRefreshes == 1,
           "the first setItems must reach the handlers the views wire to itemsPopulated");
    const auto orderedActions = orderedBox.mainMenu()->actions();
    expect(orderedActions.size() == 5,
           "a handler connected before setItems still appends its tail to the menu it filled");
    if (orderedActions.size() == 5) {
        expect(!orderedActions.at(2)->isEnabled() && orderedActions.at(2)->toolTip() == reason,
               "the first population ends with the refused package, disabled and explained");
        expect(orderedActions.at(3)->isSeparator() &&
                   orderedActions.at(4)->text() == QStringLiteral("Manage voicebanks..."),
               "the first population ends with the separator and the package manager entry");
        orderedActions.at(4)->trigger();
        expect(orderedManagerOpened == 1, "the manager entry opens the package manager");
    }
    expect(orderedBox.mainMenu()->toolTipsVisible(),
           "the tail stays readable when the menu was filled by setItems");

    orderedBox.setItems({package});
    expect(orderedBox.mainMenu()->actions().size() == 5,
           "populating again replaces the tail instead of stacking a second one");

    const char *const viewSources[] = {"src/app/UI/Views/TrackEditor/TrackControlView.cpp",
                                       "src/app/UI/Views/ClipEditor/ToolBar/"
                                       "ClipEditorToolBarView.cpp"};
    for (const auto *relativePath : viewSources)
        expectTailWiredBeforeFirstSetItems(relativePath);

    if (g_failures == 0) {
        QTextStream(stdout) << "All SingerMenuDisplay tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
