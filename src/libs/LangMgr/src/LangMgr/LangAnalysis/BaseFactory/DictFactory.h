#ifndef DICTFACTORY_H
#define DICTFACTORY_H

#include <QMap>
#include <QString>
#include <QDebug>

#include "../../ILanguageFactory.h"

namespace LangMgr {

    class TrieNode {
    public:
        QMap<QChar, TrieNode *> children;
        bool isEnd;

        TrieNode() : isEnd(false) {
        }
    };

    class Trie {
    public:
        Trie();
        ~Trie();

        void insert(const QString &word) const;

        [[nodiscard]] bool search(const QString &word) const;

        friend class DictFactory;

    protected:
        TrieNode *root;
        int depth = 0;
    };

    class DictFactory : public ILanguageFactory {
        Q_OBJECT
    public:
        explicit DictFactory(const QString &id, QObject *parent = nullptr);
        ~DictFactory() override;

        virtual void loadDict();

        [[nodiscard]] bool contains(const QChar &c) const override;
        [[nodiscard]] bool contains(const QString &input) const override;

        [[nodiscard]] QString randString() const override;

    protected:
        Trie *m_trie;
    };

} // LangMgr

#endif // DICTFACTORY_H
