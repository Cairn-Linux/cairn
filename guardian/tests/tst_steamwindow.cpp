// SPDX-License-Identifier: Apache-2.0
#include "SteamForChild.h"

#include <QDir>
#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QWindow>
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
        proc.remove(QStringLiteral("stubborn"));
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

    // The Guardian's own Steam ignores a request to shut down, until told
    // otherwise. Each visit() starts with it willing.
    Q_INVOKABLE void ownSteamWillNotQuit(bool stubborn) {
        const QString path = m_proc.filePath(QStringLiteral("stubborn"));
        if (stubborn) {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
            }
        } else {
            QFile::remove(path);
        }
    }

    // Resizes the window as the compositor does, not as the window's own
    // QML would, so its height bindings are left as they were.
    Q_INVOKABLE static void resizeFromOutside(QWindow* window, int width, int height) {
        window->resize(width, height);
    }

    // Where to save a picture of each step of the window, when pictures
    // are wanted: this path, with the step's name before ".png".
    Q_INVOKABLE QString screenshotPath() const {
        return qEnvironmentVariable("CAIRN_SCREENSHOT");
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
