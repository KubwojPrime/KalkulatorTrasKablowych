#pragma once

#include "domain/types.h"

namespace ktk {

class Calculator final {
public:
    [[nodiscard]] static CalculationResult calculate(const ProjectData &project);
};

} // namespace ktk
