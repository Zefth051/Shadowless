#include "theme.h"
#include <QFile>
#include <QHash>
#include <QWidget>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <cmath>

namespace Shadowless {
namespace {
struct Accent {
    QString name, color, hover, dim, text;
};
const QList<Accent> accents{
    {"violet", "#b69af7", "#c8b2fa", "#785ac6", "#160f2b"},
    {"mint", "#63e0bd", "#7de8cb", "#2ea184", "#0b201a"},
    {"blue", "#64b5f6", "#82c4fa", "#357ebd", "#081a28"},
    {"rose", "#ff6666", "#ff8585", "#c54242", "#270b0b"},
    {"amber", "#f5b94c", "#f8c96e", "#b97d1e", "#251702"},
};

QString tint(const QColor &base, const QColor &color, int percent);
QString contrastText(const QColor &color) {
    const auto linear = [](double channel) { return channel <= .04045 ? channel / 12.92 : std::pow((channel + .055) / 1.055, 2.4); };
    const auto luminance = .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
    return luminance > .179 ? "#000000" : "#ffffff";
}
Accent paletteFor(const QString &name) {
    if (QRegularExpression("^#[0-9a-fA-F]{6}$").match(name).hasMatch()) {
        const QColor color(name);
        return {"violet", color.name(), tint(color, Qt::white, 18), tint(color, QColor("#141719"), 35),
                contrastText(color)};
    }
    for (const auto &accent : accents)
        if (accent.name == name) return accent;
    return accents.first();
}

QString tint(const QColor &base, const QColor &color, int percent) {
    const auto blend = [percent](int a, int b) { return (a * (100 - percent) + b * percent) / 100; };
    return QColor(blend(base.red(), color.red()), blend(base.green(), color.green()),
                  blend(base.blue(), color.blue())).name();
}
}

QStringList accentNames() {
    QStringList names;
    for (const auto &accent : accents) names.append(accent.name);
    return names;
}

QColor accentColor(const QString &name) { return QColor(paletteFor(name).color); }
QIcon accentIcon(const QString &name) { return QIcon(":/shadowless/" + paletteFor(name).name + ".ico"); }

void applyTheme(QWidget *widget, const QString &name) {
    const auto &accent = paletteFor(name);
    const auto color = QColor(accent.color);
    const auto readableAccent = accent.text == "#ffffff" ? tint(color, Qt::white, 60) : accent.color;
    auto palette = widget->palette();
    palette.setColor(QPalette::Accent, color);
    palette.setColor(QPalette::Link, QColor(readableAccent));
    palette.setColor(QPalette::Highlight, color);
    palette.setColor(QPalette::HighlightedText, QColor(accent.text));
    palette.setColor(QPalette::Window, QColor("#111416"));
    palette.setColor(QPalette::Base, QColor("#111619"));
    palette.setColor(QPalette::AlternateBase, QColor("#181d20"));
    palette.setColor(QPalette::Button, QColor("#20262a"));
    palette.setColor(QPalette::ToolTipBase, QColor("#252d32"));
    palette.setColor(QPalette::ToolTipText, QColor("#e7ecea"));
    palette.setColor(QPalette::WindowText, QColor("#e7ecea"));
    palette.setColor(QPalette::Text, QColor("#e7ecea"));
    palette.setColor(QPalette::ButtonText, QColor("#e7ecea"));
    widget->setPalette(palette);
    QFile file(":/shadowless/theme.xaml"); QHash<QString, QString> colors;
    if (file.open(QIODevice::ReadOnly)) {
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == "Color") {
                const auto key = xml.attributes().value("http://schemas.microsoft.com/winfx/2006/xaml", "Key").toString();
                colors[key] = QColor(xml.readElementText()).name();
            }
        }
    }
    widget->setStyleSheet(QString(R"(
        QWidget { color: %1; font-family: sans-serif; font-size: 13px; }
        QDialog, QMenu { background: #111416; }
        QAbstractItemView { background: #111619; alternate-background-color: %3; color: %1; selection-background-color: %10; selection-color: %1; }
        QMenu::item:selected { background: %10; }
        QLabel { background: transparent; }
        QLabel[muted="true"] { color: %2; font-size: 12px; }
        QLabel#heading { font-size: 22px; font-weight: 600; padding: 2px 0 6px 0; }
        QLabel#brand { font-size: 18px; font-weight: 600; }
        QLabel#statusTitle { font-size: 14px; font-weight: 600; }
        QLabel#sectionTitle, QLabel#clipAudioHeading { color: %2; font-size: 11px; font-weight: 600; padding: 3px 0; }
        QFrame#shadowlessPanel { background: #111416; border: 1px solid #30363b; border-radius: 16px; }
        QFrame#shadowlessPanel[floating="true"] { background: transparent; border: 0; }
        QFrame#shadowlessPanel[floating="true"] QFrame#panel { background: rgba(26, 30, 34, 214); border: 1px solid rgba(255, 255, 255, 22); border-radius: 14px; }
        QFrame#shadowlessPanel[floating="true"] QPushButton#captureMode { background: rgba(26, 30, 34, 214); border-color: rgba(255, 255, 255, 22); }
        QFrame#shadowlessPanel[floating="true"] QPushButton#captureMode:checked { background: %9; border-color: %11; }
        QWidget#dashboardChrome QPushButton { background: rgba(22, 26, 30, 225); border: 1px solid rgba(255, 255, 255, 26); border-radius: 10px; padding: 7px 14px; }
        QWidget#dashboardChrome QPushButton:hover, QWidget#dashboardChrome QPushButton:focus { background: rgba(38, 44, 50, 235); border-color: %11; }
        QWidget#dashboardChrome QPushButton:pressed { background: rgba(14, 17, 20, 240); }
        QFrame#segments { background: #101416; border: 1px solid %4; border-radius: 8px; }
        QFrame#divider { background: #22282c; }
        QLabel#notice { color: %1; background: #1d2226; padding: 5px 8px; border-radius: 6px; }
        QLabel#emptyClips { color: %2; background: %3; padding: 14px 12px; border-radius: 8px; }
        QLabel#overlayMark { background: %9; color: %11; border-radius: 10px; font-size: 26px; }
        QLabel#thumbnail { background: #101416; color: %11; border-radius: 5px; }
        QLabel#clipName { font-weight: 600; }
        QProgressBar { background: #242a2e; border: 0; border-radius: 2px; }
        QProgressBar::chunk { background: %7; border-radius: 2px; }
        QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: %4; }
        QSlider::sub-page:horizontal { border-radius: 2px; background: %7; }
        QSlider::handle:horizontal { width: 12px; margin: -4px 0; border-radius: 6px; background: %5; }
        QSlider::handle:horizontal:disabled, QSlider::sub-page:horizontal:disabled { background: #434b50; }
        QSlider::handle:horizontal:focus { border: 1px solid white; }
        QFrame#audioCard { background: %3; border: 1px solid %4; border-radius: 10px; }
        QFrame#audioGroup { background: transparent; border: none; }
        QPushButton[audioRow="true"] { text-align: left; padding: 0 14px; min-height: 40px; border-radius: 9px; border-top-right-radius: 0; border-bottom-right-radius: 0; background: transparent; border: 1px solid transparent; color: #8b979d; font-weight: 600; }
        QPushButton[audioRow="true"]:checked { background: transparent; border: 1px solid transparent; color: %11; }
        QPushButton[audioRow="true"]:hover { background: rgba(255, 255, 255, 12); border: 1px solid transparent; }
        QPushButton[audioRow="true"]:focus { border: 1px solid %11; }
        QPushButton[audioRow="true"]:disabled { color: #5d686e; background: transparent; border: 1px solid transparent; }
        QPushButton[audioValue="true"] { min-height: 40px; min-width: 62px; padding: 0 12px; border-radius: 0; border-top-right-radius: 9px; border-bottom-right-radius: 9px; background: transparent; border: 1px solid transparent; border-left: 1px solid %4; color: #9aa5aa; font-family: monospace; font-size: 11px; }
        QPushButton[audioValue="true"]:hover { background: rgba(255, 255, 255, 12); color: %1; }
        QPushButton[audioValue="true"]:focus { border: 1px solid %11; }
        QPushButton[audioValue="true"]:disabled { color: #5d686e; background: transparent; }
        QFrame#volumePopup { background: %3; border: 1px solid %4; border-radius: 10px; }
        QSlider[audioRow="true"]::groove:horizontal { height: 3px; border-radius: 2px; background: rgba(255, 255, 255, 26); }
        QSlider[audioRow="true"]::sub-page:horizontal { height: 3px; border-radius: 2px; background: %5; }
        QSlider[audioRow="true"]::handle:horizontal { width: 12px; height: 12px; margin: -5px 0; border-radius: 6px; background: #f2f5f6; border: none; }
        QSlider[audioRow="true"]::handle:horizontal:hover { background: white; width: 14px; height: 14px; margin: -6px 0; border-radius: 7px; }
        QSlider[audioRow="true"]::sub-page:horizontal:disabled { background: #434b50; }
        QSlider[audioRow="true"]::handle:horizontal:disabled { background: #5d686e; }
        QSlider#clipSeek::sub-page:horizontal { background: %5; }
        QSlider#clipSeek::groove:horizontal { background: #2a3235; height: 5px; }
        QSlider#clipSeek::handle:horizontal { border: 1px solid %7; width: 10px; margin: -4px 0; }
        QSlider#clipSeek::handle:horizontal:hover { border-color: %6; }
        QLabel#clipTime { font-family: monospace; font-size: 11px; color: %2; }
        QPushButton[transport="true"] { padding: 0; min-height: 0; border-radius: 17px; background: rgba(255, 255, 255, 12); border: 1px solid rgba(255, 255, 255, 20); }
        QPushButton[transport="true"]:hover { background: rgba(255, 255, 255, 26); }
        QPushButton#clipPlayPause { background: %5; border-color: transparent; }
        QPushButton#clipPlayPause:hover { background: %6; }
        QPushButton#clipTrim, QPushButton#clipExport { background: %5; color: %8; border-color: %11; font-weight: bold; }
        QPushButton#clipTrim:hover, QPushButton#clipExport:hover { background: %6; color: %13; }
        QWidget#clipSidebar { background: rgba(255, 255, 255, 8); border: 1px solid rgba(255, 255, 255, 18); border-radius: 14px; }
        QListWidget#clipRecent { background: transparent; border: 0; padding: 0; outline: none; }
        QListWidget#clipRecent::item { border: 1px solid transparent; border-radius: 7px; padding: 5px; margin: 3px 0; font-size: 11px; }
        QListWidget#clipRecent::item:selected { background: %10; border-color: %7; color: %1; }
        QListWidget#clipRecent::item:hover { background: %9; }
        QPushButton#clipMute { padding: 0; min-height: 0; border-radius: 15px; }
        QPushButton#clipMute:checked { background: %10; border-color: %7; }
        QWidget#clipAudioRow { background: transparent; border: 0; }
        QCheckBox[clipAudioTrack="true"] { background: transparent; color: %2; border: 1px solid transparent; border-radius: 6px; padding: 0 8px; spacing: 8px; }
        QCheckBox[clipAudioTrack="true"]:checked { background: %9; color: %1; }
        QCheckBox[clipAudioTrack="true"]:hover, QCheckBox[clipAudioTrack="true"]:focus { border-color: %11; }
        QCheckBox[clipAudioTrack="true"]::indicator { width: 14px; height: 14px; border: 1px solid %2; border-radius: 7px; background: transparent; }
        QCheckBox[clipAudioTrack="true"]::indicator:checked { background: %5; border-color: %11; image: url(:/shadowless/icons/speaker.svg); }
        QDoubleSpinBox#clipIn, QDoubleSpinBox#clipOut { font-family: monospace; background: transparent; border: 1px solid transparent; padding: 3px 6px; border-radius: 5px; }
        QDoubleSpinBox#clipIn:hover, QDoubleSpinBox#clipOut:hover { background: %3; }
        QDoubleSpinBox#clipIn:focus, QDoubleSpinBox#clipOut:focus { border-color: %11; }
        QComboBox#clipSpeed { font-size: 12px; padding: 4px 2px 4px 8px; }
        QFrame#panel { background: #181c1f; border: 1px solid #262d32; border-radius: 12px; }
        QFrame#settingsSection { background: #171b1e; border: 1px solid #292f34; border-radius: 12px; }
        QWidget#settingsPage QPushButton#sectionLink { background: transparent; border-color: transparent; padding: 5px 8px; color: %2; }
        QWidget#settingsPage QPushButton#sectionLink:hover, QWidget#settingsPage QPushButton#sectionLink:focus { background: %9; border-color: %7; color: %11; }
        QWidget#settingsPage QPushButton#sectionLink[active="true"] { background: %9; border-color: %7; color: %11; font-weight: 600; }
        QWidget#settingsPage QKeySequenceEdit { padding: 3px 6px; }
        QWidget#clip { background: transparent; border: 1px solid transparent; border-radius: 8px; }
        QWidget#clip:hover { background: #20262a; }
        QPushButton { background: #20262a; border: 1px solid #30383d; border-radius: 8px; padding: 6px 12px; min-height: 20px; }
        QPushButton:hover { border-color: %7; background: #2a3237; }
        QPushButton:pressed { background: #101416; border-color: %7; }
        QPushButton#openClipFolder, QPushButton#deleteClip { padding: 0; min-height: 30px; }
        QPushButton#deleteClip:hover { color: #ff9999; background: #40272b; border-color: #864750; }
        QPushButton:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus { border: 1px solid %11; }
        QPushButton:checked { background: %9; border-color: %11; color: %11; }
        QPushButton:disabled { color: %2; background: #151a1c; }
        QPushButton:checked:disabled { color: %2; background: %9; border-color: %10; }
        QPushButton#captureMode { background: %3; border: 1px solid %4; border-radius: 12px; padding: 0; min-height: 136px; }
        QPushButton#captureMode:checked { background: %9; border-color: %11; }
        QPushButton#captureMode:hover, QPushButton#captureMode:focus { border-color: %11; }
        QPushButton#captureMode:checked:disabled { border-color: %7; }
        QPushButton#segment { border: 1px solid transparent; background: transparent; color: %2; font-weight: bold; }
        QPushButton#segment:checked { color: %11; background: %9; }
        QPushButton#segment:checked:disabled { color: %2; background: %9; }
        QPushButton#segment:hover, QPushButton#segment:focus { border-color: %11; }
        QPushButton#hideOverlay, QPushButton#refreshClips { background: transparent; padding: 2px 8px; }
        QPushButton#refreshClips { font-size: 17px; }
        QPushButton#sectionLink { padding: 3px 11px; min-height: 20px; }
        QPushButton#sectionLink:hover { color: %11; }
        QPushButton#settingsLink { background: transparent; text-align: left; color: %2; border-color: transparent; padding: 1px 0; }
        QPushButton#settingsLink:hover { color: %11; }
        QPushButton#settingsLink:focus { border-color: %11; }
        QPushButton#primary, QPushButton#captureAction { background: %5; color: %8; border: 1px solid transparent; font-weight: bold; }
        QPushButton#captureAction { min-height: 27px; }
        QPushButton#primary:hover, QPushButton#captureAction:hover { background: %6; color: %13; }
        QPushButton#primary:pressed, QPushButton#captureAction:pressed { background: %7; color: %14; }
        QPushButton#primary:focus, QPushButton#captureAction:focus { border-color: %8; }
        QPushButton#primary:disabled, QPushButton#captureAction:disabled { background: %10; color: %2; }
        QCheckBox { spacing: 9px; padding: 7px 0; }
        QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid #71818b; border-radius: 5px; background: #111619; }
        QCheckBox::indicator:hover { border-color: %11; }
        QCheckBox::indicator:checked { background: %5; border-color: %11; image: url(%12); }
        QCheckBox::indicator:disabled { border-color: #56636b; background: #20262a; }
        QCheckBox::indicator:checked:disabled { background: %5; border-color: %11; image: url(%12); }
        QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit { background: #101416; border: 1px solid %4; border-radius: 6px; padding: 6px; selection-background-color: %10; selection-color: %1; }
        QComboBox { padding-right: 27px; }
        QComboBox::drop-down { border: 0; width: 25px; subcontrol-origin: padding; }
        QComboBox::down-arrow { image: url(:/shadowless/chevron.svg); width: 12px; height: 8px; }
        QComboBox::down-arrow:on { image: url(:/shadowless/chevron-up.svg); }
        QComboBox:disabled, QSpinBox:disabled, QLineEdit:disabled { color: %2; }
        QSpinBox, QDoubleSpinBox { padding-right: 25px; }
        QSpinBox::up-button, QDoubleSpinBox::up-button,
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border; border: 0; width: 22px; margin: 2px 2px 2px 0;
        }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background: %9; border-radius: 3px; }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/shadowless/chevron-up.svg); width: 9px; height: 6px; }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/shadowless/chevron.svg); width: 9px; height: 6px; }
        QComboBox QAbstractItemView { background: #181d20; selection-background-color: %10; selection-color: %1; }
        QScrollArea { border: 0; background: transparent; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 8px; }
        QScrollBar::handle:vertical { background: #39444a; border-radius: 4px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: %7; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QListWidget { background: #101416; border: 1px solid %4; border-radius: 8px; padding: 6px; }
        QListWidget::item { padding: 10px; }
        QListWidget::item:selected { background: %9; color: %11; }
        QTableWidget { background: #101416; border: 1px solid %4; border-radius: 6px; gridline-color: %4; }
        QHeaderView::section { background: %3; color: %2; border: 0; padding: 5px; }
        QToolTip { background: #252d32; color: %1; border: 1px solid #46525a; padding: 5px 7px; }
        QLabel#settingsError { color: #ff9999; }
        QKeySequenceEdit { background: #111619; border: 1px solid %4; border-radius: 6px; padding: 2px; }
    )").arg(colors.value("ColorTextPrimary", "#e7ecea"), colors.value("ColorTextSecondary", "#8b969d"),
             colors.value("ColorBgSurface", "#181d20"), colors.value("ColorBorder", "#242a2e"), accent.color, accent.hover, accent.dim, accent.text,
             tint(QColor("#141719"), color, 13), tint(QColor("#141719"), color, 24),
             readableAccent,
             accent.text == "#ffffff" ? ":/shadowless/check-light.svg" : ":/shadowless/check.svg",
             contrastText(QColor(accent.hover)), contrastText(QColor(accent.dim))));
}
} // namespace Shadowless
