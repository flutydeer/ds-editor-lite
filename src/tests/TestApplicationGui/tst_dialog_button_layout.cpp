#include "tst_application_gui.h"

#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Base/OKCancelDialog.h"
#include "UI/Dialogs/Base/ProgressDialog.h"
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/PlatformDialogButtonBox.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <QApplication>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtTest/QTest>

void ApplicationGuiTests::dialogButtonRolesFollowPlatformLayout_data() {
    QTest::addColumn<int>("policy");
    QTest::addColumn<bool>("acceptOnRight");
    QTest::newRow("windows") << int(QDialogButtonBox::WinLayout) << false;
    QTest::newRow("macos") << int(QDialogButtonBox::MacLayout) << true;
    QTest::newRow("gnome") << int(QDialogButtonBox::GnomeLayout) << true;
}

void ApplicationGuiTests::dialogButtonRolesFollowPlatformLayout() {
    QFETCH(int, policy);
    QFETCH(bool, acceptOnRight);
    const auto previousTheme = ThemeManager::instance()->currentThemeId();
    const auto restoreTheme =
        qScopeGuard([&] { ThemeManager::instance()->applyThemePreference(previousTheme); });
    Dialog dialog;
    auto *box = new PlatformDialogButtonBox(QDialogButtonBox::ButtonLayout(policy), dialog.body());
    auto *layout = new QVBoxLayout(dialog.body());
    layout->addWidget(box);
    auto *ok = new AccentButton(QStringLiteral("OK"));
    auto *cancel = new Button(QStringLiteral("Cancel"));
    box->addButton(ok, QDialogButtonBox::AcceptRole);
    box->addButton(cancel, QDialogButtonBox::RejectRole);
    ok->setDefault(true);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    for (const auto &theme : {QStringLiteral("lite-light"), QStringLiteral("lite-dark")}) {
        QVERIFY(ThemeManager::instance()->applyThemePreference(theme));
        QCOMPARE(box->style()->styleHint(QStyle::SH_DialogButtonLayout, nullptr, box), policy);
        QTRY_COMPARE(ok->geometry().center().x() > cancel->geometry().center().x(), acceptOnRight);
        QCOMPARE(box->buttonRole(ok), QDialogButtonBox::AcceptRole);
        QVERIFY(ok->isDefault());
    }
    PlatformDialogButtonBox native;
    QCOMPARE(native.style()->styleHint(QStyle::SH_DialogButtonLayout, nullptr, &native),
             int(PlatformDialogButtonBox::platformLayout()));
}

void ApplicationGuiTests::dialogButtonResetPreservesOwnershipAndCallbacks() {
    Dialog dialog;
    auto *ok = new AccentButton(QStringLiteral("Save"));
    auto *discard = new Button(QStringLiteral("Don't save"));
    auto *cancel = new Button(QStringLiteral("Cancel"));
    QPointer<Button> guardedOk = ok;
    int saves = 0;
    connect(ok, &Button::clicked, &dialog, [&] { ++saves; });
    dialog.setPositiveButton(ok);
    dialog.setNegativeButton(discard, QDialogButtonBox::DestructiveRole);
    dialog.setNeutralButton(cancel, QDialogButtonBox::RejectRole);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *box = dialog.buttonBar()->buttonBox();
    QCOMPARE(box->buttons().size(), 3);
    QCOMPARE(box->buttonRole(discard), QDialogButtonBox::DestructiveRole);
    QCOMPARE(box->buttonRole(cancel), QDialogButtonBox::RejectRole);
    dialog.setPositiveButton(ok);
    QVERIFY(guardedOk);
    QCOMPARE(box->buttons().size(), 3);
    QVERIFY(ok->isVisible());
    QVERIFY(ok->isDefault());
    QTest::keyClick(ok, Qt::Key_Return);
    QCOMPARE(saves, 1);
    QVERIFY(dialog.isVisible()); // No implicit accepted -> accept connection.
    dialog.setNegativeButton(nullptr);
    QCOMPARE(box->buttons().size(), 2);
    QVERIFY(!discard->isVisible());
    dialog.setNegativeButton(discard, QDialogButtonBox::DestructiveRole);
    QVERIFY(discard->isVisible());
    QCOMPARE(box->buttons().size(), 3);
    QTest::keyClick(&dialog, Qt::Key_Escape);
    QVERIFY(!dialog.isVisible());
}

void ApplicationGuiTests::messageDialogRolesPreserveResultAndDefault() {
    MessageDialog dialog(QStringLiteral("Confirm"), QStringLiteral("Message"));
    dialog.addButton(QStringLiteral("Cancel"), 42, QDialogButtonBox::RejectRole);
    dialog.addAccentButton(QStringLiteral("Proceed"), 7, QDialogButtonBox::AcceptRole);
    auto *box = dialog.buttonBar()->buttonBox();
    Button *cancel = nullptr;
    Button *proceed = nullptr;
    for (auto *button : box->buttons()) {
        if (box->buttonRole(button) == QDialogButtonBox::RejectRole)
            cancel = qobject_cast<Button *>(button);
        else
            proceed = qobject_cast<Button *>(button);
    }
    QVERIFY(cancel);
    QVERIFY(proceed);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QVERIFY(cancel->isDefault()); // Preserve the original first-added default.
    QTimer::singleShot(0, proceed, &Button::click);
    QCOMPARE(dialog.exec(), 7); // IDs are independent of layout positions and roles.

    MessageDialog escaped;
    escaped.addAccentButton(QStringLiteral("OK"), 7, QDialogButtonBox::AcceptRole);
    QTimer::singleShot(0, &escaped, [&] { QTest::keyClick(&escaped, Qt::Key_Escape); });
    QCOMPARE(escaped.exec(), -1);

    MessageDialog keyboard;
    keyboard.addButton(QStringLiteral("Cancel"), 42, QDialogButtonBox::RejectRole);
    keyboard.addAccentButton(QStringLiteral("Proceed"), 7, QDialogButtonBox::AcceptRole);
    QTimer::singleShot(0, &keyboard, [&] { QTest::keyClick(&keyboard, Qt::Key_Return); });
    QCOMPARE(keyboard.exec(), 42); // The layout must not make Enter destructive.
}

void ApplicationGuiTests::dialogButtonBoxDoesNotCloseProgressOnCancel() {
    ProgressDialog dialog(true, true);
    QSignalSpy canceled(&dialog, &ProgressDialog::canceled);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *box = dialog.buttonBar()->buttonBox();
    QAbstractButton *cancel = nullptr;
    QAbstractButton *hide = nullptr;
    for (auto *button : box->buttons()) {
        if (box->buttonRole(button) == QDialogButtonBox::RejectRole)
            cancel = button;
        else
            hide = button;
    }
    QVERIFY(cancel);
    QVERIFY(hide);
    cancel->click();
    QCOMPARE(canceled.count(), 1);
    QVERIFY(dialog.isVisible());
    hide->click();
    QVERIFY(!dialog.isVisible());
}
