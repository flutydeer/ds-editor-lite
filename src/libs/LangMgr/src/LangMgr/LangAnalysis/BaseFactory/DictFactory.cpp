#include "DictFactory.h"

#include "MultiCharFactory.h"

#include <QRandomGenerator>

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

    QList<LangNote> DictFactory::split(const QString &input) const {
        QList<LangNote> result;

        int pos = 0;
        while (pos < input.length()) {
            const auto &currentChar = input[pos];
            LangNote note;

            if (m_trie->search(currentChar)) {
                const int start = pos;
                TrieNode *currentNode = m_trie->root;

                // Greedily match the longest possible word
                for (int i = pos; i < input.length(); ++i) {
                    if (currentNode->children.contains(input[i])) {
                        currentNode = currentNode->children[input[i]];
                        if (currentNode->isEnd) {
                            pos++;
                        }
                    } else {
                        break;
                    }
                }

                if (pos > start) {
                    note.lyric = input.mid(start, pos - start);
                    note.language = id();
                    note.category = category();
                } else {
                    note.lyric = currentChar;
                    note.language = QStringLiteral("unknown");
                    note.category = QStringLiteral("unknown");
                    pos++;
                }
            } else {
                const int start = pos;
                while (pos < input.length() && !m_trie->search(input[pos])) {
                    pos++;
                }
                note.lyric = input.mid(start, pos - start);
                note.language = QStringLiteral("unknown");
                note.category = QStringLiteral("unknown");
            }

            if (!note.lyric.isEmpty()) {
                result.append(note);
            }
        }

        return result;
    }

    QString DictFactory::randString() const {
        TrieNode *node = m_trie->root;
        QString word;
        while (true) {
            if (node->isEnd && QRandomGenerator::global()->bounded(2) == 0) {
                return word;
            }

            QList<QChar> keys = node->children.keys();
            if (keys.isEmpty()) {
                return word;
            }
            QChar randomChar = keys[QRandomGenerator::global()->bounded(keys.size())];
            node = node->children[randomChar];
            word.append(randomChar);
        }
    }

} // LangMgr