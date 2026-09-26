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
            appOptions->appearance()->enableDirectManipulation = false;
            appOptions->developer()->enablePanelDetach = true;
            appOptions->developer()->enableEmbeddedOptionsDialog = true;
            window = std::make_unique<MainWindow>();
            window->findChild<MainMenuView *>()->setNativeMenuBar(false);
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
            appOptions->appearance()->enableDirectManipulation = directManipulation;
            appOptions->developer()->enablePanelDetach = detachEnabled;
            appOptions->developer()->enableEmbeddedOptionsDialog = embeddedEnabled;
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
        const bool directManipulation = appOptions->appearance()->enableDirectManipulation;
        const bool detachEnabled = appOptions->developer()->enablePanelDetach;
        const bool embeddedEnabled = appOptions->developer()->enableEmbeddedOptionsDialog;
        const bool trackVisible = !appStatus->trackPanelCollapsed;
        const bool bottomVisible = !appStatus->bottomPanelCollapsed;
        std::unique_ptr<MainWindow> window;
    };
}
