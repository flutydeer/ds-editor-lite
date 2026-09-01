#ifndef DSPXLOADSESSION_H
#define DSPXLOADSESSION_H

#include "ProjectLoadSessionBase.h"

#include <QMetaObject>

class IDocumentWorkflowUi;

// DSPX open session: package readiness wait (with the confirm-without-
// metadata escape), background parse and Replace materialization. Created
// by DspxFormatHandler for the Open purpose since migration step 6.
class DspxLoadSession final : public ProjectLoadSessionBase {
    Q_OBJECT

public:
    DspxLoadSession(QString filePath, quint64 requestId, IDocumentWorkflowUi *ui,
                    bool allowWithoutPackageMetadata = false, QObject *parent = nullptr);

private:
    void onStart() override;
    void onCancel() override;
    Task *createParseTask() override;
    void handleParseResult(Task *task) override;
    bool shouldPublishProgress() const override;

    void handlePackageStatus();

    IDocumentWorkflowUi *m_ui = nullptr;
    bool m_allowWithoutPackageMetadata = false;
    QMetaObject::Connection m_packageConnection;
};

#endif // DSPXLOADSESSION_H
