#pragma once
#include "domain/types.h"
namespace ktk {
class DxfExport {
public:
    static bool write(const QString &path, const ProjectData &project, QString *error);
};
}
