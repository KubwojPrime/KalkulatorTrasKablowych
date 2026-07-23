#include "licensing/accesscontroller.h"

#include "config.h"

#include <QDateTime>
#include <QEventLoop>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUuid>

namespace ktk {

AccessDecision AccessController::verify(QWidget *parent)
{
#if !KTK_LICENSE_REQUIRED
    Q_UNUSED(parent)
    return {true, QStringLiteral("Tryb bez zdalnej autoryzacji."), false};
#else
    const QString endpoint = QString::fromUtf8(KTK_LICENSE_ENDPOINT).trimmed();
    if (endpoint.isEmpty()) {
        return {
            false,
            QStringLiteral("Kompilacja wymaga licencji, lecz nie skonfigurowano serwera autoryzacji."),
            false
        };
    }

    QSettings settings;
    QString licenseKey = settings.value(QStringLiteral("license/key")).toString().trimmed();
    if (licenseKey.isEmpty()) {
        bool accepted = false;
        licenseKey = QInputDialog::getText(
                         parent,
                         QStringLiteral("Aktywacja programu"),
                         QStringLiteral("Wprowadź klucz dostępu:"),
                         QLineEdit::Normal,
                         {},
                         &accepted)
                         .trimmed();
        if (!accepted || licenseKey.isEmpty()) {
            return {false, QStringLiteral("Nie podano klucza dostępu."), false};
        }
        settings.setValue(QStringLiteral("license/key"), licenseKey);
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(10000);

    const QJsonObject payload{
        {QStringLiteral("licenseKey"), licenseKey},
        {QStringLiteral("installationId"), installationId()},
        {QStringLiteral("application"), QStringLiteral("KalkulatorTrasKablowych")},
        {QStringLiteral("version"), QString::fromUtf8(KTK_APP_VERSION)}
    };

    QNetworkReply *reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(12000);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return offlineDecision(QStringLiteral("Przekroczono czas oczekiwania na serwer licencji."));
    }

    const auto networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const QByteArray responseBody = reply->readAll();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        return offlineDecision(networkErrorText);
    }

    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(responseBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
        return offlineDecision(QStringLiteral("Serwer licencji zwrócił nieprawidłową odpowiedź."));
    }

    const auto object = response.object();
    const bool allowed = object.value(QStringLiteral("allowed")).toBool(false);
    const QString message = object.value(QStringLiteral("message")).toString();
    if (!allowed) {
        settings.remove(QStringLiteral("license/lastAllowedUtc"));
        return {
            false,
            message.isEmpty() ? QStringLiteral("Dostęp został cofnięty.") : message,
            false
        };
    }

    settings.setValue(
        QStringLiteral("license/lastAllowedUtc"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return {
        true,
        message.isEmpty() ? QStringLiteral("Dostęp potwierdzony.") : message,
        false
    };
#endif
}

void AccessController::clearStoredLicense()
{
    QSettings settings;
    settings.remove(QStringLiteral("license"));
}

QString AccessController::installationId() const
{
    QSettings settings;
    QString identifier =
        settings.value(QStringLiteral("license/installationId")).toString();
    if (identifier.isEmpty()) {
        identifier = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("license/installationId"), identifier);
    }
    return identifier;
}

AccessDecision AccessController::offlineDecision(const QString &networkError) const
{
    QSettings settings;
    const auto lastAllowed = QDateTime::fromString(
        settings.value(QStringLiteral("license/lastAllowedUtc")).toString(),
        Qt::ISODate);
    if (lastAllowed.isValid()
        && lastAllowed.secsTo(QDateTime::currentDateTimeUtc())
            <= static_cast<qint64>(KTK_OFFLINE_GRACE_HOURS) * 3600) {
        return {
            true,
            QStringLiteral("Tryb offline: %1").arg(networkError),
            true
        };
    }
    return {
        false,
        QStringLiteral("Nie można potwierdzić dostępu: %1").arg(networkError),
        false
    };
}

} // namespace ktk
