//
// Created by FlutyDeer on 2026/6/8.
//

#ifndef DS_EDITOR_LITE_AWAITINGEDITSESSIONAPPLYSTATE_H
#define DS_EDITOR_LITE_AWAITINGEDITSESSIONAPPLYSTATE_H

#include <QMetaObject>
#include <QState>

class InferPipeline;

class AwaitingEditSessionApplyState final : public QState {
    Q_OBJECT

public:
    enum class Stage { Duration, Pitch, Variance, Acoustic };

    explicit AwaitingEditSessionApplyState(InferPipeline &pipeline, Stage stage,
                                           QState *parent = nullptr);
    ~AwaitingEditSessionApplyState() override = default;

signals:
    void resumeRequested();

protected:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

private:
    [[nodiscard]] QString stageName() const;
    void requestResume();

    InferPipeline &m_pipeline;
    Stage m_stage;
    bool m_resumeRequested = false;
    QMetaObject::Connection m_editSessionEndedConnection;
};

#endif // DS_EDITOR_LITE_AWAITINGEDITSESSIONAPPLYSTATE_H
