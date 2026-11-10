#include "overlay_editor.h"
#include "value_controls.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QStackedWidget>
#include <QScrollArea>

namespace Shadowless {
OverlayCanvas::OverlayCanvas(QWidget *parent) : QWidget(parent) {
    setMinimumSize(320, 200); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_clock.start(); m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        bool changed = false; for (auto &entry : m_media) changed |= entry.second.second->advance(m_clock.elapsed());
        if (changed) update();
    });
}
QRectF OverlayCanvas::canvasRect() const {
    QSizeF size(1920, 1080); size.scale(QSizeF(width() - 12, height() - 12), Qt::KeepAspectRatio);
    return QRectF(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
}
void OverlayCanvas::setScene(const OverlayScene &scene, int selected) {
    m_scene = scene; m_selected = selected; m_error.clear();
    if (!isVisible()) return;
    std::map<QString, std::pair<QString, std::shared_ptr<Media>>> assets;
    bool animated = false;
    for (const auto &layer : scene.layers) {
        if (!layer.enabled) continue;
        const auto previous = m_media.find(layer.id);
        if (previous != m_media.end() && previous->second.first == layer.path) assets.emplace(layer.id, previous->second);
        else {
            auto media = std::make_shared<Media>(); QString error;
            if (!media->open(layer.path, QSize(640, 360), &error)) { m_error = error; continue; }
            assets.emplace(layer.id, std::make_pair(layer.path, media));
        }
        animated |= assets.at(layer.id).second->animated();
    }
    m_media = std::move(assets);
    if (animated && isVisible()) m_timer.start(); else m_timer.stop(); update();
}
void OverlayCanvas::paintEvent(QPaintEvent *) {
    QPainter painter(this); const auto canvas = canvasRect();
    painter.fillRect(canvas, QColor("#101416"));
    painter.setPen(QColor("#1d2528"));
    for (int i = 1; i < 4; ++i) {
        const double x = canvas.x() + canvas.width() * i / 4.; painter.drawLine(QPointF(x, canvas.top()), QPointF(x, canvas.bottom()));
        const double y = canvas.y() + canvas.height() * i / 4.; painter.drawLine(QPointF(canvas.left(), y), QPointF(canvas.right(), y));
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (auto it = m_scene.layers.crbegin(); it != m_scene.layers.crend(); ++it) {
        const auto found = m_media.find(it->id); if (found == m_media.end()) continue;
        painter.setOpacity(it->opacity / 100. * (m_scene.enabled ? 1. : .3));
        const auto &image = found->second.second->frame();
        const auto rect = it->fitRect(canvas.size().toSize(), image.size()).translated(canvas.topLeft());
        painter.drawImage(rect, image);
    }
    painter.setOpacity(1.);
    if (m_selected >= 0 && m_selected < m_scene.layers.size()) {
        const auto &layer = m_scene.layers[m_selected];
        painter.setPen(QPen(palette().color(QPalette::Accent), 2, Qt::DashLine));
        painter.drawRect(QRectF(canvas.x() + layer.x * canvas.width(), canvas.y() + layer.y * canvas.height(), layer.width * canvas.width(), layer.height * canvas.height()));
    }
    painter.setPen(QColor("#8b969d")); painter.drawText(canvas.adjusted(12, 12, -12, -12), Qt::AlignBottom | Qt::AlignLeft, "Composition preview · 16:9 canvas");
}
void OverlayCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || m_selected < 0 || m_selected >= m_scene.layers.size()) return;
    const auto rect = canvasRect(); const auto &layer = m_scene.layers[m_selected];
    m_drag = event->position() - QPointF(rect.x() + layer.x * rect.width(), rect.y() + layer.y * rect.height());
}
void OverlayCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton) || m_selected < 0 || m_selected >= m_scene.layers.size()) return;
    const auto rect = canvasRect(); const auto point = event->position() - m_drag - rect.topLeft();
    auto &layer = m_scene.layers[m_selected]; layer.setPosition(point.x() / rect.width(), point.y() / rect.height());
    emit moved(m_selected, layer.x, layer.y); update();
}
void OverlayCanvas::showEvent(QShowEvent *e) { QWidget::showEvent(e); m_clock.restart(); setScene(m_scene, m_selected); }
void OverlayCanvas::hideEvent(QHideEvent *e) { m_timer.stop(); m_media.clear(); QWidget::hideEvent(e); }

OverlayEditor::OverlayEditor(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
    auto *header = new QHBoxLayout; auto *title = new QLabel("Capture overlays"); title->setObjectName("heading"); header->addWidget(title);
    header->addStretch(); m_master = new QCheckBox("Enable composition"); header->addWidget(m_master); layout->addLayout(header);
    auto *description = new QLabel("Layers are recorded into new frames, even when this panel is hidden. Top layer in the list renders above the others."); description->setWordWrap(true); description->setProperty("muted", true); layout->addWidget(description);
    auto *contentWidget = new QWidget;
    auto *content = new QHBoxLayout(contentWidget);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(16);

    auto *preview = new QVBoxLayout;
    m_canvas = new OverlayCanvas;
    preview->addWidget(m_canvas, 1);
    auto *dragHint = new QLabel("Drag the selected layer to position it in the capture.");
    dragHint->setWordWrap(true);
    dragHint->setProperty("muted", true);
    preview->addWidget(dragHint);
    content->addLayout(preview, 1);

    auto *controls = new QWidget;
    auto *right = new QVBoxLayout(controls);
    right->setContentsMargins(0, 0, 6, 0);
    auto *layersHeader = new QHBoxLayout;
    auto *layersTitle = new QLabel("LAYERS");
    layersTitle->setObjectName("sectionTitle");
    layersHeader->addWidget(layersTitle);
    layersHeader->addStretch();
    auto *browse = new QPushButton("Browse…");
    layersHeader->addWidget(browse);
    right->addLayout(layersHeader);

    m_layers = new QListWidget;
    m_layers->setMinimumHeight(140);
    m_layers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    right->addWidget(m_layers);
    auto *order = new QHBoxLayout;
    auto *up = new QPushButton("↑");
    auto *down = new QPushButton("↓");
    auto *remove = new QPushButton("Remove");
    up->setAccessibleName("Move selected layer up");
    up->setToolTip(up->accessibleName());
    down->setAccessibleName("Move selected layer down");
    down->setToolTip(down->accessibleName());
    order->addWidget(up);
    order->addWidget(down);
    order->addStretch();
    order->addWidget(remove);
    right->addLayout(order);
    m_path = new QLineEdit;
    m_path->setPlaceholderText("Absolute image or video path");
    m_path->setAccessibleName("Overlay media path");
    right->addWidget(m_path);
    auto *add = new QPushButton("Add local media");
    right->addWidget(add);

    m_fields = new QWidget;
    auto *form = new QFormLayout(m_fields);
    form->setContentsMargins(0, 4, 0, 0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_visible = new QCheckBox("Layer visible");
    form->addRow(m_visible);
    auto percentage = [] {
        auto *spin = new ClickToAdjust<QDoubleSpinBox>;
        spin->setRange(0, 100);
        spin->setDecimals(1);
        spin->setSuffix(" %");
        return spin;
    };
    m_x = percentage();
    m_y = percentage();
    m_width = percentage();
    m_height = percentage();
    m_width->setMinimum(1);
    m_height->setMinimum(1);
    form->addRow("Horizontal position", m_x);
    form->addRow("Vertical position", m_y);
    form->addRow("Fit box width", m_width);
    form->addRow("Fit box height", m_height);
    m_opacity = new ClickToAdjust<QSpinBox>;
    m_opacity->setRange(0, 100);
    m_opacity->setSuffix(" %");
    form->addRow("Opacity", m_opacity);
    right->addWidget(m_fields);
    right->addStretch();
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(275);
    scroll->setMaximumWidth(325);
    scroll->setWidget(controls);
    content->addWidget(scroll);

    auto *pages = new QStackedWidget;
    pages->addWidget(contentWidget);
    auto *picker = new QFileDialog(this, "Choose overlay media"); picker->setOption(QFileDialog::DontUseNativeDialog); picker->setWindowFlags(Qt::Widget); picker->setFileMode(QFileDialog::ExistingFile);
    picker->setNameFilter("Images and videos (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.mp4 *.webm *.mov *.mkv)"); pages->addWidget(picker); layout->addWidget(pages, 1);
    connect(browse, &QPushButton::clicked, this, [pages, picker] { pages->setCurrentWidget(picker); });
    connect(picker, &QFileDialog::accepted, this, [this, pages, picker, contentWidget, add] { m_path->setText(picker->selectedFiles().value(0)); pages->setCurrentWidget(contentWidget); add->click(); });
    connect(picker, &QFileDialog::rejected, this, [pages, contentWidget] { pages->setCurrentWidget(contentWidget); });
    m_status = new QLabel("Animated layers loop silently."); m_status->setWordWrap(true); m_status->setTextFormat(Qt::PlainText); m_status->setProperty("muted", true); layout->addWidget(m_status);
    auto *footer = new QHBoxLayout; auto *back = new QPushButton("Back to dashboard"); footer->addWidget(back); footer->addStretch();
    auto *apply = new QPushButton("Apply now"); apply->setObjectName("primary"); footer->addWidget(apply); layout->addLayout(footer);
    m_debounce.setSingleShot(true); m_debounce.setInterval(350);
    connect(&m_debounce, &QTimer::timeout, this, [this] { emit changed(m_scene); });
    connect(apply, &QPushButton::clicked, this, &OverlayEditor::flush);
    connect(back, &QPushButton::clicked, this, [this] { flush(); emit this->back(); });
    connect(m_layers, &QListWidget::currentRowChanged, this, &OverlayEditor::select);
    connect(m_master, &QCheckBox::toggled, this, [this](bool enabled) { if (!m_loading) { m_scene.enabled = enabled; schedule(); } });
    connect(add, &QPushButton::clicked, this, [this] {
        const auto path = m_path->text().trimmed();
        if (m_scene.layers.size() >= 16) { m_status->setText("A maximum of 16 layers is supported."); return; }
        if (!QFileInfo(path).isAbsolute() || !OverlayLayer::supports(path)) { m_status->setText("Enter an absolute path to PNG, JPEG, BMP, WebP, GIF, MP4, WebM, MOV or MKV."); return; }
        Media check; QString error; if (!check.open(path, QSize(320, 180), &error)) { m_status->setText(error); return; }
        OverlayLayer layer; layer.path = path; m_scene.layers.prepend(layer); m_path->clear(); refreshList(0); schedule();
    });
    connect(remove, &QPushButton::clicked, this, [this] { const int row = m_layers->currentRow(); if (row < 0) return; m_scene.layers.removeAt(row); refreshList(qMin(row, static_cast<int>(m_scene.layers.size()) - 1)); schedule(); });
    auto reorder = [this](int delta) { const int row = m_layers->currentRow(), next = row + delta; if (row < 0 || next < 0 || next >= m_scene.layers.size()) return; m_scene.layers.swapItemsAt(row, next); refreshList(next); schedule(); };
    connect(up, &QPushButton::clicked, this, [reorder] { reorder(-1); }); connect(down, &QPushButton::clicked, this, [reorder] { reorder(1); });
    for (auto *spin : {m_x, m_y, m_width, m_height}) connect(spin, &QDoubleSpinBox::valueChanged, this, &OverlayEditor::updateLayer);
    connect(m_opacity, &QSpinBox::valueChanged, this, &OverlayEditor::updateLayer); connect(m_visible, &QCheckBox::toggled, this, &OverlayEditor::updateLayer);
    connect(m_canvas, &OverlayCanvas::moved, this, [this](int row, double x, double y) {
        m_scene.layers[row].setPosition(x, y); m_loading = true;
        const auto &layer = m_scene.layers[row]; m_x->setValue(layer.width < 1 ? layer.x / (1 - layer.width) * 100 : 0); m_y->setValue(layer.height < 1 ? layer.y / (1 - layer.height) * 100 : 0);
        m_loading = false; schedule();
    });
}
void OverlayEditor::edit(const OverlayScene &scene) { m_debounce.stop(); m_scene = scene; m_loading = true; m_master->setChecked(scene.enabled); m_loading = false; refreshList(scene.layers.isEmpty() ? -1 : 0); }
void OverlayEditor::refreshList(int selected) {
    m_loading = true; m_layers->clear();
    for (const auto &layer : m_scene.layers) { auto *item = new QListWidgetItem((layer.enabled ? "●  " : "○  ") + QFileInfo(layer.path).fileName(), m_layers); item->setToolTip(layer.path); }
    m_layers->setCurrentRow(selected); m_loading = false; select(selected);
}
void OverlayEditor::select(int row) {
    if (m_loading) return;
    m_loading = true; m_fields->setEnabled(row >= 0 && row < m_scene.layers.size());
    if (m_fields->isEnabled()) {
        const auto &layer = m_scene.layers[row]; m_visible->setChecked(layer.enabled); m_width->setValue(layer.width * 100); m_height->setValue(layer.height * 100); m_opacity->setValue(layer.opacity);
        m_x->setValue(layer.width < 1 ? layer.x / (1 - layer.width) * 100 : 0); m_y->setValue(layer.height < 1 ? layer.y / (1 - layer.height) * 100 : 0);
    }
    m_loading = false; m_canvas->setScene(m_scene, row);
    if (!m_canvas->mediaError().isEmpty()) m_status->setText(m_canvas->mediaError());
}
void OverlayEditor::updateLayer() {
    const int row = m_layers->currentRow(); if (m_loading || row < 0) return;
    auto &layer = m_scene.layers[row];
    const bool resized = layer.width != m_width->value() / 100. || layer.height != m_height->value() / 100.;
    const double oldX = layer.x, oldY = layer.y;
    layer.width = m_width->value() / 100.; layer.height = m_height->value() / 100.;
    layer.setPosition(resized ? oldX : m_x->value() / 100. * (1 - layer.width), resized ? oldY : m_y->value() / 100. * (1 - layer.height));
    layer.opacity = m_opacity->value(); layer.enabled = m_visible->isChecked();
    m_layers->item(row)->setText((layer.enabled ? "●  " : "○  ") + QFileInfo(layer.path).fileName()); select(row); schedule();
}
void OverlayEditor::schedule() { m_canvas->setScene(m_scene, m_layers->currentRow()); m_status->setText("Applying layer changes…"); m_debounce.start(); }
void OverlayEditor::flush() { if (m_debounce.isActive()) { m_debounce.stop(); emit changed(m_scene); } }
void OverlayEditor::setResult(bool success, const QString &error) { m_status->setText(success ? "Layers applied and saved." : error); }
} // namespace Shadowless
