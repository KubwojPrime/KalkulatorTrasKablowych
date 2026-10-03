#pragma once
#include "domain/types.h"
namespace ktk {
struct CablePlacement { int row; double x, y, diameter; bool overflow; };
inline constexpr int MaximumPreviewCables = 5000;
QVector<CablePlacement> cableLayout(const ProjectData &project, int limit = 100000);
}
