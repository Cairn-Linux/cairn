// SPDX-License-Identifier: Apache-2.0
#include <QColor>
#include <QFile>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QTest>

namespace {

class TokensLoadedTest : public QObject {
    Q_OBJECT

    QJsonObject m_palette;
    QJsonObject m_semantic;
    QJsonObject m_mark;
    QJsonObject m_motion;

private slots:
    void initTestCase() {
        QFile file(QStringLiteral(CAIRN_TOKENS_JSON), this);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(document.isObject());
        m_palette = document.object().value("color").toObject();
        m_semantic = document.object().value("semantic").toObject();
        m_mark = document.object().value("mark").toObject();
        m_motion = document.object().value("motion").toObject();
    }

    void labelsMatchJson() {
        const QJsonObject labels = m_semantic.value("on").toObject();
        QQmlEngine engine(this);
        const auto* tokens = engine.singletonInstance<QObject*>("Cairn.Brand", "Tokens");
        QVERIFY(tokens != nullptr);
        const QStringList kinds{QStringLiteral("make"), QStringLiteral("practice"),
                                QStringLiteral("machine")};
        for (const QString& kind : kinds) {
            const QString paletteName = labels.value(kind).toString();
            const QColor expected(m_palette.value(paletteName).toObject().value("hex").toString());
            QVERIFY(expected.isValid());
            const QByteArray propertyName = (kind + QStringLiteral("Label")).toUtf8();
            QCOMPARE(tokens->property(propertyName.constData()).value<QColor>(), expected);
        }
    }

    void groundIsSand() {
        QQmlEngine engine(this);
        const auto* tokens = engine.singletonInstance<QObject*>("Cairn.Brand", "Tokens");
        QVERIFY(tokens != nullptr);
        QCOMPARE(m_semantic.value("ground").toString(), QStringLiteral("sand"));
        const QColor sand(m_palette.value("sand").toObject().value("hex").toString());
        QVERIFY(sand.isValid());
        QCOMPARE(tokens->property("ground").value<QColor>(), sand);
    }

    // The starting screen draws the mark from these, never from a shape of
    // its own (ADR-0028).
    void markMatchesJson() {
        QQmlEngine engine(this);
        const auto* tokens = engine.singletonInstance<QObject*>("Cairn.Brand", "Tokens");
        QVERIFY(tokens != nullptr);
        const QJsonArray expected = m_mark.value("stones").toArray();
        // A QML var property may reach C++ as a QJSValue; its variant is the list.
        QVariant value = tokens->property("markStones");
        if (value.metaType() == QMetaType::fromType<QJSValue>()) {
            value = value.value<QJSValue>().toVariant();
        }
        const QVariantList stones = value.toList();
        QCOMPARE(stones.size(), expected.size());
        for (qsizetype i = 0; i < stones.size(); ++i) {
            const QVariantList stone = stones.at(i).toList();
            const QJsonArray want = expected.at(i).toArray();
            QCOMPARE(stone.size(), want.size());
            for (qsizetype j = 0; j < stone.size(); ++j) {
                QCOMPARE(stone.at(j).toInt(), want.at(j).toInt());
            }
        }
        QCOMPARE(tokens->property("markStoneRadius").toInt(), m_mark.value("rx").toInt());
        QCOMPARE(tokens->property("motionStone").toInt(), m_motion.value("stone").toInt());
    }
};

} // namespace

QTEST_MAIN(TokensLoadedTest)
#include "tst_tokens_loaded.moc"
