#pragma once

#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MainTitleBar/MainTitleBar.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/Toast.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QtTest/QTest>
#include <memory>

namespace TestSupport {
    struct MainWindowFixture {
        enum class MenuPlacement { InWindow, PlatformDefault };

        explicit MainWindowFixture(MenuPlacement menuPlacement = MenuPlacement::InWindow) {
            appOptions->appearance()->useNativeFrame = true;
            appOptions->developer()->enablePanelDetach = true;
            appOptions->developer()->enableEmbeddedOptionsDialog = true;
            window = std::make_unique<MainWindow>();
            if (menuPlacement == MenuPlacement::InWindow) {
                window->findChild<MainMenuView *>()->setNativeMenuBar(false);
                // MainWindow chose its plan before the fixture disabled the platform menu.
                // Keep the title-bar section and its overflow policy in sync with Qt.
                auto *titleBar = window->findChild<MainTitleBar *>();
                auto plan = titleBar->layoutPlan();
                plan.inWindowMenu = true;
                titleBar->setLayoutPlan(plan);
            }
        }

        ~MainWindowFixture() {
            window->closeAppOptions();
            appOptions->developer()->enablePanelDetach = false;
            window->updatePanelDetachEnabled();
            window->setEditorPanelVisibility(trackVisible, bottomVisible);
            documentWorkflowController->setUi(nullptr);
            trackController->setParentWidget(nullptr);
            Dialog::setGlobalContext(nullptr);
            Toast::setGlobalContext(nullptr);
            window.reset();
            appOptions->appearance()->useNativeFrame = nativeFrame;
            appOptions->developer()->enablePanelDetach = detachEnabled;
            appOptions->developer()->enableEmbeddedOptionsDialog = embeddedEnabled;
        }

        void show() const {
            if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
                window->resize(1200, 900);
            } else {
                // Oversized windows can start zoomed on Cocoa, leaving no normal size to restore.
                const auto available =
                    window->screen()->availableGeometry().adjusted(40, 40, -40, -40);
                window->resize(QSize(1200, 900).boundedTo(available.size()));
                window->move(available.topLeft());
            }
            window->show();
            window->activateWindow();
            QTRY_VERIFY(window->isActiveWindow());
            QVERIFY(window->setEditorPanelVisibility(true, true));
            QCoreApplication::processEvents();
        }

        const bool nativeFrame = appOptions->appearance()->useNativeFrame;
        const bool detachEnabled = appOptions->developer()->enablePanelDetach;
        const bool embeddedEnabled = appOptions->developer()->enableEmbeddedOptionsDialog;
        const bool trackVisible = !appStatus->trackPanelCollapsed;
        const bool bottomVisible = !appStatus->bottomPanelCollapsed;
        std::unique_ptr<MainWindow> window;
    };
}
