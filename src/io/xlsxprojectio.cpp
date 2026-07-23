#include "io/xlsxprojectio.h"

#include <xlsxdocument.h>
#include <xlsxformat.h>

#include <QFileInfo>

namespace ktk {

namespace {

constexpr int SchemaVersion = 1;

void setError(QString *target, const QString &message)
{
    if (target) {
        *target = message;
    }
}

QXlsx::Format headerFormat()
{
    QXlsx::Format format;
    format.setFontBold(true);
    format.setFontColor(Qt::white);
    format.setFillPattern(QXlsx::Format::PatternSolid);
    format.setPatternBackgroundColor(QColor(QStringLiteral("#1d4ed8")));
    format.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    format.setVerticalAlignment(QXlsx::Format::AlignVCenter);
    format.setBorderStyle(QXlsx::Format::BorderThin);
    return format;
}

QXlsx::Format sectionFormat()
{
    QXlsx::Format format;
    format.setFontBold(true);
    format.setFillPattern(QXlsx::Format::PatternSolid);
    format.setPatternBackgroundColor(QColor(QStringLiteral("#dbeafe")));
    return format;
}

void writeKeyValue(QXlsx::Document &document, int row, const QString &key,
                   const QVariant &value)
{
    document.write(row, 1, key, sectionFormat());
    document.write(row, 2, value);
}

double readDouble(QXlsx::Document &document, int row, int column, double fallback)
{
    bool ok = false;
    const double value = document.read(row, column).toDouble(&ok);
    return ok ? value : fallback;
}

} // namespace

bool XlsxProjectIo::exportProject(
    const QString &path,
    const ProjectData &project,
    const CalculationResult &result,
    QString *errorMessage)
{
    if (path.isEmpty()) {
        setError(errorMessage, QStringLiteral("Nie wskazano pliku docelowego."));
        return false;
    }

    QXlsx::Document document;
    document.addSheet(QStringLiteral("Projekt"));
    document.selectSheet(QStringLiteral("Projekt"));
    document.setColumnWidth(1, 34);
    document.setColumnWidth(2, 26);

    writeKeyValue(document, 1, QStringLiteral("Nazwa projektu"), project.route.projectName);
    writeKeyValue(document, 2, QStringLiteral("Szerokość wewnętrzna [mm]"), project.route.internalWidthMm);
    writeKeyValue(document, 3, QStringLiteral("Wysokość wewnętrzna [mm]"), project.route.internalHeightMm);
    writeKeyValue(document, 4, QStringLiteral("Limit wypełnienia [%]"), project.route.maximumFillPercent);
    writeKeyValue(document, 5, QStringLiteral("Limit obciążenia ogniowego [MJ/m]"), project.route.fireLoadLimitMjPerM);
    writeKeyValue(document, 7, QStringLiteral("Masa koryta [kg/m]"), project.route.trayMassKgPerM);
    writeKeyValue(document, 8, QStringLiteral("Masa pokrywy [kg/m]"), project.route.coverMassKgPerM);
    writeKeyValue(document, 9, QStringLiteral("Masa bazowa zawieszenia [kg]"), project.route.hangerBaseMassKg);
    writeKeyValue(document, 10, QStringLiteral("Wysokość zwieszenia [m]"), project.route.suspensionHeightM);
    writeKeyValue(document, 11, QStringLiteral("Masa elementów pionowych [kg/m zwieszenia]"),
                  project.route.suspensionVerticalMassKgPerM);
    writeKeyValue(document, 12, QStringLiteral("Rozstaw podpór [m]"), project.route.supportSpacingM);

    document.addSheet(QStringLiteral("Kable"));
    document.selectSheet(QStringLiteral("Kable"));
    const QStringList cableHeaders = {
        QStringLiteral("Producent"),
        QStringLiteral("Oznaczenie"),
        QStringLiteral("Kod katalogowy"),
        QStringLiteral("Ilość"),
        QStringLiteral("D [mm]"),
        QStringLiteral("Masa [kg/km]"),
        QStringLiteral("Obciążenie ogniowe [MJ/m/szt.]"),
        QStringLiteral("CPR"),
        QStringLiteral("Źródło")
    };
    for (int column = 0; column < cableHeaders.size(); ++column) {
        document.write(1, column + 1, cableHeaders.at(column), headerFormat());
    }
    document.setColumnWidth(1, 18);
    document.setColumnWidth(2, 30);
    document.setColumnWidth(3, 18);
    document.setColumnWidth(4, 10);
    document.setColumnWidth(5, 12);
    document.setColumnWidth(6, 15);
    document.setColumnWidth(7, 26);
    document.setColumnWidth(8, 18);
    document.setColumnWidth(9, 54);

    for (int index = 0; index < project.cables.size(); ++index) {
        const int row = index + 2;
        const auto &cable = project.cables.at(index);
        document.write(row, 1, cable.manufacturer);
        document.write(row, 2, cable.designation);
        document.write(row, 3, cable.catalogCode);
        document.write(row, 4, cable.quantity);
        document.write(row, 5, cable.outerDiameterMm);
        document.write(row, 6, cable.massKgPerKm);
        if (cable.fireLoadMjPerM.has_value()) {
            document.write(row, 7, cable.fireLoadMjPerM.value());
        }
        document.write(row, 8, cable.cprClass);
        document.write(row, 9, cable.source);
    }

    document.addSheet(QStringLiteral("Raport"));
    document.selectSheet(QStringLiteral("Raport"));
    document.setColumnWidth(1, 42);
    document.setColumnWidth(2, 22);
    writeKeyValue(document, 1, QStringLiteral("Pole zarezerwowane Σ(n × D²) [mm²]"),
                  result.reservedCableAreaMm2);
    writeKeyValue(document, 2, QStringLiteral("Pole trasy [mm²]"), result.routeAreaMm2);
    writeKeyValue(document, 3, QStringLiteral("Wypełnienie [%]"), result.fillPercent);
    writeKeyValue(document, 5, QStringLiteral("Masa kabli [kg/m]"), result.cableMassKgPerM);
    writeKeyValue(document, 6, QStringLiteral("Masa trasy i zawieszeń [kg/m]"),
                  result.supportSystemMassKgPerM);
    writeKeyValue(document, 7, QStringLiteral("Masa kompletna [kg/m]"),
                  result.totalInstalledMassKgPerM);
    writeKeyValue(document, 9, QStringLiteral("Znane obciążenie ogniowe [MJ/m]"),
                  result.knownFireLoadMjPerM);
    writeKeyValue(document, 10, QStringLiteral("Wiersze bez danych ogniowych"),
                  result.unknownFireLoadRows);
    writeKeyValue(document, 12, QStringLiteral("Wniosek"),
                  result.unknownFireLoadRows > 0
                      ? QStringLiteral("Wynik obciążenia ogniowego jest niepełny.")
                      : QStringLiteral("Wszystkie wiersze mają dane ogniowe."));

    document.addSheet(QStringLiteral("Meta"));
    document.selectSheet(QStringLiteral("Meta"));
    document.write(1, 1, QStringLiteral("schema"));
    document.write(1, 2, QStringLiteral("KalkulatorTrasKablowych"));
    document.write(2, 1, QStringLiteral("schemaVersion"));
    document.write(2, 2, SchemaVersion);
    document.write(3, 1, QStringLiteral("formula"));
    document.write(3, 2, QStringLiteral("sum(quantity * outerDiameterMm^2)"));

    if (!document.saveAs(path)) {
        setError(errorMessage,
                 QStringLiteral("Nie udało się zapisać pliku XLSX: %1")
                     .arg(QFileInfo(path).fileName()));
        return false;
    }
    return true;
}

bool XlsxProjectIo::importProject(
    const QString &path,
    ProjectData *project,
    QString *errorMessage)
{
    if (!project) {
        setError(errorMessage, QStringLiteral("Brak obiektu projektu docelowego."));
        return false;
    }

    QXlsx::Document document(path);
    if (!document.load()) {
        setError(errorMessage, QStringLiteral("Nie można odczytać pliku XLSX."));
        return false;
    }

    if (!document.sheetNames().contains(QStringLiteral("Meta"))
        || !document.selectSheet(QStringLiteral("Meta"))
        || document.read(1, 2).toString() != QStringLiteral("KalkulatorTrasKablowych")) {
        setError(errorMessage,
                 QStringLiteral("Plik nie jest projektem Kalkulatora Tras Kablowych."));
        return false;
    }

    const int schemaVersion = document.read(2, 2).toInt();
    if (schemaVersion > SchemaVersion) {
        setError(errorMessage,
                 QStringLiteral("Plik pochodzi z nowszej wersji programu."));
        return false;
    }

    ProjectData loaded;
    if (!document.selectSheet(QStringLiteral("Projekt"))) {
        setError(errorMessage, QStringLiteral("Brak arkusza „Projekt”."));
        return false;
    }
    loaded.route.projectName = document.read(1, 2).toString();
    loaded.route.internalWidthMm = readDouble(document, 2, 2, 300.0);
    loaded.route.internalHeightMm = readDouble(document, 3, 2, 60.0);
    loaded.route.maximumFillPercent = readDouble(document, 4, 2, 40.0);
    loaded.route.fireLoadLimitMjPerM = readDouble(document, 5, 2, 0.0);
    loaded.route.trayMassKgPerM = readDouble(document, 7, 2, 0.0);
    loaded.route.coverMassKgPerM = readDouble(document, 8, 2, 0.0);
    loaded.route.hangerBaseMassKg = readDouble(document, 9, 2, 0.0);
    loaded.route.suspensionHeightM = readDouble(document, 10, 2, 0.0);
    loaded.route.suspensionVerticalMassKgPerM = readDouble(document, 11, 2, 0.0);
    loaded.route.supportSpacingM = readDouble(document, 12, 2, 1.5);

    if (!document.selectSheet(QStringLiteral("Kable"))) {
        setError(errorMessage, QStringLiteral("Brak arkusza „Kable”."));
        return false;
    }

    constexpr int MaximumRows = 100000;
    for (int row = 2; row <= MaximumRows; ++row) {
        const QString manufacturer = document.read(row, 1).toString().trimmed();
        const QString designation = document.read(row, 2).toString().trimmed();
        if (manufacturer.isEmpty() && designation.isEmpty()) {
            break;
        }

        CableRow cable;
        cable.manufacturer = manufacturer;
        cable.designation = designation;
        cable.catalogCode = document.read(row, 3).toString();
        cable.quantity = document.read(row, 4).toInt();
        cable.outerDiameterMm = readDouble(document, row, 5, 0.0);
        cable.massKgPerKm = readDouble(document, row, 6, 0.0);
        const QVariant fireLoad = document.read(row, 7);
        if (fireLoad.isValid() && !fireLoad.toString().trimmed().isEmpty()) {
            bool ok = false;
            const double value = fireLoad.toDouble(&ok);
            if (ok) {
                cable.fireLoadMjPerM = value;
            }
        }
        cable.cprClass = document.read(row, 8).toString();
        cable.source = document.read(row, 9).toString();
        loaded.cables.append(cable);
    }

    *project = loaded;
    return true;
}

} // namespace ktk
