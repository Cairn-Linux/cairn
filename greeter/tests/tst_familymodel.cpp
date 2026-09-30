// SPDX-License-Identifier: Apache-2.0
#include "FamilyModel.h"

#include <QStandardItemModel>
#include <QTest>

namespace {

using Kind = AccountKind::Kind;
using Tint = FamilyModel::Tint;

constexpr int nameRole = Qt::UserRole + 1;
constexpr int realNameRole = Qt::UserRole + 2;

// Stands in for SDDM's user model: the same role names, rows in the order
// SDDM happened to read them.
class Users : public QStandardItemModel {
public:
    explicit Users(QObject* parent) : QStandardItemModel(parent) {
        setItemRoleNames({{nameRole, "name"}, {realNameRole, "realName"}});
    }

    void add(const QString& name, const QString& realName = {}) {
        auto* item = new QStandardItem;
        item->setData(name, nameRole);
        item->setData(realName, realNameRole);
        appendRow(item);
    }
};

std::optional<Account> family(const QString& name) {
    static const QHash<QString, Account> accounts = {
        {"bazzite", {.uid = 1000, .groups = {"bazzite", "wheel"}}},
        {"guardian", {.uid = 1001, .groups = {"guardian", "wheel", "cairn-guardian"}}},
        {"ada", {.uid = 1002, .groups = {"ada", "cairn-l1"}}},
        {"ben", {.uid = 1003, .groups = {"ben", "cairn-l3"}}},
        {"cleo", {.uid = 1004, .groups = {"cleo", "cairn-l2"}}},
        {"dad", {.uid = 1005, .groups = {"dad", "cairn-guardian"}}},
        {"eve", {.uid = 1006, .groups = {"eve", "cairn-l1"}}},
    };
    const auto found = accounts.constFind(name);
    return found == accounts.constEnd() ? std::nullopt : std::optional<Account>(*found);
}

QStringList loginNames(const FamilyModel& model) {
    QStringList names;
    for (int row = 0; row < model.rowCount(); ++row) {
        names.append(model.data(model.index(row, 0), FamilyModel::LoginNameRole).toString());
    }
    return names;
}

QVariant at(const FamilyModel& model, const QString& name, int role) {
    const int row = static_cast<int>(loginNames(model).indexOf(name));
    return model.data(model.index(row, 0), role);
}

class FamilyModelTest : public QObject {
    Q_OBJECT

private slots:
    // Children first, then Guardians, then anyone else; oldest account first
    // in each. Nobody is left out, and siblings are not sorted by level.
    void everyoneInFamilyOrder() {
        Users users(this);
        for (const char* name : {"eve", "bazzite", "dad", "ben", "ada", "guardian", "cleo"}) {
            users.add(QString::fromLatin1(name));
        }
        FamilyModel model(this);
        model.setLookUp(family);
        model.setSourceModel(&users);
        QCOMPARE(loginNames(model),
                 (QStringList{"ada", "ben", "cleo", "eve", "guardian", "dad", "bazzite"}));
    }

    void passwordOnlyWherePamAsksForOne() {
        Users users(this);
        for (const char* name : {"ada", "ben", "cleo", "guardian", "bazzite"}) {
            users.add(QString::fromLatin1(name));
        }
        FamilyModel model(this);
        model.setLookUp(family);
        model.setSourceModel(&users);
        QCOMPARE(at(model, "ada", FamilyModel::AsksForPasswordRole).toBool(), false);
        QCOMPARE(at(model, "cleo", FamilyModel::AsksForPasswordRole).toBool(), false);
        QCOMPARE(at(model, "ben", FamilyModel::AsksForPasswordRole).toBool(), true);
        QCOMPARE(at(model, "guardian", FamilyModel::AsksForPasswordRole).toBool(), true);
        QCOMPARE(at(model, "bazzite", FamilyModel::AsksForPasswordRole).toBool(), true);
        QCOMPARE(at(model, "guardian", FamilyModel::KindRole).value<Kind>(), Kind::Guardian);
    }

    // An account the system does not know is shown, asks for a password,
    // and goes last.
    void anUnknownAccountAsksForAPassword() {
        Users users(this);
        users.add("stranger");
        users.add("ada");
        FamilyModel model(this);
        model.setLookUp(family);
        model.setSourceModel(&users);
        QCOMPARE(loginNames(model), (QStringList{"ada", "stranger"}));
        QCOMPARE(at(model, "stranger", FamilyModel::KindRole).value<Kind>(), Kind::NotFamily);
        QCOMPARE(at(model, "stranger", FamilyModel::AsksForPasswordRole).toBool(), true);
    }

    void theNameShownIsTheRealNameIfThereIsOne() {
        Users users(this);
        users.add("ada", "Ada");
        users.add("bazzite", "cairn");
        users.add("eve", "  ");
        users.add("cleo", QString::fromUtf8("Élodie"));
        FamilyModel model(this);
        model.setLookUp(family);
        model.setSourceModel(&users);
        QCOMPARE(at(model, "ada", FamilyModel::DisplayNameRole).toString(), QString("Ada"));
        QCOMPARE(at(model, "bazzite", FamilyModel::DisplayNameRole).toString(), QString("cairn"));
        QCOMPARE(at(model, "eve", FamilyModel::DisplayNameRole).toString(), QString("eve"));
        QCOMPARE(at(model, "bazzite", FamilyModel::InitialRole).toString(), QString("C"));
        QCOMPARE(at(model, "eve", FamilyModel::InitialRole).toString(), QString("E"));
        QCOMPARE(at(model, "cleo", FamilyModel::InitialRole).toString(), QString::fromUtf8("É"));
    }

    // A letter written as a base and a combining accent stays whole.
    void theInitialIsAWholeLetter() {
        Users users(this);
        users.add("ada", QString::fromUtf8("Émile"));
        FamilyModel model(this);
        model.setLookUp(family);
        model.setSourceModel(&users);
        QCOMPARE(at(model, "ada", FamilyModel::InitialRole).toString(), QString::fromUtf8("É"));
    }

    // Siblings side by side never share a colour; grown-ups get the plain one.
    void childrenTakeColoursInTurn() {
        Users users(this);
        for (const char* name : {"ada", "ben", "cleo", "eve", "guardian", "bazzite"}) {
            users.add(QString::fromLatin1(name));
        }
        FamilyModel model(this);
        model.setLookUp(family);
        model.setSourceModel(&users);
        QCOMPARE(at(model, "ada", FamilyModel::TintRole).value<Tint>(), Tint::Make);
        QCOMPARE(at(model, "ben", FamilyModel::TintRole).value<Tint>(), Tint::Practice);
        QCOMPARE(at(model, "cleo", FamilyModel::TintRole).value<Tint>(), Tint::Machine);
        QCOMPARE(at(model, "eve", FamilyModel::TintRole).value<Tint>(), Tint::Make);
        QCOMPARE(at(model, "guardian", FamilyModel::TintRole).value<Tint>(), Tint::Plain);
        QCOMPARE(at(model, "bazzite", FamilyModel::TintRole).value<Tint>(), Tint::Plain);
    }

    // SDDM's own roles still come through for anything that wants them.
    void sourceRolesPassThrough() {
        Users users(this);
        users.add("ada", "Ada");
        FamilyModel model(this);
        model.setLookUp(family);
        model.setSourceModel(&users);
        QCOMPARE(model.roleNames().value(realNameRole), QByteArray("realName"));
        QCOMPARE(model.data(model.index(0, 0), realNameRole).toString(), QString("Ada"));
        QCOMPARE(model.roleNames().value(FamilyModel::AsksForPasswordRole),
                 QByteArray("asksForPassword"));
    }

    // The system's own lookup finds the account running the test and gives
    // it groups; an account that cannot exist is nothing.
    void theSystemLookUp() {
        QVERIFY(!lookUpAccount("no-such-account-cairn-test").has_value());
        QVERIFY(!lookUpAccount(QString()).has_value());
        const std::optional<Account> found = lookUpAccount("root");
        QVERIFY(found.has_value());
        const Account root = found.value_or(Account{.uid = 1, .groups = {}});
        QCOMPARE(root.uid, 0U);
        QVERIFY(root.groups.contains("root"));
    }
};

} // namespace

QTEST_MAIN(FamilyModelTest)
#include "tst_familymodel.moc"
