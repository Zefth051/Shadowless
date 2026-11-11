#include "clip_player.h"
#include "../player/mpv_player.h"
#include "../player/media_tools.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHideEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyleOptionSlider>
#include <QStackedWidget>
#include <QScrollArea>
#include <QRegularExpression>
#include "value_controls.h"
#include <QVBoxLayout>
#include <functional>
#include <limits>

namespace Shadowless {
class AudioTrackButton : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
};
class ClipTimeEdit : public ClickToAdjust<QDoubleSpinBox> {
public:
    ClipTimeEdit() {
        setDecimals(3); setSingleStep(.1); setButtonSymbols(QAbstractSpinBox::NoButtons);
        setKeyboardTracking(false); setAlignment(Qt::AlignCenter); setFixedWidth(132);
        setToolTip("Minutes:seconds.milliseconds, or seconds. Arrow keys adjust by 0.1 seconds.");
    }
protected:
    QString textFromValue(double value) const override {
        const qint64 ms=qRound64(value*1000);
        return QString("%1:%2.%3").arg(ms/60000,2,10,QChar('0'))
            .arg(ms/1000%60,2,10,QChar('0')).arg(ms%1000,3,10,QChar('0'));
    }
    double valueFromText(const QString &text) const override {
        const auto parts=text.trimmed().split(':');
        return parts.size()==2?parts[0].toDouble()*60+parts[1].toDouble():text.toDouble();
    }
    QValidator::State validate(QString &text,int &) const override {
        static const QRegularExpression format("^[0-9]+(?::[0-5]?[0-9]?)?(?:\\.[0-9]{0,3})?$");
        if(text.isEmpty()) return QValidator::Intermediate;
        if(!format.match(text).hasMatch()) return QValidator::Invalid;
        if(text.endsWith(':')||text.endsWith('.')) return QValidator::Intermediate;
        const double value=valueFromText(text);
        return value>=minimum()&&value<=maximum()?QValidator::Acceptable:QValidator::Intermediate;
    }
};
class PlaybackSlider : public QSlider {
public:
    PlaybackSlider():QSlider(Qt::Horizontal) { setCursor(Qt::PointingHandCursor); setFixedHeight(20); }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()!=Qt::LeftButton) { QSlider::mousePressEvent(event); return; }
        setFocus(); setSliderDown(true); moveTo(event); event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if(isSliderDown()) { moveTo(event); event->accept(); } else QSlider::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if(event->button()!=Qt::LeftButton || !isSliderDown()) { QSlider::mouseReleaseEvent(event); return; }
        moveTo(event); setSliderDown(false); event->accept();
    }
private:
    void moveTo(QMouseEvent *event) {
        QStyleOptionSlider option; initStyleOption(&option);
        const auto groove=style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderGroove,this);
        const auto handle=style()->subControlRect(QStyle::CC_Slider,&option,QStyle::SC_SliderHandle,this);
        setSliderPosition(QStyle::sliderValueFromPosition(minimum(),maximum(),qRound(event->position().x())-groove.x()-handle.width()/2,
                                                        qMax(1,groove.width()-handle.width()),option.upsideDown));
    }
};
class ClipTimeline : public QWidget {
public:
    explicit ClipTimeline(QWidget *parent=nullptr):QWidget(parent) {
        setObjectName("clipTimeline"); setMinimumHeight(76); setMouseTracking(true); setCursor(Qt::PointingHandCursor);
        setAccessibleName("Clip timeline: click to seek, drag trim handles");
        setToolTip("Drag the end handles to trim; click inside to preview. In/Out fields also adjust the range.");
    }
    QVector<QImage> images;
    qint64 duration=0, position=0, in=0, out=0;
    std::function<void(qint64)> seek;
    std::function<void(qint64,bool)> trim;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.fillRect(rect(),QColor("#15151d"));
        for(int i=0;i<images.size();++i) p.drawImage(QRect(i*width()/images.size(),0,(i+1)*width()/images.size()-i*width()/images.size(),height()),images[i]);
        if(duration<=0) return;
        const int left=int(in*width()/duration),right=int(out*width()/duration);
        p.fillRect(QRect(0,0,left,height()),QColor(0,0,0,150)); p.fillRect(QRect(right,0,width()-right,height()),QColor(0,0,0,150));
        p.setPen(QPen(palette().color(QPalette::Accent),3)); p.drawRect(QRect(left,1,qMax(1,right-left-1),height()-3));
        p.setPen(QPen(Qt::white,2)); const int x=int(position*width()/duration); p.drawLine(x,0,x,height());
        for(bool start:{true,false}) {
            const auto handle=handleRect(start);
            p.setPen(Qt::NoPen); p.setBrush(palette().color(QPalette::Accent)); p.drawRoundedRect(handle,4,4);
            p.setPen(QPen(palette().color(QPalette::HighlightedText),2));
            p.drawLine(handle.center().x(),height()/2-11,handle.center().x(),height()/2+11);
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        if(duration<=0 || e->button()!=Qt::LeftButton) return;
        m_handle=handleAt(e->position().toPoint());
        m_dragOffset=m_handle?e->position().x()-double(m_handle==1?in:out)*width()/duration:0;
        move(e); e->accept();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if(e->buttons()&Qt::LeftButton) move(e);
        setCursor(m_handle||handleAt(e->position().toPoint())?Qt::SizeHorCursor:Qt::PointingHandCursor);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if(e->button()!=Qt::LeftButton) return;
        move(e); m_handle=0; m_dragOffset=0; e->accept();
        setCursor(handleAt(e->position().toPoint())?Qt::SizeHorCursor:Qt::PointingHandCursor);
    }
private:
    int m_handle=0;
    qreal m_dragOffset=0;
    QRect handleRect(bool start) const {
        const int size=qMin(16,width());
        const int edge=int((start?in:out)*width()/qMax<qint64>(1,duration));
        return QRect(qBound(0,start?edge:edge-size,width()-size),0,size,height());
    }
    int handleAt(QPoint point) const {
        if(duration<=0) return 0;
        const auto left=handleRect(true),right=handleRect(false);
        if(left.contains(point)&&right.contains(point)) return point.x()<double(in+out)*width()/(2*duration)?1:2;
        return left.contains(point)?1:(right.contains(point)?2:0);
    }
    void move(QMouseEvent *e) {
        if(duration<=0) return;
        const qint64 ms=qBound<qint64>(0,qRound64((e->position().x()-m_dragOffset)*duration/qMax(1,width())),duration);
        const qint64 minimum=qMin<qint64>(250,duration);
        if(m_handle && trim) {
            const qint64 bounded=m_handle==1
                ?qBound<qint64>(0,ms,qMax<qint64>(0,out-minimum))
                :qBound(qMin(duration,in+minimum),ms,duration);
            trim(bounded,m_handle==1);
        } else if(seek) seek(ms);
    }
};
class Waveform : public QWidget {
public:
    QVector<float> peaks;
    explicit Waveform(QWidget *p=nullptr):QWidget(p) { setFixedHeight(32); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen);
        p.setBrush(palette().color(QPalette::Base)); p.drawRoundedRect(rect(),5,5);
        p.setPen(isEnabled()?palette().color(QPalette::Accent):palette().color(QPalette::Mid));
        p.drawLine(0,height()/2,width(),height()/2);
        for(int i=0;i<peaks.size();++i) { int x=i*width()/peaks.size(),h=qRound(qBound(0.f,peaks[i]*3.f,1.f)*(height()/2.-1)); p.drawLine(x,height()/2-h,x,height()/2+h); }
    }
};
namespace {
QString clockText(qint64 ms) {
    if(ms>=3600000) return QString("%1:%2:%3").arg(ms/3600000).arg(ms/60000%60,2,10,QChar('0')).arg(ms/1000%60,2,10,QChar('0'));
    return QString("%1:%2").arg(ms/60000,2,10,QChar('0')).arg(ms/1000%60,2,10,QChar('0'));
}
QPushButton *button(const QString &text,const char *name,QHBoxLayout *layout) { auto *b=new QPushButton(text); b->setObjectName(name); layout->addWidget(b); return b; }
void playerIcon(QPushButton *button,const QString &icon,const QString &label) {
    button->setText({}); button->setIcon(QIcon(":/shadowless/icons/"+icon+".svg")); button->setIconSize(QSize(18,18));
    button->setToolTip(label); button->setAccessibleName(label); button->setProperty("transport",true); button->setFixedSize(34,34);
}
}
ClipPlayer::ClipPlayer(QWidget *parent):QWidget(parent),m_player(new MpvPlayer(this)),m_tools(new MediaTools(this)) {
    setObjectName("clipPlayer"); setFocusPolicy(Qt::StrongFocus); m_player->setObjectName("mpvPlayer");
    auto *outer=new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0);
    m_pages=new QStackedWidget; outer->addWidget(m_pages); auto *content=new QWidget; m_pages->addWidget(content);
    auto *layout=new QVBoxLayout(content); layout->setContentsMargins(0,0,0,0);
    m_header=new QWidget; auto *header=new QHBoxLayout(m_header); header->setContentsMargins(0,0,0,0);
    auto *backButton=button("Back to dashboard","clipBack",header); playerIcon(backButton,"back","Back to dashboard");
    auto *heading=new QLabel("Replay player"); heading->setObjectName("brand"); header->addWidget(heading);
    m_title=new QLabel; m_title->setTextFormat(Qt::PlainText); m_title->setProperty("muted",true); m_title->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); header->addWidget(m_title,1);
    layout->addWidget(m_header);
    auto *body=new QHBoxLayout; body->setSpacing(20); auto *playback=new QVBoxLayout; playback->setSpacing(8); body->addLayout(playback,1);
    auto *video=new QWidget; m_videoLayout=new QGridLayout(video); m_videoLayout->setContentsMargins(0,0,0,0); m_videoLayout->setSpacing(8);
    m_videoLayout->setRowStretch(0,1); m_videoLayout->addWidget(m_player,0,0); playback->addWidget(video,1);
    m_recentPanel=new QWidget; m_recentPanel->setObjectName("clipSidebar"); m_recentPanel->setFixedWidth(240); auto *recentLayout=new QVBoxLayout(m_recentPanel); recentLayout->setContentsMargins(12,12,12,12);
    auto *recentTitle=new QLabel("All clips"); recentTitle->setObjectName("sectionTitle"); recentLayout->addWidget(recentTitle);
    m_recent=new QListWidget; m_recent->setObjectName("clipRecent"); m_recent->setTextElideMode(Qt::ElideRight); m_recent->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); m_recent->setIconSize(QSize(48,28)); recentLayout->addWidget(m_recent,1); body->addWidget(m_recentPanel); layout->addLayout(body,1);
    m_controls=new QWidget; auto *transport=new QVBoxLayout(m_controls); transport->setContentsMargins(0,0,0,0);
    m_controls->setObjectName("clipControls");
    m_seek=new PlaybackSlider; m_seek->setObjectName("clipSeek"); m_seek->setAccessibleName("Playback position"); m_seek->setRange(0,0); m_seek->setSingleStep(1000); m_seek->setPageStep(5000); m_seek->setTracking(false); transport->addWidget(m_seek);
    auto *controls=new QHBoxLayout; controls->setSpacing(7); m_next=button("Next","clipNext",controls); playerIcon(m_next,"skip-previous","Next replay (Page Up)");
    auto *rewind=button("","clipSeekBack",controls); playerIcon(rewind,"back","Back 5 seconds (Left)");
    m_play=button("Play","clipPlayPause",controls);
    auto *forward=button("","clipSeekForward",controls); playerIcon(forward,"forward","Forward 5 seconds (Right)");
    m_previous=button("Previous","clipPrevious",controls); playerIcon(m_previous,"skip-next","Previous replay (Page Down)");
    controls->addStretch(); m_time=new QLabel("00:00 / 00:00"); m_time->setObjectName("clipTime"); controls->addWidget(m_time); controls->addStretch();
    m_speed=new QComboBox; m_speed->setObjectName("clipSpeed"); m_speed->setAccessibleName("Playback speed");
    for(double speed:{.25,.5,.75,1.,1.25,1.5,2.}) m_speed->addItem(QString::number(speed)+"×",speed);
    m_speed->setCurrentIndex(3); controls->addWidget(m_speed);
    m_mute=new QCheckBox; m_mute->setObjectName("clipMute"); m_mute->setIcon(QIcon(":/shadowless/icons/speaker.svg")); m_mute->setToolTip("Mute (M)"); m_mute->setAccessibleName("Mute"); controls->addWidget(m_mute); m_volume=new QSlider(Qt::Horizontal); m_volume->setRange(0,100); m_volume->setValue(80); m_volume->setFixedWidth(90); m_volume->setAccessibleName("Playback volume"); controls->addWidget(m_volume);
    m_edit=button("Edit clip","clipTrim",controls); m_edit->setCheckable(true);
    m_fullscreenButton=button("Fullscreen","clipFullscreen",controls); playerIcon(m_fullscreenButton,"fullscreen","Fullscreen (F)"); transport->addLayout(controls); m_videoLayout->addWidget(m_controls,1,0);
    m_audio=new QWidget; m_trackLayout=new QVBoxLayout(m_audio); m_trackLayout->setContentsMargins(0,0,0,0); m_trackLayout->setSpacing(6);
    m_trackLayout->setAlignment(Qt::AlignTop);
    m_audioScroll=new QScrollArea; m_audioScroll->setObjectName("clipAudioTracks"); m_audioScroll->setWidgetResizable(true);
    m_audioScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); m_audioScroll->setWidget(m_audio); m_audioScroll->hide(); playback->addWidget(m_audioScroll);
    m_editing=new QWidget; auto *editing=new QVBoxLayout(m_editing); editing->setContentsMargins(0,0,0,0); m_timeline=new ClipTimeline; editing->addWidget(m_timeline);
    auto *range=new QHBoxLayout; range->setSpacing(8); range->addWidget(new QLabel("Start")); m_in=new ClipTimeEdit; m_in->setObjectName("clipIn"); m_in->setAccessibleName("Trim start"); range->addWidget(m_in); auto *setIn=button("Set here","clipSetIn",range); setIn->setToolTip("Set start to playback position (I)");
    range->addSpacing(12); range->addWidget(new QLabel("End")); m_out=new ClipTimeEdit; m_out->setObjectName("clipOut"); m_out->setAccessibleName("Trim end"); range->addWidget(m_out); auto *setOut=button("Set here","clipSetOut",range); setOut->setToolTip("Set end to playback position (O)"); range->addStretch();
    for(auto *field:{m_in,m_out}) { field->setDecimals(3); field->setRange(0,0); field->setSingleStep(.1); }
    editing->addLayout(range); m_mix=new QCheckBox("Merge selected audio tracks into one"); editing->addWidget(m_mix);
    auto *note=new QLabel("Video is copied to keep exports light. Cuts can begin at an earlier keyframe; selected audio stays separate unless merged."); note->setWordWrap(true); editing->addWidget(note);
    auto *jobs=new QHBoxLayout; m_export=button("Save trimmed copy…","clipExport",jobs); m_cancel=button("Cancel job","clipCancel",jobs); m_jobStatus=new QLabel; m_jobStatus->setTextFormat(Qt::PlainText); jobs->addWidget(m_jobStatus,1); editing->addLayout(jobs); playback->addWidget(m_editing); m_editing->hide(); m_cancel->hide();
    m_error=new QLabel; m_error->setTextFormat(Qt::PlainText); m_error->setWordWrap(true); playback->addWidget(m_error);
    connect(backButton,&QPushButton::clicked,this,&ClipPlayer::back);
    connect(m_play,&QPushButton::clicked,this,[this] { m_player->pause(!m_player->paused()); showControls(); });
    connect(m_next,&QPushButton::clicked,this,[this] { navigate(-1); }); connect(m_previous,&QPushButton::clicked,this,[this] { navigate(1); });
    connect(rewind,&QPushButton::clicked,this,[this] { m_player->seek(m_player->position()-5000); });
    connect(forward,&QPushButton::clicked,this,[this] { m_player->seek(m_player->position()+5000); });
    connect(m_recent,&QListWidget::itemClicked,this,[this](QListWidgetItem *item) { const auto path=item->data(Qt::UserRole).toString(); if(path==m_player->source()) m_player->pause(!m_player->paused()); else open(path); });
    connect(m_recent,&QListWidget::itemActivated,this,[this](QListWidgetItem *item) { const auto path=item->data(Qt::UserRole).toString(); if(path!=m_player->source()) open(path); });
    connect(m_seek,&QSlider::valueChanged,m_player,&MpvPlayer::seek); connect(m_volume,&QSlider::valueChanged,m_player,&MpvPlayer::setVolume); connect(m_mute,&QCheckBox::toggled,m_player,&MpvPlayer::setMuted);
    connect(m_seek,&QSlider::sliderMoved,this,[this](int ms) { m_time->setText(clockText(ms)+" / "+clockText(m_player->duration())); });
    connect(m_seek,&QSlider::sliderPressed,this,[this] { m_resumeAfterScrub=!m_player->paused(); m_player->pause(true); });
    connect(m_seek,&QSlider::sliderReleased,this,[this] { if(m_resumeAfterScrub) m_player->pause(false); m_resumeAfterScrub=false; });
    connect(m_speed,&QComboBox::currentIndexChanged,this,[this] { m_player->setSpeed(m_speed->currentData().toDouble()); });
    connect(m_fullscreenButton,&QPushButton::clicked,this,[this] { setFullscreen(!m_fullscreen); }); connect(m_player,&MpvPlayer::doubleClicked,this,[this] { setFullscreen(!m_fullscreen); });
    connect(m_player,&MpvPlayer::stateChanged,this,&ClipPlayer::updateState); connect(m_player,&MpvPlayer::tracksChanged,this,&ClipPlayer::updateTracks); connect(m_player,&MpvPlayer::error,m_error,&QLabel::setText);
    connect(m_edit,&QPushButton::toggled,this,[this](bool enabled) {
        m_editingEnabled=enabled; m_editing->setVisible(enabled&&!m_fullscreen); showControls();
        if(enabled) analyze(); else { m_analyzePending=false; if(!m_exporting) m_tools->stop(); }
        for(int id:m_waveforms.keys()) m_waveforms[id]->setVisible(enabled);
        updateState(); emit editingChanged(enabled);
    });
    const auto updateRange=[this] { m_timeline->in=qRound64(m_in->value()*1000); m_timeline->out=qRound64(m_out->value()*1000); m_timeline->update(); updateState(); };
    connect(m_in,&QDoubleSpinBox::valueChanged,this,updateRange); connect(m_out,&QDoubleSpinBox::valueChanged,this,updateRange);
    connect(setIn,&QPushButton::clicked,this,[this] { m_in->setValue(m_player->position()/1000.); }); connect(setOut,&QPushButton::clicked,this,[this] { m_out->setValue(m_player->position()/1000.); });
    m_timeline->seek=[this](qint64 ms) { m_player->pause(true); m_player->seek(ms); };
    m_timeline->trim=[this](qint64 ms,bool in) { m_player->pause(true); (in?m_in:m_out)->setValue(ms/1000.); m_player->seek(ms); };
    connect(m_export,&QPushButton::clicked,this,&ClipPlayer::chooseExport); connect(m_cancel,&QPushButton::clicked,m_tools,&MediaTools::stop);
    connect(m_tools,&MediaTools::metadataReady,this,[this](const MediaInfo &info) {
        if(!isVisible() || !m_editingEnabled || m_analyzePending || info.path!=QFileInfo(m_player->source()).absoluteFilePath()) return;
        m_metadataReady=true; m_timeline->duration=info.durationMs; m_in->setMaximum(info.durationMs/1000.); m_out->setMaximum(info.durationMs/1000.); m_in->setValue(0); m_out->setValue(info.durationMs/1000.); updateState();
    });
    connect(m_tools,&MediaTools::thumbnailsReady,this,[this](const QVector<QImage> &images) { if(isVisible()&&m_editingEnabled&&!m_analyzePending&&m_metadataReady) { m_timeline->images=images; m_timeline->update(); } });
    connect(m_tools,&MediaTools::waveformReady,this,[this](int ordinal,const QVector<float> &peaks) {
        if(!isVisible()||!m_editingEnabled||m_analyzePending||!m_metadataReady) return;
        const auto tracks=m_player->audioTracks(); if(ordinal<0||ordinal>=tracks.size()) return;
        auto *wave=static_cast<Waveform *>(m_waveforms.value(tracks[ordinal].id)); if(wave) { wave->peaks=peaks; wave->update(); }
    });
    connect(m_tools,&MediaTools::failed,this,[this](const QString &message) { m_error->setText(message); m_jobStatus->setText("Job failed"); });
    connect(m_tools,&MediaTools::cancelled,this,[this] { m_jobStatus->setText("Cancelled"); });
    connect(m_tools,&MediaTools::progress,this,[this](double fraction) { if(!m_analyzePending) m_jobStatus->setText(QString(m_exporting?"Exporting %1%":"Analyzing %1%").arg(qRound(fraction*100))); });
    connect(m_tools,&MediaTools::exported,this,[this](const QString &path) {
        m_jobStatus->setText("Export complete"); emit clipExported(path);
        if(path==QFileInfo(m_player->source()).absoluteFilePath()) { m_metadataReady=false; m_player->open(path); m_player->pause(true); m_analyzePending=m_editingEnabled&&isVisible(); }
    });
    connect(m_tools,&MediaTools::finished,this,[this] { if(m_exporting) emit exportingChanged(false); m_exporting=false; m_cancel->hide(); if(m_metadataReady&&m_jobStatus->text().startsWith("Analyzing")) m_jobStatus->setText("Ready to export"); updateState(); if(m_analyzePending) QTimer::singleShot(0,this,&ClipPlayer::analyze); });
    m_idle.setSingleShot(true); m_idle.setInterval(2400); connect(&m_idle,&QTimer::timeout,this,[this] { if(m_fullscreen&&!m_player->paused()&&!m_controls->underMouse()&&m_pages->currentIndex()==0) { m_controls->hide(); m_player->setCursor(Qt::BlankCursor); } });
    qApp->installEventFilter(this); updateState();
}
void ClipPlayer::setClips(const QStringList &paths) {
    m_clips=paths; m_highlightedSource.clear(); m_recent->clear(); for(const auto &path:paths) { auto *item=new QListWidgetItem(QIcon(":/shadowless/icons/video.svg"),QFileInfo(path).completeBaseName(),m_recent); item->setData(Qt::UserRole,path); item->setToolTip(path); item->setSizeHint(QSize(0,52)); } updateState();
}
void ClipPlayer::setThumbnail(const QString &path,const QPixmap &image) {
    const int index=m_clips.indexOf(path);
    if(index>=0) m_recent->item(index)->setIcon(QIcon(image));
}
void ClipPlayer::open(const QString &path) {
    if(m_exporting) return;
    m_edit->setChecked(false);
    m_analyzePending=false; m_metadataReady=false; m_tools->stop(); m_error->clear(); m_jobStatus->clear();
    m_timeline->images.clear(); m_timeline->duration=0; m_timeline->update();
    m_tracks.clear();
    m_waveforms.clear();
    while(auto *item=m_trackLayout->takeAt(0)) { delete item->widget(); delete item; }
    if(!m_clips.contains(path)) { auto paths=m_clips; paths.prepend(path); setClips(paths); }
    m_title->setText(QFileInfo(path).fileName()); m_title->setToolTip(path); m_player->open(path); m_player->setVolume(m_volume->value()); m_player->setMuted(m_mute->isChecked()); m_player->setSpeed(m_speed->currentData().toDouble());
    m_pages->setCurrentIndex(0); showControls(); updateState(); if(m_editingEnabled) analyze();
}
void ClipPlayer::navigate(int delta) { const int index=m_clips.indexOf(m_player->source()); if(index>=0 && index+delta>=0 && index+delta<m_clips.size()) open(m_clips[index+delta]); }
void ClipPlayer::updateState() {
    m_player->setPlaybackRange(m_editingEnabled&&m_metadataReady?qRound64(m_in->value()*1000):0,
                              m_editingEnabled&&m_metadataReady?qRound64(m_out->value()*1000):0);
    const int index=m_clips.indexOf(m_player->source()); m_next->setEnabled(index>0&&!m_exporting); m_previous->setEnabled(index>=0&&index+1<m_clips.size()&&!m_exporting); m_recent->setEnabled(!m_exporting);
    m_edit->setEnabled(!m_exporting); m_in->setEnabled(!m_exporting); m_out->setEnabled(!m_exporting); m_mix->setEnabled(!m_exporting);
    playerIcon(m_play,m_player->paused()?"play":"pause",m_player->paused()?"Play (Space)":"Pause (Space)"); m_play->setEnabled(m_player->loaded());
    m_time->setText(clockText(m_seek->isSliderDown()?m_seek->sliderPosition():m_player->position())+" / "+clockText(m_player->duration()));
    const QSignalBlocker block(m_seek); m_seek->setRange(0,int(qMin<qint64>(m_player->duration(),std::numeric_limits<int>::max()))); m_seek->setEnabled(m_player->loaded()); if(!m_seek->isSliderDown()) m_seek->setValue(int(m_player->position()));
    m_timeline->position=m_player->position(); m_timeline->update();
    m_export->setEnabled(m_metadataReady&&!m_tools->busy()&&m_in->value()<m_out->value());
    if(m_highlightedSource!=m_player->source()) { m_highlightedSource=m_player->source(); m_recent->setCurrentRow(index); }
    if(m_player->paused()) showControls();
}
void ClipPlayer::updateTracks() {
    const auto tracks=m_player->audioTracks(); QVector<int> ids; for(const auto &track:tracks) ids.append(track.id);
    bool changed=false;
    for(int id:m_tracks.keys()) if(!ids.contains(id)) { auto *row=m_tracks.take(id)->parentWidget(); m_waveforms.remove(id); delete row; changed=true; }
    for(int i=0;i<tracks.size();++i) {
        const auto &track=tracks[i];
        const auto title=fontMetrics().elidedText(track.title,Qt::ElideRight,150);
        if(m_tracks.contains(track.id)) { m_tracks[track.id]->setText(title); continue; }
        auto *row=new QWidget; row->setObjectName("clipAudioRow"); auto *line=new QHBoxLayout(row); line->setContentsMargins(6,4,6,4); line->setSpacing(12);
        auto *check=new AudioTrackButton(title); check->setObjectName(QString("clipTrack%1").arg(i)); check->setProperty("clipAudioTrack",true); check->setFixedSize(190,32); check->setCursor(Qt::PointingHandCursor); check->setChecked(true); check->setAccessibleName(track.title); check->setToolTip(track.title+"\nInclude in playback and export"); line->addWidget(check); m_tracks.insert(track.id,check);
        auto *wave=new Waveform; line->addWidget(wave,1); m_trackLayout->addWidget(row); m_waveforms.insert(track.id,wave); wave->setVisible(m_editingEnabled); changed=true;
        connect(check,&QCheckBox::toggled,this,[this,wave](bool checked) { wave->setEnabled(checked); wave->update(); QVector<int> selected; for(const auto &t:m_player->audioTracks()) if(m_tracks.value(t.id)&&m_tracks[t.id]->isChecked()) selected.append(t.id); m_player->setAudioTracks(selected); });
    }
    m_audioScroll->setFixedHeight(qMin(tracks.size(),4)*46);
    m_audioScroll->setVisible(!tracks.isEmpty()&&!m_fullscreen);
    if(changed) { QVector<int> selected; for(const auto &t:tracks) if(m_tracks[t.id]->isChecked()) selected.append(t.id); m_player->setAudioTracks(selected); }
}
void ClipPlayer::showControls() { m_header->setVisible(!m_fullscreen); m_controls->show(); m_recentPanel->setVisible(!m_fullscreen); m_edit->setVisible(!m_fullscreen); m_audioScroll->setVisible(!m_fullscreen&&!m_tracks.isEmpty()); m_player->unsetCursor(); if(m_fullscreen&&!m_player->paused()) m_idle.start(); else m_idle.stop(); }
void ClipPlayer::setFullscreen(bool enabled) {
    if(m_fullscreen==enabled) return;
    m_fullscreen=enabled;
    m_videoLayout->removeWidget(m_controls);
    m_videoLayout->addWidget(m_controls,enabled?0:1,0,enabled?Qt::AlignBottom:Qt::Alignment{});
    m_videoLayout->setSpacing(enabled?0:8);
    m_controls->layout()->setContentsMargins(enabled?12:0,enabled?8:0,enabled?12:0,enabled?8:0);
    m_controls->setStyleSheet(enabled?"QWidget#clipControls { background: rgba(16, 20, 22, 220); }":"");
    m_controls->raise();
    m_fullscreenButton->setAccessibleName(enabled?"Exit fullscreen":"Fullscreen"); m_fullscreenButton->setToolTip(enabled?"Exit fullscreen (Esc)":"Fullscreen (F)");
    m_editing->setVisible(!enabled&&m_editingEnabled); m_error->setVisible(!enabled); showControls(); emit fullscreenChanged(enabled);
}
bool ClipPlayer::handleEscape() {
    if(m_pages->currentIndex()!=0) { QMetaObject::invokeMethod(m_pages->currentWidget(),"reject"); return true; }
    if(m_fullscreen) { setFullscreen(false); return true; }
    return false;
}
void ClipPlayer::analyze() {
    if(!isVisible()||!m_editingEnabled||m_player->source().isEmpty()) { m_analyzePending=false; return; }
    if(m_tools->busy()) { m_analyzePending=true; return; }
    m_analyzePending=false; m_metadataReady=false; m_jobStatus->setText("Analyzing clip…");
    if(m_tools->analyze(m_player->source())) m_cancel->show();
    updateState();
}
void ClipPlayer::chooseExport() {
    if(m_tools->busy()||!m_metadataReady) return;
    m_player->pause(true); showControls();
    auto *picker=new QFileDialog(this,"Export trimmed clip"); picker->setOption(QFileDialog::DontUseNativeDialog); picker->setOption(QFileDialog::DontConfirmOverwrite); picker->setWindowFlags(Qt::Widget); picker->setAcceptMode(QFileDialog::AcceptSave); picker->setFileMode(QFileDialog::AnyFile); picker->setNameFilter("Video clips (*.mp4 *.mkv)");
    const QFileInfo source(m_player->source()); picker->setDirectory(source.absolutePath()); picker->selectFile(source.completeBaseName()+"_trim."+(source.suffix().toLower()=="mp4"?"mp4":"mkv"));
    m_pages->addWidget(picker); m_pages->setCurrentWidget(picker);
    connect(picker,&QFileDialog::rejected,this,[this,picker] { m_pages->setCurrentIndex(0); m_pages->removeWidget(picker); picker->deleteLater(); showControls(); });
    connect(picker,&QFileDialog::accepted,this,[this,picker] {
        const QString path=picker->selectedFiles().value(0); m_pages->setCurrentIndex(0); m_pages->removeWidget(picker); picker->deleteLater(); if(path.isEmpty()) return;
        ExportRequest request; request.sourcePath=m_player->source(); request.destinationPath=path; request.inMs=qRound64(m_in->value()*1000); request.outMs=qRound64(m_out->value()*1000); request.mixAudio=m_mix->isChecked(); request.overwrite=QFileInfo::exists(path);
        const auto tracks=m_player->audioTracks(); for(int i=0;i<tracks.size();++i) if(m_tracks.value(tracks[i].id)&&m_tracks[tracks[i].id]->isChecked()) request.audioTracks.append(i);
        const auto start=[this,request] {
            if(m_player->source()!=request.sourcePath || !isVisible()) return;
            m_error->clear(); m_exporting=m_tools->exportClip(request); if(m_exporting) { m_cancel->show(); emit exportingChanged(true); } updateState();
        };
        if(!request.overwrite) { start(); return; }
        auto *confirm=new QMessageBox(QMessageBox::Question,"Replace clip?","Replace "+QFileInfo(path).fileName()+"? The original is kept until the new clip passes validation.",QMessageBox::Yes|QMessageBox::No,this);
        confirm->setTextFormat(Qt::PlainText); confirm->setDefaultButton(QMessageBox::No); confirm->setWindowFlags(Qt::Widget); m_pages->addWidget(confirm); m_pages->setCurrentWidget(confirm);
        connect(confirm,&QMessageBox::finished,this,[this,confirm,start](int result) {
            m_pages->setCurrentIndex(0); m_pages->removeWidget(confirm); confirm->deleteLater(); if(result==QMessageBox::Yes) start(); else showControls();
        });
    });
}
void ClipPlayer::hideEvent(QHideEvent *event) {
    m_analyzePending=false; m_metadataReady=false;
    m_edit->setChecked(false);
    if(!m_exporting) m_tools->stop();
    m_player->release(); m_idle.stop();
    if(m_pages->currentIndex()!=0) QMetaObject::invokeMethod(m_pages->currentWidget(),"reject");
    setFullscreen(false); QWidget::hideEvent(event);
}
bool ClipPlayer::eventFilter(QObject *watched,QEvent *event) {
    auto *widget=qobject_cast<QWidget *>(watched); if(!isVisible()||!widget||(widget!=this&&!isAncestorOf(widget))) return QWidget::eventFilter(watched,event);
    if(event->type()==QEvent::MouseMove||event->type()==QEvent::MouseButtonPress) showControls();
    if(event->type()!=QEvent::KeyPress||m_pages->currentIndex()!=0) return QWidget::eventFilter(watched,event);
    if(qobject_cast<QLineEdit *>(widget)||qobject_cast<QAbstractSpinBox *>(widget)||qobject_cast<QComboBox *>(widget)) return QWidget::eventFilter(watched,event);
    const auto *key=static_cast<QKeyEvent *>(event); if(key->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier)) return false;
    if(key->key()==Qt::Key_Space && (qobject_cast<QCheckBox *>(widget)||qobject_cast<QAbstractItemView *>(widget))) return false;
    if(key->key()>=Qt::Key_Left && key->key()<=Qt::Key_Down && (qobject_cast<QSlider *>(widget)||qobject_cast<QAbstractItemView *>(widget))) return false;
    if(m_exporting && (key->key()==Qt::Key_PageUp||key->key()==Qt::Key_PageDown)) return true;
    switch(key->key()) {
    case Qt::Key_Space: m_player->pause(!m_player->paused()); break;
    case Qt::Key_Left: m_player->seek(m_player->position()-5000); break;
    case Qt::Key_Right: m_player->seek(m_player->position()+5000); break;
    case Qt::Key_Up: m_speed->setCurrentIndex(qMin(m_speed->count()-1,m_speed->currentIndex()+1)); break;
    case Qt::Key_Down: m_speed->setCurrentIndex(qMax(0,m_speed->currentIndex()-1)); break;
    case Qt::Key_M: m_mute->toggle(); break;
    case Qt::Key_F: setFullscreen(!m_fullscreen); break;
    case Qt::Key_PageUp: navigate(-1); break;
    case Qt::Key_PageDown: navigate(1); break;
    case Qt::Key_I: if(m_editingEnabled) m_in->setValue(m_player->position()/1000.); break;
    case Qt::Key_O: if(m_editingEnabled) m_out->setValue(m_player->position()/1000.); break;
    default: return QWidget::eventFilter(watched,event);
    }
    showControls(); return true;
}
}
