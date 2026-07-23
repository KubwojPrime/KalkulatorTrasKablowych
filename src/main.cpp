#include "data/catalogrepository.h"
#include "licensing/accesscontroller.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

#include "config.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("KubwojPrime"));
    QApplication::setOrganizationDomain(QStringLiteral("local"));
    QApplication::setApplicationName(QStringLiteral("KalkulatorTrasKablowych"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kalkulator Tras Kablowych"));
    QApplication::setApplicationVersion(QString::fromUtf8(KTK_APP_VERSION));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    ktk::Theme::applyDark(application);

    ktk::AccessController accessController;
    const auto access = accessController.verify();
    if (!access.allowed) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Brak dostępu"),
            access.message);
        return 23;
    }

    if (application.arguments().contains(QStringLiteral("--smoke-test"))) {
        ktk::CatalogRepository repository;
        QString error;
        if (!repository.open(&error)) {
            qCritical().noquote() << error;
            return 31;
        }
        const auto items = repository.allItems(&error);
        if (!error.isEmpty() || items.isEmpty()) {
            qCritical().noquote() << (error.isEmpty()
                                             ? QStringLiteral("Katalog startowy jest pusty.")
                                             : error);
            return 32;
        }
        qInfo() << "smoke test passed; catalog items:" << items.size();
        return 0;
    }

    ktk::MainWindow window;
    window.show();
    if (access.usedOfflineGrace) {
        window.showAccessNotice(access.message);
    }
    return application.exec();
}
