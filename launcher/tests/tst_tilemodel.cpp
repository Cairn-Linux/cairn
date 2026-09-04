// SPDX-License-Identifier: Apache-2.0
#include "TileModel.h"

#include <QTest>

namespace {

class TileModelTest : public QObject {
    Q_OBJECT

private slots:
    void tiles() {
        const TileModel model(this);
        const QList<TileModel::Kind> expectedKinds{
            TileModel::Kind::Make, TileModel::Kind::Make,     TileModel::Kind::Make,
            TileModel::Kind::Make, TileModel::Kind::Practice, TileModel::Kind::Machine};

        QCOMPARE(model.rowCount(), 6);
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row);
            QCOMPARE(model.data(index, TileModel::KindRole).value<TileModel::Kind>(),
                     expectedKinds.at(row));
            QVERIFY(!model.data(index, TileModel::TitleRole).toString().isEmpty());
            QVERIFY(!model.data(index, TileModel::AccessibleNameRole).toString().isEmpty());
        }
    }

    void roleNames() {
        const TileModel model(this);
        QCOMPARE(model.roleNames().value(TileModel::TitleRole), QByteArray("title"));
        QCOMPARE(model.roleNames().value(TileModel::KindRole), QByteArray("kind"));
        QCOMPARE(model.roleNames().value(TileModel::AccessibleNameRole),
                 QByteArray("accessibleName"));
    }

    void invalidRequests() {
        const TileModel model(this);
        const TileModel otherModel(this);
        QCOMPARE(model.rowCount(model.index(0)), 0);
        QVERIFY(!model.data({}, TileModel::TitleRole).isValid());
        QVERIFY(!model.data(model.index(6), TileModel::TitleRole).isValid());
        QVERIFY(!model.data(otherModel.index(0), TileModel::TitleRole).isValid());
        QVERIFY(!model.data(model.index(0), Qt::UserRole).isValid());
    }
};

} // namespace

QTEST_GUILESS_MAIN(TileModelTest)
#include "tst_tilemodel.moc"
