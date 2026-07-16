//
// Created by fluty on 2023/11/14.
//

#include "TracksGraphicsView.h"

#include "TracksGraphicsScene.h"
#include "Controller/AppController.h"
#include "Controller/ClipController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AbstractClipView.h"
#include "GraphicsItem/AudioClipView.h"
#include "GraphicsItem/SingingClipView.h"
#include "GraphicsItem/TrackEditorBackgroundView.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Model/ClipboardDataModel/ClipsInfo.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Extractors/MidiExtractController.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Menu.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Utils/IconUtils.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "UI/Views/Common/ScrollBarView.h"
#include "Utils/TimelineSnapUtils.h"

#include <QClipboard>
#include <QFileDialog>
#include <climits>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>

#include <TalcsWidgets/AudioFileDialog.h>

TracksGraphicsView::TracksGraphicsView(TracksGraphicsScene *scene, const QWidget *parent)
    : TimeGraphicsView(scene, parent), m_scene(scene) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksGraphicsView");
    setScaleYMin(0.575);
    setEnsureSceneFillViewY(false);
    setScaleXMax(10000);
    setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    setDragBehavior(DragBehavior::RectSelect);
    setMinimumHeight(0);

    m_actionNewSingingClip = new QAction(tr("New singing clip"), this);
    m_actionNewSingingClip->setIcon(
        IconUtils::menuIcon(QStringLiteral(":/svg/icons/midi_clip_16_filled.svg")));
    connect(m_actionNewSingingClip, &QAction::triggered, this,
            &TracksGraphicsView::onNewSingingClip);

    m_actionAddAudioClip = new QAction(tr("Insert audio clip..."), this);
    m_actionAddAudioClip->setIcon(
        IconUtils::menuIcon(QStringLiteral(":/svg/icons/audio_clip_16_filled.svg")));
    connect(m_actionAddAudioClip, &QAction::triggered, this, &TracksGraphicsView::onAddAudioClip);

    connect(appStatus, &AppStatus::activeClipIdChanged, this, [this](const int clipId) {
        if (clipId == -1) {
            resetActiveClips();
            return;
        }

        if (const auto clipItem = findClipById(clipId)) {
            resetActiveClips();
            clipItem->setActiveClip(true);
        } else
            qFatal() << "Clip not found: " << clipId;
    });
}

void TracksGraphicsView::setSnapGrid(TrackEditorBackgroundView *grid) {
    m_snapGrid = grid;
}

int TracksGraphicsView::snapStep(const bool snapOff) const {
    if (snapOff)
        return 1;
    return m_snapGrid ? m_snapGrid->logicalGridStepForCurrentScale()
                      : TimelineSnapUtils::quantizeToTicks(128);
}

QList<int> TracksGraphicsView::selectedClipsId() const {
    QList<int> result;
    for (const auto clipItem : selectedClipItems())
        result.append(clipItem->id());
    return result;
}

void TracksGraphicsView::onNewSingingClip() const {
    trackController->onNewSingingClip(m_trackIndex, m_tick);
}

void TracksGraphicsView::onAddAudioClip() {
    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto io = talcs::AudioFileDialog::getOpenAudioFileIO(AudioContext::instance()->formatManager(),
                                                         fileName, userData, entryClassName, this,
                                                         tr("Select an Audio File"), ".");

    QByteArray dataBuffer;
    QDataStream o(&dataBuffer, QIODevice::WriteOnly);
    o << userData;
    const QJsonObject workspace{
        {"userData",       QString::fromLatin1(dataBuffer.toBase64())},
        {"entryClassName", entryClassName                            },
    };

    if (fileName.isNull())
        return;
    const auto track = appModel->tracks().at(m_trackIndex);
    trackController->onAddAudioClip(fileName, io, workspace, track->id(), m_tick);
}

void TracksGraphicsView::onDeleteTriggered() const {
    trackController->onRemoveClips(selectedClipsId());
}

void TracksGraphicsView::onExtractMidiTriggered(const int clipId) {
    const auto audioClip = dynamic_cast<AudioClip *>(appModel->findClipById(clipId));
    Q_ASSERT(audioClip);
    midiExtractController->runExtractMidi(audioClip);
}

void TracksGraphicsView::onRelocateAudioTriggered(const int clipId) {
    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto io = talcs::AudioFileDialog::getOpenAudioFileIO(AudioContext::instance()->formatManager(),
                                                         fileName, userData, entryClassName, this,
                                                         tr("Select an Audio File"), ".");
    if (fileName.isNull())
        return;

    QByteArray dataBuffer;
    QDataStream o(&dataBuffer, QIODevice::WriteOnly);
    o << userData;
    const QJsonObject workspace{
        {"userData",       QString::fromLatin1(dataBuffer.toBase64())},
        {"entryClassName", entryClassName                            },
    };
    trackController->onRelocateAudioClip(clipId, fileName, io, workspace);
}

bool TracksGraphicsView::event(QEvent *event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        const auto key = dynamic_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape) {
            discardAction();
        }
    } else if (event->type() == QEvent::WindowDeactivate) {
        discardAction();
    }
    return TimeGraphicsView::event(event);
}

bool TracksGraphicsView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Leave)
        clearPastePreviewClipViews();
    return TimeGraphicsView::eventFilter(watched, event);
}

void TracksGraphicsView::mousePressEvent(QMouseEvent *event) {
    // 在滚动条上按下时，交还给基类处理
    if (dynamic_cast<ScrollBarView *>(itemAt(event->pos()))) {
        TimeGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    cancelRequested = false;

    if (const auto item = itemAt(event->pos())) {
        if (const auto clipItem = dynamic_cast<AbstractClipView *>(item)) {
            qDebug() << "TracksGraphicsView::mousePressEvent mouse down on clip";
            if (selectedClipItems().count() <= 1 || !selectedClipItems().contains(clipItem))
                clearSelections();
            clipItem->setSelected(true);
            trackController->setActiveClip(clipItem->id());
            if (event->button() != Qt::LeftButton) {
                m_mouseMoveBehavior = None;
                setCursor(Qt::ArrowCursor);
            } else {
                prepareForMovingOrResizingClip(event, clipItem);
            }
        } else {
            clearSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
    }
    syncClipSelectionToAppStatus();
    event->ignore();
}

void TracksGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (cancelRequested) {
        TimeGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (event->modifiers() == Qt::AltModifier)
        m_tempQuantizeOff = true;
    else
        m_tempQuantizeOff = false;

    const auto curPos = mapToScene(event->pos());
    const auto dx = (curPos.x() - m_mouseDownPos.x()) / scaleX() /
                    TracksEditorGlobal::pixelsPerQuarterNote * AppGlobal::ticksPerQuarterNote;

    int start;
    int left;
    int clipLen;
    const int delta = qRound(dx);
    const int quantize = snapStep(m_tempQuantizeOff);
    if (m_mouseMoveBehavior == Move) {
        m_movedBeforeMouseUp = true;
        left = TimelineSnapUtils::snapNearest(m_mouseDownStart + m_mouseDownClipStart + delta,
                                              quantize);
        start = left - m_mouseDownClipStart;
        m_currentEditingClip->setStart(start);
        const auto targetTrackIndex = m_scene->trackIndexAt(curPos.y());
        if (targetTrackIndex >= 0) {
            m_currentEditingClip->setTrackIndex(targetTrackIndex);
            const auto colorIndex = appModel->tracks().at(targetTrackIndex)->colorIndex();
            m_currentEditingClip->setColorIndex(colorIndex);
            clipController->notifyLiveTrackColorChanged(colorIndex);
        }
    } else if (m_mouseMoveBehavior == ResizeLeft) {
        m_movedBeforeMouseUp = true;
        left = TimelineSnapUtils::snapNearest(m_mouseDownStart + m_mouseDownClipStart + delta,
                                              quantize);
        start = m_mouseDownStart;
        const int clipStart = left - start;
        clipLen = m_mouseDownStart + m_mouseDownClipStart + m_mouseDownClipLen - left;
        if (clipLen <= 0) {
            TimeGraphicsView::mouseMoveEvent(event);
            return;
        }

        if (clipStart < 0) {
            m_currentEditingClip->setClipStart(0);
            m_currentEditingClip->setClipLen(m_mouseDownClipStart + m_mouseDownClipLen);
        } else if (clipStart <= m_mouseDownClipStart + m_mouseDownClipLen) {
            m_currentEditingClip->setClipStart(clipStart);
            m_currentEditingClip->setClipLen(clipLen);
        } else {
            m_currentEditingClip->setClipStart(m_mouseDownClipStart + m_mouseDownClipLen);
            m_currentEditingClip->setClipLen(0);
        }
    } else if (m_mouseMoveBehavior == ResizeRight) {
        m_movedBeforeMouseUp = true;
        const int right = TimelineSnapUtils::snapNearest(
            m_mouseDownStart + m_mouseDownClipStart + m_mouseDownClipLen + delta, quantize);
        clipLen = right - (m_mouseDownStart + m_mouseDownClipStart);
        if (clipLen <= 0) {
            TimeGraphicsView::mouseMoveEvent(event);
            return;
        }

        const auto curClipStart = m_currentEditingClip->clipStart();
        const auto curLength = m_currentEditingClip->length();
        if (!m_currentEditingClip->canResizeLength()) { // Audio Clip
            if (curClipStart + clipLen >= curLength)
                m_currentEditingClip->setClipLen(curLength - curClipStart);
            else
                m_currentEditingClip->setClipLen(clipLen);
        } else { // Singing Clip
            auto targetLen = curClipStart + clipLen;
            if (targetLen < m_currentEditingClip->contentLength())
                targetLen = m_currentEditingClip->contentLength();
            m_currentEditingClip->setLength(targetLen);
            m_currentEditingClip->setClipLen(clipLen);
        }
    }
    TimeGraphicsView::mouseMoveEvent(event);
}

void TracksGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_mouseMoveBehavior != None && m_movedBeforeMouseUp && !cancelRequested)
        commitAction();
    else {
        editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
        resetEditState();
    }
    cancelRequested = false;
    syncClipSelectionToAppStatus();
    TimeGraphicsView::mouseReleaseEvent(event);
}

void TracksGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    const auto scenePos = mapToScene(event->position().toPoint());
    m_trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (m_trackIndex == -1)
        return;

    const auto tick = m_scene->tickAt(scenePos.x());
    if (const auto item = itemAt(event->pos())) {
        if (auto clipItem = dynamic_cast<AbstractClipView *>(item)) {
            // appController->setActivePanel(AppGlobal::ClipEditor);
            if (appStatus->clipPanelCollapsed) {
                appController->setTrackAndClipPanelCollapsed(false, false);
                clipController->centerAt(playbackController->position(), 60);
            }
        } else if (dynamic_cast<TrackEditorBackgroundView *>(item)) {
            m_tick = TimelineSnapUtils::snapDown(tick, snapStep(false));
            onNewSingingClip();
        }
    }

    TimeGraphicsView::mouseDoubleClickEvent(event);
}

void TracksGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    const auto scenePos = mapToScene(event->pos());
    const auto trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (trackIndex == -1)
        return;

    const auto tick = m_scene->tickAt(scenePos.x());
    if (const auto item = itemAt(event->pos())) {
        if (dynamic_cast<TrackEditorBackgroundView *>(item)) {
            m_trackIndex = trackIndex;
            m_tick = TimelineSnapUtils::snapDown(tick, snapStep(false));

            Menu menu(this);
            menu.installEventFilter(this);
            menu.addAction(m_actionNewSingingClip);
            menu.addAction(m_actionAddAudioClip);
            menu.addSeparator();

            const auto mimeData = QGuiApplication::clipboard()->mimeData();
            const auto hasClipData =
                mimeData &&
                mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip));
            const auto actionPaste = menu.addAction(tr("&Paste"));
            actionPaste->setIcon(
                IconUtils::menuIcon(QStringLiteral(":/svg/icons/clipboard_paste_16_regular.svg")));
            actionPaste->setEnabled(hasClipData);
            if (hasClipData) {
                const auto array =
                    mimeData->data(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip));
                const auto json = QJsonDocument::fromJson(array);
                ClipsInfo info = ClipsInfo::deserializeFromJson(json.object());
                const auto pasteTick = m_tick;
                const auto previewTick = TimelineSnapUtils::snapNearest(
                    pasteTick, TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize));
                const auto pasteTrack = trackIndex;
                connect(actionPaste, &QAction::triggered, this, [info, pasteTick, pasteTrack] {
                    trackController->pasteClips(info, pasteTick, pasteTrack);
                });

                int firstClipStart = INT_MAX;
                for (const auto clip : info.clips)
                    firstClipStart = qMin(firstClipStart, clip->start());

                connect(actionPaste, &QAction::hovered, this,
                        [this, info, previewTick, pasteTrack, firstClipStart] {
                            if (!m_pastePreviewClipViews.isEmpty())
                                return;
                            for (int i = 0; i < info.clips.count(); i++) {
                                const auto clip = info.clips.at(i);
                                int targetTrack = pasteTrack + info.trackIndexOffsets.value(i, 0);
                                targetTrack =
                                    qBound(0, targetTrack, appModel->tracks().count() - 1);
                                const auto track = appModel->tracks().at(targetTrack);
                                const int targetStart =
                                    previewTick + (clip->start() - firstClipStart);

                                AbstractClipView *clipView = nullptr;
                                if (clip->clipType() == IClip::Singing) {
                                    const auto sc = static_cast<SingingClip *>(clip);
                                    auto view = new SingingClipView(-1);
                                    view->loadCommonProperties(Clip::ClipCommonProperties(*clip));
                                    view->setTrackIndex(targetTrack);
                                    view->setStart(targetStart);
                                    view->loadNotes(sc->notes());
                                    view->setSingerName(track->singerInfo().name());
                                    view->setSpeakerName(SpeakerMixDisplayUtils::speakerDisplayName(
                                        track->singerInfo(), track->speakerInfo(),
                                        track->speakerMixData()));
                                    view->setDefaultLanguage(sc->defaultLanguage());
                                    clipView = view;
                                } else if (clip->clipType() == IClip::Audio) {
                                    const auto ac = static_cast<AudioClip *>(clip);
                                    auto view = new AudioClipView(-1);
                                    view->loadCommonProperties(Clip::ClipCommonProperties(*clip));
                                    view->setTrackIndex(targetTrack);
                                    view->setStart(targetStart);
                                    view->setPath(ac->path());
                                    view->setTempo(appModel->tempo());
                                    view->setAudioInfo(ac->audioInfo());
                                    clipView = view;
                                }

                                if (clipView) {
                                    clipView->setColorIndex(track->colorIndex());
                                    clipView->setOpacity(0.35);
                                    clipView->setAcceptedMouseButtons(Qt::NoButton);
                                    clipView->setAcceptHoverEvents(false);
                                    clipView->setFlag(QGraphicsItem::ItemIsSelectable, false);
                                    m_scene->addCommonItem(clipView);
                                    m_pastePreviewClipViews.append(clipView);
                                }
                            }
                        });

                for (auto a : menu.actions()) {
                    if (a != actionPaste && !a->isSeparator())
                        connect(a, &QAction::hovered, this,
                                [this] { clearPastePreviewClipViews(); });
                }

                connect(&menu, &QMenu::aboutToHide, this, [this] { clearPastePreviewClipViews(); });
            }

            menu.exec(event->globalPos());
        } else if (auto clip = dynamic_cast<AbstractClipView *>(item)) {
            Menu menu(this);

            if (clip->clipType() == IClip::Audio) {
                const auto audioClip =
                    dynamic_cast<AudioClip *>(appModel->findClipById(clip->id()));
                const bool missing =
                    audioClip && audioClip->pathStatus() == AudioClip::PathStatus::Missing;

                const auto actionRelocate = new QAction(tr("Relink Audio File..."));
                actionRelocate->setIcon(
                    IconUtils::menuIcon(QStringLiteral(":/svg/icons/link_16_filled.svg")));
                connect(actionRelocate, &QAction::triggered, this,
                        [clip, this] { onRelocateAudioTriggered(clip->id()); });

                const auto actionExtractMidi = new QAction(tr("Extract MIDI Score"));
                actionExtractMidi->setIcon(
                    IconUtils::menuIcon(QStringLiteral(":/svg/icons/arrow_export_16_regular.svg")));
                connect(actionExtractMidi, &QAction::triggered, this,
                        [clip, this] { onExtractMidiTriggered(clip->id()); });

                // When the file is missing, put relink first in the menu as the nearest repair entry
                if (missing) {
                    menu.addAction(actionRelocate);
                    menu.addSeparator();
                    menu.addAction(actionExtractMidi);
                } else {
                    menu.addAction(actionExtractMidi);
                    menu.addAction(actionRelocate);
                }
                menu.addSeparator();
            }

            const auto actionCut = menu.addAction(tr("Cu&t"));
            actionCut->setIcon(
                IconUtils::menuIcon(QStringLiteral(":/svg/icons/cut_16_regular.svg")));
            connect(actionCut, &QAction::triggered, this,
                    [] { trackController->cutSelectedClips(); });

            const auto actionCopy = menu.addAction(tr("&Copy"));
            actionCopy->setIcon(
                IconUtils::menuIcon(QStringLiteral(":/svg/icons/copy_16_regular.svg")));
            connect(actionCopy, &QAction::triggered, this,
                    [] { trackController->copySelectedClips(); });

            const auto actionDelete = menu.addAction(tr("&Delete"));
            actionDelete->setIcon(
                IconUtils::menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
            connect(actionDelete, &QAction::triggered, this,
                    &TracksGraphicsView::onDeleteTriggered);

            menu.exec(event->globalPos());
        } else {
            TimeGraphicsView::contextMenuEvent(event);
        }
    }
}

void TracksGraphicsView::discardAction() {
    cancelRequested = true;
    if (m_currentEditingClip && m_movedBeforeMouseUp) {
        m_currentEditingClip->setStart(m_mouseDownStart);
        m_currentEditingClip->setClipStart(m_mouseDownClipStart);
        m_currentEditingClip->setLength(m_mouseDownLength);
        m_currentEditingClip->setClipLen(m_mouseDownClipLen);
        m_currentEditingClip->setTrackIndex(m_mouseDownTrackIndex);
        m_currentEditingClip->setColorIndex(m_mouseDownColorIndex);
        clipController->notifyLiveTrackColorChanged(m_mouseDownColorIndex);
    }
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    resetEditState();
}

void TracksGraphicsView::commitAction() {
    if (m_currentEditingClip && m_movedBeforeMouseUp) {
        const Clip::ClipCommonProperties args(*m_currentEditingClip);
        const int newTrackIndex = m_currentEditingClip->trackIndex();
        trackController->onClipPropertyChanged(args, newTrackIndex);
    }
    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    resetEditState();
}

void TracksGraphicsView::resetEditState() {
    m_mouseMoveBehavior = None;
    m_movedBeforeMouseUp = false;
    m_currentEditingClip = nullptr;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void TracksGraphicsView::clearPastePreviewClipViews() {
    for (auto view : m_pastePreviewClipViews) {
        m_scene->removeCommonItem(view);
        delete view;
    }
    m_pastePreviewClipViews.clear();
}

void TracksGraphicsView::syncClipSelectionToAppStatus() const {
    const auto ids = selectedClipsId();
    appStatus->selectedClips = ids;
    if (!ids.isEmpty()) {
        Track *track;
        appModel->findClipById(ids.first(), track);
        const auto trackIndex = appModel->tracks().indexOf(track);
        if (trackIndex >= 0)
            appStatus->selectedTrackIndex = trackIndex;
    }
}

void TracksGraphicsView::prepareForMovingOrResizingClip(const QMouseEvent *event,
                                                        AbstractClipView *clipItem) {
    const auto scenePos = mapToScene(event->pos());

    const bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (!ctrlDown) {
        if (selectedClipItems().count() <= 1 || !selectedClipItems().contains(clipItem))
            clearSelections();
        clipItem->setSelected(true);
    } else {
        clipItem->setSelected(!clipItem->isSelected());
    }
    const auto rPos = clipItem->mapFromScene(scenePos);
    const auto rx = rPos.x();
    if (rx >= 0 && rx <= AppGlobal::resizeTolerance) {
        m_mouseMoveBehavior = ResizeLeft;
        clearSelections();
        clipItem->setSelected(true);
    } else if (rx >= clipItem->rect().width() - AppGlobal::resizeTolerance &&
               rx <= clipItem->rect().width()) {
        m_mouseMoveBehavior = ResizeRight;
        clearSelections();
        clipItem->setSelected(true);
    } else {
        m_mouseMoveBehavior = Move;
    }

    m_currentEditingClip = clipItem;
    m_mouseDownPos = scenePos;
    m_mouseDownStart = m_currentEditingClip->start();
    m_mouseDownClipStart = m_currentEditingClip->clipStart();
    m_mouseDownLength = m_currentEditingClip->length();
    m_mouseDownClipLen = m_currentEditingClip->clipLen();
    m_mouseDownTrackIndex = m_currentEditingClip->trackIndex();
    m_mouseDownColorIndex = m_currentEditingClip->colorIndex();
    m_movedBeforeMouseUp = false;

    QList<int> clipIds;
    for (const auto clip : selectedClipItems())
        clipIds.append(clip->id());
    if (clipIds.isEmpty())
        clipIds.append(clipItem->id());
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Clip, clipItem->id(), clipIds);
    appStatus->currentEditObject = AppStatus::EditObjectType::Clip;
}

AbstractClipView *TracksGraphicsView::findClipById(const int id) const {
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->id() == id)
                return clip;
    return nullptr;
}

void TracksGraphicsView::clearSelections() const {
    for (const auto item : m_scene->items())
        if (item->isSelected())
            item->setSelected(false);
    syncClipSelectionToAppStatus();
}

void TracksGraphicsView::resetActiveClips() const {
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->activeClip())
                clip->setActiveClip(false);
}

QList<AbstractClipView *> TracksGraphicsView::selectedClipItems() const {
    QList<AbstractClipView *> result;
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->isSelected())
                result.append(clip);
    return result;
}

void TracksGraphicsView::changeEvent(QEvent *event) {
    TimeGraphicsView::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        m_actionNewSingingClip->setText(tr("New singing clip"));
        m_actionAddAudioClip->setText(tr("Insert audio clip..."));
    }
}
