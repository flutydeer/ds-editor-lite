#pragma once

#include "UI/Dialogs/Options/AppOptionsDialog.h"

#include <lite/GUI/Theme/ThemeManager.h>

#include <QListWidget>
#include <QtTest/QTest>

namespace TestSupport {
    inline void openOptionsPage(AppOptionsDialog &panel, AppOptionsGlobal::Option option) {
        // Product windows supply this style root to their embedded options panel.
        ThemeManager::instance()->addStyleRoot(&panel);
        panel.resize(920, 720);
        panel.show();
        panel.activateWindow();
        QTRY_VERIFY(panel.isVisible());
        QTRY_VERIFY(panel.isActiveWindow());
        auto *tabs = panel.findChild<QListWidget *>("AppOptionsDialogTabListWidget");
        QVERIFY(tabs);
        auto *item = tabs->item(static_cast<int>(option) - 1);
        QVERIFY(item);
        QTest::mouseClick(tabs->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tabs->visualItemRect(item).center());
        QTRY_COMPARE(tabs->currentItem(), item);
    }
}
