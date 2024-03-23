#include "ILanguageFactory.h"
#include "ILanguageFactory_p.h"

#include <QDebug>
#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

#include "../G2pMgr/IG2pManager.h"
#include "ILanguageManager.h"

#include <QtConcurrent/QtConcurrent>

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

    bool ILanguageFactory::initialize(QString &errMsg) {
        Q_UNUSED(errMsg);
        return true;
    }

    ILanguageFactory::ILanguageFactory(ILanguageFactoryPrivate &d, const QString &id,
                                       QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;
        d.categroy = id;

        d.init();
        d.m_selectedG2p = id;
        d.m_g2pConfig = new QJsonObject();
    }

    QString ILanguageFactory::id() const {
        Q_D(const ILanguageFactory);
        return d->id;
    }

    QString ILanguageFactory::displayName() const {
        Q_D(const ILanguageFactory);
        return d->displayName;
    }

    void ILanguageFactory::setDisplayName(const QString &name) {
        Q_D(ILanguageFactory);
        d->displayName = name;
    }

    QString ILanguageFactory::category() const {
        Q_D(const ILanguageFactory);
        return d->categroy;
    }

    void ILanguageFactory::setCategory(const QString &category) {
        Q_D(ILanguageFactory);
        d->categroy = category;
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
        if (!d->enabled) {
            return input;
        }

        QList<LangNote> result;
        for (const auto &note : input) {
            if (note.language == "Unknown") {
                const auto splitRes = split(note.lyric);
                for (const auto &res : splitRes) {
                    if (res.language == category() && d->discardResult) {
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

        const auto langMgr = ILanguageManager::instance();

        const auto cateLayout = new QHBoxLayout();
        const auto cateLabel = new QLabel(tr("Analysis results "));
        const auto cateComboBox = new ComboBox();
        cateComboBox->setMaximumWidth(120);
        cateComboBox->setMinimumHeight(28);
        const auto cateList = langMgr->categoryList();
        const auto cateTrans = langMgr->categoryTrans();

        cateComboBox->addItems(cateTrans);
        if (cateList.contains(d->categroy)) {
            cateComboBox->setCurrentText(cateTrans.at(cateList.indexOf(d->categroy)));
        } else {
            cateComboBox->setCurrentText(tr("Unknown"));
        }

        const auto g2pMgr = G2pMgr::IG2pManager::instance();

        const auto g2pLayout = new QHBoxLayout();
        const auto g2pLabel = new QLabel(tr("Subordinate G2p"));
        const auto g2pComboBox = new ComboBox();
        g2pComboBox->setMaximumWidth(120);
        g2pComboBox->setMinimumHeight(28);
        const auto g2pList = g2pMgr->g2pList();
        const auto g2pTrans = g2pMgr->g2pTrans();

        g2pComboBox->addItems(g2pTrans);
        if (g2pList.contains(d->m_selectedG2p)) {
            g2pComboBox->setCurrentText(g2pTrans.at(g2pList.indexOf(d->m_selectedG2p)));
        } else {
            g2pComboBox->setCurrentText(tr("Unknown"));
        }

        mainLayout->addWidget(enabledCheckBox);
        mainLayout->addWidget(discardResultCheckBox);

        cateLayout->addWidget(cateLabel);
        cateLayout->addWidget(cateComboBox);
        cateLayout->addStretch();
        mainLayout->addLayout(cateLayout);

        g2pLayout->addWidget(g2pLabel);
        g2pLayout->addWidget(g2pComboBox);
        g2pLayout->addStretch();
        mainLayout->addLayout(g2pLayout);

        widget->setLayout(mainLayout);

        connect(enabledCheckBox, &QCheckBox::toggled, [this](const bool &checked) {
            setEnabled(checked);
            Q_EMIT langConfigChanged(id());
        });

        connect(discardResultCheckBox, &QCheckBox::toggled, [this](const bool &checked) {
            setDiscardResult(checked);
            Q_EMIT langConfigChanged(id());
        });

        connect(cateComboBox, &ComboBox::currentTextChanged,
                [this, cateList, cateTrans](const QString &text) {
                    const auto index = cateTrans.indexOf(text);
                    setCategory(index >= 0 ? cateList.at(index) : tr("Unknown"));
                    Q_EMIT langConfigChanged(id());
                });

        connect(g2pComboBox, &ComboBox::currentTextChanged,
                [this, g2pList, g2pTrans](const QString &text) {
                    const auto index = g2pTrans.indexOf(text);
                    setG2p(index >= 0 ? g2pList.at(index) : tr("Unknown"));
                    Q_EMIT g2pChanged(selectedG2p());
                    Q_EMIT langConfigChanged(id());
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

    void ILanguageFactory::loadConfig(const QJsonObject &config) {
        Q_D(ILanguageFactory);
        if (config.contains("enabled")) {
            d->enabled = config.value("enabled").toBool();
        }
        if (config.contains("discardResult")) {
            d->discardResult = config.value("discardResult").toBool();
        }
        if (config.contains("category")) {
            d->categroy = config.value("category").toString();
        }
        if (config.contains("g2p")) {
            d->m_selectedG2p = config.value("g2p").toString();
        }
        if (config.contains("g2pConfig")) {
            d->m_g2pConfig = new QJsonObject(config.value("g2pConfig").toObject());
        }
    }

    QJsonObject ILanguageFactory::exportConfig() const {
        Q_D(const ILanguageFactory);
        QJsonObject config;
        config.insert("enabled", d->enabled);
        config.insert("discardResult", d->discardResult);
        config.insert("category", d->categroy);
        config.insert("g2p", d->m_selectedG2p);
        config.insert("g2pConfig", *d->m_g2pConfig);
        return config;
    }

} // LangMgr