// SPDX-License-Identifier: Apache-2.0
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

namespace {

// Gives the tests the folder CMake put a stand-in game picture in, as the
// picture step's folder holds one for each game (ADR-0047). And, as
// `heldKey`, the keyboard's own repeat of a held key, which a child holding
// a key down sends and TestCase.keyPress cannot mark.
//
// And, as `steamStandIn`, a Steam of the tests' own (ADR-0061): its logs in
// a data folder of theirs, set before any QML runs, so no test reads the
// Steam of whoever runs them; two process lists, one with a `steam` in it;
// and a stand-in steam first on PATH that hands off at once, so a press on a
// game never starts a real Steam.
class Setup : public QObject {
    Q_OBJECT

    QTemporaryDir m_home;

    QString steamLog(const QString& name) const {
        return m_home.filePath(QStringLiteral("data/Steam/logs/") + name);
    }

    void steamLogs(const QString& name, const QByteArray& says) const {
        QFile log(steamLog(name));
        if (log.open(QIODevice::WriteOnly | QIODevice::Append)) {
            const QString time =
                QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            log.write('[' + time.toLatin1() + "] " + says + '\n');
        }
    }

public slots:
    void applicationAvailable() {
        const QDir home(m_home.path());
        home.mkpath(QStringLiteral("data/Steam/logs"));
        qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
        home.mkpath(QStringLiteral("processes/none"));
        home.mkpath(QStringLiteral("processes/steam/4242"));
        QFile comm(home.filePath(QStringLiteral("processes/steam/4242/comm")));
        if (comm.open(QIODevice::WriteOnly)) {
            comm.write("steam\n");
        }
        home.mkpath(QStringLiteral("bin"));
        QFile steam(home.filePath(QStringLiteral("bin/steam")));
        if (steam.open(QIODevice::WriteOnly)) {
            steam.write("#!/bin/sh\nexit 0\n");
            steam.close();
            steam.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        }
        qputenv("PATH", home.filePath(QStringLiteral("bin")).toUtf8() + ':' + qgetenv("PATH"));
    }

    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("standInPictures"),
                                                  QStringLiteral(STAND_IN_PICTURES));
        engine->rootContext()->setContextProperty(QStringLiteral("heldKey"), this);
        engine->rootContext()->setContextProperty(QStringLiteral("steamStandIn"), this);
    }

    // One repeat of `key`, delivered to whatever has the focus in `window`.
    void repeat(QObject* window, int key) {
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, QString(), true);
        QCoreApplication::sendEvent(window, &event);
    }

    // A process list with a `steam` of this user in it, and one with none.
    QString steamRunning() const {
        return m_home.filePath(QStringLiteral("processes/steam"));
    }

    QString noSteamRunning() const {
        return m_home.filePath(QStringLiteral("processes/none"));
    }

    // The updater starting the client now and installing an update first.
    void startsUpdating() const {
        const QString bootstrap = QStringLiteral("bootstrap_log.txt");
        steamLogs(bootstrap, "Startup - updater built Sep  3 2026 01:31:48");
        steamLogs(bootstrap, "Found pending update");
        steamLogs(bootstrap, "Installing update...");
    }

    // The client running its own start command now: Steam is done.
    void startsItself() const {
        steamLogs(QStringLiteral("console_log.txt"),
                  "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-srt-logger-opened' "
                  "'-silent'\"");
    }

    // Steam's logs as no client has written them yet.
    void forget() const {
        QFile::remove(steamLog(QStringLiteral("bootstrap_log.txt")));
        QFile::remove(steamLog(QStringLiteral("console_log.txt")));
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(launcher, Setup)
#include "tst_navigation.moc"
