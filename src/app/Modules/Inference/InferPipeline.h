//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERPIPELINE_H
#define DS_EDITOR_LITE_INFERPIPELINE_H

#include "Model/AppModel/InferPiece.h"
#include "Models/InferInputNote.h"
#include "Models/InferParamCurve.h"
#include "Tasks/InferVarianceTask.h"

#include <QObject>
#include <QStateMachine>

class InferDurationTask;
class InferDurationState;
class UpdateDurationState;
class InferPitchState;
class UpdatePitchState;
class InferVarianceState;
class UpdateVarianceState;
class InferAcousticState;
class UpdateAcousticState;
class QFinalState;

class InferPipeline : public QObject {
    Q_OBJECT

public:
    explicit InferPipeline(InferPiece &piece);
    ~InferPipeline() override;
    [[nodiscard]] int pieceId() const;
    void run();
    [[nodiscard]] InferPiece &piece() const;

    [[nodiscard]] const QList<InferInputNote> &durationResult() const;
    void setDurationResult(const QList<InferInputNote> &result);

    [[nodiscard]] const InferParamCurve &pitchResult() const;
    void setPitchResult(const InferParamCurve &result);

    [[nodiscard]] const InferVarianceTask::InferVarianceResult &varianceResult() const;
    void setVarianceResult(const InferVarianceTask::InferVarianceResult &result);

public slots:
    // App model change signals
    void onExpressivenessChanged();
    void onPitchChanged();
    void onVarianceChanged();

private:
    // Params
    Q_SIGNAL void phonemeNameChanged();
    Q_SIGNAL void phonemeOffsetChanged();
    Q_SIGNAL void expressivenessChanged();
    Q_SIGNAL void pitchChanged();
    Q_SIGNAL void varianceChanged();

    // Inference Options
    Q_SIGNAL void pitchStepsChanged();
    Q_SIGNAL void varianceStepsChanged();
    Q_SIGNAL void acousticStepsChanged();
    Q_SIGNAL void acousticDepthChanged();
    Q_SIGNAL void lazyInferAcousticOn();

    // Events
    Q_SIGNAL void pieceRemoved();
    Q_SIGNAL void playbackStarted();
    Q_SIGNAL void appModelUnlocked();

    void initStates();
    void initTransitions();

    InferPiece &m_piece;

    QStateMachine stateMachine;
    QFinalState *finalState{};
    InferDurationState *inferDurationState{};
    UpdateDurationState *updateDurationState{};
    InferPitchState *inferPitchState{};
    UpdatePitchState *updatePitchState{};
    InferVarianceState *inferVarianceState{};
    UpdateVarianceState *updateVarianceState{};
    QState *awaitingPlaybackState{};
    InferAcousticState *inferAcousticState{};
    UpdateAcousticState *updateAcousticState{};

    QList<InferInputNote> m_durationResult;
    InferParamCurve m_pitchResult;
    InferVarianceTask::InferVarianceResult m_varianceResult;
};

#endif // DS_EDITOR_LITE_INFERPIPELINE_H
