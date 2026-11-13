// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>

// What the Guardian tool's About page shows (ADR-0040): which version of
// Cairn Linux this computer runs, when its os-release says; which Fedora
// release that copy is derived from; and where the licence texts are. It
// reads those two places on this computer once, when it is made. It opens
// a web browser or a file manager only when a button is pressed, and looks
// nothing up, fetches nothing and checks for no browser before that.
// Children never see the page: only a Guardian can run Family (ADR-0040).
class AboutPage : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The window is given one by main().")
    // Cairn's own version, from IMAGE_VERSION; empty when os-release is
    // not Cairn's or does not say. Never Fedora's number.
    Q_PROPERTY(QString version READ version CONSTANT)
    // The Fedora release Cairn's os-release names, such as "45"; empty
    // when the file is not Cairn's or has no plain number.
    Q_PROPERTY(QString fedoraRelease READ fedoraRelease CONSTANT)
    // The folder with the four licence texts; empty when one is missing.
    Q_PROPERTY(QString licenceFolder READ licenceFolder CONSTANT)
    // Written here, not in a translatable sentence, so no translation can
    // change where Fedora's notice points.
    Q_PROPERTY(QString fedoraAddress READ fedoraAddress CONSTANT)
    // The button whose last press opened nothing, by the opener's answer.
    Q_PROPERTY(Place notOpened READ notOpened NOTIFY notOpenedChanged)

public:
    enum class Place : std::uint8_t { None, FedoraWebsite, LicenceFolder };
    Q_ENUM(Place)

    struct Setup {
        QString osRelease = QStringLiteral("/usr/lib/os-release");
        QString licenceFolder = QStringLiteral("/usr/share/licenses/cairn");
        // Asks the desktop to open an address, and gives the desktop's
        // answer. The real one's answer is Qt's, which under Wayland is
        // yes whatever happened. Empty here, so that nothing opens unless
        // main() gives the real one: a test that forgets it starts no
        // browser.
        std::function<bool(const QUrl&)> open;
    };

    explicit AboutPage(Setup setup, QObject* parent = nullptr);

    QString version() const;
    QString fedoraRelease() const;
    QString licenceFolder() const;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    QString fedoraAddress() const;
    Place notOpened() const;

    // Opens Fedora's website or the licence folder. The address is this
    // class's own: QML only names the place.
    Q_INVOKABLE void open(Place place);

signals:
    void notOpenedChanged();

private:
    Setup m_setup;
    QString m_version;
    QString m_fedoraRelease;
    QString m_licenceFolder;
    Place m_notOpened = Place::None;
};
