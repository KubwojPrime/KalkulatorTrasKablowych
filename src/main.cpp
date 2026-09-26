#include "data/catalogrepository.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

#include "config.h"

#include <QApplication>
#include <QDebug>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

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

    if (application.arguments().contains(QStringLiteral("--smoke-test"))
        || qEnvironmentVariableIntValue("KTK_SMOKE_TEST") == 1) {
        QTemporaryDir smokeDirectory;
        if (!smokeDirectory.isValid()) {
            qCritical() << "cannot create smoke-test directory";
            return 30;
        }
        ktk::CatalogRepository repository(
            smokeDirectory.filePath(QStringLiteral("catalog.sqlite")));
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

    const auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    QLockFile sessionLock(QDir(dataDir).filePath(QStringLiteral("session.lock")));
    sessionLock.setStaleLockTime(0);
    if (!sessionLock.tryLock()) {
        QMessageBox::information(nullptr, QObject::tr("Kalkulator Tras Kablowych"),
            QObject::tr("Program jest już uruchomiony lub katalog sesji jest niedostępny. Sprawdź otwarte okno programu."));
        return 1;
    }
    ktk::MainWindow window;
    window.show();
    return application.exec();
}
