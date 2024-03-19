#include "ILanguageFactory.h"
#include "ILanguageFactory_p.h"

#include <QDebug>
#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

#include "../G2pMgr/IG2pManager.h"

namespace LangMgr {

    ILanguageFactoryPrivate::ILanguageFactoryPrivate() {
    }

    ILanguageFactoryPrivate::~ILanguageFactoryPrivate() = default;

    void ILanguageFactoryPrivate::init() {
    }

    ILanguageFactory::ILanguageFactory(const QString &id, QObject *parent)
        : ILanguageFactory(*new ILanguageFactoryPrivate(), id, parent) {
    }

    ILanguageFactory::~ILanguageFactory() = default;

    ILanguageFactory::ILanguageFactory(ILanguageFactoryPrivate &d, const QString &id,
                                       QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;

        d.init();
        d.m_selectedG2p = id;
        d.m_g2pConfig = new QJsonObject();
    }

    QString ILanguageFactory::id() const {
        Q_D(const ILanguageFactory);
        return d->id;
    }

    QString ILanguageFactory::selectedG2p() const {
        Q_D(const ILanguageFactory);
        return d->m_selectedG2p;
    }

    void ILanguageFactory::setG2p(const QString &g2pId) {
        Q_D(ILanguageFactory);
        d->m_selectedG2p = g2pId;
    }

    bool ILanguageFactory::enabled() const {
        Q_D(const ILanguageFactory);
        return d->enabled;
    }

    void ILanguageFactory::setEnabled(const bool &enable) {
        Q_D(ILanguageFactory);
        d->enabled = enable;
    }

    bool ILanguageFactory::discardResult() const {
        Q_D(const ILanguageFactory);
        return d->discardResult;
    }

    void ILanguageFactory::setDiscardResult(const bool &discard) {
        Q_D(ILanguageFactory);
        d->discardResult = discard;
    }

    QString ILanguageFactory::description() const {
        Q_D(const ILanguageFactory);
        return d->description;
    }

    void ILanguageFactory::setDescription(const QString &description) {
        Q_D(ILanguageFactory);
        d->description = description;
    }

    QString ILanguageFactory::displayName() const {
        Q_D(const ILanguageFactory);
        return d->displayName;
    }

    void ILanguageFactory::setDisplayName(const QString &displayName) {
        Q_D(ILanguageFactory);
        d->displayName = displayName;
    }

    QString ILanguageFactory::author() const {
        Q_D(const ILanguageFactory);
        return d->author;
    }

    void ILanguageFactory::setAuthor(const QString &author) {
        Q_D(ILanguageFactory);
        d->author = author;
    }

    bool ILanguageFactory::contains(const QChar &c) const {
        Q_UNUSED(c);
        return false;
    }

    bool ILanguageFactory::contains(const QString &input) const {
        Q_UNUSED(input);
        return false;
    }

    QList<LangNote> ILanguageFactory::split(const QString &input) const {
        Q_UNUSED(input);
        return {};
    }

    QList<LangNote> ILanguageFactory::split(const QList<LangNote> &input) const {
        Q_D(const ILanguageFactory);
        QList<LangNote> result;
        for (const auto &note : input) {
            if (note.language == "Unknown") {
                const auto splitRes = split(note.lyric);
                for (const auto &res : splitRes) {
                    if (res.language == id() && d->discardResult) {
                        continue;
                    }
                    result.append(res);
                }
            } else {
                result.append(note);
            }
        }
        return result;
    }

    QString ILanguageFactory::analysis(const QString &input) const {
        return contains(input) ? id() : "Unknown";
    }

    void ILanguageFactory::correct(const QList<LangNote *> &input) const {
        for (const auto &note : input) {
            if (note->language == "Unknown") {
                if (contains(note->lyric))
                    note->language = id();
            }
        }
    }

    QWidget *ILanguageFactory::configWidget() {
        Q_D(const ILanguageFactory);
        auto *widget = new QWidget();
        const auto mainLayout = new QVBoxLayout(widget);

        const auto enabledCheckBox = new QCheckBox(tr("Enabled"));
        enabledCheckBox->setChecked(d->enabled);

        const auto discardResultCheckBox = new QCheckBox(tr("Discard result"));
        discardResultCheckBox->setChecked(d->discardResult);

        const auto g2pMgr = G2pMgr::IG2pManager::instance();

        const auto g2pLabel = new QLabel(tr("Select G2P:"));
        const auto g2pComboBox = new ComboBox();
        g2pComboBox->setMaximumWidth(120);
        const auto g2pList = g2pMgr->g2pList();
        g2pComboBox->addItems(g2pList);
        if (g2pList.contains(d->m_selectedG2p)) {
            g2pComboBox->setCurrentText(d->m_selectedG2p);
        } else {
            g2pComboBox->setCurrentText("Unknown");
        }

        mainLayout->addWidget(enabledCheckBox);
        mainLayout->addWidget(discardResultCheckBox);

        mainLayout->addWidget(g2pLabel);
        mainLayout->addWidget(g2pComboBox);

        widget->setLayout(mainLayout);

        connect(enabledCheckBox, &QCheckBox::toggled,
                [this](const bool &checked) { setEnabled(checked); });
        connect(discardResultCheckBox, &QCheckBox::toggled,
                [this](const bool &checked) { setDiscardResult(checked); });
        connect(g2pComboBox, &ComboBox::currentTextChanged, [this](const QString &text) {
            d_ptr->m_selectedG2p = text;
            Q_EMIT g2pChanged(text);
        });
        return widget;
    }

    QWidget *ILanguageFactory::g2pConfigWidget() {
        Q_D(const ILanguageFactory);
        const auto g2pMgr = G2pMgr::IG2pManager::instance();
        return g2pMgr->g2p(d->m_selectedG2p)->configWidget(d->m_g2pConfig);
    }

    QJsonObject *ILanguageFactory::g2pConfig() {
        Q_D(const ILanguageFactory);
        return d->m_g2pConfig;
    }

} // LangMgr