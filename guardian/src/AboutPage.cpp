// SPDX-License-Identifier: Apache-2.0
#include "AboutPage.h"

#include "OsRelease.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <utility>

namespace {

// What this key holds when the file is Cairn's own and the whole value
// fits the pattern; empty otherwise. Another system's file, such as a
// developer's Fedora, says nothing about Cairn.
QString plainValue(const std::optional<OsRelease>& release, const QString& key,
                   const QString& pattern) {
    if (!release ||
        release->value(QStringLiteral("ID")).value_or(QString()) != QStringLiteral("cairn")) {
        return {};
    }
    const QRegularExpression plain(QRegularExpression::anchoredPattern(pattern));
    const QString value = release->value(key).value_or(QString());
    return plain.match(value).hasMatch() ? value : QString();
}

// The folder when it holds every text the page names, as plain files.
QString folderWithTheFullTexts(const QString& folder) {
    const QStringList texts = {QStringLiteral("LICENSE"), QStringLiteral("LICENSE-CC-BY-SA-4.0"),
                               QStringLiteral("NOTICE"), QStringLiteral("TRADEMARKS.md")};
    const bool allThere = std::ranges::all_of(texts, [&folder](const QString& text) {
        return QFileInfo(QDir(folder).filePath(text)).isFile();
    });
    return allThere ? folder : QString();
}

} // namespace

AboutPage::AboutPage(Setup setup, QObject* parent)
    : QObject(parent), m_setup(std::move(setup)),
      m_licenceFolder(folderWithTheFullTexts(m_setup.licenceFolder)) {
    const std::optional<OsRelease> release = OsRelease::read(m_setup.osRelease);
    m_version = plainValue(release, QStringLiteral("IMAGE_VERSION"),
                           QStringLiteral("[A-Za-z0-9._-]{1,64}"));
    // VERSION_ID is the Fedora release of the base (ADR-0038), so it is
    // only ever shown as Fedora's.
    m_fedoraRelease =
        plainValue(release, QStringLiteral("VERSION_ID"), QStringLiteral("[0-9]{1,4}"));
}

QString AboutPage::version() const {
    return m_version;
}

QString AboutPage::fedoraRelease() const {
    return m_fedoraRelease;
}

QString AboutPage::licenceFolder() const {
    return m_licenceFolder;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
QString AboutPage::fedoraAddress() const {
    return QStringLiteral("https://fedoraproject.org/");
}

AboutPage::Place AboutPage::notOpened() const {
    return m_notOpened;
}

void AboutPage::open(Place place) {
    QUrl address;
    if (place == Place::FedoraWebsite) {
        address = QUrl(fedoraAddress());
    } else if (place == Place::LicenceFolder && !m_licenceFolder.isEmpty()) {
        address = QUrl::fromLocalFile(m_licenceFolder);
    }
    if (address.isEmpty()) {
        return;
    }
    const bool opened = m_setup.open && m_setup.open(address);
    const Place notOpened = opened ? Place::None : place;
    if (notOpened != m_notOpened) {
        m_notOpened = notOpened;
        emit notOpenedChanged();
    }
}
