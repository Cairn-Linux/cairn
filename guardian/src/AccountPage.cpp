// SPDX-License-Identifier: Apache-2.0
#include "AccountPage.h"

#include "AccountPicture.h"
#include "FamilyChange.h"
#include "NewPassword.h"
#include "PictureFile.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QVariantMap>

#include <utility>

namespace {

AccountPage::Reason reasonForProblem(NewPassword::Problem problem) {
    switch (problem) {
    case NewPassword::Problem::Empty:
        return AccountPage::Reason::Empty;
    case NewPassword::Problem::TooLong:
        return AccountPage::Reason::TooLong;
    case NewPassword::Problem::Unprintable:
        return AccountPage::Reason::Unprintable;
    }
    return AccountPage::Reason::Unknown;
}

// The PNG and JPEG files in one folder, in name order whatever the case,
// as Plasma's own folder mixes both. None when there is no such folder.
QVariantList picturesIn(const QDir& folder) {
    QVariantList pictures;
    const QStringList names = folder.entryList(
        {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg")}, QDir::Files,
        QDir::Name | QDir::IgnoreCase);
    for (const QString& name : names) {
        const QString file = folder.filePath(name);
        pictures.append(QVariantMap{
            {QStringLiteral("url"), QUrl::fromLocalFile(file)},
            {QStringLiteral("name"), QFileInfo(file).completeBaseName()},
        });
    }
    return pictures;
}

} // namespace

AccountPage::AccountPage(Setup setup, QObject* parent)
    : QObject(parent), m_setup(std::move(setup)),
      m_change(new FamilyChange(m_setup.pkexec, m_setup.command, this)) {
    connect(m_change, &FamilyChange::finished, this, &AccountPage::passwordAnswered);
    if (m_setup.bus) {
        m_picture = new AccountPicture(*m_setup.bus, m_setup.uid, this);
        connect(m_picture, &AccountPicture::finished, this, &AccountPage::pictureAnswered);
    }
    const std::optional<Account> account = m_setup.lookUpUid(m_setup.uid);
    m_name = account ? account->displayName() : QString();
    reload();
}

QString AccountPage::name() const {
    return m_name;
}

// The same file holds each new picture, so a number on the URL makes the
// page load it again.
QUrl AccountPage::picture() const {
    if (m_pictureFile.isEmpty()) {
        return {};
    }
    QUrl url = QUrl::fromLocalFile(m_pictureFile);
    url.setQuery(QStringLiteral("v=%1").arg(m_pictureVersion));
    return url;
}

// KDE's drawings at the top of Plasma's folder, then the photos KDE keeps
// in a folder of their own. Any other folder there is not KDE's.
QVariantList AccountPage::pictures() const {
    const QDir folder(m_setup.stockPictures);
    return picturesIn(folder) + picturesIn(QDir(folder.filePath(QStringLiteral("photos"))));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
int AccountPage::maxPasswordLength() const {
    return static_cast<int>(NewPassword::maxLength);
}

AccountPage::Step AccountPage::step() const {
    return m_step;
}

AccountPage::Reason AccountPage::reason() const {
    return m_reason;
}

bool AccountPage::busy() const {
    return m_step == Step::Asking || m_step == Step::SettingPicture;
}

// Checked here first, so the current password is asked for only when the
// new one will be taken. cairn-family checks it again.
void AccountPage::changePassword(const QString& password, const QString& again) {
    if (busy()) {
        return;
    }
    if (const std::optional<NewPassword::Problem> problem = NewPassword::check(password)) {
        setStep(Step::PasswordNotChanged, reasonForProblem(*problem));
        return;
    }
    if (password != again) {
        setStep(Step::PasswordNotChanged, Reason::Different);
        return;
    }
    setStep(Step::Asking);
    m_change->start({QStringLiteral("set-own-password")}, password.toUtf8() + '\n');
}

void AccountPage::choosePicture(const QUrl& file) {
    if (busy()) {
        return;
    }
    if (m_picture == nullptr || !m_work.isValid()) {
        setStep(Step::PictureNotChanged, Reason::PictureRefused);
        return;
    }
    const QString target = m_work.filePath(QStringLiteral("picture.png"));
    if (!file.isLocalFile() || !PictureFile::prepare(file.toLocalFile(), target)) {
        setStep(Step::PictureNotChanged, Reason::PictureUnreadable);
        return;
    }
    if (!m_picture->set(target)) {
        setStep(Step::PictureNotChanged, Reason::PictureRefused);
        return;
    }
    setStep(Step::SettingPicture);
}

void AccountPage::reload() {
    m_pictureFile = m_picture != nullptr ? m_picture->current() : QString();
    ++m_pictureVersion;
    emit pictureChanged();
}

AccountPage::Reason AccountPage::reasonFor(const QString& answer) {
    static const QHash<QString, Reason> reasons{
        {QStringLiteral("not-root"), Reason::NotRoot},
        {QStringLiteral("not-a-grown-up"), Reason::NotAGrownUp},
        {QStringLiteral("usage"), Reason::Usage},
        {QStringLiteral("bad-password"), Reason::BadPassword},
        {QStringLiteral("failed"), Reason::Failed},
        {QStringLiteral("dismissed"), Reason::Dismissed},
        {QStringLiteral("not-authorized"), Reason::NotAuthorized},
    };
    return reasons.value(answer, Reason::Unknown);
}

void AccountPage::passwordAnswered(const QString& answer) {
    if (answer == QStringLiteral("done")) {
        setStep(Step::PasswordChanged);
        return;
    }
    setStep(Step::PasswordNotChanged, reasonFor(answer));
}

void AccountPage::pictureAnswered(bool changed) {
    reload();
    setStep(changed ? Step::PictureChanged : Step::PictureNotChanged,
            changed ? Reason::None : Reason::PictureRefused);
}

void AccountPage::setStep(Step step, Reason reason) {
    m_step = step;
    m_reason = reason;
    emit stepChanged();
}
