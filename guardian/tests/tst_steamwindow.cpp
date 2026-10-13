// SPDX-License-Identifier: Apache-2.0
#include "SteamForChild.h"

#include <QDir>
#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include <unistd.h>

namespace {

// Makes the window's SteamForChild for each test, running the stand-ins for
// pkexec with the helper behind it and for the Guardian's own Steam.
class StandInVisits : public QObject {
    Q_OBJECT

public:
    explicit StandInVisits(QObject* parent) : QObject(parent) {
        qputenv("CAIRN_TEST_PROC", m_proc.path().toLocal8Bit());
    }

    // The scenario names what the stand-in helper says. The window's QML
    // owns what this returns.
    Q_INVOKABLE SteamForChild* visit(const QString& scenario, bool ownSteamRunning) {
        QDir proc(m_proc.path());
        proc.remove(QStringLiteral("started"));
        QDir(m_proc.filePath(QStringLiteral("4242"))).removeRecursively();
        if (ownSteamRunning) {
            proc.mkdir(QStringLiteral("4242"));
            QFile comm(m_proc.filePath(QStringLiteral("4242/comm")));
            if (comm.open(QIODevice::WriteOnly)) {
                comm.write("steam\n");
            }
        }
        SteamForChild::Setup setup;
        setup.child = QStringLiteral("nobody-here");
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec");
        setup.helper = scenario;
        setup.uid = getuid();
        setup.ownSteam.steam = QStringLiteral(FIXTURES "/steam");
        setup.ownSteam.atLogin = QStringLiteral(FIXTURES "/bazzite-steam");
        setup.ownSteam.proc = m_proc.path();
        setup.ownSteam.shutdownMilliseconds = 1000;
        auto* visit = new SteamForChild(setup);
        QQmlEngine::setObjectOwnership(visit, QQmlEngine::JavaScriptOwnership);
        return visit;
    }

private:
    QTemporaryDir m_proc;
};

class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("standIns"),
                                                  new StandInVisits(engine));
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(steamwindow, Setup)
#include "tst_steamwindow.moc"
