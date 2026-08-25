#ifndef UNSUPPORTEDPARAMETERPROMPTSTATE_H
#define UNSUPPORTEDPARAMETERPROMPTSTATE_H

#include <lite/ProjectModel/AppModel/Params.h>

#include <QSet>

class UnsupportedParameterPromptState final {
public:
    void resetForProject() {
        m_acknowledgedParameters.clear();
    }

    void acknowledge(const ParamInfo::Name parameter) {
        if (parameter != ParamInfo::Unknown)
            m_acknowledgedParameters.insert(parameter);
    }

    [[nodiscard]] bool shouldPrompt(const ParamInfo::Name parameter, const bool supported) const {
        return !supported && !m_acknowledgedParameters.contains(parameter);
    }

private:
    QSet<ParamInfo::Name> m_acknowledgedParameters;
};

#endif // UNSUPPORTEDPARAMETERPROMPTSTATE_H
