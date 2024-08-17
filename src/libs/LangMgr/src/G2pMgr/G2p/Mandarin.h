#ifndef MANDARIN_H
#define MANDARIN_H

#include <QObject>

#include <cpp-pinyin/Pinyin.h>

#include "../IG2pFactory.h"

namespace LangMgr {

    class Mandarin final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit Mandarin(QObject *parent = nullptr);
        ~Mandarin() override;

        bool initialize(QString &errMsg) override;

        QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const override;

        QJsonObject config() override;

        [[nodiscard]] bool tone() const;
        void setTone(const bool &tone);

        [[nodiscard]] bool convertNum() const;
        void setConvetNum(const bool &convertNum);

    private:
        Pinyin::Pinyin *m_mandarin;

        bool m_tone = false;
        bool m_convertNum = false;
    };
} // LangMgr

#endif // MANDARIN_H
