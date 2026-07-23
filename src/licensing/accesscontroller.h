#pragma once

#include <QString>

class QWidget;

namespace ktk {

struct AccessDecision {
    bool allowed = false;
    QString message;
    bool usedOfflineGrace = false;
};

class AccessController final {
public:
    [[nodiscard]] AccessDecision verify(QWidget *parent = nullptr);
    void clearStoredLicense();

private:
    [[nodiscard]] QString installationId() const;
    [[nodiscard]] AccessDecision offlineDecision(const QString &networkError) const;
};

} // namespace ktk
