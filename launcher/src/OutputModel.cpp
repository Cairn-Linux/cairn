// SPDX-License-Identifier: Apache-2.0
#include "OutputModel.h"

namespace {
// More than any screen shows; the child never scrolls (TERMINAL-DESIGN §6).
constexpr qsizetype keptLines = 200;
} // namespace

OutputModel::OutputModel(QObject* parent) : QAbstractListModel(parent) {}

int OutputModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_lines.size());
}

QVariant OutputModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 ||
        index.row() >= rowCount()) {
        return {};
    }
    const Line& line = m_lines.at(index.row());
    switch (role) {
    case TextRole:
        return line.text;
    case IconRole:
        return QVariant::fromValue(line.icon);
    case IsInputRole:
        return line.isInput;
    default:
        return {};
    }
}

QHash<int, QByteArray> OutputModel::roleNames() const {
    return {{TextRole, "text"}, {IconRole, "icon"}, {IsInputRole, "isInput"}};
}

void OutputModel::append(const Line& line) {
    if (m_lines.size() >= keptLines) {
        beginRemoveRows({}, 0, 0);
        m_lines.removeFirst();
        endRemoveRows();
    }
    const int row = rowCount();
    beginInsertRows({}, row, row);
    m_lines.append(line);
    endInsertRows();
}

void OutputModel::clear() {
    beginResetModel();
    m_lines.clear();
    endResetModel();
}

QList<OutputModel::Line> OutputModel::lines() const {
    return m_lines;
}
