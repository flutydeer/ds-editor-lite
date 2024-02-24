#include "WrapToggleFermataAction.h"

namespace FillLyric {

    WrapToggleFermataAction *WrapToggleFermataAction::build(PhonicModel *model) {
        auto action = new WrapToggleFermataAction;
        action->m_model = model;
        action->m_fermataState = model->fermataState;

        action->m_modelColumnCount = model->columnCount();

        action->m_rawPhonics = model->m_phonics;

        QList<Phonic> tempPhonics;
        if (!model->fermataState) {
            int pos = 0;
            while (pos < action->m_rawPhonics.size()) {
                Phonic phonic = action->m_rawPhonics.at(pos);
                if (phonic.lyricType != TextType::Slur) {
                    tempPhonics.append(phonic);
                    pos++;
                } else {
                    int start = pos;
                    while (pos < action->m_rawPhonics.size() &&
                           action->m_rawPhonics.at(pos).lyricType == TextType::Slur) {
                        pos++;
                    }

                    if (!tempPhonics.isEmpty()) {
                        for (int i = 0; i < pos - start; i++) {
                            tempPhonics.last().fermata.append(
                                action->m_rawPhonics.at(start + i).lyric);
                        }
                    }
                }
            }
        } else {
            for (auto phonic : action->m_rawPhonics) {
                if (phonic.fermata.isEmpty()) {
                    tempPhonics.append(phonic);
                } else {
                    auto fermataList = phonic.fermata;
                    phonic.fermata.clear();
                    tempPhonics.append(phonic);
                    for (const auto &fermata : fermataList) {
                        Phonic temp;
                        temp.lyric = fermata;
                        temp.syllable = fermata;
                        temp.candidates = QStringList(fermata);
                        temp.lyricType = TextType::Slur;
                        tempPhonics.append(temp);
                    }
                }
            }
        }

        action->m_targetPhonics = tempPhonics;
        return action;
    }

    void WrapToggleFermataAction::execute() {
        int maxRow = (int) m_targetPhonics.size() / m_modelColumnCount;
        if (m_targetPhonics.size() % m_modelColumnCount != 0) {
            maxRow++;
        }

        m_model->clear();
        m_model->setRowCount(maxRow);
        m_model->setColumnCount(m_modelColumnCount);

        for (int i = 0; i < m_targetPhonics.size(); i++) {
            m_model->putData(i / m_modelColumnCount, i % m_modelColumnCount, m_targetPhonics[i]);
        }

        m_model->fermataState = !m_model->fermataState;
        m_model->shrinkModel();
    }

    void WrapToggleFermataAction::undo() {
        int maxRow = (int) m_rawPhonics.size() / m_modelColumnCount;
        if (m_rawPhonics.size() % m_modelColumnCount != 0) {
            maxRow++;
        }

        m_model->clear();
        m_model->setRowCount(maxRow);
        m_model->setColumnCount(m_modelColumnCount);

        for (int i = 0; i < m_rawPhonics.size(); i++) {
            m_model->putData(i / m_modelColumnCount, i % m_modelColumnCount, m_rawPhonics[i]);
        }

        m_model->fermataState = !m_model->fermataState;
        m_model->shrinkModel();
    }
} // FillLyric