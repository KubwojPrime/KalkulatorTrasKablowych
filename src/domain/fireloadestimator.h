#pragma once

#include "domain/types.h"

#include <optional>

namespace ktk {

struct FireLoadEstimate {
    double fireLoadMjPerM = 0.0;
    QString material;
    QString basis;
};

class FireLoadEstimator final {
public:
    [[nodiscard]] static std::optional<FireLoadEstimate> estimate(
        const CableRow &cable);
    static bool applyIfMissing(CableRow *cable);
};

} // namespace ktk
