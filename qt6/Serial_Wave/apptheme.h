#ifndef APPTHEME_H
#define APPTHEME_H

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QString>

// 全局深色主题：Fusion 风格 + 深色调色板 + 常用控件的少量样式。
// 想调整整体配色，改这里的颜色即可。
inline void applyAppTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const QColor window(30, 30, 30);      // 窗口背景
    const QColor base(24, 24, 24);        // 输入框/列表背景
    const QColor panel(37, 37, 37);       // 按钮/面板
    const QColor text(220, 220, 220);
    const QColor disabled(120, 120, 120);
    const QColor accent(13, 110, 253);

    QPalette p;
    p.setColor(QPalette::Window,          window);
    p.setColor(QPalette::WindowText,      text);
    p.setColor(QPalette::Base,            base);
    p.setColor(QPalette::AlternateBase,   QColor(38, 38, 38));
    p.setColor(QPalette::ToolTipBase,     panel);
    p.setColor(QPalette::ToolTipText,     text);
    p.setColor(QPalette::Text,            text);
    p.setColor(QPalette::Button,          panel);
    p.setColor(QPalette::ButtonText,      text);
    p.setColor(QPalette::BrightText,      QColor(239, 83, 80));
    p.setColor(QPalette::Link,            QColor(64, 156, 255));
    p.setColor(QPalette::Highlight,       accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(130, 130, 130));

    p.setColor(QPalette::Disabled, QPalette::Text,       disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    app.setPalette(p);

    app.setStyleSheet(QStringLiteral(R"(
QPushButton {
    background: #2d2d2d;
    border: 1px solid #3f3f3f;
    border-radius: 4px;
    padding: 4px 12px;
}
QPushButton:hover   { background: #383838; }
QPushButton:pressed { background: #242424; }
QPushButton:disabled { color: #777777; }
QPushButton:checked {
    background: #0d6efd;
    border-color: #0d6efd;
    color: white;
}

QPushButton#primaryButton {
    background: #0d6efd;
    border-color: #0d6efd;
    color: white;
}
QPushButton#primaryButton:hover   { background: #2b7cff; }
QPushButton#primaryButton:pressed { background: #0a58ca; }
QPushButton#primaryButton:disabled { background: #2a3a4d; border-color: #2a3a4d; }

QComboBox, QLineEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
    background: #202020;
    border: 1px solid #3f3f3f;
    border-radius: 4px;
    padding: 3px 6px;
    selection-background-color: #0d6efd;
}
QComboBox:disabled, QLineEdit:disabled, QPlainTextEdit:disabled {
    color: #777777;
}
QComboBox QAbstractItemView {
    background: #202020;
    border: 1px solid #3f3f3f;
    selection-background-color: #0d6efd;
}
QCheckBox { spacing: 6px; }

QLabel#panelTitle {
    color: #e0e0e0;
    font-weight: 600;
    font-size: 16px;
}

QLabel#sectionTitle {
    color: #b0b0b0;
    font-weight: 600;
    font-size: 13px;
}

QLabel#hintText {
    color: #777777;
    font-size: 11px;
}

QLabel#formLabel {
    color: #9e9e9e;
    font-size: 12px;
    font-weight: 600;
}

QLabel#deviceDesc {
    color: #8a8a8a;
    font-size: 11px;
}

QLabel#deviceId {
    color: #b0b0b0;
    font-size: 12px;
    font-weight: 600;
}

QToolTip {
    color: #e0e0e0;
    background: #2d2d2d;
    border: 1px solid #444444;
}

QScrollBar:vertical {
    background: #1e1e1e;
    width: 12px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #454545;
    border-radius: 6px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #555555; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

QScrollBar:horizontal {
    background: #1e1e1e;
    height: 12px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #454545;
    border-radius: 6px;
    min-width: 24px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QMenu { background: #262626; border: 1px solid #3f3f3f; }
QMenu::item:selected { background: #0d6efd; }
)"));
}

#endif // APPTHEME_H
