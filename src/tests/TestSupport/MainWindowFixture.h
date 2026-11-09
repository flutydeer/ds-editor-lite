#pragma once

#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/Toast.h>

#include <QCoreApplication>
#include <QtTest/QTest>
#include <memory>

namespace TestSupport {
    struct MainWindowFixture {
        MainWindowFixture() {
            appOptions->appearance()->useNativeFrame = true;
            appOptions->developer()->enablePanelDetach = true;
            window = std::make_unique<MainWindow>();
            window->findChild<MainMenuView *>()->setNativeMenuBar(false);
        }

        ~MainWindowFixture() {
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
        }

        void show() const {
            window->resize(1200, 900);
            window->show();
            window->activateWindow();
            QTRY_VERIFY(window->isActiveWindow());
            QVERIFY(window->setEditorPanelVisibility(true, true));
            QCoreApplication::processEvents();
        }

        const bool nativeFrame = appOptions->appearance()->useNativeFrame;
        const bool detachEnabled = appOptions->developer()->enablePanelDetach;
        const bool trackVisible = !appStatus->trackPanelCollapsed;
        const bool bottomVisible = !appStatus->bottomPanelCollapsed;
        std::unique_ptr<MainWindow> window;
    };
}
