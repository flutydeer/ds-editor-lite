#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.h"
#include "../TestSupport/WaveFixture.h"
#include "../TestSupport/MainWindowFixture.h"
#include "UI/Window/DetachedPanelWindow.h"
#include "UI/Window/MainWindow.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"

#include <lite/GUI/Controls/AdaptiveToolBar.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ControlGroup.h>
#include <lite/GUI/Controls/InlineEditLabel.h>
#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QAbstractScrollArea>
#include <QAbstractButton>
#include <QCloseEvent>
#include <QDir>
#include <QEvent>
#include <QLineEdit>
#include <QPixmap>
#include <QPointer>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QSignalSpy>
#include <QtTest/QTest>

namespace {
    void settleToolbar() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }

    bool saveToolbarEvidence(QWidget &widget, const QString &stage) {
        const auto directory = qEnvironmentVariable("LITE_TOOLBAR_SCREENSHOT_DIR");
        if (directory.isEmpty())
            return true;
        const QDir output(directory);
        if (!output.mkpath(QStringLiteral(".")))
            return false;
        const auto filename = QStringLiteral("clip-toolbar-%1-%2.png")
                                  .arg(QString::fromLatin1(QTest::currentDataTag()), stage);
        return widget.grab().save(output.filePath(filename), "PNG");
    }

    auto toolbarDocument(const QString &audioPath = {}) {
        auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
        Automation::TrackDraftDto track;
        for (const auto &name : {QStringLiteral("First clip"), QStringLiteral("Second clip")}) {
            Automation::ClipDraftDto clip;
            clip.properties.name = name;
            clip.properties.length = 1920;
            clip.properties.clipLen = 1920;
            if (!audioPath.isEmpty() && name == QStringLiteral("Second clip")) {
                clip.type = Automation::ClipDraftDto::Type::Audio;
                clip.audioPath = audioPath;
            }
            track.clips.append(clip);
        }
        document.tracks = {track};
        return document;
    }

    Clip *namedClip(AppModel &model, const QString &name) {
        for (auto *clip : model.tracks().first()->clips()) {
            if (clip->name() == name)
                return clip;
        }
        return nullptr;
    }

    int nameOverflowWidth(ClipEditorToolBarView &toolbar, AdaptiveToolBar &adaptive,
                          InlineEditLabel &label) {
        const int narrow = qMax(toolbar.minimumWidth(), toolbar.minimumSizeHint().width());
        for (int width = toolbar.width(); width >= narrow; width -= 4) {
            toolbar.resize(width, toolbar.height());
            settleToolbar();
            if (adaptive.isWidgetOverflowed(&label))
                return toolbar.width();
        }
        return -1;
    }

    const QStringList priorityOrder{
        QStringLiteral("cbClipLanguage"),     QStringLiteral("cbClipSinger"),
        QStringLiteral("leClipName"),         QStringLiteral("btnPitchModulation"),
        QStringLiteral("btnPitchTrace"),      QStringLiteral("btnPitchEraser"),
        QStringLiteral("btnPitchPencil"),     QStringLiteral("btnPitchAnchor"),
        QStringLiteral("btnNoteSplit"),       QStringLiteral("btnNoteEraser"),
        QStringLiteral("btnNotePencil"),      QStringLiteral("btnBeam"),
        QStringLiteral("btnArrow"),           QStringLiteral("btnAutoPageTurn"),
        QStringLiteral("cbPianoRollQuantize")};

    int toolbarGroup(const QString &name) {
        if (name == QStringLiteral("leClipName") || name == QStringLiteral("cbClipSinger") ||
            name == QStringLiteral("cbClipLanguage"))
            return 0;
        if (name == QStringLiteral("cbPianoRollQuantize"))
            return 2;
        if (name == QStringLiteral("btnAutoPageTurn"))
            return 3;
        return 1;
    }

    void verifyGroupAnchors(AdaptiveToolBar &toolbar, const QList<QWidget *> &items) {
        for (const int group : {0, 1, 2, 3}) {
            auto *more = toolbar.overflowButton(group);
            auto *popup = toolbar.overflowPopup(group);
            QVERIFY(more && popup);
            QVERIFY(qobject_cast<ControlGroup *>(more->parentWidget()));
            // Original combo boxes retain their own nested popup windows and scrolling lists.
            // Only scroll wrappers in this group's outer popup are forbidden.
            for (auto *area : popup->findChildren<QAbstractScrollArea *>())
                QVERIFY2(area->window() != popup,
                         "The group popup must not contain a scroll wrapper");
            for (auto *bar : popup->findChildren<QScrollBar *>())
                QVERIFY2(bar->window() != popup, "The group popup must not contain a scrollbar");
            bool hasOverflow = false;
            QRect lastInline;
            for (auto *item : items) {
                if (toolbarGroup(item->objectName()) != group || !toolbar.isItemApplicable(item))
                    continue;
                if (toolbar.isWidgetOverflowed(item)) {
                    hasOverflow = true;
                    QVERIFY(popup->isAncestorOf(item));
                    for (const int otherGroup : {0, 1, 2, 3}) {
                        if (otherGroup != group)
                            QVERIFY(!toolbar.overflowPopup(otherGroup)->isAncestorOf(item));
                    }
                } else {
                    QCOMPARE(item->parentWidget(), more->parentWidget());
                    const QRect itemRect(item->mapTo(&toolbar, QPoint()), item->size());
                    if (lastInline.isEmpty() || itemRect.right() > lastInline.right())
                        lastInline = itemRect;
                }
            }
            QCOMPARE(more->isVisible(), hasOverflow);
            if (!hasOverflow)
                continue;
            const QRect moreRect(more->mapTo(&toolbar, QPoint()), more->size());
            QVERIFY(toolbar.rect().contains(moreRect));
            if (!lastInline.isEmpty())
                QCOMPARE(moreRect.left() - lastInline.right() - 1, 1);
            else
                QCOMPARE(more->parentWidget()->width(), more->width());
        }
    }
}

void ApplicationGuiTests::adaptiveClipToolbarKeepsSizesInTitleBarAndDetachedPanel() {
    TestSupport::MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    auto &window = *host.window;
    window.activateWindow();
    const QPointer<BottomPanelView> bottom = window.findChild<BottomPanelView *>();
    QVERIFY(bottom);
    auto *attachedParent = bottom->parentWidget();
    QVERIFY(attachedParent);
    bottom->clipEditorView()->onActiveClipChanged(singingClip->id());
    QTRY_VERIFY(window.isActiveWindow());
    settleToolbar();
    const QPointer<ClipEditorToolBarView> toolbar = bottom->findChild<ClipEditorToolBarView *>();
    QVERIFY(toolbar);
    const QPointer<AdaptiveToolBar> adaptive = toolbar->findChild<AdaptiveToolBar *>();
    QVERIFY(adaptive);
    QList<QPointer<QWidget>> items;
    for (const auto &name : priorityOrder) {
        auto *item = toolbar->findChild<QWidget *>(name);
        QVERIFY2(item, qPrintable(name));
        items.append(item);
    }
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto verifyGeometry = [&] {
        QVERIFY(bottom && toolbar && adaptive);
        QCOMPARE(bottom->findChild<ClipEditorToolBarView *>(), toolbar.data());
        QCOMPARE(toolbar->findChild<AdaptiveToolBar *>(), adaptive.data());
        QVERIFY(bottom->clipEditorView()->hasActiveSingingClip());
        QVERIFY(toolbar->isVisible());
        auto *root = bottom->window();
        for (auto *child = static_cast<QWidget *>(adaptive.data()); child != root;
             child = child->parentWidget()) {
            QVERIFY(child->parentWidget());
            QVERIFY2(child->parentWidget()->rect().contains(child->geometry()),
                     qPrintable(QStringLiteral("Clipped %1 inside %2 at host width %3")
                                    .arg(QString::fromLatin1(child->metaObject()->className()),
                                         QString::fromLatin1(
                                             child->parentWidget()->metaObject()->className()))
                                    .arg(root->width())));
        }
        for (int i = 0; i < items.size(); ++i) {
            auto *item = items.at(i).data();
            QVERIFY(item);
            QCOMPARE(toolbar->findChild<QWidget *>(priorityOrder.at(i)), item);
            QVERIFY(adaptive->isItemApplicable(item));
            if (adaptive->isWidgetOverflowed(item)) {
                const int group = toolbarGroup(item->objectName());
                QVERIFY(adaptive->overflowPopup(group)->isAncestorOf(item));
                continue;
            }
            QVERIFY(item->isVisible());
            const int naturalWidth =
                qMin(item->maximumWidth(),
                     qMax(item->minimumWidth(),
                          qMax(item->sizeHint().width(), item->minimumSizeHint().width())));
            QVERIFY2(item->width() >= naturalWidth, qPrintable(priorityOrder.at(i)));
            const QRect itemRect(item->mapTo(toolbar.data(), QPoint()), item->size());
            QVERIFY2(
                toolbar->rect().contains(itemRect),
                qPrintable(QStringLiteral("Clipped item %1: (%2,%3 %4x%5), toolbar %6x%7, host %8")
                               .arg(priorityOrder.at(i))
                               .arg(itemRect.x())
                               .arg(itemRect.y())
                               .arg(itemRect.width())
                               .arg(itemRect.height())
                               .arg(toolbar->width())
                               .arg(toolbar->height())
                               .arg(root->width())));
        }
        const bool needsOverflow = adaptive->width() < adaptive->sizeHint().width();
        QCOMPARE(adaptive->overflowButton()->isVisible(), needsOverflow);
        if (needsOverflow) {
            auto *more = adaptive->overflowButton();
            QVERIFY(toolbar->rect().contains(
                QRect(more->mapTo(toolbar.data(), QPoint()), more->size())));
        }
        QList<QWidget *> registeredItems;
        for (const auto &item : items)
            registeredItems.append(item.data());
        verifyGroupAnchors(*adaptive, registeredItems);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QVERIFY(!historyManager->canUndo());
        QVERIFY(!historyManager->canRedo());
    };

    const int hostMinimum = qMax(window.minimumWidth(), window.minimumSizeHint().width());
    for (const int width : {1200, 900, 700, hostMinimum}) {
        window.resize(width, window.height());
        settleToolbar();
        verifyGeometry();
        if (QTest::currentTestFailed())
            return;
        QVERIFY(saveToolbarEvidence(
            window, QStringLiteral("host-%1-actual-%2").arg(width).arg(window.width())));
    }

    auto *detach = bottom->findChild<QAbstractButton *>(QStringLiteral("btnPanelDetach"));
    QVERIFY(detach && detach->isVisible() && detach->isEnabled());
    QTest::mouseClick(detach, Qt::LeftButton);
    auto *detached = window.findChild<DetachedPanelWindow *>();
    QVERIFY(detached && detached->isWindow() && detached->isVisible());
    QCOMPARE(detached->contentWidget(), bottom.data());
    detached->activateWindow();
    QTRY_VERIFY(detached->isActiveWindow());
    const int floatingMinimum =
        qMax(detached->minimumWidth(), detached->minimumSizeHint().width());
    for (const int width : {900, 600, floatingMinimum}) {
        // Respect the production detached-window minimum instead of overriding constraints.
        detached->resize(width, detached->height());
        settleToolbar();
        verifyGeometry();
        if (QTest::currentTestFailed())
            return;
        QVERIFY(saveToolbarEvidence(
            *detached,
            QStringLiteral("floating-%1-actual-%2").arg(width).arg(detached->width())));
    }

    QCloseEvent close;
    QApplication::sendEvent(bottom.data(), &close);
    QVERIFY(!close.isAccepted());
    QVERIFY(!detached->isVisible());
    QVERIFY(bottom->isVisible());
    QCOMPARE(bottom->parentWidget(), attachedParent);
    settleToolbar();
    verifyGeometry();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(saveToolbarEvidence(window, QStringLiteral("host-reattached")));
}

void ApplicationGuiTests::adaptiveClipToolbarApplicabilityAndPriorities_data() {
    QTest::addColumn<int>("clipState");
    QTest::addColumn<QString>("themeId");
    for (const auto &theme : {ThemeIds::defaultThemeId(), ThemeIds::lightThemeId()}) {
        QTest::newRow(qPrintable(QStringLiteral("singing-%1").arg(theme))) << 0 << theme;
        QTest::newRow(qPrintable(QStringLiteral("audio-%1").arg(theme))) << 1 << theme;
        QTest::newRow(qPrintable(QStringLiteral("null-%1").arg(theme))) << 2 << theme;
    }
}

void ApplicationGuiTests::adaptiveClipToolbarApplicabilityAndPriorities() {
    QFETCH(int, clipState);
    QFETCH(QString, themeId);
    auto *themes = ThemeManager::instance();
    const auto previousTheme = themes->currentThemeId();
    const auto restoreTheme = qScopeGuard([&] { themes->applyTheme(previousTheme); });
    QVERIFY(themes->applyTheme(themeId));
    QTemporaryDir files;
    QVERIFY(files.isValid());
    QString audioPath;
    if (clipState == 1) {
        audioPath = files.filePath(QStringLiteral("toolbar.wav"));
        QVERIFY(TestSupport::writeWave(audioPath, QVector<float>(4800, 0.125f)));
    }
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), toolbarDocument(audioPath)));
    const auto releaseAudio = qScopeGuard([&] {
        if (audioPath.isEmpty())
            return;
        QVERIFY(runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });
    auto *clip = clipState == 2 ? nullptr
                                : namedClip(*context->m_appModel,
                                            clipState == 1 ? QStringLiteral("Second clip")
                                                           : QStringLiteral("First clip"));
    QVERIFY(clipState == 2 || clip);
    ClipEditorToolBarView toolbar;
    ThemeManager::instance()->addStyleRoot(&toolbar);
    toolbar.setDataContext(clip);
    toolbar.resize(1400, 48);
    toolbar.show();
    toolbar.activateWindow();
    settleToolbar();
    auto *adaptive = toolbar.findChild<AdaptiveToolBar *>();
    QVERIFY(adaptive);
    QList<QWidget *> items;
    QList<QWidget *> inlineHosts;
    for (const auto &name : priorityOrder) {
        auto *item = toolbar.findChild<QWidget *>(name);
        QVERIFY2(item, qPrintable(name));
        items.append(item);
        inlineHosts.append(item->parentWidget());
        const bool applicable = clipState == 0 || (clipState == 1 && name == "leClipName");
        QCOMPARE(adaptive->isItemApplicable(item), applicable);
        QCOMPARE(item->isVisible(), applicable);
        if (applicable) {
            QVERIFY(item->isEnabled());
            QVERIFY(qobject_cast<ControlGroup *>(item->parentWidget()));
        } else if (name != QStringLiteral("btnAutoPageTurn")) {
            QVERIFY(!item->isEnabled());
        }
    }
    int expectedMinimum = 0;
    int applicableGroups = 0;
    for (const int group : {0, 1, 2, 3}) {
        if (clipState == 2 || (clipState == 1 && group != 0))
            continue;
        expectedMinimum += adaptive->overflowButton(group)->width();
        ++applicableGroups;
    }
    expectedMinimum += qMax(0, applicableGroups - 1) * 18;
    QCOMPARE(adaptive->minimumSizeHint().width(), expectedMinimum);
    verifyGroupAnchors(*adaptive, items);
    QVERIFY2(saveToolbarEvidence(toolbar, QStringLiteral("full")),
             "Could not save the requested full toolbar screenshot");
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    if (clipState != 2) {
        const int narrow = qMax(toolbar.minimumWidth(), toolbar.minimumSizeHint().width());
        QSet<int> toolOverflowCounts;
        for (int width = toolbar.width(); width >= narrow; --width) {
            toolbar.resize(width, toolbar.height());
            settleToolbar();
            for (int i = 0; i < items.size(); ++i) {
                auto *item = items.at(i);
                if (!adaptive->isItemApplicable(item)) {
                    QVERIFY(!item->isVisible());
                    QVERIFY(!adaptive->isWidgetOverflowed(item));
                    continue;
                }
                if (adaptive->isWidgetOverflowed(item)) {
                    const int group = toolbarGroup(item->objectName());
                    QVERIFY(adaptive->overflowPopup(group)->isAncestorOf(item));
                    for (int lower = 0; lower < i; ++lower) {
                        if (adaptive->isItemApplicable(items.at(lower)))
                            QVERIFY2(adaptive->isWidgetOverflowed(items.at(lower)),
                                     qPrintable(priorityOrder.at(lower)));
                    }
                } else {
                    QCOMPARE(item->parentWidget(), inlineHosts.at(i));
                    const int naturalWidth =
                        qMin(item->maximumWidth(),
                             qMax(item->minimumWidth(),
                                  qMax(item->sizeHint().width(), item->minimumSizeHint().width())));
                    QVERIFY2(item->width() >= naturalWidth, qPrintable(priorityOrder.at(i)));
                }
            }
            verifyGroupAnchors(*adaptive, items);
            int overflowedTools = 0;
            for (int i = 3; i < 13; ++i)
                overflowedTools += adaptive->isWidgetOverflowed(items.at(i));
            toolOverflowCounts.insert(overflowedTools);
        }
        if (clipState == 0) {
            for (int count = 0; count <= 10; ++count)
                QVERIFY2(
                    toolOverflowCounts.contains(count),
                    qPrintable(
                        QStringLiteral("Missing individual tool-overflow stage %1").arg(count)));
        }
        QVERIFY(adaptive->overflowButton()->isVisible());
        QTest::mouseClick(adaptive->overflowButton(), Qt::LeftButton);
        QTRY_VERIFY(adaptive->overflowPopup()->isVisible());
        QVERIFY2(saveToolbarEvidence(toolbar, QStringLiteral("narrow")),
                 "Could not save the requested narrow toolbar screenshot");
        QVERIFY2(saveToolbarEvidence(*adaptive->overflowPopup(), QStringLiteral("popup")),
                 "Could not save the requested overflow popup screenshot");
        for (auto *item : items) {
            if (adaptive->isItemApplicable(item) && adaptive->isWidgetOverflowed(item))
                QCOMPARE(item->isVisible(), toolbarGroup(item->objectName()) == 0);
        }
        adaptive->closeOverflow();
        if (clipState == 0) {
            QTest::mouseClick(adaptive->overflowButton(1), Qt::LeftButton);
            auto *toolPopup = adaptive->overflowPopup(1);
            QTRY_VERIFY(toolPopup->isVisible());
            QVERIFY(!adaptive->overflowPopup(0)->isVisible());
            QCOMPARE(toolPopup->height(), 28 + 8 + toolPopup->contentsMargins().top() +
                                              toolPopup->contentsMargins().bottom());
            for (auto *item : items) {
                if (adaptive->isItemApplicable(item) && adaptive->isWidgetOverflowed(item))
                    QCOMPARE(item->isVisible(), toolbarGroup(item->objectName()) == 1);
            }
            QVERIFY2(saveToolbarEvidence(*toolPopup, QStringLiteral("popup-tools")),
                     "Could not save the requested tool-group popup screenshot");
            adaptive->closeOverflow();
        }
        toolbar.resize(1400, toolbar.height());
        settleToolbar();
        for (int i = 0; i < items.size(); ++i) {
            if (adaptive->isItemApplicable(items.at(i)))
                QCOMPARE(items.at(i)->parentWidget(), inlineHosts.at(i));
        }
    } else {
        toolbar.resize(30, toolbar.height());
        settleToolbar();
        for (const int group : {0, 1, 2, 3}) {
            QVERIFY(!adaptive->overflowButton(group)->isVisible());
            QVERIFY(!adaptive->overflowPopup(group)->isVisible());
        }
    }
    verifyGroupAnchors(*adaptive, items);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::adaptiveClipToolbarNameMigrationKeepsTargetAndUndo_data() {
    QTest::addColumn<bool>("popupFirst");
    QTest::addColumn<bool>("secondAudio");
    QTest::newRow("inline-to-popup-singing") << false << false;
    QTest::newRow("popup-to-inline-singing") << true << false;
    QTest::newRow("inline-to-popup-audio") << false << true;
    QTest::newRow("popup-to-inline-audio") << true << true;
}

void ApplicationGuiTests::adaptiveClipToolbarNameMigrationKeepsTargetAndUndo() {
    QFETCH(bool, popupFirst);
    QFETCH(bool, secondAudio);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    QString audioPath;
    if (secondAudio) {
        audioPath = files.filePath(QStringLiteral("toolbar.wav"));
        QVERIFY(TestSupport::writeWave(audioPath, QVector<float>(4800, 0.125f)));
    }
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), toolbarDocument(audioPath)));
    const auto releaseAudio = qScopeGuard([&] {
        if (!secondAudio)
            return;
        QVERIFY(runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });
    auto *first = namedClip(*context->m_appModel, QStringLiteral("First clip"));
    auto *second = namedClip(*context->m_appModel, QStringLiteral("Second clip"));
    QVERIFY(first && second);
    ClipEditorToolBarView toolbar;
    ThemeManager::instance()->addStyleRoot(&toolbar);
    toolbar.setDataContext(first);
    toolbar.resize(1400, 48);
    toolbar.show();
    toolbar.activateWindow();
    settleToolbar();
    auto *adaptive = toolbar.findChild<AdaptiveToolBar *>();
    auto *label = toolbar.findChild<InlineEditLabel *>("leClipName");
    QVERIFY(adaptive && label);
    auto *inlineHost = label->parentWidget();
    const int narrow = nameOverflowWidth(toolbar, *adaptive, *label);
    QVERIFY(narrow > 0);
    auto *popupHost = label->parentWidget();
    QVERIFY(popupHost != inlineHost);
    QVERIFY(adaptive->overflowPopup()->isAncestorOf(popupHost));
    if (!popupFirst) {
        toolbar.resize(1400, toolbar.height());
        settleToolbar();
        QCOMPARE(label->parentWidget(), inlineHost);
    } else {
        QTest::mouseClick(adaptive->overflowButton(), Qt::LeftButton);
        QTRY_VERIFY(adaptive->overflowPopup()->isVisible());
    }
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    QSignalSpy commits(label, &InlineEditLabel::editCompleted);
    QTest::mouseDClick(label, Qt::LeftButton);
    auto *overlay = label->parentWidget()->findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay && overlay->isEditing());
    auto *input = overlay->findChild<QLineEdit *>();
    QVERIFY(input);
    QTRY_VERIFY(input->hasFocus());
    input->setText(QStringLiteral("Committed on migration"));
    const QPointer<InlineTextEditOverlay> identity(overlay);
    toolbar.resize(popupFirst ? 1400 : narrow, toolbar.height());
    settleToolbar();
    QCOMPARE(commits.size(), 1);
    QCOMPARE(first->name(), QStringLiteral("Committed on migration"));
    QCOMPARE(second->name(), QStringLiteral("Second clip"));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QCOMPARE(label->parentWidget(), popupFirst ? inlineHost : popupHost);
    QCOMPARE(overlay->parentWidget(), label->parentWidget());
    QCOMPARE(identity.data(), overlay);
    QVERIFY(!overlay->isEditing());
    if (!popupFirst) {
        QTest::mouseClick(adaptive->overflowButton(), Qt::LeftButton);
        QTRY_VERIFY(adaptive->overflowPopup()->isVisible());
    }
    QTest::mouseDClick(label, Qt::LeftButton);
    QVERIFY(overlay->isEditing());
    input->setText(QStringLiteral("Committed to the old clip"));
    toolbar.setDataContext(second);
    settleToolbar();
    QCOMPARE(commits.size(), 2);
    QCOMPARE(first->name(), QStringLiteral("Committed to the old clip"));
    QCOMPARE(second->name(), QStringLiteral("Second clip"));
    QCOMPARE(label->text(), second->name());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(first->name(), QStringLiteral("Committed on migration"));
    QCOMPARE(label->text(), second->name());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(first->name(), QStringLiteral("First clip"));
    QCOMPARE(label->text(), second->name());
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
    QCOMPARE(commits.size(), 2);
}

void ApplicationGuiTests::adaptiveClipToolbarOverflowKeepsTheNoteShortcut() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), toolbarDocument()));
    auto *clip = namedClip(*context->m_appModel, QStringLiteral("First clip"));
    QVERIFY(clip);
    ClipEditorToolBarView toolbar;
    ThemeManager::instance()->addStyleRoot(&toolbar);
    toolbar.setDataContext(clip);
    toolbar.resize(1400, 48);
    toolbar.show();
    toolbar.activateWindow();
    settleToolbar();
    auto *adaptive = toolbar.findChild<AdaptiveToolBar *>();
    auto *note = toolbar.findChild<Button *>("btnNotePencil");
    QVERIFY(adaptive && note);
    toolbar.resize(qMax(toolbar.minimumWidth(), toolbar.minimumSizeHint().width()),
                   toolbar.height());
    settleToolbar();
    QVERIFY(adaptive->isWidgetOverflowed(note));
    QVERIFY(!note->isVisible());
    QVERIFY(note->isEnabled());
    QVERIFY(toolbar.setEditMode(ClipEditorGlobal::Select));
    toolbar.setFocus();
    QTRY_VERIFY(toolbar.hasFocus());
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    QSignalSpy modes(&toolbar, &ClipEditorToolBarView::editModeChanged);
    QTest::keyClick(&toolbar, Qt::Key_N);
    QTRY_COMPARE(toolbar.editMode(), ClipEditorGlobal::DrawNote);
    QCOMPARE(modes.size(), 1);
    QVERIFY(note->isChecked());
    QVERIFY(!adaptive->overflowPopup()->isVisible());
    auto *label = toolbar.findChild<InlineEditLabel *>("leClipName");
    QVERIFY(label);
    QVERIFY(adaptive->isWidgetOverflowed(label));
    QTest::mouseClick(adaptive->overflowButton(), Qt::LeftButton);
    QTRY_VERIFY(adaptive->overflowPopup()->isVisible() && label->isVisible());
    QTest::mouseDClick(label, Qt::LeftButton);
    auto *overlay = label->parentWidget()->findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay && overlay->isEditing());
    auto *input = overlay->findChild<QLineEdit *>();
    QVERIFY(input);
    QTRY_VERIFY(input->hasFocus());
    const auto beforeTyping = modes.size();
    QTest::keySequence(input, QKeySequence::SelectAll);
    QTest::keyClicks(input, "n");
    QCOMPARE(input->text(), QStringLiteral("n"));
    QCOMPARE(modes.size(), beforeTyping);
    QTest::keyClick(input, Qt::Key_Escape);
    QVERIFY(!overlay->isEditing());

    QPointer<QShortcut> noteShortcut;
    for (auto *shortcut : toolbar.findChildren<QShortcut *>()) {
        if (shortcut->key() == QKeySequence(Qt::Key_N)) {
            QVERIFY(!noteShortcut);
            noteShortcut = shortcut;
        }
    }
    QVERIFY(noteShortcut && noteShortcut->isEnabled());
    noteShortcut->setEnabled(false);
    QTest::mouseDClick(label, Qt::LeftButton);
    QVERIFY(overlay->isEditing());
    QTRY_VERIFY(input->hasFocus());
    settleToolbar();
    QVERIFY(noteShortcut && !noteShortcut->isEnabled());
    QTest::keyClick(input, Qt::Key_Escape);
    adaptive->closeOverflow();
    toolbar.setFocus();
    settleToolbar();
    QTRY_VERIFY(toolbar.hasFocus());
    QVERIFY(noteShortcut && !noteShortcut->isEnabled());
    QCOMPARE(modes.size(), beforeTyping);
    noteShortcut->setEnabled(true);
    toolbar.setDataContext(nullptr);
    settleToolbar();
    QVERIFY(!note->isEnabled());
    const auto mode = toolbar.editMode();
    const auto changes = modes.size();
    QTest::keyClick(&toolbar, Qt::Key_N);
    settleToolbar();
    QCOMPARE(toolbar.editMode(), mode);
    QCOMPARE(modes.size(), changes);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
}
