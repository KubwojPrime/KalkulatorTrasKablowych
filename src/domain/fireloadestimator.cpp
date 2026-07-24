#include "domain/fireloadestimator.h"

#include "domain/catalogfilter.h"

#include <QLocale>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ktk {

namespace {

struct MaterialProfile {
    QString tag;
    double massCoefficientMjPerKg = 0.0;
    double volumetricCoefficientMjPerL = 0.0;
};

const QList<MaterialProfile> &materialProfiles()
{
    // Współczynniki są celowo konserwatywnymi wartościami projektowymi, a nie
    // danymi producenta. Podstawa i marginesy są opisane w
    // docs/fire-load-estimation.md.
    static const QList<MaterialProfile> profiles = {
        {QStringLiteral("PVC"), 25.0, 35.0},
        {QStringLiteral("XLPE / polietylen sieciowany"), 40.0, 45.0},
        {QStringLiteral("PE / polietylen"), 40.0, 45.0},
        {QStringLiteral("bezhalogenowa (LSZH)"), 40.0, 45.0},
        {QStringLiteral("guma / elastomer"), 30.0, 40.0},
        {QStringLiteral("PUR / poliuretan"), 30.0, 40.0}
    };
    return profiles;
}

QString normalizedDesignation(QString text)
{
    text.replace(QRegularExpression(QStringLiteral(R"((?<=\d),(?=\d))")),
                 QStringLiteral("."));
    return text;
}

std::optional<double> conductorAreaMm2(const QString &designation)
{
    const QString text = normalizedDesignation(designation);
    static const QRegularExpression nestedConfiguration(
        QStringLiteral(
            R"(\b\d+\s*[x×]\s*\d+\s*[x×]\s*\d+(?:\.\d+)?)"),
        QRegularExpression::CaseInsensitiveOption);
    if (nestedConfiguration.match(text).hasMatch()) {
        // Układy par/czwórek nie mają jednoznacznej interpretacji bez karty
        // konstrukcyjnej. Nie odejmujemy ich, zachowując zawyżający charakter
        // oszacowania.
        return std::nullopt;
    }

    static const QRegularExpression configuration(
        QStringLiteral(
            R"(\b(\d+)\s*(?:x|×|g)\s*(\d+(?:\.\d+)?))"),
        QRegularExpression::CaseInsensitiveOption);
    auto iterator = configuration.globalMatch(text);
    double area = 0.0;
    bool found = false;
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        bool countOk = false;
        bool sectionOk = false;
        const int count = match.captured(1).toInt(&countOk);
        const double section = match.captured(2).toDouble(&sectionOk);
        if (countOk && sectionOk && count > 0 && section > 0.0) {
            area += static_cast<double>(count) * section;
            found = true;
        }
    }
    return found ? std::optional<double>(area) : std::nullopt;
}

bool aluminumConductors(const QString &designation)
{
    static const QRegularExpression aluminum(
        QStringLiteral(
            R"(\b(?:YAKY|YAKXS|NA2X|A2XS|AALXS|XRUHAKXS|AXMK)[A-Z0-9]*)"),
        QRegularExpression::CaseInsensitiveOption);
    return aluminum.match(designation).hasMatch();
}

QString display(double value, int precision)
{
    return QLocale().toString(value, 'f', precision);
}

} // namespace

std::optional<FireLoadEstimate> FireLoadEstimator::estimate(
    const CableRow &cable)
{
    const QStringList tags = CatalogFilter::insulationTags(cable);
    if (tags.isEmpty()) {
        return std::nullopt;
    }

    double massCoefficient = 0.0;
    double volumetricCoefficient = 0.0;
    QStringList recognizedMaterials;
    for (const auto &profile : materialProfiles()) {
        if (!tags.contains(profile.tag, Qt::CaseInsensitive)) {
            continue;
        }
        recognizedMaterials.append(profile.tag);
        massCoefficient =
            std::max(massCoefficient, profile.massCoefficientMjPerKg);
        volumetricCoefficient =
            std::max(volumetricCoefficient, profile.volumetricCoefficientMjPerL);
    }
    if (recognizedMaterials.isEmpty()) {
        return std::nullopt;
    }

    const auto conductorArea = conductorAreaMm2(cable.designation);
    QStringList methods;
    double estimate = 0.0;

    if (cable.outerDiameterMm > 0.0) {
        const double cableArea =
            std::numbers::pi * cable.outerDiameterMm * cable.outerDiameterMm / 4.0;
        double nonMetalArea = cableArea;
        bool conductorAreaSubtracted = false;
        if (conductorArea.has_value()
            && conductorArea.value() > 0.0
            && conductorArea.value() < cableArea) {
            nonMetalArea -= conductorArea.value();
            conductorAreaSubtracted = true;
        }
        const double geometryEstimate =
            nonMetalArea * volumetricCoefficient / 1000.0;
        estimate = std::max(estimate, geometryEstimate);
        methods.append(
            QStringLiteral("%1 %2 mm² × %3 MJ/l")
                .arg(conductorAreaSubtracted
                         ? QStringLiteral("pole po odjęciu żył")
                         : QStringLiteral("pole kabla bez odjęcia żył"),
                     display(nonMetalArea, 1),
                     display(volumetricCoefficient, 1)));
    }

    if (cable.massKgPerKm.has_value()
        && cable.massKgPerKm.value() > 0.0) {
        double nonMetalMassKgPerKm = cable.massKgPerKm.value();
        bool conductorMassSubtracted = false;
        if (conductorArea.has_value() && conductorArea.value() > 0.0) {
            const double conductorDensity =
                aluminumConductors(cable.designation) ? 2.70 : 8.96;
            const double conductorMass =
                conductorArea.value() * conductorDensity;
            if (conductorMass < nonMetalMassKgPerKm) {
                nonMetalMassKgPerKm -= conductorMass;
                conductorMassSubtracted = true;
            }
        }
        const double massEstimate =
            nonMetalMassKgPerKm * massCoefficient / 1000.0;
        estimate = std::max(estimate, massEstimate);
        methods.append(
            QStringLiteral("%1 %2 kg/km × %3 MJ/kg")
                .arg(conductorMassSubtracted
                         ? QStringLiteral("masa po odjęciu żył")
                         : QStringLiteral("masa katalogowa bez odjęcia żył"),
                     display(nonMetalMassKgPerKm, 1),
                     display(massCoefficient, 1)));
    }

    if (!(estimate > 0.0) || methods.isEmpty()) {
        return std::nullopt;
    }

    FireLoadEstimate result;
    result.fireLoadMjPerM = estimate;
    result.material = recognizedMaterials.join(QStringLiteral(" + "));
    result.basis =
        QStringLiteral(
            "Oszacowanie materiałowe*: %1. Przyjęto większy wynik z: %2. "
            "Wartość nie pochodzi z karty producenta.")
            .arg(result.material, methods.join(QStringLiteral("; ")));
    return result;
}

bool FireLoadEstimator::applyIfMissing(CableRow *cable)
{
    if (!cable || cable->fireLoadMjPerM.has_value()) {
        return false;
    }
    const auto estimated = estimate(*cable);
    if (!estimated.has_value()) {
        return false;
    }
    cable->fireLoadMjPerM = estimated->fireLoadMjPerM;
    cable->fireLoadEstimated = true;
    cable->fireLoadBasis = estimated->basis;
    return true;
}

} // namespace ktk
