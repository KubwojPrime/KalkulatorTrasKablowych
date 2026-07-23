#include "data/catalogrepository.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace ktk {

namespace {

void setError(QString *target, const QString &message)
{
    if (target) {
        *target = message;
    }
}

} // namespace

CatalogRepository::CatalogRepository()
    : m_connectionName(QStringLiteral("catalog-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDirectory);
    m_databasePath = QDir(dataDirectory).filePath(QStringLiteral("catalog.sqlite"));
}

CatalogRepository::~CatalogRepository()
{
    if (m_database.isValid()) {
        m_database.close();
    }
    m_database = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool CatalogRepository::open(QString *errorMessage)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        setError(errorMessage, QStringLiteral("Brak sterownika Qt SQLite."));
        return false;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    return createSchema(errorMessage) && seedIfEmpty(errorMessage);
}

QVector<CatalogItem> CatalogRepository::allItems(QString *errorMessage) const
{
    QVector<CatalogItem> items;
    if (!m_database.isOpen()) {
        setError(errorMessage, QStringLiteral("Baza katalogowa nie jest otwarta."));
        return items;
    }

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT id, manufacturer, designation, catalog_code, diameter_mm, mass_kg_km, "
            "fire_load_mj_m, cpr_class, source, source_date, notes, verified "
            "FROM cable_catalog ORDER BY manufacturer, designation"))) {
        setError(errorMessage, query.lastError().text());
        return items;
    }

    while (query.next()) {
        CatalogItem item;
        item.id = query.value(0).toLongLong();
        item.cable.manufacturer = query.value(1).toString();
        item.cable.designation = query.value(2).toString();
        item.cable.catalogCode = query.value(3).toString();
        item.cable.outerDiameterMm = query.value(4).toDouble();
        item.cable.massKgPerKm = query.value(5).toDouble();
        if (!query.value(6).isNull()) {
            item.cable.fireLoadMjPerM = query.value(6).toDouble();
        }
        item.cable.cprClass = query.value(7).toString();
        item.cable.source = query.value(8).toString();
        item.sourceDate = query.value(9).toString();
        item.notes = query.value(10).toString();
        item.verified = query.value(11).toBool();
        items.append(item);
    }

    return items;
}

QString CatalogRepository::databasePath() const
{
    return m_databasePath;
}

bool CatalogRepository::createSchema(QString *errorMessage)
{
    QSqlQuery query(m_database);
    const QString statement = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS cable_catalog ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "manufacturer TEXT NOT NULL,"
        "designation TEXT NOT NULL,"
        "catalog_code TEXT NOT NULL DEFAULT '',"
        "diameter_mm REAL NOT NULL,"
        "mass_kg_km REAL NOT NULL,"
        "fire_load_mj_m REAL NULL,"
        "cpr_class TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT '',"
        "source_date TEXT NOT NULL DEFAULT '',"
        "notes TEXT NOT NULL DEFAULT '',"
        "verified INTEGER NOT NULL DEFAULT 0,"
        "UNIQUE(manufacturer, designation, catalog_code)"
        ")");

    if (!query.exec(statement)) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool CatalogRepository::seedIfEmpty(QString *errorMessage)
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM cable_catalog")) || !query.next()) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    if (query.value(0).toLongLong() > 0) {
        return true;
    }
    return importSeed(errorMessage);
}

bool CatalogRepository::importSeed(QString *errorMessage)
{
    QFile file(QStringLiteral(":/catalog-seed.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Nie można otworzyć startowego katalogu kabli."));
        return false;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("Błąd katalogu startowego: %1").arg(parseError.errorString()));
        return false;
    }

    if (!m_database.transaction()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    QSqlQuery insert(m_database);
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO cable_catalog("
        "manufacturer, designation, catalog_code, diameter_mm, mass_kg_km, "
        "fire_load_mj_m, cpr_class, source, source_date, notes, verified"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    const auto items = document.object().value(QStringLiteral("items")).toArray();
    for (const auto &value : items) {
        const auto object = value.toObject();
        insert.bindValue(0, object.value(QStringLiteral("manufacturer")).toString());
        insert.bindValue(1, object.value(QStringLiteral("designation")).toString());
        insert.bindValue(2, object.value(QStringLiteral("catalogCode")).toString());
        insert.bindValue(3, object.value(QStringLiteral("outerDiameterMm")).toDouble());
        insert.bindValue(4, object.value(QStringLiteral("massKgPerKm")).toDouble());
        const auto fireLoad = object.value(QStringLiteral("fireLoadMjPerM"));
        insert.bindValue(5, fireLoad.isNull() || fireLoad.isUndefined()
                                ? QVariant()
                                : QVariant(fireLoad.toDouble()));
        insert.bindValue(6, object.value(QStringLiteral("cprClass")).toString());
        insert.bindValue(7, object.value(QStringLiteral("source")).toString());
        insert.bindValue(8, object.value(QStringLiteral("sourceDate")).toString());
        insert.bindValue(9, object.value(QStringLiteral("notes")).toString());
        insert.bindValue(10, object.value(QStringLiteral("verified")).toBool());

        if (!insert.exec()) {
            m_database.rollback();
            setError(errorMessage, insert.lastError().text());
            return false;
        }
        insert.finish();
    }

    if (!m_database.commit()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }
    return true;
}

} // namespace ktk
