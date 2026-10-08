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

    /// Checks the tail of a singer menu that lists a package load failure: the disabled row, a
    /// separator and the entry that opens the package manager.
    void checkMenuTail(const QList<QAction *> &tail, const QString &reason, int &managerOpened) {
        if (tail.size() != 4) {
            expect(false, "the menu tail must hold exactly one row per reportable failure");
            return;
        }
        const auto *row = tail.at(1);
        expect(!row->isEnabled(), "a package that failed to load must not be selectable");
        expect(row->text() == QStringLiteral("⚠ 0913_wolf_club@1.0.0 — Unable to load"),
               "the failed package row is titled after the package directory");
        expect(row->toolTip() == reason, "the row tooltip is the unmodified loader reason");
        expect(tail.at(2)->isSeparator(), "a separator precedes the package manager entry");
        const auto *manage = tail.at(3);
        expect(manage->isEnabled() && manage->text() == QStringLiteral("Manage voicebanks..."),
               "the tail ends with an entry that opens the package manager");
        tail.at(3)->trigger();
        expect(managerOpened == 1, "the manager entry opens the package manager");
    }

    /// Set if the order check below cannot read the sources.
    bool g_sourceUnavailable = false;

    /// Returns the source root: the source directory that this test was configured from, otherwise
    /// a directory found by searching upward from the executable directory and the current
    /// directory. Returns an empty string if neither contains the sources. A check that reads no
    /// source is worse than no check, so a caller that receives an empty result reports the check
    /// as skipped.
    QString sourceRoot() {
        const QString marker = QStringLiteral("src/app/UI/Controls/TwoLevelComboBox.cpp");
        if (QDir configured(QStringLiteral(TEST_SOURCE_ROOT)); configured.exists(marker))
            return configured.absolutePath();
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

    /// Checks the order of the tail wiring in a view source. The singer views cannot be
    /// constructed in this test, because their constructors access the running application
    /// through the same globals as every other view. The order is therefore checked in each view
    /// source: the tail wiring must precede the first setItems call. A view constructed after the
    /// package scan has finished is filled by that single call and is never populated again, so
    /// wiring connected after the call leaves the tail unwritten.
    void expectTailWiredBeforeFirstSetItems(const char *relativePath) {
        const auto root = sourceRoot();
        if (root.isEmpty()) {
            g_sourceUnavailable = true;
            return;
        }

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
            QStringLiteral("%1: the tail wiring must precede the first setItems call, because a "
                           "view built after the package scan has finished is never populated "
                           "again")
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

    // A package that failed to load is still listed with the loader reason, so that a user who
    // installed an unusable voicebank sees the cause. The entry offers no singer, and its reason is
    // the unmodified loader text, not a text defined here.
    const auto reason = QStringLiteral("failed to resolve dependency of 0913_wolf_club: no installed "
                                       "Package satisfies dependency wolf/lang-ja");
    const PackageInfo unavailable =
        PackageInfo::unavailable(QStringLiteral("voicebanks/0913_wolf_club@1.0.0"), reason);
    expect(unavailable.id() == QStringLiteral("0913_wolf_club@1.0.0"),
           "an unavailable entry is titled after its package directory");
    expect(unavailable.path() == QStringLiteral("voicebanks/0913_wolf_club@1.0.0"),
           "an unavailable entry keeps its package path");
    expect(unavailable.unavailableReason() == reason,
           "an unavailable entry carries the unmodified loader reason");
    expect(unavailable.isUnavailable() && !unavailable.isEmpty(),
           "an unavailable entry is not empty");
    expect(package.unavailableReason().isEmpty() && !package.isUnavailable(),
           "a loaded package carries no reason");
    expect(unavailable != package, "an unavailable entry is not equal to a loaded package");

    TwoLevelComboBox unavailableBox;
    unavailableBox.setItems({unavailable});
    expect(unavailableBox.mainMenu()->actions().size() == 1,
           "a package that failed to load offers no singer");

    // Singer menu tail: one disabled row per package load failure, with the loader reason as its
    // tooltip, followed by the entry that opens the package manager. A failure without a reason is
    // not listed, and the reason is never reworded.
    int managerOpened = 0;
    SingerMenuUnavailablePackages::append(
        unavailableBox.mainMenu(),
        {GetInstalledPackagesResult::FailedPackage(unavailable.path(), reason),
         GetInstalledPackagesResult::FailedPackage(QStringLiteral("voicebanks/wordless"), {})},
        [&managerOpened] { ++managerOpened; });

    const auto tail = unavailableBox.mainMenu()->actions();
    expect(tail.size() == 4,
           "the menu tail lists the failed package and ends with the package manager entry");
    checkMenuTail(tail, reason, managerOpened);
    expect(unavailableBox.mainMenu()->toolTipsVisible(),
           "menu tooltips must be enabled so that the reason is readable");

    // The views do not fill the menu directly: they connect the tail to itemsPopulated and let
    // setItems populate the menu. A view constructed after the package scan state is Ready is
    // filled by the single setItems call in its constructor and is never populated again, so a
    // tail connected after that call is never written. The following code replays the order that
    // the views must use: connect, then setItems.
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

    // Expected menu: (No singer), the loaded singer and the tail, which consists of the failed
    // package, a separator and the package manager entry.
    expect(orderedRefreshes == 1,
           "the first setItems call must invoke the handlers connected to itemsPopulated");
    const auto orderedActions = orderedBox.mainMenu()->actions();
    expect(orderedActions.size() == 5,
           "a handler connected before setItems appends the tail to the populated menu");
    if (orderedActions.size() == 5) {
        expect(!orderedActions.at(2)->isEnabled() && orderedActions.at(2)->toolTip() == reason,
               "the first population lists the failed package as disabled, with its reason");
        expect(orderedActions.at(3)->isSeparator() &&
                   orderedActions.at(4)->text() == QStringLiteral("Manage voicebanks..."),
               "the first population ends with the separator and the package manager entry");
        orderedActions.at(4)->trigger();
        expect(orderedManagerOpened == 1, "the manager entry opens the package manager");
    }
    expect(orderedBox.mainMenu()->toolTipsVisible(),
           "menu tooltips stay enabled if setItems populated the menu");

    orderedBox.setItems({package});
    expect(orderedBox.mainMenu()->actions().size() == 5,
           "populating again replaces the tail instead of appending a second tail");

    const char *const viewSources[] = {"src/app/UI/Views/TrackEditor/TrackControlView.cpp",
                                       "src/app/UI/Views/ClipEditor/ToolBar/"
                                       "ClipEditorToolBarView.cpp"};
    for (const auto *relativePath : viewSources)
        expectTailWiredBeforeFirstSetItems(relativePath);

    if (g_failures == 0 && g_sourceUnavailable) {
        // The menu checks passed, but the order check read no source, so the primary check of
        // the test did not run. ctest reports this result as skipped instead of passed.
        QTextStream(stderr) << "The source tree was not found at " << TEST_SOURCE_ROOT
                            << " or above the test directory; the view order check was skipped"
                            << Qt::endl;
        return 77;
    }
    if (g_failures == 0) {
        QTextStream(stdout) << "All SingerMenuDisplay tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
