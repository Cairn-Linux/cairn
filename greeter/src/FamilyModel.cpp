// SPDX-License-Identifier: Apache-2.0
#include "FamilyModel.h"

#include <QTextBoundaryFinder>

#include <limits>

namespace {

// The role names SDDM's user model gives its rows.
constexpr const char* loginNameRole = "name";
constexpr const char* realNameRole = "realName";

constexpr int childTints = 3;

// The first letter as a reader sees it, so an accented or combined letter is
// not cut in half.
QString firstLetter(const QString& text) {
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    const qsizetype end = finder.toNextBoundary();
    return end > 0 ? text.first(end).toUpper() : QString();
}

} // namespace

FamilyModel::FamilyModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    sort(0);
}

void FamilyModel::setLookUp(LookUp lookUp) {
    m_lookUp = std::move(lookUp);
    invalidate();
}

QVariant FamilyModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.model() != this) {
        return {};
    }
    const QModelIndex sourceIndex = mapToSource(index);
    const QString loginName = sourceText(sourceIndex, loginNameRole);
    const AccountKind::Kind kind = kindOf(loginName);
    switch (role) {
    case KindRole:
        return QVariant::fromValue(kind);
    case AsksForPasswordRole:
        return AccountKind::needsPassword(kind);
    case LoginNameRole:
        return loginName;
    case DisplayNameRole:
    case InitialRole: {
        const QString realName = sourceText(sourceIndex, realNameRole).trimmed();
        const QString shown = realName.isEmpty() ? loginName : realName;
        return role == DisplayNameRole ? shown : firstLetter(shown);
    }
    case TintRole: {
        const bool child =
            kind == AccountKind::Kind::YoungChild || kind == AccountKind::Kind::OlderChild;
        // Children sort first, so a child's row is its place among the children.
        return QVariant::fromValue(child ? static_cast<Tint>(index.row() % childTints)
                                         : Tint::Plain);
    }
    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

QHash<int, QByteArray> FamilyModel::roleNames() const {
    QHash<int, QByteArray> names = QSortFilterProxyModel::roleNames();
    names.insert(KindRole, "kind");
    // Not "needsPassword": SDDM's model has a role of that name, and QML
    // would see only one of the two.
    names.insert(AsksForPasswordRole, "asksForPassword");
    names.insert(DisplayNameRole, "displayName");
    names.insert(InitialRole, "initial");
    names.insert(TintRole, "tint");
    names.insert(LoginNameRole, "loginName");
    return names;
}

bool FamilyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    const QString leftName = sourceText(left, loginNameRole);
    const QString rightName = sourceText(right, loginNameRole);
    const AccountKind::Kind leftKind = kindOf(leftName);
    const AccountKind::Kind rightKind = kindOf(rightName);
    // Young and older children are one group on screen: siblings are not
    // sorted by level.
    const auto group = [](AccountKind::Kind kind) {
        return kind == AccountKind::Kind::OlderChild ? AccountKind::Kind::YoungChild : kind;
    };
    if (group(leftKind) != group(rightKind)) {
        return group(leftKind) < group(rightKind);
    }
    const unsigned int leftUid = uidOf(leftName);
    const unsigned int rightUid = uidOf(rightName);
    if (leftUid != rightUid) {
        return leftUid < rightUid;
    }
    return leftName < rightName;
}

QString FamilyModel::sourceText(const QModelIndex& sourceIndex, const char* roleName) const {
    const QAbstractItemModel* source = sourceModel();
    if (source == nullptr) {
        return {};
    }
    const int role = source->roleNames().key(QByteArray(roleName), -1);
    return role < 0 ? QString() : source->data(sourceIndex, role).toString();
}

AccountKind::Kind FamilyModel::kindOf(const QString& loginName) const {
    const std::optional<Account> account = m_lookUp(loginName);
    return account ? AccountKind::fromGroups(account->groups) : AccountKind::Kind::NotFamily;
}

unsigned int FamilyModel::uidOf(const QString& loginName) const {
    const std::optional<Account> account = m_lookUp(loginName);
    return account ? account->uid : std::numeric_limits<unsigned int>::max();
}
