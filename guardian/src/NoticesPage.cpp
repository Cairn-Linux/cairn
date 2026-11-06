// SPDX-License-Identifier: Apache-2.0
#include "NoticesPage.h"

#include "NoticeWords.h"

#include <QDateTime>

#include <utility>

NoticesPage::NoticesPage(Setup setup, QObject* parent)
    : QObject(parent), m_setup(std::move(setup)), m_reader(new TryReader(m_setup.reader, this)) {
    if (!m_setup.now) {
        m_setup.now = [] { return QDateTime::currentMSecsSinceEpoch() * 1000; };
    }
    connect(m_reader, &TryReader::finished, this, &NoticesPage::read);
}

NoticesPage::State NoticesPage::state() const {
    return m_state;
}

QStringList NoticesPage::notices() const {
    return m_notices;
}

QString NoticesPage::recordStart() const {
    return m_recordStart;
}

QString NoticesPage::notShown() const {
    return m_notShown;
}

QString NoticesPage::footer() const {
    return m_footer;
}

int NoticesPage::days() const {
    return m_setup.reader.days;
}

void NoticesPage::reload() {
    if (m_reader->state() == TryReader::State::Reading) {
        return;
    }
    // faillock's settings are read each time, as faillock reads them at
    // each try.
    m_settings = FaillockSettings::read(m_setup.faillockConf);
    m_state = State::Reading;
    emit changed();
    m_reader->read(m_settings);
}

void NoticesPage::read() {
    const NoticeWords words(m_setup.uid, m_settings, m_setup.zone);
    m_notices.clear();
    m_recordStart.clear();
    m_notShown.clear();
    m_footer = words.footer();
    switch (m_reader->state()) {
    case TryReader::State::Ready:
        m_state = State::Ready;
        for (const TryEpisode& episode : m_reader->notices().shown) {
            m_notices << words.sentence(episode);
        }
        m_notShown = words.notShown(m_reader->notices().notShown);
        if (m_reader->oldest()) {
            m_recordStart = words.recordStart(*m_reader->oldest(), m_setup.now(), days());
        }
        break;
    case TryReader::State::Unreadable:
        m_state = State::Unreadable;
        break;
    default:
        m_state = State::Failed;
        break;
    }
    emit changed();
}
