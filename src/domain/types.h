#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace ktk {

struct CableRow {
    QString manufacturer;
    QString designation;
    QString catalogCode;
    int quantity = 1;
    double outerDiameterMm = 0.0;
    std::optional<double> massKgPerKm;
    std::optional<double> fireLoadMjPerM;
    QString cprClass;
    QString source;
};

struct RouteParameters {
    QString projectName;
    double internalWidthMm = 300.0;
    double internalHeightMm = 60.0;
    double maximumFillPercent = 40.0;
    double fireLoadLimitMjPerM = 0.0;

    double trayMassKgPerM = 0.0;
    double coverMassKgPerM = 0.0;
    double hangerBaseMassKg = 0.0;
    double suspensionHeightM = 0.0;
    double suspensionVerticalMassKgPerM = 0.0;
    double supportSpacingM = 1.5;
};

struct ProjectData {
    RouteParameters route;
    QVector<CableRow> cables;
};

struct CalculationResult {
    double reservedCableAreaMm2 = 0.0;
    double routeAreaMm2 = 0.0;
    double fillPercent = 0.0;

    double cableMassKgPerM = 0.0;
    double supportSystemMassKgPerM = 0.0;
    double totalInstalledMassKgPerM = 0.0;

    double knownFireLoadMjPerM = 0.0;
    int unknownMassRows = 0;
    int unknownFireLoadRows = 0;
    int invalidRows = 0;

    bool exceedsFillLimit = false;
    bool exceedsFireLoadLimit = false;
};

struct CatalogItem {
    qint64 id = 0;
    CableRow cable;
    QString sourceDate;
    QString notes;
    QString sourceFile;
    int sourcePage = 0;
    QString sourceUrl;
    QString extractionMethod;
    bool verified = false;
};

} // namespace ktk
