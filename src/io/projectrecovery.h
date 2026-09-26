#pragma once
#include "domain/types.h"
namespace ktk {
class ProjectRecovery {
public:
    static bool save(const QString &path, const ProjectData &project, const QString &originalFile, QString *error);
    static bool load(const QString &path, ProjectData *project, QString *originalFile, QString *error);
};
}
