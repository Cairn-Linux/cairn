// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include <algorithm>

namespace {

// Makes the window's ChildrenPage for each test, over a family of two
// children and a Guardian, with the stand-ins for pkexec and cairn-family.
class StandInPages : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // The scenario names what the stand-ins answer. An older child is a
    // third, at level 3. The window's QML owns what this returns.
    Q_INVOKABLE ChildrenPage* page(const QString& scenario, bool withChildren,
                                   bool withOlderChild = false) {
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        std::vector<Account> accounts{
            {.uid = 1000,
             .groups = {"guardian", "wheel", "cairn-guardian"},
             .login = "guardian",
             .fullName = "Guardian"},
        };
        if (withChildren) {
            accounts.push_back(
                {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"});
            accounts.push_back(
                {.uid = 1002, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"});
        }
        if (withOlderChild) {
            accounts.push_back({.uid = 1003,
                                .groups = {"child", "cairn-l3"},
                                .login = "child",
                                .fullName = "A child with a long name"});
        }
        ChildrenPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = QStringLiteral(FIXTURES "/cairn-family");
        setup.accounts = [accounts] { return accounts; };
        setup.nameInUse = [accounts](const QString& name) {
            return std::ranges::any_of(
                accounts, [&name](const Account& account) { return account.login == name; });
        };
        auto* page = new ChildrenPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // Where to save a picture of the window, when one is wanted.
    Q_INVOKABLE QString screenshotPath() const {
        return qEnvironmentVariable("CAIRN_SCREENSHOT");
    }
};

class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("standIns"),
                                                  new StandInPages(engine));
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(familywindow, Setup)
#include "tst_familywindow.moc"
