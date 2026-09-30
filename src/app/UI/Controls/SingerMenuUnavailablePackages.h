#ifndef SINGERMENUUNAVAILABLEPACKAGES_H
#define SINGERMENUUNAVAILABLEPACKAGES_H

#include <lite/PackageManager/Models/GetInstalledPackagesResult.h>
#include <lite/PackageManager/Models/PackageInfo.h>

#include <QCoreApplication>
#include <QMenu>
#include <QString>
#include <functional>

/// The tail of a singer menu: one row per package the loader would not open, then the entry that
/// opens the package manager.
///
/// A package that never loaded has no singer, so its row offers nothing to pick; it is shown
/// disabled, titled after the directory it was found in, and carries the loader's reason as its
/// tooltip. The reason is passed through unchanged -- only the loader knows why it refused.
struct SingerMenuUnavailablePackages {
    Q_DECLARE_TR_FUNCTIONS(SingerMenuUnavailablePackages)

public:
    /// Appends the rows to \a menu. \a openPackageManager runs when the last entry is picked.
    ///
    /// Both singer menus build their tail here so that an unloaded package is described the same
    /// way wherever it is listed.
    static void append(QMenu *menu,
                       const QList<GetInstalledPackagesResult::FailedPackage> &failures,
                       const std::function<void()> &openPackageManager) {
        if (!menu)
            return;

        // Qt keeps menu tooltips switched off by default, and the reason lives in one.
        menu->setToolTipsVisible(true);

        for (const auto &failure : failures) {
            if (!PackageInfo::isFailureReportable(failure.reason))
                continue;
            const auto unavailable = PackageInfo::unavailable(failure.path, failure.reason);
            auto *action = menu->addAction(tr("⚠ %1 — Unable to load").arg(unavailable.id()));
            // Nothing to pick: the row is here to explain, not to select.
            action->setEnabled(false);
            action->setToolTip(failure.reason);
        }

        menu->addSeparator();
        auto *manage = menu->addAction(tr("Manage voicebanks..."));
        QObject::connect(manage, &QAction::triggered, menu,
                         [openPackageManager] { openPackageManager(); });
    }
};

#endif // SINGERMENUUNAVAILABLEPACKAGES_H
