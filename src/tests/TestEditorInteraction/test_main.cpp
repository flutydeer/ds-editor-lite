#include "tst_editor_interaction.h"
#include "Bootstrap/AppEnvironment.h"

#include <QtTest/QTest>

void EditorInteractionTests::initTestCase() {
    AppEnvironment::postInit(AppHostMode::Gui);
}

QTEST_MAIN(EditorInteractionTests)
