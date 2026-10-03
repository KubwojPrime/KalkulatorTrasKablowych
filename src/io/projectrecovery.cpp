#include "io/projectrecovery.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace ktk {
namespace {
QJsonObject encode(const BaksProduct &p) {
    QJsonObject o;
#define FIELD(f) o[#f] = p.f;
    FIELD(id) FIELD(role) FIELD(name) FIELD(symbol) FIELD(catalogCode)
    FIELD(widthMm) FIELD(heightMm) FIELD(lengthM) FIELD(massKgPerUnit)
    FIELD(massUnit) FIELD(sourceFile) FIELD(sourcePage) FIELD(sourceUrl) FIELD(sourceDate)
#undef FIELD
    return o;
}
BaksProduct product(const QJsonObject &o) {
    BaksProduct p;
#define TEXT(f) p.f = o[#f].toString();
    TEXT(id) TEXT(role) TEXT(name) TEXT(symbol) TEXT(catalogCode)
    TEXT(massUnit) TEXT(sourceFile) TEXT(sourceUrl) TEXT(sourceDate)
#undef TEXT
#define NUMBER(f) p.f = o[#f].toDouble();
    NUMBER(widthMm) NUMBER(heightMm) NUMBER(lengthM) NUMBER(massKgPerUnit)
#undef NUMBER
    p.sourcePage = o["sourcePage"].toInt();
    return p;
}
}
bool ProjectRecovery::save(const QString &path, const ProjectData &p, const QString &originalFile, QString *error) {
    QJsonObject route;
#define FIELD(f) route[#f] = p.route.f;
    FIELD(projectName) FIELD(internalWidthMm) FIELD(internalHeightMm) FIELD(maximumFillPercent)
    FIELD(fireLoadLimitMjPerM) FIELD(trayMassKgPerM) FIELD(coverMassKgPerM) FIELD(hangerBaseMassKg)
    FIELD(suspensionHeightM) FIELD(suspensionVerticalMassKgPerM) FIELD(supportSpacingM)
#undef FIELD
    QJsonArray cables, assembly;
    for (const auto &c : p.cables) {
        QJsonObject o;
#define FIELD(f) o[#f] = c.f;
        FIELD(manufacturer) FIELD(designation) FIELD(catalogCode) FIELD(quantity) FIELD(outerDiameterMm)
        FIELD(fireLoadEstimated) FIELD(fireLoadBasis) FIELD(cprClass) FIELD(source)
#undef FIELD
        if (c.massKgPerKm) o["massKgPerKm"] = *c.massKgPerKm;
        if (c.fireLoadMjPerM) o["fireLoadMjPerM"] = *c.fireLoadMjPerM;
        cables.append(o);
    }
    for (const auto &item : p.routeAssembly)
        assembly.append(QJsonObject{{"product", encode(item.product)}, {"quantity", item.quantity}});
    const QByteArray bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"originalFile", originalFile},
        {"route", route}, {"cables", cables}, {"assembly", assembly}}).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
bool ProjectRecovery::load(const QString &path, ProjectData *p, QString *originalFile, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { if (error) *error = file.errorString(); return false; }
    if (file.size() > 64 * 1024 * 1024) { if (error) *error = QStringLiteral("Zbyt duża kopia odzyskiwania."); return false; }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    const auto root = document.object();
    if (parse.error != QJsonParseError::NoError || root["schema"].toInt() != 1 ||
        !root["route"].isObject() || !root["cables"].isArray() || !root["assembly"].isArray()) {
        if (error) *error = QStringLiteral("Nieprawidłowa kopia odzyskiwania (format lub wersja).");
        return false;
    }
    ProjectData result;
    const auto route = root["route"].toObject();
    result.route.projectName = route["projectName"].toString();
#define FIELD(f) result.route.f = route[#f].toDouble(result.route.f);
    FIELD(internalWidthMm) FIELD(internalHeightMm) FIELD(maximumFillPercent)
    FIELD(fireLoadLimitMjPerM) FIELD(trayMassKgPerM) FIELD(coverMassKgPerM) FIELD(hangerBaseMassKg)
    FIELD(suspensionHeightM) FIELD(suspensionVerticalMassKgPerM) FIELD(supportSpacingM)
#undef FIELD
    for (const auto &value : root["cables"].toArray()) {
        if (!value.isObject()) { if(error) *error = QStringLiteral("Nieprawidłowy wiersz kopii."); return false; }
        const auto o = value.toObject();
        CableRow c;
#define TEXT(f) c.f = o[#f].toString();
        TEXT(manufacturer) TEXT(designation) TEXT(catalogCode) TEXT(fireLoadBasis) TEXT(cprClass) TEXT(source)
#undef TEXT
        c.quantity = o["quantity"].toInt(); c.outerDiameterMm = o["outerDiameterMm"].toDouble();
        c.fireLoadEstimated = o["fireLoadEstimated"].toBool();
        if (o["massKgPerKm"].isDouble()) c.massKgPerKm = o["massKgPerKm"].toDouble();
        if (o["fireLoadMjPerM"].isDouble()) c.fireLoadMjPerM = o["fireLoadMjPerM"].toDouble();
        result.cables.append(c);
    }
    for (const auto &value : root["assembly"].toArray()) {
        const auto o = value.toObject();
        result.routeAssembly.append({product(o["product"].toObject()), o["quantity"].toDouble()});
    }
    *p = result; *originalFile = root["originalFile"].toString();
    return true;
}
}
