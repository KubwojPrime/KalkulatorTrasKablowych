#include "domain/cablelayout.h"
#include <algorithm>
#include <cmath>
namespace ktk {
QVector<CablePlacement> cableLayout(const ProjectData &p) {
    QVector<int> rows;
    for (int i=0; i<p.cables.size(); ++i)
        if (p.cables[i].quantity>0 && std::isfinite(p.cables[i].outerDiameterMm) && p.cables[i].outerDiameterMm>0) rows.append(i);
    std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) { return p.cables[a].outerDiameterMm>p.cables[b].outerDiameterMm; });
    QVector<CablePlacement> result;
    double x=0, y=0, height=0;
    for (int row: rows) {
        const auto &c=p.cables[row];
        for (int i=0; i<c.quantity; ++i) {
            const double d=c.outerDiameterMm;
            if (x>0 && x+d>p.route.internalWidthMm+1e-6) { y+=height; x=0; height=0; }
            result.append({row,x,y,d,x+d>p.route.internalWidthMm+1e-6 || y+d>p.route.internalHeightMm+1e-6});
            x+=d; height=std::max(height,d);
        }
    }
    return result;
}
}
