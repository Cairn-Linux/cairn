// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

#include <cstdint>

// The lines on the terminal screen, oldest first: what the child typed and
// what the shell answered. Each line carries an icon slot so ls can show a
// picture beside a name, and only the last screenful or so is kept.
class OutputModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by a TerminalSession.")

public:
    // Mirrors Reply::Icon in the shell, which has no Qt meta-object.
    enum class Icon : std::uint8_t { None, Folder, Make, Practice, Machine, Note };
    Q_ENUM(Icon)

    enum Role : std::uint16_t { TextRole = Qt::UserRole + 1, IconRole, IsInputRole };

    struct Line {
        QString text;
        Icon icon = Icon::None;
        bool isInput = false;
    };

    explicit OutputModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const Line& line);
    void clear();
    QList<Line> lines() const;

private:
    QList<Line> m_lines;
};
