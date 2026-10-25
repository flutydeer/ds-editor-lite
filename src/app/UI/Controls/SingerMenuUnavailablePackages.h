#ifndef SINGERMENUUNAVAILABLEPACKAGES_H
#define SINGERMENUUNAVAILABLEPACKAGES_H

#include <lite/PackageManager/Models/GetInstalledPackagesResult.h>
#include <lite/PackageManager/Models/PackageInfo.h>

#include <QCoreApplication>
#include <QMenu>
#include <QString>
#include <functional>

/// Append disabled package-failure rows and a package-manager action to a singer menu.
struct SingerMenuUnavailablePackages {
    Q_DECLARE_TR_FUNCTIONS(SingerMenuUnavailablePackages)

public:
    static void append(QMenu *menu,
                       const QList<GetInstalledPackagesResult::FailedPackage> &failures,
                       const std::function<void()> &openPackageManager) {
        if (!menu)
            return;

        // Enable tooltips so disabled rows can show the loader's reason.
        menu->setToolTipsVisible(true);

        for (const auto &failure : failures) {
            if (!PackageInfo::isFailureReportable(failure.reason))
                continue;
            const auto unavailable = PackageInfo::unavailable(failure.path, failure.reason);
            auto *action = menu->addAction(tr("⚠ %1 — Unable to load").arg(unavailable.id()));
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
