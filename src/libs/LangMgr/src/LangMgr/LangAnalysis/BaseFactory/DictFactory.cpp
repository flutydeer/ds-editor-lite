#include "DictFactory.h"

#include "MultiCharFactory.h"
#include <QDebug>

namespace LangMgr {

    Trie::Trie() {
        root = new TrieNode();
    }

    Trie::~Trie() {
        delete root;
    }

    void Trie::insert(const QString &word) const {
        TrieNode *node = root;
        for (const auto &c : word) {
            if (!node->children.contains(c)) {
                node->children[c] = new TrieNode();
            }
            node = node->children[c];
        }
        node->isEnd = true;
    }

    bool Trie::search(const QString &word) const {
        TrieNode *node = root;
        for (const auto &c : word) {
            if (!node->children.contains(c)) {
                return false;
            }
            node = node->children[c];
        }
        return node->isEnd;
    }

    DictFactory::DictFactory(const QString &id, QObject *parent) : ILanguageFactory(id, parent) {
        m_trie = new Trie();
    }

    DictFactory::~DictFactory() {
        delete m_trie;
    }

    void DictFactory::loadDict() {
    }

    bool DictFactory::contains(const QChar &c) const {
        return m_trie->search(c);
    }

    bool DictFactory::contains(const QString &input) const {
        return m_trie->search(input);
    }

} // LangMgr