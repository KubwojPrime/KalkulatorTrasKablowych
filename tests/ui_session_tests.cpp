#include "ui/mainwindow.h"
#include "ui/cabletablemodel.h"
#include "io/projectrecovery.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QLineEdit>
#include <QTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QStandardPaths>
#include <iostream>
#include <cstdlib>

static void check(bool value, const char *label) {
    if (!value) { std::cerr << label << '\n'; std::exit(1); }
}
namespace ktk {
struct MainWindowTest {
    static void run(const QString &dir) {
        MainWindow w(nullptr, dir);
        QApplication::processEvents();
        w.show();
        check(!w.m_dirty, "initial project clean");
        w.m_projectName->setText(QStringLiteral("Projekt odzyskiwany"));
        w.addCustomCable();
        QEventLoop loop;
        QTimer::singleShot(5500, &loop, &QEventLoop::quit);
        loop.exec();
        check(QFileInfo::exists(w.recoveryPath()), "timer created autosave");
        check(w.m_dirty, "autosave does not mark exported");
        ProjectData restored; QString original, error;
        check(ProjectRecovery::load(w.recoveryPath(), &restored, &original, &error), "read autosave");
        check(restored.cables.size() == 1, "autosave cable count");

        QTimer answer;
        answer.setInterval(10);
        auto connection = QObject::connect(&answer, &QTimer::timeout, [] {
            if (auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Cancel)->click();
        });
        answer.start(); w.newProject(); answer.stop();
        check(w.m_dirty && w.currentProject().cables.size()==1, "cancel new preserves data");
        answer.start(); w.close(); answer.stop();
        check(w.isVisible(), "cancel close preserves window");
        QObject::disconnect(connection);

        const QString output = dir + QStringLiteral("/projekt.xlsx");
        connection = QObject::connect(&answer, &QTimer::timeout, [&] {
            if (auto *dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                dialog->selectFile(output);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            }
        });
        answer.start(); w.exportXlsx(); answer.stop();
        check(QFileInfo::exists(output) && !w.m_dirty, "export saves and clears dirty");
        check(!QFileInfo::exists(w.recoveryPath()), "export clears recovery");
        check(QFileInfo(w.exportLocation("next.dxf")).absolutePath()==dir, "remember export directory");
        QObject::disconnect(connection);

        w.m_projectName->setText(QStringLiteral("Po awarii"));
        w.autosave();
        // Simulate a crash by destroying the window without a close event.
        {
            MainWindow recovered(nullptr, dir);
            connection = QObject::connect(&answer, &QTimer::timeout, [] {
                if (auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click();
            });
            answer.start(); QApplication::processEvents(); answer.stop();
            check(recovered.m_dirty && recovered.currentProject().route.projectName==QStringLiteral("Po awarii"), "startup recovery");
            check(recovered.m_currentFile == output, "startup original path");
            check(QFileInfo(recovered.exportLocation("next.xlsx")).absolutePath()==dir, "folder across window instances");
            QObject::disconnect(connection);
        }
        connection = QObject::connect(&answer, &QTimer::timeout, [] {
            if (auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->button(QMessageBox::Discard)->click();
        });
        answer.start(); w.newProject(); answer.stop();
        check(!w.m_dirty && w.currentProject().cables.isEmpty(), "discard creates clean project");
        check(!QFileInfo::exists(w.recoveryPath()), "discard removes old recovery");
        w.close();
    }
};
}
int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QTemporaryDir dir;
    QCoreApplication::setOrganizationName("KTK-session-test");
    QCoreApplication::setApplicationName("session-test");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QStandardPaths::setTestModeEnabled(true);
    ktk::MainWindowTest::run(dir.path());
    std::cout << "GUI autosave, recovery, export directory and unsaved-work tests passed.\n";
}
