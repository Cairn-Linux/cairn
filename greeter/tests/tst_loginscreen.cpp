// SPDX-License-Identifier: Apache-2.0
#include "FamilyModel.h"
#include "StandInSddm.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QStandardItemModel>
#include <QtQuickTest/quicktest.h>

namespace {

constexpr int nameRole = Qt::UserRole + 1;
constexpr int realNameRole = Qt::UserRole + 2;

std::optional<Account> family(const QString& name) {
    static const QHash<QString, Account> accounts = {
        {"guardian", {.uid = 1001, .groups = {"guardian", "cairn-guardian"}}},
        {"ada", {.uid = 1002, .groups = {"ada", "cairn-l1"}}},
        {"ben", {.uid = 1003, .groups = {"ben", "cairn-l3"}}},
    };
    const auto found = accounts.constFind(name);
    return found == accounts.constEnd() ? std::nullopt : std::optional<Account>(*found);
}

// Gives the theme what SDDM would: `sddm`, `userModel` and `sessionModel`,
// and a family model over a table of accounts instead of the system's.
class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        auto* users = new QStandardItemModel(engine);
        users->setItemRoleNames({{nameRole, "name"}, {realNameRole, "realName"}});
        for (const auto& [name, realName] :
             {std::pair{"guardian", "Sam"}, std::pair{"ben", "Ben"}, std::pair{"ada", "Ada"}}) {
            auto* item = new QStandardItem;
            item->setData(QString::fromLatin1(name), nameRole);
            item->setData(QString::fromLatin1(realName), realNameRole);
            users->appendRow(item);
        }
        auto* familyModel = new FamilyModel(engine);
        familyModel->setLookUp(family);
        familyModel->setSourceModel(users);

        QQmlContext* context = engine->rootContext();
        context->setContextProperty("userModel", users);
        context->setContextProperty("sddm", new StandInSddm(engine));
        context->setContextProperty("sessionModel", new StandInSessions(engine));
        context->setContextProperty("testFamily", familyModel);
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(loginscreen, Setup)
#include "tst_loginscreen.moc"
