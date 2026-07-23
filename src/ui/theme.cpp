#include "ui/theme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace ktk {

void Theme::applyDark(QApplication &application)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#0b1220")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e5e7eb")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#0f172a")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#162033")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#111827")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#f8fafc")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#e5e7eb")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#1e293b")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f1f5f9")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Link, QColor(QStringLiteral("#60a5fa")));
    palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#c084fc")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#2563eb")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#7c8aa1")));
    palette.setColor(QPalette::Light, QColor(QStringLiteral("#475569")));
    palette.setColor(QPalette::Midlight, QColor(QStringLiteral("#334155")));
    palette.setColor(QPalette::Mid, QColor(QStringLiteral("#263449")));
    palette.setColor(QPalette::Dark, QColor(QStringLiteral("#020617")));
    palette.setColor(QPalette::Shadow, QColor(QStringLiteral("#000000")));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#64748b")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#64748b")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#64748b")));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(QStringLiteral("#111827")));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#172033")));

    application.setPalette(palette);
    application.setStyleSheet(QStringLiteral(R"(
        QMainWindow,
        QDialog {
            background-color: #0b1220;
            color: #e5e7eb;
        }

        QWidget {
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }

        QLabel {
            color: #e5e7eb;
            background: transparent;
        }

        QGroupBox {
            color: #f1f5f9;
            font-weight: 600;
            background-color: #111827;
            border: 1px solid #334155;
            border-radius: 8px;
            margin-top: 13px;
            padding: 13px 9px 9px 9px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #f8fafc;
            background-color: #111827;
        }

        QLineEdit,
        QSpinBox,
        QDoubleSpinBox,
        QComboBox,
        QDateEdit,
        QTimeEdit,
        QDateTimeEdit,
        QPlainTextEdit,
        QTextEdit {
            color: #f1f5f9;
            background-color: #0b1220;
            border: 1px solid #3b4a61;
            border-radius: 5px;
            padding: 6px 8px;
        }

        QLineEdit:focus,
        QSpinBox:focus,
        QDoubleSpinBox:focus,
        QComboBox:focus,
        QPlainTextEdit:focus,
        QTextEdit:focus {
            border: 1px solid #60a5fa;
            background-color: #0d182b;
        }

        QLineEdit:disabled,
        QSpinBox:disabled,
        QDoubleSpinBox:disabled,
        QComboBox:disabled {
            color: #64748b;
            background-color: #111827;
            border-color: #263449;
        }

        QSpinBox::up-button,
        QDoubleSpinBox::up-button,
        QSpinBox::down-button,
        QDoubleSpinBox::down-button {
            width: 18px;
            background-color: #1e293b;
            border-left: 1px solid #334155;
        }

        QComboBox::drop-down {
            width: 24px;
            border-left: 1px solid #334155;
            background-color: #1e293b;
        }

        QComboBox QAbstractItemView {
            color: #f1f5f9;
            background-color: #111827;
            border: 1px solid #475569;
            outline: none;
        }

        QPushButton {
            color: #f8fafc;
            background-color: #1e293b;
            border: 1px solid #475569;
            border-radius: 6px;
            padding: 7px 14px;
            min-height: 18px;
        }

        QPushButton:hover {
            background-color: #29364b;
            border-color: #60a5fa;
        }

        QPushButton:pressed {
            background-color: #1d4ed8;
            border-color: #93c5fd;
        }

        QPushButton:default {
            background-color: #1d4ed8;
            border-color: #60a5fa;
        }

        QPushButton:disabled {
            color: #64748b;
            background-color: #172033;
            border-color: #263449;
        }

        QTabWidget::pane {
            background-color: #0f172a;
            border: 1px solid #334155;
            border-radius: 7px;
            top: -1px;
        }

        QTabBar::tab {
            color: #aebbd0;
            background-color: #111827;
            border: 1px solid #2b3a50;
            border-bottom: none;
            padding: 9px 18px;
            margin-right: 2px;
            min-width: 130px;
        }

        QTabBar::tab:hover {
            color: #f8fafc;
            background-color: #1a2740;
        }

        QTabBar::tab:selected {
            color: #ffffff;
            background-color: #1d4ed8;
            border-color: #3b82f6;
        }

        QTableView,
        QTableWidget,
        QListView,
        QTreeView {
            color: #e5e7eb;
            background-color: #0f172a;
            alternate-background-color: #162033;
            gridline-color: #2b3a50;
            border: 1px solid #334155;
            border-radius: 5px;
            outline: none;
        }

        QTableView::item,
        QTableWidget::item,
        QListView::item,
        QTreeView::item {
            padding: 4px;
        }

        QTableView::item:selected,
        QTableWidget::item:selected,
        QListView::item:selected,
        QTreeView::item:selected {
            color: #ffffff;
            background-color: #1d4ed8;
        }

        QTableView::item:hover,
        QTableWidget::item:hover {
            background-color: #22314a;
        }

        QHeaderView::section {
            color: #f1f5f9;
            background-color: #1e293b;
            padding: 7px;
            border: none;
            border-right: 1px solid #334155;
            border-bottom: 1px solid #475569;
            font-weight: 600;
        }

        QTableCornerButton::section {
            background-color: #1e293b;
            border: none;
            border-right: 1px solid #334155;
            border-bottom: 1px solid #475569;
        }

        QMenuBar {
            color: #e5e7eb;
            background-color: #111827;
            border-bottom: 1px solid #263449;
        }

        QMenuBar::item {
            padding: 6px 10px;
            background: transparent;
        }

        QMenuBar::item:selected {
            background-color: #1e293b;
        }

        QMenu {
            color: #e5e7eb;
            background-color: #111827;
            border: 1px solid #475569;
            padding: 5px;
        }

        QMenu::item {
            padding: 7px 26px 7px 10px;
            border-radius: 4px;
        }

        QMenu::item:selected {
            color: #ffffff;
            background-color: #1d4ed8;
        }

        QMenu::separator {
            height: 1px;
            background-color: #334155;
            margin: 5px 8px;
        }

        QStatusBar {
            color: #aebbd0;
            background-color: #111827;
            border-top: 1px solid #263449;
        }

        QStatusBar::item {
            border: none;
        }

        QScrollBar:vertical {
            background-color: #0b1220;
            width: 13px;
            margin: 0;
        }

        QScrollBar::handle:vertical {
            background-color: #475569;
            border-radius: 6px;
            min-height: 28px;
            margin: 2px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #64748b;
        }

        QScrollBar:horizontal {
            background-color: #0b1220;
            height: 13px;
            margin: 0;
        }

        QScrollBar::handle:horizontal {
            background-color: #475569;
            border-radius: 6px;
            min-width: 28px;
            margin: 2px;
        }

        QScrollBar::add-line,
        QScrollBar::sub-line {
            width: 0;
            height: 0;
        }

        QScrollBar::add-page,
        QScrollBar::sub-page {
            background: none;
        }

        QSplitter::handle {
            background-color: #263449;
        }

        QToolTip {
            color: #f8fafc;
            background-color: #111827;
            border: 1px solid #64748b;
            padding: 5px;
        }

        QMessageBox {
            background-color: #0b1220;
        }

        QMessageBox QLabel {
            color: #e5e7eb;
        }
    )"));
}

} // namespace ktk
