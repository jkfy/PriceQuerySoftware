#pragma once

#include <QString>
#include <QColor>
#include <QWidget>
#include <QStyle>

namespace theme {

// 颜色令牌（与 界面设计.md 的 8.1 节一致）
inline const QColor Primary()    { return QColor("#0a6ed1"); }
inline const QColor Bg()         { return QColor("#f3f3f3"); }
inline const QColor Sidebar()    { return QColor("#f7f7f7"); }
inline const QColor Border()     { return QColor("#e0e0e0"); }
inline const QColor Muted()      { return QColor("#8a8a8a"); }
inline const QColor Warning()    { return QColor("#b25d00"); }
inline const QColor Danger()     { return QColor("#c62828"); }

// 样式钩子约定：
//   .ui 里给控件加一个动态属性 kind（<property name="kind" stdset="0">），
//   这里用属性选择器上色。这样 Designer 里改布局不会丢样式，
//   也不会像 objectName 那样占住控件名。
inline QString styleSheet()
{
    return QStringLiteral(R"(
QWidget {
    font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI";
    font-size: 12px;
    color: #1f1f1f;
}
QMainWindow, QDialog { background: #f3f3f3; }

/* ── 侧边导航 ─────────────────────────────── */
QWidget[kind="sidebar"] { background: #f7f7f7; border-right: 1px solid #e0e0e0; }
QLabel[kind="appTitle"] { color: #111; font-size: 13px; font-weight: 600; padding: 14px 14px 10px 14px; }
QLabel[kind="sideSection"] { color: #9a9a9a; font-size: 11px; padding: 14px 14px 4px 14px; }
QLabel[kind="recentItem"] { color: #777; font-size: 11px; padding: 4px 14px; }
QListWidget[kind="nav"] { background: transparent; border: none; outline: none; }
QListWidget[kind="nav"]::item {
    height: 34px; padding-left: 12px; border: none;
    border-left: 3px solid transparent; color: #333;
}
QListWidget[kind="nav"]::item:hover { background: #eef5fd; }
QListWidget[kind="nav"]::item:selected {
    background: #e8f1fb; color: #0a6ed1; border-left: 3px solid #0a6ed1;
    font-weight: 600;
}

/* ── 按钮 ─────────────────────────────────── */
QPushButton {
    background: #fdfdfd; border: 1px solid #c8c8c8; border-radius: 3px;
    padding: 5px 12px; color: #222;
}
QPushButton:hover  { background: #f2f7fd; border-color: #a9c9ea; }
QPushButton:pressed { background: #e3eefb; }
QPushButton:disabled { color: #b0b0b0; background: #f5f5f5; border-color: #e0e0e0; }
QPushButton[kind="primary"] {
    background: #0a6ed1; border-color: #0a6ed1; color: #ffffff; font-weight: 600;
}
QPushButton[kind="primary"]:hover   { background: #1a7bdd; }
QPushButton[kind="primary"]:pressed { background: #085aa8; }
QPushButton[kind="chip"] {
    border-radius: 11px; padding: 3px 10px; font-size: 11px;
    background: #ffffff; border: 1px solid #d8d8d8; color: #444;
}
QPushButton[kind="chip"]:checked { background: #fff3e0; border-color: #ffb74d; color: #b25d00; }
QPushButton[kind="link"] { border: none; background: transparent; color: #0a6ed1; padding: 2px 4px; }
QPushButton[kind="link"]:hover { text-decoration: underline; background: transparent; }
QPushButton[kind="danger"] { color: #c62828; }
QPushButton[kind="small"] { padding: 3px 9px; font-size: 11px; }

/* ── 输入控件 ─────────────────────────────── */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: #ffffff; border: 1px solid #c8c8c8; border-radius: 3px;
    padding: 4px 6px; min-height: 18px;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: #0a6ed1; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    background: #ffffff; border: 1px solid #c8c8c8;
    selection-background-color: #e8f1fb; selection-color: #111;
}

/* ── 容器条 ───────────────────────────────── */
QWidget[kind="toolbar"] { background: #fafafa; border-bottom: 1px solid #dcdcdc; }
QWidget[kind="tabStrip"] { background: #f7f7f7; border-bottom: 1px solid #dcdcdc; }
QWidget[kind="filterBar"] { background: #ffffff; border-bottom: 1px solid #ececec; }
QWidget[kind="leftPane"] { background: #fafafa; }
QWidget[kind="detailPane"] { background: #ffffff; }

/* ── Tab ──────────────────────────────────── */
QTabBar { background: transparent; }
QTabBar::tab {
    background: transparent; padding: 7px 16px; margin-right: 2px;
    border: 1px solid transparent; border-bottom: none;
    border-top-left-radius: 3px; border-top-right-radius: 3px;
    color: #555;
}
QTabBar::tab:hover { color: #0a6ed1; }
QTabBar::tab:selected {
    background: #ffffff; border-color: #dcdcdc; color: #111; font-weight: 600;
}

/* ── 表格 / 树 ────────────────────────────── */
QTreeView, QTableView {
    background: #ffffff; border: 1px solid #e0e0e0;
    gridline-color: #f0f0f0;
    selection-background-color: #e8f1fb; selection-color: #111;
    alternate-background-color: #fbfbfb;
    outline: none;
}
QTreeView::item, QTableView::item { height: 26px; padding-left: 4px; }
QTreeView::item:hover, QTableView::item:hover { background: #f2f8ff; }
QHeaderView::section {
    background: #f6f6f6; border: none;
    border-right: 1px solid #ececec; border-bottom: 1px solid #dcdcdc;
    padding: 5px 8px; color: #444; font-weight: 600; font-size: 11px;
}
QTableCornerButton::section { background: #f6f6f6; border: none; }

/* ── 面板 / 标签 ──────────────────────────── */
QFrame[kind="summary"] { background: #fafafa; border-left: 1px solid #e0e0e0; }
QFrame[kind="card"] { background: #fcfcfc; border: 1px solid #e6e6e6; border-radius: 4px; }
QFrame[kind="hline"] { background: #ececec; border: none; }
QFrame[kind="totalBar"] {
    background: #f7f9fb; border: 1px solid #e3e9ef; border-radius: 3px;
}
QLabel[kind="total"] { font-size: 26px; font-weight: 600; color: #111; }
QLabel[kind="h1"] { font-size: 15px; font-weight: 600; color: #111; }
QLabel[kind="h2"] { font-size: 12px; font-weight: 600; color: #111; }
QLabel[kind="muted"] { color: #8a8a8a; font-size: 11px; }
QLabel[kind="sumLabel"] { color: #888; font-size: 11px; }
QLabel[kind="totalValue"] { font-size: 17px; font-weight: 600; }
QLabel[kind="tag"] {
    background: #fff3e0; color: #b25d00; border-radius: 3px;
    padding: 1px 6px; font-size: 11px;
}
QLabel[kind="warnNote"] {
    color: #8a5a00; background: #fff8e6; border: 1px solid #ffe0a3;
    border-radius: 3px; padding: 6px 8px; font-size: 11px;
}

/* ── 状态栏 ───────────────────────────────── */
QStatusBar { background: #f3f3f3; border-top: 1px solid #dcdcdc; color: #666; font-size: 11px; }
QStatusBar::item { border: none; }

/* ── 分隔器 / 滚动条 ──────────────────────── */
QSplitter::handle { background: #ececec; }
QSplitter::handle:horizontal { width: 1px; }
QScrollBar:vertical { background: #f5f5f5; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #cfcfcf; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #b5b5b5; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QScrollBar:horizontal { background: #f5f5f5; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #cfcfcf; border-radius: 5px; min-width: 24px; }

)");
}

// 动态属性（kind）选择器在控件首次 polish 时不会生效：
// Qt 只在那之后重新解析规则。所以 setupUi 之后统一重刷一次。
// 每个顶层页面构造完成后调用一次即可。
inline void repolish(QWidget *root)
{
    if (!root)
        return;
    root->style()->unpolish(root);
    root->style()->polish(root);
    const auto children = root->findChildren<QWidget *>();
    for (QWidget *w : children) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

} // namespace theme
