#pragma once

#include "domain/types.h"

#include <QString>

namespace ktk {

class XlsxProjectIo final {
public:
    [[nodiscard]] static bool exportProject(
        const QString &path,
        const ProjectData &project,
        const CalculationResult &result,
        QString *errorMessage = nullptr);

    [[nodiscard]] static bool importProject(
        const QString &path,
        ProjectData *project,
        QString *errorMessage = nullptr);
};

} // namespace ktk
