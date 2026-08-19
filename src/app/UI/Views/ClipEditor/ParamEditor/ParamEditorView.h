#ifndef PARAMEDITORVIEW_H
#define PARAMEDITORVIEW_H

#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include <lite/ProjectModel/AppModel/Params.h>
#include <lite/ProjectModel/AppModel/SpeakerMixData.h>

#include <QWidget>
#include <QSet>

#include <optional>

class ParamEditorInfoArea;
class SingingClip;
class ParamEditorGraphicsView;
class ParamEditorToolBarView;
class Button;
class QLabel;
class QVBoxLayout;

class ParamEditorView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorView(QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip);
    [[nodiscard]] ParamEditorGraphicsView *graphicsView() const;

public slots:
    void onForegroundChanged(ParamInfo::Name name);
    void onBackgroundChanged(ParamInfo::Name name);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onPreviousKeyframe() const;
    void onNextKeyframe() const;
    void onSpeakerMixEdited(const SpeakerMixModel::SpeakerMixData &data) const;
    void onEmptyStateAction();
    void onEnableDynamicMix();
    void onBypassDynamicMix() const;
    void onResumeDynamicMix() const;
    void onStopDynamicMix();
    void refreshSpeakerMixToolBar();

private:
    enum class EmptyStateKind { None, SpeakerMix, UnsupportedParameter };
    enum class EmptyStateAction { None, EnableDynamicMix, EditUnsupportedParameter };

    void refreshSpeakerMixEmptyState(const SpeakerMixModel::SpeakerMixData &data);
    void refreshParameterSupportState();
    void setEmptyState(EmptyStateKind kind, const QString &title, const QString &message,
                       const std::optional<QString> &actionText,
                       EmptyStateAction action = EmptyStateAction::None,
                       ParamInfo::Name parameter = ParamInfo::Unknown);
    void hideEmptyState();
    void updateEmptyStateGeometry();
    static bool hasFixedMixBase(const SpeakerMixModel::SpeakerMixData &data);
    static SpeakerMixModel::SpeakerMixData
        dataWithDynamicEnabled(const SpeakerMixModel::SpeakerMixData &data);
    static SpeakerMixModel::SpeakerMixData
        dataWithDynamicStopped(const SpeakerMixModel::SpeakerMixData &data);

    SingingClip *m_clip = nullptr;
    ParamEditorGraphicsView *m_graphicsView;
    ParamEditorInfoArea *m_infoArea;
    ParamEditorToolBarView *m_toolBar;
    QWidget *m_emptyState = nullptr;
    QVBoxLayout *m_emptyStateLayout = nullptr;
    QLabel *m_emptyStateTitle = nullptr;
    QLabel *m_emptyStateMessage = nullptr;
    Button *m_emptyStateActionButton = nullptr;
    QString m_emptyStateMessageText;
    EmptyStateKind m_emptyStateKind = EmptyStateKind::None;
    EmptyStateAction m_emptyStateAction = EmptyStateAction::None;
    ParamInfo::Name m_emptyStateParameter = ParamInfo::Unknown;
    ParamInfo::Name m_foregroundParam = ParamInfo::Breathiness;
    ParamInfo::Name m_backgroundParam = ParamInfo::Tension;
    QSet<ParamInfo::Name> m_acknowledgedUnsupportedParameters;
};

#endif // PARAMEDITORVIEW_H
