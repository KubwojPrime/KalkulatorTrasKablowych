#pragma once
#include "domain/types.h"
namespace ktk {
struct CablePlacement { int row; double x, y, diameter; bool overflow; };
QVector<CablePlacement> cableLayout(const ProjectData &project);
}
