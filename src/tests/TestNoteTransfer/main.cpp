#include "Automation/NoteTransfer.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "TestRuntime.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>

#include <limits>
#include <optional>

namespace {
    using Automation::ClipId;
    using Automation::CommandContext;
    using Automation::CoreRuntime;
    using Automation::CurveDraftDto;
    using Automation::NoteId;
    using Automation::TrackId;
    using AutomationTestSupport::TestRuntime;

    int failures = 0;

    void expect(const bool condition, const QString &message) {
        if (condition)
            return;
        ++failures;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
    }

    CommandContext commandContext(const CoreRuntime &runtime) {
        return {
            .expected = runtime.documentVersion(),
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::TrackDraftDto trackDraft(const QString &name) {
        Automation::TrackDraftDto result;
        result.name = name;
        result.defaultLanguage = QStringLiteral("en");
        return result;
    }

    Automation::ClipDraftDto clipDraft(const QString &name) {
        Automation::ClipDraftDto result;
        result.type = Automation::ClipDraftDto::Type::Singing;
        result.properties.name = name;
        result.properties.length = 3840;
        result.properties.clipLen = 3840;
        result.defaultLanguage = QStringLiteral("en");
        return result;
    }

    Automation::NoteDraftDto noteDraft(const int start, const int length, const int key,
                                       const QString &lyric) {
        Automation::NoteDraftDto result;
        result.localStart = start;
        result.length = length;
        result.keyIndex = key;
        result.lyric = lyric;
        result.language = QStringLiteral("en");
        return result;
    }

    CurveDraftDto draw(const int start, const int step, const QList<int> &values) {
        CurveDraftDto result;
        result.type = CurveDraftDto::Type::Draw;
        result.localStart = start;
        result.step = step;
        result.values = values;
        return result;
    }

    CurveDraftDto anchor(const QList<QPair<int, int>> &points) {
        CurveDraftDto result;
        result.type = CurveDraftDto::Type::Anchor;
        for (const auto &[position, value] : points) {
            result.nodes.append({
                .position = position,
                .value = value,
                .interpolation = AnchorNode::Linear,
            });
        }
        return result;
    }

    TrackId insertTrack(CoreRuntime &runtime, const QString &name) {
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        const auto index = project ? project.get().tracks.size() : 0;
        const auto result = runtime.project().insertTrack(commandContext(runtime), index,
                                                          trackDraft(name));
        return result && !result.get().affectedObjects.isEmpty()
                   ? TrackId(result.get().affectedObjects.first().value)
                   : TrackId{};
    }

    ClipId insertClip(CoreRuntime &runtime, const TrackId trackId, const QString &name) {
        const auto result = runtime.project().insertClips(
            commandContext(runtime), {{.trackId = trackId, .clip = clipDraft(name)}});
        return result && !result.get().affectedObjects.isEmpty()
                   ? ClipId(result.get().affectedObjects.first().value)
                   : ClipId{};
    }

    QList<NoteId> insertNotes(CoreRuntime &runtime, const ClipId clipId) {
        const auto result = runtime.notes().insertNotes(
            commandContext(runtime), clipId,
            {noteDraft(100, 100, 60, QStringLiteral("a")),
             noteDraft(300, 200, 62, QStringLiteral("b"))});
        QList<NoteId> ids;
        if (result) {
            for (const auto &object : result.get().affectedObjects)
                ids.append(NoteId(object.value));
        }
        return ids;
    }

    Automation::ParameterSnapshotDto parameter(CoreRuntime &runtime, const ClipId clipId,
                                                const ParamInfo::Name name,
                                                const Param::Type type) {
        const auto result = runtime.parameters().getParameter(runtime.documentVersion().documentId,
                                                              clipId, name, type);
        expect(static_cast<bool>(result), QStringLiteral("parameter query must succeed"));
        return result ? result.get() : Automation::ParameterSnapshotDto{};
    }

    std::optional<int> drawValueAt(const Automation::ParameterSnapshotDto &parameter,
                                   const int tick) {
        for (const auto &curve : parameter.curves) {
            if (curve.type != CurveDraftDto::Type::Draw || curve.step <= 0 ||
                tick < curve.localStart) {
                continue;
            }
            const auto offset = tick - curve.localStart;
            if (offset % curve.step != 0)
                continue;
            const auto index = offset / curve.step;
            if (index >= 0 && index < curve.values.size())
                return curve.values.at(index);
        }
        return std::nullopt;
    }

    bool sameShape(const Automation::ParameterSnapshotDto &left,
                   const Automation::ParameterSnapshotDto &right) {
        if (left.curves.size() != right.curves.size())
            return false;
        for (qsizetype index = 0; index < left.curves.size(); ++index) {
            const auto &a = left.curves.at(index);
            const auto &b = right.curves.at(index);
            if (a.type != b.type || a.localStart != b.localStart || a.step != b.step ||
                a.values != b.values || a.nodes.size() != b.nodes.size()) {
                return false;
            }
            for (qsizetype nodeIndex = 0; nodeIndex < a.nodes.size(); ++nodeIndex) {
                const auto &an = a.nodes.at(nodeIndex);
                const auto &bn = b.nodes.at(nodeIndex);
                if (an.position != bn.position || an.value != bn.value ||
                    an.interpolation != bn.interpolation) {
                    return false;
                }
            }
        }
        return true;
    }

    SingingClip *singingClip(TestRuntime &testRuntime, const ClipId id) {
        Track *track = nullptr;
        auto *clip = testRuntime.model().findClipById(id.value(), track);
        return clip && clip->clipType() == Clip::Singing ? static_cast<SingingClip *>(clip)
                                                        : nullptr;
    }

    void replace(CoreRuntime &runtime, const ClipId clipId, const ParamInfo::Name name,
                 const Param::Type type, const QList<CurveDraftDto> &curves) {
        const auto result = runtime.parameters().replaceParameter(commandContext(runtime), clipId,
                                                                  name, type, curves);
        expect(result && result.get().changed,
               QStringLiteral("parameter fixture must be installed"));
    }

    void testDuplicateWithParameters() {
        TestRuntime testRuntime;
        auto &runtime = testRuntime.runtime();
        const auto sourceTrack = insertTrack(runtime, QStringLiteral("Source"));
        const auto targetTrack = insertTrack(runtime, QStringLiteral("Target"));
        const auto sourceClip = insertClip(runtime, sourceTrack, QStringLiteral("Source Clip"));
        const auto targetClip = insertClip(runtime, targetTrack, QStringLiteral("Target Clip"));
        const auto guiTargetClip =
            insertClip(runtime, targetTrack, QStringLiteral("GUI Target Clip"));
        const auto noteIds = insertNotes(runtime, sourceClip);
        expect(noteIds.size() == 2, QStringLiteral("source notes must be created"));

        replace(runtime, sourceClip, ParamInfo::Pitch, Param::Edited,
                {draw(0, 100, {6000, 6100, 6200, 6300, 6400, 6500, 6600})});
        replace(runtime, sourceClip, ParamInfo::Pitch, Param::Original,
                {draw(100, 100, {7100, 7200, 7300, 7400})});
        replace(runtime, sourceClip, ParamInfo::Energy, Param::Envelope,
                {draw(100, 100, {-1000, -2000, -3000, -4000})});
        replace(runtime, sourceClip, ParamInfo::Gender, Param::Edited,
                {anchor({{150, -100}, {350, 100}})});
        replace(runtime, sourceClip, ParamInfo::Tension, Param::Edited,
                {anchor({{50, -500}, {250, 500}, {550, -500}})});
        replace(runtime, sourceClip, ParamInfo::Breathiness, Param::Edited,
                {anchor({{0, 0}, {std::numeric_limits<int>::max(), 0}})});

        replace(runtime, targetClip, ParamInfo::Pitch, Param::Edited,
                {draw(0, 100,
                      {5000, 5001, 5002, 5003, 5004, 5005, 5006, 5007, 5008, 5009, 5010,
                       5011})});
        replace(runtime, targetClip, ParamInfo::Pitch, Param::Original,
                {draw(0, 100, {8000, 8001, 8002, 8003})});
        replace(runtime, targetClip, ParamInfo::Energy, Param::Envelope,
                {draw(0, 100,
                      {-5000, -5001, -5002, -5003, -5004, -5005, -5006, -5007, -5008,
                       -5009, -5010, -5011})});

        const auto pitchBefore = parameter(runtime, targetClip, ParamInfo::Pitch, Param::Edited);
        const auto originalBefore =
            parameter(runtime, targetClip, ParamInfo::Pitch, Param::Original);
        const auto energyBefore =
            parameter(runtime, targetClip, ParamInfo::Energy, Param::Envelope);
        const auto beforeVersion = runtime.documentVersion();
        const auto duplicate = runtime.notes().duplicateNotes(commandContext(runtime), sourceClip,
                                                              noteIds, targetClip, 603);
        expect(duplicate && duplicate.get().changed &&
                   runtime.documentVersion().revision == beforeVersion.revision + 1,
               QStringLiteral("notes and parameters must commit as one document revision"));
        expect(duplicate && duplicate.get().createdObjects.size() == 2,
               QStringLiteral("duplicate must report both created notes"));

        const auto targetNotes = runtime.notes().getNotes(runtime.documentVersion().documentId,
                                                          targetClip);
        expect(targetNotes && targetNotes.get().size() == 2 &&
                   targetNotes.get().at(0).data.localStart == 603 &&
                   targetNotes.get().at(1).data.localStart == 803,
               QStringLiteral("cross-clip duplicate must preserve relative note layout"));

        const auto pitchAfter = parameter(runtime, targetClip, ParamInfo::Pitch, Param::Edited);
        expect(drawValueAt(pitchAfter, 500) == 5005 && drawValueAt(pitchAfter, 1100) == 5011,
               QStringLiteral("target parameter values outside the pasted range must remain"));
        expect(drawValueAt(pitchAfter, 603) == 6100 && drawValueAt(pitchAfter, 703) == 6200 &&
                   drawValueAt(pitchAfter, 803) == 6300 && drawValueAt(pitchAfter, 903) == 6400,
               QStringLiteral("cropped source draw curve must be translated into target range"));

        const auto originalAfter =
            parameter(runtime, targetClip, ParamInfo::Pitch, Param::Original);
        expect(sameShape(originalBefore, originalAfter),
               QStringLiteral("inference-generated Original parameters must not be copied"));
        const auto energyAfter = parameter(runtime, targetClip, ParamInfo::Energy, Param::Envelope);
        expect(drawValueAt(energyAfter, 500) == -5005 && drawValueAt(energyAfter, 1100) == -5011 &&
                   drawValueAt(energyAfter, 603) == -1000 &&
                   drawValueAt(energyAfter, 903) == -4000,
               QStringLiteral("Envelope must copy while preserving its target exterior"));

        const auto genderAfter = parameter(runtime, targetClip, ParamInfo::Gender, Param::Edited);
        expect(genderAfter.curves.size() == 1 &&
                   genderAfter.curves.first().type == CurveDraftDto::Type::Anchor &&
                   genderAfter.curves.first().nodes.first().position == 653 &&
                   genderAfter.curves.first().nodes.last().position == 853,
               QStringLiteral("fully selected Anchor curves must remain editable anchors"));
        const auto tensionAfter = parameter(runtime, targetClip, ParamInfo::Tension, Param::Edited);
        expect(!tensionAfter.curves.isEmpty() &&
                   tensionAfter.curves.first().type == CurveDraftDto::Type::Draw &&
                   tensionAfter.curves.first().localStart == 603,
               QStringLiteral("partially selected Anchor curves must be shape-safe sampled draws"));
        const auto breathinessAfter =
            parameter(runtime, targetClip, ParamInfo::Breathiness, Param::Edited);
        expect(breathinessAfter.curves.size() == 1 &&
                   breathinessAfter.curves.first().type == CurveDraftDto::Type::Draw &&
                   breathinessAfter.curves.first().localStart == 603 &&
                   breathinessAfter.curves.first().values.size() == 80,
               QStringLiteral("note transfer must sample only the selected anchor intersection"));

        const auto undo = runtime.history().undo(commandContext(runtime));
        expect(undo && undo.get().changed,
               QStringLiteral("one undo must revert the complete duplicate"));
        const auto notesAfterUndo = runtime.notes().getNotes(runtime.documentVersion().documentId,
                                                             targetClip);
        expect(notesAfterUndo && notesAfterUndo.get().isEmpty(),
               QStringLiteral("undo must remove all duplicated notes"));
        expect(sameShape(pitchBefore,
                         parameter(runtime, targetClip, ParamInfo::Pitch, Param::Edited)) &&
                   sameShape(energyBefore,
                             parameter(runtime, targetClip, ParamInfo::Energy, Param::Envelope)),
               QStringLiteral("undo must restore every target parameter layer"));

        const auto redo = runtime.history().redo(commandContext(runtime));
        expect(redo && redo.get().changed,
               QStringLiteral("one redo must restore the complete duplicate"));
        const auto notesAfterRedo = runtime.notes().getNotes(runtime.documentVersion().documentId,
                                                             targetClip);
        expect(notesAfterRedo && notesAfterRedo.get().size() == 2 &&
                   drawValueAt(parameter(runtime, targetClip, ParamInfo::Pitch, Param::Edited),
                               603) == 6100,
               QStringLiteral("redo must restore notes and parameter curves together"));

        auto *sourceModel = singingClip(testRuntime, sourceClip);
        expect(sourceModel != nullptr, QStringLiteral("source model clip must resolve"));
        if (sourceModel) {
            QList<Note *> sourceNotes;
            for (const auto id : noteIds)
                sourceNotes.append(sourceModel->findNoteById(id.value()));
            NotesParamsInfo clipboard;
            clipboard.payload = Automation::captureNoteTransfer(*sourceModel, sourceNotes);
            const auto encoded = NotesParamsInfo::serializeToJson(clipboard);
            const auto decoded = NotesParamsInfo::deserializeFromJson(encoded);
            expect(decoded.payload.notes.size() == clipboard.payload.notes.size() &&
                       decoded.payload.parameters.size() == clipboard.payload.parameters.size() &&
                       decoded.payload.sourceStart == clipboard.payload.sourceStart &&
                       decoded.payload.sourceEnd == clipboard.payload.sourceEnd,
                   QStringLiteral("GUI transport serialization must retain the domain payload"));

            auto malformed = encoded;
            auto malformedNotes = malformed.value(QStringLiteral("notes")).toArray();
            auto malformedNote = malformedNotes.first().toObject();
            malformedNote.insert(QStringLiteral("localStart"), std::numeric_limits<int>::max());
            malformedNote.insert(QStringLiteral("length"), 1);
            malformedNotes.replace(0, malformedNote);
            malformed.insert(QStringLiteral("notes"), malformedNotes);
            const auto rejected = NotesParamsInfo::deserializeFromJson(malformed);
            expect(rejected.payload.notes.isEmpty() && rejected.payload.parameters.isEmpty(),
                   QStringLiteral("GUI transport must discard overflowing note geometry"));

            const auto paste = runtime.notes().pasteNotes(commandContext(runtime), guiTargetClip,
                                                          1203, decoded.payload);
            const auto pastedNotes = runtime.notes().getNotes(
                runtime.documentVersion().documentId, guiTargetClip);
            expect(paste && paste.get().changed && pastedNotes && pastedNotes.get().size() == 2 &&
                       pastedNotes.get().first().data.localStart == 1203 &&
                       drawValueAt(parameter(runtime, guiTargetClip, ParamInfo::Pitch, Param::Edited),
                                   1203) == 6100,
                   QStringLiteral("GUI payload paste must use the same note-and-parameter commit"));
        }
    }

} // namespace

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    testDuplicateWithParameters();
    QTextStream(stdout) << "Note transfer: " << (failures == 0 ? "PASS" : "FAIL") << Qt::endl;
    return failures == 0 ? 0 : 1;
}
