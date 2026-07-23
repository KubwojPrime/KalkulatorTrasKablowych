#include "data/catalogrepository.h"
#include "licensing/accesscontroller.h"
#include "ui/mainwindow.h"

#include "config.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("KubwojPrime"));
    QApplication::setOrganizationDomain(QStringLiteral("local"));
    QApplication::setApplicationName(QStringLiteral("KalkulatorTrasKablowych"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kalkulator Tras Kablowych"));
    QApplication::setApplicationVersion(QString::fromUtf8(KTK_APP_VERSION));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    application.setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f8fafc; }"
        "QGroupBox { font-weight: 600; border: 1px solid #cbd5e1; border-radius: 7px;"
        " margin-top: 12px; padding: 12px 8px 8px 8px; background: white; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }"
        "QPushButton { padding: 7px 13px; border-radius: 5px; border: 1px solid #94a3b8;"
        " background: white; }"
        "QPushButton:hover { background: #eff6ff; border-color: #2563eb; }"
        "QHeaderView::section { background: #e2e8f0; padding: 6px; border: 0;"
        " border-right: 1px solid #cbd5e1; font-weight: 600; }"
        "QTableView { background: white; gridline-color: #e2e8f0; }"));

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
