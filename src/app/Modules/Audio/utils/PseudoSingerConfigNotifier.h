#ifndef PSEUDOSINGERCONFIGNOTIFIER_H
#define PSEUDOSINGERCONFIGNOTIFIER_H

#include <QObject>

namespace talcs {
    class NoteSynthesizerConfig;
}

class PseudoSingerConfigNotifier : public QObject {
    Q_OBJECT
public:
    explicit PseudoSingerConfigNotifier(QObject *parent = nullptr);
    ~PseudoSingerConfigNotifier() override;

    static PseudoSingerConfigNotifier *instance();

    static talcs::NoteSynthesizerConfig config(int synthIndex);
    static void notify(int synthIndex);

signals:
    void configChanged(int synthIndex, const talcs::NoteSynthesizerConfig &config);
};



#endif //PSEUDOSINGERCONFIGNOTIFIER_H
