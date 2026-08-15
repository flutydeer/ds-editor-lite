#include "AboutDialog.h"

#include <lite/BuildInfo.h>
#include <lite/ProductMetadata.h>

#include <QMCore/qmsystem.h>

#include <QApplication>
#include <QGridLayout>
#include <QIcon>
#include <QScreen>
#include <QSpacerItem>
#include <QtGlobal>

AboutDialog::AboutDialog(QWidget *parent) : QMessageBox(parent) {
    const QString appName = qApp->applicationName();

    const QString copyrightInfo =
        QApplication::translate("Application", "<p>Based on Qt version %1.<br>%2</p>")
            .arg(QStringLiteral(QT_VERSION_STR), QString::fromUtf8(LiteProductMetadata::Copyright));

    const QString buildInfo =
        QApplication::translate("Application", "<h3>Build Information</h3>"
                                               "<p>"
                                               "Version: %1<br>"
                                               "Branch: %2<br>"
                                               "Commit: %3<br>"
                                               "Build date: %4<br>"
                                               "Toolchain: %5 %6 %7"
                                               "</p>")
            .arg(QApplication::applicationVersion(),
                 QStringLiteral(LITE_GIT_BRANCH),           //
                 QStringLiteral(LITE_GIT_LAST_COMMIT_HASH), //
                 QStringLiteral(LITE_BUILD_TIME),           //
                 QStringLiteral(LITE_COMPILER_ARCH),        //
                 QStringLiteral(LITE_COMPILER_ID),          //
                 QStringLiteral(LITE_COMPILER_VERSION));

    const QString aboutInfo =
        QApplication::translate(
            "Application",
            "<h3>About Application</h3>"
            "<p>%1 is a cross-platform SVS editing application powered by "
            "DiffSinger for virtual singer producers to make song compositions.</p>")
            .arg(appName);

    const QString licenseInfo =
        QApplication::translate(
            "Application",
            "<h3>License</h3>"
            "<p>Licensed under the Apache License, Version 2.0.<br>"
            R"(You may obtain a copy of the License at %1.</p>)"
            "<p>This application is distributed "
            "<b>AS IS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND</b>, either express or "
            "implied.</p>")
            .arg(QStringLiteral(
                R"(<a href="https://www.apache.org/licenses/LICENSE-2.0">apache.org/licenses</a>)"));

    setWindowTitle(QApplication::translate("Application", "About %1").arg(appName));
    setText(QApplication::translate("Application", "<h2>%1</h2>%2%3%4%5")
                .arg(appName, copyrightInfo, buildInfo, aboutInfo, licenseInfo));
    setIcon(QMessageBox::Information);

    // Let users select/copy the build details (and still click the license link).
    setTextInteractionFlags(Qt::TextBrowserInteraction);

    const double ratio = screen()->logicalDotsPerInch() / QM::unitDpi();

    const QIcon icon = qApp->windowIcon();
    if (!icon.isNull())
        setIconPixmap(icon.pixmap(QSize(40, 40) * ratio));

    if (const auto gridLayout = qobject_cast<QGridLayout *>(layout())) {
        const auto horizontalSpacer = new QSpacerItem(static_cast<int>(ratio * 500), 0,
                                                      QSizePolicy::Minimum, QSizePolicy::Expanding);
        gridLayout->addItem(horizontalSpacer, gridLayout->rowCount(), 0, 1,
                            gridLayout->columnCount());
    }
}
