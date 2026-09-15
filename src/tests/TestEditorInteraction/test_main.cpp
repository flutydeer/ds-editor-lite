#include "tst_editor_interaction.h"
#include "Bootstrap/AppEnvironment.h"
#include "../TestSupport/GuiAppFixture.h"

#include <QtTest/QTest>

EditorInteractionTests::EditorInteractionTests() = default;
EditorInteractionTests::~EditorInteractionTests() = default;

void EditorInteractionTests::initTestCase() {
    AppEnvironment::postInit(AppHostMode::Gui);
    application = std::make_unique<GuiAppFixture>();
    QVERIFY2(application->initialize(), qPrintable(application->error));
}

void EditorInteractionTests::cleanupTestCase() {
    application.reset();
}

QTEST_MAIN(EditorInteractionTests)
