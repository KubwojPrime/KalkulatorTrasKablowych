#include "domain/bakscatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace ktk {

namespace {

void setError(QString *target, const QString &message)
{
    if (target) {
        *target = message;
    }
}

bool isKnownRole(const QString &role)
{
    return role == QStringLiteral("route")
           || role == QStringLiteral("cover")
           || role == QStringLiteral("fixed")
           || role == QStringLiteral("vertical");
}

} // namespace

QVector<BaksProduct> BaksCatalog::load(QString *errorMessage)
{
    QFile file(QStringLiteral(":/baks-catalog.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Nie można otworzyć wbudowanego katalogu BAKS."));
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(
            errorMessage,
            QStringLiteral("Nieprawidłowy format katalogu BAKS: %1")
                .arg(parseError.errorString()));
        return {};
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt() != 1
        || !root.value(QStringLiteral("products")).isArray()) {
        setError(errorMessage, QStringLiteral("Nieobsługiwana wersja katalogu BAKS."));
        return {};
    }

    QVector<BaksProduct> products;
    QSet<QString> ids;
    const QJsonArray array = root.value(QStringLiteral("products")).toArray();
    products.reserve(array.size());

    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        BaksProduct product;
        product.id = object.value(QStringLiteral("id")).toString().trimmed();
        product.role = object.value(QStringLiteral("role")).toString().trimmed();
        product.name = object.value(QStringLiteral("name")).toString().trimmed();
        product.symbol = object.value(QStringLiteral("symbol")).toString().trimmed();
        product.catalogCode =
            object.value(QStringLiteral("catalogCode")).toString().trimmed();
        product.widthMm = object.value(QStringLiteral("widthMm")).toDouble();
        product.heightMm = object.value(QStringLiteral("heightMm")).toDouble();
        product.lengthM = object.value(QStringLiteral("lengthM")).toDouble();
        product.massKgPerUnit =
            object.value(QStringLiteral("massKgPerUnit")).toDouble();
        product.massUnit =
            object.value(QStringLiteral("massUnit")).toString().trimmed();
        product.sourceFile =
            object.value(QStringLiteral("sourceFile")).toString().trimmed();
        product.sourcePage =
            object.value(QStringLiteral("sourcePage")).toInt();
        product.sourceUrl =
            object.value(QStringLiteral("sourceUrl")).toString().trimmed();
        product.sourceDate =
            object.value(QStringLiteral("sourceDate")).toString().trimmed();

        const bool continuous =
            product.role == QStringLiteral("route")
            || product.role == QStringLiteral("cover");
        const bool valid =
            !product.id.isEmpty()
            && !ids.contains(product.id)
            && isKnownRole(product.role)
            && !product.symbol.isEmpty()
            && product.massKgPerUnit > 0.0
            && (continuous
                    ? product.massUnit == QStringLiteral("kg/m")
                    : product.massUnit == QStringLiteral("kg/szt."))
            && (product.role != QStringLiteral("vertical")
                    || product.lengthM > 0.0)
            && !product.sourceFile.isEmpty()
            && !product.sourceUrl.isEmpty();
        if (!valid) {
            setError(
                errorMessage,
                QStringLiteral("Nieprawidłowy lub zduplikowany rekord BAKS: %1")
                    .arg(product.id));
            return {};
        }

        ids.insert(product.id);
        products.append(product);
    }

    std::sort(
        products.begin(),
        products.end(),
        [](const BaksProduct &left, const BaksProduct &right) {
            if (left.role != right.role) {
                return left.role < right.role;
            }
            if (left.widthMm != right.widthMm) {
                return left.widthMm < right.widthMm;
            }
            return left.symbol.localeAwareCompare(right.symbol) < 0;
        });
    return products;
}

BaksMassSummary BaksCatalog::summarize(
    const QVector<RouteAssemblyItem> &items)
{
    BaksMassSummary summary;
    for (const auto &item : items) {
        const double quantity = std::max(0.0, item.quantity);
        const auto &product = item.product;
        if (product.role == QStringLiteral("route")) {
            summary.trayMassKgPerM += quantity * product.massKgPerUnit;
        } else if (product.role == QStringLiteral("cover")) {
            summary.coverMassKgPerM += quantity * product.massKgPerUnit;
        } else if (product.role == QStringLiteral("fixed")) {
            summary.fixedMassKgPerSupport += quantity * product.massKgPerUnit;
        } else if (product.role == QStringLiteral("vertical")
                   && product.lengthM > 0.0) {
            summary.verticalMassKgPerSuspensionM +=
                quantity * product.massKgPerUnit / product.lengthM;
        }
    }
    return summary;
}

std::optional<BaksProduct> BaksCatalog::findById(
    const QVector<BaksProduct> &products,
    const QString &id)
{
    const auto iterator = std::find_if(
        products.cbegin(),
        products.cend(),
        [&id](const BaksProduct &product) {
            return product.id == id;
        });
    if (iterator == products.cend()) {
        return std::nullopt;
    }
    return *iterator;
}

QString BaksCatalog::roleLabel(const QString &role)
{
    if (role == QStringLiteral("route")) {
        return QStringLiteral("trasa ciągła");
    }
    if (role == QStringLiteral("cover")) {
        return QStringLiteral("pokrywa ciągła");
    }
    if (role == QStringLiteral("fixed")) {
        return QStringLiteral("szt./podporę");
    }
    if (role == QStringLiteral("vertical")) {
        return QStringLiteral("szt./m zwieszenia");
    }
    return role;
}

QString BaksCatalog::contributionLabel(const RouteAssemblyItem &item)
{
    const auto &product = item.product;
    if (product.role == QStringLiteral("route")
        || product.role == QStringLiteral("cover")) {
        return QStringLiteral("%1 kg/m")
            .arg(item.quantity * product.massKgPerUnit, 0, 'f', 3);
    }
    if (product.role == QStringLiteral("fixed")) {
        return QStringLiteral("%1 kg/podporę")
            .arg(item.quantity * product.massKgPerUnit, 0, 'f', 3);
    }
    if (product.role == QStringLiteral("vertical")
        && product.lengthM > 0.0) {
        return QStringLiteral("%1 kg/m zwieszenia")
            .arg(
                item.quantity * product.massKgPerUnit / product.lengthM,
                0,
                'f',
                3);
    }
    return {};
}

} // namespace ktk
