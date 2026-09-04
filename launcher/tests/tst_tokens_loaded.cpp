// SPDX-License-Identifier: Apache-2.0
#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QTest>

namespace {

class TokensLoadedTest : public QObject {
    Q_OBJECT

    QJsonObject m_palette;
    QJsonObject m_semantic;

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
};

} // namespace

QTEST_MAIN(TokensLoadedTest)
#include "tst_tokens_loaded.moc"
