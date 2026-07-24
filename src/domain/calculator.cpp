#include "domain/calculator.h"

#include <algorithm>

namespace ktk {

CalculationResult Calculator::calculate(const ProjectData &project)
{
    CalculationResult result;

    const auto &route = project.route;
    result.routeAreaMm2 =
        std::max(0.0, route.internalWidthMm) * std::max(0.0, route.internalHeightMm);

    for (const auto &cable : project.cables) {
        if (cable.quantity <= 0 || cable.outerDiameterMm <= 0.0
            || (cable.massKgPerKm.has_value() && cable.massKgPerKm.value() < 0.0)) {
            ++result.invalidRows;
            continue;
        }

        const double count = static_cast<double>(cable.quantity);
        result.reservedCableAreaMm2 +=
            count * cable.outerDiameterMm * cable.outerDiameterMm;
        if (cable.massKgPerKm.has_value()) {
            result.cableMassKgPerM += count * cable.massKgPerKm.value() / 1000.0;
        } else {
            ++result.unknownMassRows;
        }

        if (cable.fireLoadMjPerM.has_value() && cable.fireLoadMjPerM.value() >= 0.0) {
            result.knownFireLoadMjPerM += count * cable.fireLoadMjPerM.value();
        } else {
            ++result.unknownFireLoadRows;
        }
    }

    if (result.routeAreaMm2 > 0.0) {
        result.fillPercent = 100.0 * result.reservedCableAreaMm2 / result.routeAreaMm2;
    }

    const double spacing = std::max(route.supportSpacingM, 0.001);
    const double hangerMass =
        std::max(0.0, route.hangerBaseMassKg)
        + std::max(0.0, route.suspensionHeightM)
            * std::max(0.0, route.suspensionVerticalMassKgPerM);

    result.supportSystemMassKgPerM =
        std::max(0.0, route.trayMassKgPerM)
        + std::max(0.0, route.coverMassKgPerM)
        + hangerMass / spacing;
    result.totalInstalledMassKgPerM =
        result.cableMassKgPerM + result.supportSystemMassKgPerM;

    result.exceedsFillLimit =
        route.maximumFillPercent > 0.0 && result.fillPercent > route.maximumFillPercent;
    result.exceedsFireLoadLimit =
        route.fireLoadLimitMjPerM > 0.0
        && result.knownFireLoadMjPerM > route.fireLoadLimitMjPerM;

    return result;
}

} // namespace ktk
