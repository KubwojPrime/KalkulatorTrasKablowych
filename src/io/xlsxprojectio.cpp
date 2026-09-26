#include "io/xlsxprojectio.h"

#include <xlsxdocument.h>
#include <xlsxformat.h>

#include <QFileInfo>
#include <QSaveFile>
#include <QBuffer>
#include <QLocale>
#include <QStringList>

#include <cmath>
#include <limits>

namespace ktk {

namespace {

constexpr int SchemaVersion = 4;
constexpr int MinimumSchemaVersion = 1;

struct ParsedNumber {
    bool present = false;
    bool valid = false;
    double value = 0.0;
};

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

ParsedNumber parseNumber(const QVariant &cellValue)
{
    const QString text = cellValue.toString().trimmed();
    if (!cellValue.isValid() || text.isEmpty()) {
        return {};
    }

    bool ok = false;
    double value = cellValue.toDouble(&ok);
    if (!ok) {
        value = QLocale::c().toDouble(QString(text).replace(',', '.'), &ok);
    }
    return {true, ok && std::isfinite(value), value};
}

bool readDoubleField(
    QXlsx::Document &document,
    int row,
    int column,
    double fallback,
    const QString &label,
    double *target,
    QString *errorMessage)
{
    const ParsedNumber parsed = parseNumber(document.read(row, column));
    if (!parsed.present) {
        *target = fallback;
        return true;
    }
    if (!parsed.valid) {
        setError(
            errorMessage,
            QStringLiteral("Pole %1 zawiera nieprawidlowa liczbe.").arg(label));
        return false;
    }
    *target = parsed.value;
    return true;
}

QString assemblyDescription(const QVector<RouteAssemblyItem> &items)
{
    QStringList symbols;
    for (const auto &item : items) {
        symbols << (item.quantity == 1.0
                        ? item.product.symbol
                        : QStringLiteral("%1 × %2")
                              .arg(item.quantity, 0, 'g', 4)
                              .arg(item.product.symbol));
    }
    return symbols.join(QStringLiteral(", "));
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
    document.setColumnWidth(1, 48);
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
    writeKeyValue(
        document,
        14,
        QStringLiteral("Pochodzenie masy trasy"),
        project.routeAssembly.isEmpty()
            ? QStringLiteral("wartości ręczne")
            : QStringLiteral("zestaw produktów BAKS"));
    writeKeyValue(
        document,
        15,
        QStringLiteral("Zestaw BAKS"),
        assemblyDescription(project.routeAssembly));

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
        QStringLiteral("Pochodzenie MJ/m"),
        QStringLiteral("Podstawa oszacowania"),
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
    document.setColumnWidth(8, 24);
    document.setColumnWidth(9, 70);
    document.setColumnWidth(10, 18);
    document.setColumnWidth(11, 54);

    for (int index = 0; index < project.cables.size(); ++index) {
        const int row = index + 2;
        const auto &cable = project.cables.at(index);
        document.write(row, 1, cable.manufacturer);
        document.write(row, 2, cable.designation);
        document.write(row, 3, cable.catalogCode);
        document.write(row, 4, cable.quantity);
        document.write(row, 5, cable.outerDiameterMm);
        if (cable.massKgPerKm.has_value()) {
            document.write(row, 6, cable.massKgPerKm.value());
        }
        if (cable.fireLoadMjPerM.has_value()) {
            if (cable.fireLoadEstimated) {
                document.write(
                    row,
                    7,
                    QString::number(cable.fireLoadMjPerM.value(), 'f', 3)
                        + QStringLiteral("*"));
            } else {
                document.write(row, 7, cable.fireLoadMjPerM.value());
            }
        }
        document.write(
            row,
            8,
            cable.fireLoadMjPerM.has_value()
                ? (cable.fireLoadEstimated
                       ? QStringLiteral("oszacowanie materiałowe*")
                       : QStringLiteral("wartość podana / producent"))
                : QStringLiteral("brak"));
        document.write(row, 9, cable.fireLoadBasis);
        document.write(row, 10, cable.cprClass);
        document.write(row, 11, cable.source);
    }

    document.addSheet(QStringLiteral("Zestaw BAKS"));
    document.selectSheet(QStringLiteral("Zestaw BAKS"));
    const QStringList baksHeaders = {
        QStringLiteral("ID rekordu"),
        QStringLiteral("Rola"),
        QStringLiteral("Nazwa"),
        QStringLiteral("Symbol"),
        QStringLiteral("Numer katalogowy"),
        QStringLiteral("Ilość"),
        QStringLiteral("Masa katalogowa"),
        QStringLiteral("Jednostka masy"),
        QStringLiteral("Długość odcinka [m]"),
        QStringLiteral("Szerokość [mm]"),
        QStringLiteral("Wysokość [mm]"),
        QStringLiteral("Plik źródłowy"),
        QStringLiteral("Strona PDF"),
        QStringLiteral("URL źródła"),
        QStringLiteral("Data źródła")
    };
    for (int column = 0; column < baksHeaders.size(); ++column) {
        document.write(1, column + 1, baksHeaders.at(column), headerFormat());
    }
    document.setColumnWidth(1, 24);
    document.setColumnWidth(2, 16);
    document.setColumnWidth(3, 28);
    document.setColumnWidth(4, 22);
    document.setColumnWidth(5, 18);
    document.setColumnWidth(6, 10);
    document.setColumnWidth(7, 18);
    document.setColumnWidth(8, 16);
    document.setColumnWidth(9, 20);
    document.setColumnWidth(10, 16);
    document.setColumnWidth(11, 16);
    document.setColumnWidth(12, 58);
    document.setColumnWidth(13, 14);
    document.setColumnWidth(14, 68);
    document.setColumnWidth(15, 16);
    for (int index = 0; index < project.routeAssembly.size(); ++index) {
        const int row = index + 2;
        const auto &item = project.routeAssembly.at(index);
        const auto &product = item.product;
        document.write(row, 1, product.id);
        document.write(row, 2, product.role);
        document.write(row, 3, product.name);
        document.write(row, 4, product.symbol);
        document.write(row, 5, product.catalogCode);
        document.write(row, 6, item.quantity);
        document.write(row, 7, product.massKgPerUnit);
        document.write(row, 8, product.massUnit);
        document.write(row, 9, product.lengthM);
        document.write(row, 10, product.widthMm);
        document.write(row, 11, product.heightMm);
        document.write(row, 12, product.sourceFile);
        if (product.sourcePage > 0) {
            document.write(row, 13, product.sourcePage);
        }
        document.write(row, 14, product.sourceUrl);
        document.write(row, 15, product.sourceDate);
    }

    document.addSheet(QStringLiteral("Raport"));
    document.selectSheet(QStringLiteral("Raport"));
    document.setColumnWidth(1, 42);
    document.setColumnWidth(2, 22);
    writeKeyValue(document, 1, QStringLiteral("Pole zarezerwowane Σ(n × D²) [mm²]"),
                  result.reservedCableAreaMm2);
    writeKeyValue(document, 2, QStringLiteral("Pole trasy [mm²]"), result.routeAreaMm2);
    writeKeyValue(document, 3, QStringLiteral("Wypełnienie [%]"), result.fillPercent);
    writeKeyValue(document, 5, QStringLiteral("Znana masa kabli [kg/m]"),
                  result.cableMassKgPerM);
    writeKeyValue(document, 6, QStringLiteral("Masa trasy i zawieszeń [kg/m]"),
                  result.supportSystemMassKgPerM);
    writeKeyValue(document, 7, QStringLiteral("Znana masa kompletna [kg/m]"),
                  result.totalInstalledMassKgPerM);
    writeKeyValue(document, 8, QStringLiteral("Wiersze bez danych masowych"),
                  result.unknownMassRows);
    writeKeyValue(
        document,
        9,
        result.estimatedFireLoadRows > 0
            ? QStringLiteral("Łączne obciążenie ogniowe* [MJ/m]")
            : QStringLiteral("Łączne obciążenie ogniowe [MJ/m]"),
        result.knownFireLoadMjPerM);
    writeKeyValue(document, 10, QStringLiteral("Wartości bez * [MJ/m]"),
                  result.confirmedFireLoadMjPerM);
    writeKeyValue(document, 11, QStringLiteral("Oszacowane materiałowo* [MJ/m]"),
                  result.estimatedFireLoadMjPerM);
    writeKeyValue(document, 12, QStringLiteral("Wiersze z oszacowaniem*"),
                  result.estimatedFireLoadRows);
    writeKeyValue(document, 13, QStringLiteral("Wiersze bez danych ogniowych"),
                  result.unknownFireLoadRows);
    QStringList completeness;
    if (result.unknownMassRows > 0) {
        completeness << QStringLiteral("Wynik masy jest niepełny.");
    }
    if (result.unknownFireLoadRows > 0) {
        completeness << QStringLiteral("Wynik obciążenia ogniowego jest niepełny.");
    }
    if (result.estimatedFireLoadRows > 0) {
        completeness
            << QStringLiteral(
                   "* Wynik zawiera oszacowania materiałowe. Nie zastępują one "
                   "wartości z karty producenta ani uzgodnienia z projektantem ppoż.");
    }
    writeKeyValue(
        document,
        15,
        QStringLiteral("Wniosek"),
        completeness.isEmpty()
            ? QStringLiteral("Wszystkie wiersze mają dane masowe i ogniowe.")
            : completeness.join(QLatin1Char(' ')));
    writeKeyValue(
        document,
        17,
        QStringLiteral("Pochodzenie masy trasy"),
        project.routeAssembly.isEmpty()
            ? QStringLiteral("wartości ręczne")
            : QStringLiteral("BAKS: %1")
                  .arg(assemblyDescription(project.routeAssembly)));

    document.addSheet(QStringLiteral("Meta"));
    document.selectSheet(QStringLiteral("Meta"));
    document.write(1, 1, QStringLiteral("schema"));
    document.write(1, 2, QStringLiteral("KalkulatorTrasKablowych"));
    document.write(2, 1, QStringLiteral("schemaVersion"));
    document.write(2, 2, SchemaVersion);
    document.write(3, 1, QStringLiteral("formula"));
    document.write(3, 2, QStringLiteral("sum(quantity * outerDiameterMm^2)"));
    document.write(4, 1, QStringLiteral("fireLoadEstimate"));
    document.write(
        4,
        2,
        QStringLiteral(
            "* = konserwatywne oszacowanie materiałowo-geometryczne; "
            "docs/fire-load-estimation.md"));
    document.write(5, 1, QStringLiteral("routeMassFormula"));
    document.write(
        5,
        2,
        QStringLiteral(
            "trasa + pokrywa + (elementy stale + wysokosc zwieszenia "
            "* elementy pionowe) / rozstaw podpor"));

    QSaveFile output(path);
    // QXlsx closes the device; QSaveFile must only be closed through commit().
    // Stage the ZIP in memory, then atomically replace the destination.
    QBuffer buffer;
    if (!buffer.open(QIODevice::ReadWrite) || !document.saveAs(&buffer) ||
        !output.open(QIODevice::WriteOnly) ||
        output.write(buffer.data()) != buffer.data().size() || !output.commit()) {
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

    bool schemaOk = false;
    const int schemaVersion = document.read(2, 2).toInt(&schemaOk);
    if (!schemaOk || schemaVersion < MinimumSchemaVersion) {
        setError(errorMessage,
                 QStringLiteral("Plik ma nieprawidlowa wersje schematu."));
        return false;
    }
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
    if (!parseNumber(document.read(2, 2)).present
        || !parseNumber(document.read(3, 2)).present
        || !parseNumber(document.read(4, 2)).present) {
        setError(
            errorMessage,
            QStringLiteral(
                "Arkusz Projekt nie zawiera wymaganych wymiarow lub limitu wypelnienia."));
        return false;
    }
    loaded.route.projectName = document.read(1, 2).toString();
    if (!readDoubleField(document, 2, 2, 300.0, QStringLiteral("szerokosc trasy"),
                         &loaded.route.internalWidthMm, errorMessage)
        || !readDoubleField(document, 3, 2, 60.0, QStringLiteral("wysokosc trasy"),
                            &loaded.route.internalHeightMm, errorMessage)
        || !readDoubleField(document, 4, 2, 40.0, QStringLiteral("limit wypelnienia"),
                            &loaded.route.maximumFillPercent, errorMessage)
        || !readDoubleField(document, 5, 2, 0.0, QStringLiteral("limit ogniowy"),
                            &loaded.route.fireLoadLimitMjPerM, errorMessage)
        || !readDoubleField(document, 7, 2, 0.0, QStringLiteral("masa koryta"),
                            &loaded.route.trayMassKgPerM, errorMessage)
        || !readDoubleField(document, 8, 2, 0.0, QStringLiteral("masa pokrywy"),
                            &loaded.route.coverMassKgPerM, errorMessage)
        || !readDoubleField(document, 9, 2, 0.0, QStringLiteral("masa zawieszenia"),
                            &loaded.route.hangerBaseMassKg, errorMessage)
        || !readDoubleField(document, 10, 2, 0.0, QStringLiteral("wysokosc zwieszenia"),
                            &loaded.route.suspensionHeightM, errorMessage)
        || !readDoubleField(document, 11, 2, 0.0, QStringLiteral("masa elementow pionowych"),
                            &loaded.route.suspensionVerticalMassKgPerM, errorMessage)
        || !readDoubleField(document, 12, 2, 1.5, QStringLiteral("rozstaw podpor"),
                            &loaded.route.supportSpacingM, errorMessage)) {
        return false;
    }

    if (loaded.route.internalWidthMm <= 0.0
        || loaded.route.internalHeightMm <= 0.0) {
        setError(errorMessage,
                 QStringLiteral("Wymiary wewnetrzne trasy musza byc dodatnie."));
        return false;
    }
    if (loaded.route.maximumFillPercent <= 0.0
        || loaded.route.maximumFillPercent > 100.0) {
        setError(errorMessage,
                 QStringLiteral("Limit wypelnienia musi nalezec do zakresu (0, 100]."));
        return false;
    }
    if (loaded.route.fireLoadLimitMjPerM < 0.0
        || loaded.route.trayMassKgPerM < 0.0
        || loaded.route.coverMassKgPerM < 0.0
        || loaded.route.hangerBaseMassKg < 0.0
        || loaded.route.suspensionHeightM < 0.0
        || loaded.route.suspensionVerticalMassKgPerM < 0.0
        || loaded.route.supportSpacingM <= 0.0) {
        setError(errorMessage,
                 QStringLiteral(
                     "Parametry masowe nie moga byc ujemne, a rozstaw podpor musi byc dodatni."));
        return false;
    }

    if (schemaVersion >= 4
        && document.sheetNames().contains(QStringLiteral("Zestaw BAKS"))
        && document.selectSheet(QStringLiteral("Zestaw BAKS"))) {
        constexpr int MaximumAssemblyRows = 10000;
        for (int row = 2; row <= MaximumAssemblyRows; ++row) {
            const QString id = document.read(row, 1).toString().trimmed();
            const QString symbol = document.read(row, 4).toString().trimmed();
            if (id.isEmpty() && symbol.isEmpty()) {
                break;
            }

            RouteAssemblyItem item;
            item.product.id = id;
            item.product.role = document.read(row, 2).toString().trimmed();
            item.product.name = document.read(row, 3).toString();
            item.product.symbol = symbol;
            item.product.catalogCode = document.read(row, 5).toString();
            if (!readDoubleField(
                    document,
                    row,
                    6,
                    1.0,
                    QStringLiteral("ilosc BAKS w wierszu %1").arg(row),
                    &item.quantity,
                    errorMessage)
                || !readDoubleField(
                    document,
                    row,
                    7,
                    0.0,
                    QStringLiteral("masa BAKS w wierszu %1").arg(row),
                    &item.product.massKgPerUnit,
                    errorMessage)
                || !readDoubleField(
                    document,
                    row,
                    9,
                    0.0,
                    QStringLiteral("dlugosc BAKS w wierszu %1").arg(row),
                    &item.product.lengthM,
                    errorMessage)
                || !readDoubleField(
                    document,
                    row,
                    10,
                    0.0,
                    QStringLiteral("szerokosc BAKS w wierszu %1").arg(row),
                    &item.product.widthMm,
                    errorMessage)
                || !readDoubleField(
                    document,
                    row,
                    11,
                    0.0,
                    QStringLiteral("wysokosc BAKS w wierszu %1").arg(row),
                    &item.product.heightMm,
                    errorMessage)) {
                return false;
            }
            item.product.massUnit = document.read(row, 8).toString();
            if (item.quantity <= 0.0 || item.product.massKgPerUnit < 0.0
                || item.product.lengthM < 0.0 || item.product.widthMm < 0.0
                || item.product.heightMm < 0.0) {
                setError(
                    errorMessage,
                    QStringLiteral("Wiersz %1 arkusza Zestaw BAKS ma ujemne lub zerowe dane.")
                        .arg(row));
                return false;
            }
            item.product.sourceFile = document.read(row, 12).toString();
            item.product.sourcePage = document.read(row, 13).toInt();
            item.product.sourceUrl = document.read(row, 14).toString();
            item.product.sourceDate = document.read(row, 15).toString();
            loaded.routeAssembly.append(item);
        }
    }

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
        const ParsedNumber quantity = parseNumber(document.read(row, 4));
        const ParsedNumber diameter = parseNumber(document.read(row, 5));
        if (!quantity.present || !quantity.valid || quantity.value < 1.0
            || quantity.value > std::numeric_limits<int>::max()
            || std::floor(quantity.value) != quantity.value) {
            setError(
                errorMessage,
                QStringLiteral("Wiersz %1 arkusza Kable ma nieprawidlowa ilosc.")
                    .arg(row));
            return false;
        }
        if (!diameter.present || !diameter.valid || diameter.value <= 0.0) {
            setError(
                errorMessage,
                QStringLiteral("Wiersz %1 arkusza Kable ma nieprawidlowa srednice.")
                    .arg(row));
            return false;
        }
        cable.quantity = static_cast<int>(quantity.value);
        cable.outerDiameterMm = diameter.value;
        const QVariant mass = document.read(row, 6);
        if (mass.isValid() && !mass.toString().trimmed().isEmpty()) {
            const ParsedNumber parsedMass = parseNumber(mass);
            if (!parsedMass.valid || parsedMass.value < 0.0) {
                setError(
                    errorMessage,
                    QStringLiteral("Wiersz %1 arkusza Kable ma nieprawidlowa mase.")
                        .arg(row));
                return false;
            }
            cable.massKgPerKm = parsedMass.value;
        }
        const QVariant fireLoad = document.read(row, 7);
        QString fireLoadText = fireLoad.toString().trimmed();
        const bool starMarker = fireLoadText.endsWith(QLatin1Char('*'));
        fireLoadText.remove(QLatin1Char('*'));
        if (fireLoad.isValid() && !fireLoadText.isEmpty()) {
            const ParsedNumber parsedFireLoad = parseNumber(fireLoadText);
            if (!parsedFireLoad.valid || parsedFireLoad.value < 0.0) {
                setError(
                    errorMessage,
                    QStringLiteral("Wiersz %1 arkusza Kable ma nieprawidlowe obciazenie ogniowe.")
                        .arg(row));
                return false;
            }
            cable.fireLoadMjPerM = parsedFireLoad.value;
        }
        if (schemaVersion >= 3) {
            const QString origin = document.read(row, 8).toString();
            cable.fireLoadEstimated =
                starMarker
                || origin.contains(
                    QStringLiteral("oszacowanie"),
                    Qt::CaseInsensitive);
            cable.fireLoadBasis = document.read(row, 9).toString();
            cable.cprClass = document.read(row, 10).toString();
            cable.source = document.read(row, 11).toString();
        } else {
            cable.cprClass = document.read(row, 8).toString();
            cable.source = document.read(row, 9).toString();
        }
        loaded.cables.append(cable);
    }

    *project = loaded;
    return true;
}

} // namespace ktk
