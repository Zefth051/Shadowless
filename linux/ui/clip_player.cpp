#include "clip_player.h"
#include "clip_waveform.h"
#include "clip_label.h"
#include "../player/mpv_player.h"
#include "../player/media_tools.h"
#include <QApplication>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
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
        setObjectName("clipTimeline"); setMinimumHeight(44); setMouseTracking(true); setCursor(Qt::PointingHandCursor);
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
        p.setPen(QPen(window()->palette().color(QPalette::Accent),1)); p.drawRect(QRect(left,1,qMax(1,right-left-1),height()-3));
        p.setPen(QPen(Qt::white,2)); const int x=int(position*width()/duration); p.drawLine(x,0,x,height());
        for(bool start:{true,false}) {
            const auto handle=handleRect(start);
            p.setPen(Qt::NoPen); p.setBrush(window()->palette().color(QPalette::Accent)); p.drawRoundedRect(handle,4,4);
            p.setPen(QPen(window()->palette().color(QPalette::HighlightedText),2));
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
namespace {
QString clockText(qint64 ms) {
    if(ms>=3600000) return QString("%1:%2:%3").arg(ms/3600000).arg(ms/60000%60,2,10,QChar('0')).arg(ms/1000%60,2,10,QChar('0'));
    return QString("%1:%2").arg(ms/60000,2,10,QChar('0')).arg(ms/1000%60,2,10,QChar('0'));
}
QPushButton *button(const QString &text,const char *name,QHBoxLayout *layout) { auto *b=new QPushButton(text); b->setObjectName(name); layout->addWidget(b); return b; }
void playerIcon(QPushButton *button,const QString &icon,const QString &label) {
    button->setText({}); button->setIcon(QIcon(":/shadowless/icons/"+icon+".svg")); button->setIconSize(QSize(18,18));
    if(button->objectName()=="clipPlayPause") {
        // The accent-filled button needs an icon that contrasts with the accent, not the muted grey used elsewhere.
        QFile file(":/shadowless/icons/"+icon+".svg");
        if(file.open(QIODevice::ReadOnly)) {
            const bool darkAccent=button->palette().color(QPalette::Accent).lightness()<140;
            const auto svg=QString::fromUtf8(file.readAll()).replace("#a7b4bd",darkAccent?"#ffffff":"#0e1114").toUtf8();
            const qreal ratio=button->devicePixelRatioF(); QPixmap pixmap; pixmap.loadFromData(svg,"SVG");
            if(!pixmap.isNull()) { pixmap=pixmap.scaled((QSizeF(18,18)*ratio).toSize(),Qt::KeepAspectRatio,Qt::SmoothTransformation); pixmap.setDevicePixelRatio(ratio); button->setIcon(QIcon(pixmap)); }
        }
    }
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
    auto *body=new QHBoxLayout; body->setSpacing(20); auto *playback=new QVBoxLayout; playback->setSpacing(10); body->addLayout(playback,1);
    m_video=new QWidget; m_video->setMinimumSize(320,200); m_player->setParent(m_video); m_video->installEventFilter(this); playback->addWidget(m_video,1);
    m_recentPanel=new QWidget; m_recentPanel->setObjectName("clipSidebar"); m_recentPanel->setFixedWidth(220); auto *recentLayout=new QVBoxLayout(m_recentPanel); recentLayout->setContentsMargins(14,14,14,14); recentLayout->setSpacing(10);
    auto *recentTitle=new QLabel("All clips"); recentTitle->setObjectName("sectionTitle"); recentLayout->addWidget(recentTitle);
    m_gameFilter=new QComboBox; m_gameFilter->setObjectName("clipGameFilter"); m_gameFilter->setAccessibleName("Filter clips by game");
    m_gameFilter->addItem("All games",QString()); recentLayout->addWidget(m_gameFilter);
    m_modeFilter=new QComboBox; m_modeFilter->setObjectName("clipModeFilter"); m_modeFilter->setAccessibleName("Filter clips by recording mode");
    m_modeFilter->addItem("All recording modes",QString()); m_modeFilter->addItem("Instant clips","replay");
    m_modeFilter->addItem("Recordings","recording"); m_modeFilter->addItem("Other clips","unknown"); recentLayout->addWidget(m_modeFilter);
    for(auto *filter:{m_gameFilter,m_modeFilter}) { filter->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); filter->setMinimumContentsLength(12); }
    connect(m_gameFilter,&QComboBox::currentIndexChanged,this,&ClipPlayer::filterClips);
    connect(m_modeFilter,&QComboBox::currentIndexChanged,this,&ClipPlayer::filterClips);
    m_recent=new QListWidget; m_recent->setObjectName("clipRecent"); m_recent->setTextElideMode(Qt::ElideRight); m_recent->setWordWrap(true); m_recent->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); m_recent->setIconSize(QSize(48,28)); recentLayout->addWidget(m_recent,1); body->addWidget(m_recentPanel); layout->addLayout(body,1);
    m_recentEmpty=new QLabel("No clips match these filters."); m_recentEmpty->setObjectName("clipFilterEmpty"); m_recentEmpty->setWordWrap(true); m_recentEmpty->setProperty("muted",true); recentLayout->insertWidget(recentLayout->indexOf(m_recent),m_recentEmpty); m_recentEmpty->hide();
    m_controls=new QWidget; auto *transport=new QVBoxLayout(m_controls); transport->setContentsMargins(0,0,0,0); transport->setSpacing(12);
    m_controls->setObjectName("clipControls");
    m_seek=new PlaybackSlider; m_seek->setObjectName("clipSeek"); m_seek->setAccessibleName("Playback position"); m_seek->setRange(0,0); m_seek->setSingleStep(1000); m_seek->setPageStep(5000); m_seek->setTracking(false); transport->addWidget(m_seek);
    auto *controls=new QHBoxLayout; controls->setSpacing(10);
    m_time=new QLabel("00:00 / 00:00"); m_time->setObjectName("clipTime"); m_time->setFixedWidth(220); controls->addWidget(m_time); controls->addStretch();
    auto *navigationWidget=new QWidget; auto *navigation=new QHBoxLayout(navigationWidget); navigation->setContentsMargins(0,0,0,0); navigation->setSpacing(7); controls->addWidget(navigationWidget);
    m_next=button("Next","clipNext",navigation); playerIcon(m_next,"skip-previous","Next replay (Page Up)");
    auto *rewind=button("","clipSeekBack",navigation); playerIcon(rewind,"back","Back 5 seconds (Left)");
    m_play=button("Play","clipPlayPause",navigation);
    auto *forward=button("","clipSeekForward",navigation); playerIcon(forward,"forward","Forward 5 seconds (Right)");
    m_previous=button("Previous","clipPrevious",navigation); playerIcon(m_previous,"skip-next","Previous replay (Page Down)");
    controls->addStretch();
    auto *utilitiesWidget=new QWidget; utilitiesWidget->setFixedWidth(220); auto *utilities=new QHBoxLayout(utilitiesWidget); utilities->setContentsMargins(0,0,0,0); utilities->setSpacing(6); controls->addWidget(utilitiesWidget);
    m_speed=new QComboBox; m_speed->setObjectName("clipSpeed"); m_speed->setAccessibleName("Playback speed");
    for(double speed:{.25,.5,.75,1.,1.25,1.5,2.}) m_speed->addItem(QString::number(speed)+"×",speed);
    m_speed->setCurrentIndex(3); m_speed->setFixedWidth(72); utilities->addWidget(m_speed);
    m_mute=new QPushButton; m_mute->setObjectName("clipMute"); m_mute->setCheckable(true); playerIcon(m_mute,"speaker","Mute (M)"); m_mute->setAccessibleName("Mute"); m_mute->setFixedSize(30,30); utilities->addWidget(m_mute); m_volume=new QSlider(Qt::Horizontal); m_volume->setRange(0,100); m_volume->setValue(80); m_volume->setFixedWidth(60); m_volume->setAccessibleName("Playback volume"); utilities->addWidget(m_volume);
    m_edit=button("Edit clip","clipTrim",header); m_edit->setCheckable(true);
    m_fullscreenButton=button("Fullscreen","clipFullscreen",utilities); playerIcon(m_fullscreenButton,"fullscreen","Fullscreen (F)"); transport->addLayout(controls); m_controls->setParent(m_video);
    m_audio=new ClipAudioTracks;
    m_audio->selectionChanged=[this] { m_player->setAudioTracks(m_audio->selectedIds()); };
    m_audio->layoutChanged=[this] { m_audioScroll->setFixedHeight(qMin(m_audio->visibleRowCount(),6)*52+6); };
    m_audioScroll=new QScrollArea; m_audioScroll->setObjectName("clipAudioTracks"); m_audioScroll->setWidgetResizable(true); m_audioScroll->setFrameShape(QFrame::NoFrame);
    m_audioScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); m_audioScroll->setWidget(m_audio); m_audioScroll->hide(); playback->addWidget(m_audioScroll);
    m_editing=new QWidget; auto *editing=new QVBoxLayout(m_editing); editing->setContentsMargins(0,0,0,0); editing->setSpacing(6);
    auto *videoLabel=new QLabel("VIDEO RANGE"); videoLabel->setObjectName("sectionTitle"); editing->addWidget(videoLabel);
    m_timeline=new ClipTimeline; editing->addWidget(m_timeline);
    auto *range=new QHBoxLayout; range->setSpacing(8); range->addWidget(new QLabel("Start")); m_in=new ClipTimeEdit; m_in->setObjectName("clipIn"); m_in->setAccessibleName("Trim start"); range->addWidget(m_in); auto *setIn=button("Set here","clipSetIn",range); setIn->setToolTip("Set start to playback position (I)");
    range->addSpacing(12); range->addWidget(new QLabel("End")); m_out=new ClipTimeEdit; m_out->setObjectName("clipOut"); m_out->setAccessibleName("Trim end"); range->addWidget(m_out); auto *setOut=button("Set here","clipSetOut",range); setOut->setToolTip("Set end to playback position (O)"); range->addStretch();
    for(auto *field:{m_in,m_out}) { field->setDecimals(3); field->setRange(0,0); field->setSingleStep(.1); }
    editing->addLayout(range);
    auto *audioLabel=new QLabel; audioLabel->setObjectName("clipAudioHeading"); audioLabel->hide(); editing->addWidget(audioLabel);
    connect(m_player,&MpvPlayer::tracksChanged,this,[this,audioLabel] {
        const int count=m_player->audioTracks().size();
        audioLabel->setText(QString("AUDIO TRACKS · %1 · Select tracks to keep").arg(count));
        audioLabel->setVisible(count>0);
    });
    m_mix=new QCheckBox("Merge selected audio tracks into one");
    auto *jobs=range; jobs->addSpacing(16); jobs->addWidget(m_mix); m_jobStatus=new QLabel; m_jobStatus->setTextFormat(Qt::PlainText); jobs->addWidget(m_jobStatus,1); m_cancel=button("Cancel job","clipCancel",jobs); m_export=button("Save trimmed copy…","clipExport",jobs); m_export->setToolTip("Original video quality. Cuts may start at the preceding keyframe. Selected audio stays separate unless merged."); playback->addWidget(m_editing); m_editing->hide(); m_cancel->hide();
    m_error=new QLabel; m_error->setTextFormat(Qt::PlainText); m_error->setWordWrap(true); playback->addWidget(m_error);
    connect(backButton,&QPushButton::clicked,this,&ClipPlayer::back);
    connect(m_player,&MpvPlayer::aspectChanged,this,&ClipPlayer::layoutVideo);
    connect(m_play,&QPushButton::clicked,this,[this] { m_player->pause(!m_player->paused()); showControls(); });
    connect(m_next,&QPushButton::clicked,this,[this] { navigate(-1); }); connect(m_previous,&QPushButton::clicked,this,[this] { navigate(1); });
    connect(rewind,&QPushButton::clicked,this,[this] { m_player->seek(m_player->position()-5000); });
    connect(forward,&QPushButton::clicked,this,[this] { m_player->seek(m_player->position()+5000); });
    connect(m_recent,&QListWidget::itemClicked,this,[this](QListWidgetItem *item) { const auto path=item->data(Qt::UserRole).toString(); if(path==m_player->source()) m_player->pause(!m_player->paused()); else open(path); });
    connect(m_recent,&QListWidget::itemActivated,this,[this](QListWidgetItem *item) { const auto path=item->data(Qt::UserRole).toString(); if(path!=m_player->source()) open(path); });
    connect(m_seek,&QSlider::valueChanged,m_player,&MpvPlayer::seek); connect(m_volume,&QSlider::valueChanged,m_player,&MpvPlayer::setVolume); connect(m_mute,&QPushButton::toggled,m_player,&MpvPlayer::setMuted); connect(m_mute,&QPushButton::toggled,this,[this](bool muted){ m_mute->setIcon(QIcon(muted?":/shadowless/icons/speaker-off.svg":":/shadowless/icons/speaker.svg")); });
    connect(m_seek,&QSlider::sliderMoved,this,[this](int ms) { m_time->setText(clockText(ms)+" / "+clockText(m_player->duration())); });
    connect(m_seek,&QSlider::sliderPressed,this,[this] { m_resumeAfterScrub=!m_player->paused(); m_player->pause(true); });
    connect(m_seek,&QSlider::sliderReleased,this,[this] { if(m_resumeAfterScrub) m_player->pause(false); m_resumeAfterScrub=false; });
    connect(m_speed,&QComboBox::currentIndexChanged,this,[this] { m_player->setSpeed(m_speed->currentData().toDouble()); });
    connect(m_fullscreenButton,&QPushButton::clicked,this,[this] { setFullscreen(!m_fullscreen); }); connect(m_player,&MpvPlayer::doubleClicked,this,[this] { setFullscreen(!m_fullscreen); });
    connect(m_player,&MpvPlayer::stateChanged,this,&ClipPlayer::updateState); connect(m_player,&MpvPlayer::tracksChanged,this,&ClipPlayer::updateTracks); connect(m_player,&MpvPlayer::error,this,[this](const QString &message) { m_error->setText(message); updateState(); });
    connect(m_edit,&QPushButton::toggled,this,[this, editing, playback](bool enabled) {
        if(enabled) editing->insertWidget(4,m_audioScroll);
        else playback->insertWidget(playback->indexOf(m_editing),m_audioScroll);
        m_editingEnabled=enabled; m_editing->setVisible(enabled&&!m_fullscreen); showControls();
        if(enabled) analyze(); else { m_analyzePending=false; if(!m_exporting) m_tools->stop(); }
        m_audio->setEditing(enabled); showControls();
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
        m_audioTitles.clear(); for(const auto &stream:info.audioStreams) m_audioTitles.append(stream.title);
        updateTracks();
        m_metadataReady=true; m_timeline->duration=info.durationMs; m_in->setMaximum(info.durationMs/1000.); m_out->setMaximum(info.durationMs/1000.); m_in->setValue(0); m_out->setValue(info.durationMs/1000.); updateState();
    });
    connect(m_tools,&MediaTools::thumbnailsReady,this,[this](const QVector<QImage> &images) { if(isVisible()&&m_editingEnabled&&!m_analyzePending&&m_metadataReady) { m_timeline->images=images; m_timeline->update(); } });
    connect(m_tools,&MediaTools::waveformReady,this,[this](int ordinal,const QVector<float> &peaks) {
        if(!isVisible()||!m_editingEnabled||m_analyzePending||!m_metadataReady) return;
        const auto tracks=m_player->audioTracks(); if(ordinal<0||ordinal>=tracks.size()) return;
        m_audio->setPeaks(tracks[ordinal].id,peaks);
    });
    connect(m_tools,&MediaTools::failed,this,[this](const QString &message) { m_error->setText(message); m_jobStatus->setText("Job failed"); updateState(); });
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
void ClipPlayer::setClips(const QStringList &paths,const QString &directory) {
    m_clips=paths;
    if(!directory.isEmpty()) m_clipDirectory=QDir(directory).absolutePath();
    for(const auto &path:m_thumbnails.keys()) if(!paths.contains(path)) m_thumbnails.remove(path);
    const auto selected=m_gameFilter->currentData().toString();
    const QSignalBlocker blocker(m_gameFilter);
    m_gameFilter->clear(); m_gameFilter->addItem("All games",QString());
    QStringList folders;
    for(const auto &path:paths) { const auto folder=QFileInfo(path).absolutePath(); if(!folders.contains(folder)) folders.append(folder); }
    folders.sort(Qt::CaseInsensitive);
    for(const auto &folder:folders) {
        const auto label=folder==m_clipDirectory?QString("Desktop / Unidentified"):
            m_clipDirectory.isEmpty()?QFileInfo(folder).fileName():QDir(m_clipDirectory).relativeFilePath(folder);
        m_gameFilter->addItem(label,folder);
    }
    m_gameFilter->setCurrentIndex(qMax(0,m_gameFilter->findData(selected)));
    filterClips();
}
void ClipPlayer::filterClips() {
    const auto folder=m_gameFilter->currentData().toString(),mode=m_modeFilter->currentData().toString();
    m_visibleClips.clear(); m_recent->clear(); m_highlightedSource.clear();
    for(const auto &path:m_clips) {
        const QFileInfo file(path);
        if(!folder.isEmpty() && file.absolutePath()!=folder) continue;
        const auto name=file.fileName();
        const QString type=name.startsWith("Replay_",Qt::CaseInsensitive)?"replay":
            name.startsWith("Recording_",Qt::CaseInsensitive)?"recording":"unknown";
        if(!mode.isEmpty() && mode!=type) continue;
        m_visibleClips.append(path);
        const auto icon=m_thumbnails.contains(path)?QIcon(m_thumbnails.value(path)):QIcon(":/shadowless/icons/film.svg");
        auto *item=new QListWidgetItem(icon,clipWhen(file),m_recent); item->setData(Qt::AccessibleTextRole,clipTitle(file)+" "+clipWhen(file));
        item->setData(Qt::UserRole,path); item->setToolTip(path); item->setSizeHint(QSize(0,52));
    }
    m_recentEmpty->setVisible(m_visibleClips.isEmpty());
    updateState();
}
void ClipPlayer::setThumbnail(const QString &path,const QPixmap &image) {
    if(!m_clips.contains(path)) return;
    m_thumbnails.insert(path,image);
    const int index=m_visibleClips.indexOf(path);
    if(index>=0) m_recent->item(index)->setIcon(QIcon(image));
}
void ClipPlayer::open(const QString &path) {
    if(m_exporting) return;
    m_edit->setChecked(false);
    m_analyzePending=false; m_metadataReady=false; m_tools->stop(); m_error->clear(); m_jobStatus->clear();
    m_timeline->images.clear(); m_timeline->duration=0; m_timeline->update();
    m_audio->clear();
    m_audioTitles.clear();
    if(m_metadataTools) m_metadataTools->stop();
    auto *metadata=new MediaTools(this); m_metadataTools=metadata;
    connect(metadata,&MediaTools::finished,metadata,&QObject::deleteLater);
    connect(metadata,&MediaTools::metadataReady,this,[this](const MediaInfo &info) {
        if(info.path!=QFileInfo(m_player->source()).absoluteFilePath()) return;
        m_audioTitles.clear(); for(const auto &stream:info.audioStreams) m_audioTitles.append(stream.title);
        updateTracks();
    });
    metadata->analyze(path,true);
    if(!m_clips.contains(path)) { auto paths=m_clips; paths.prepend(path); setClips(paths); }
    m_title->setText(QFileInfo(path).fileName()); m_title->setToolTip(path); m_player->open(path); m_player->setVolume(m_volume->value()); m_player->setMuted(m_mute->isChecked()); m_player->setSpeed(m_speed->currentData().toDouble());
    m_pages->setCurrentIndex(0); showControls(); updateState(); if(m_editingEnabled) analyze();
}
void ClipPlayer::navigate(int delta) { const int index=m_visibleClips.indexOf(m_player->source()); if(index>=0 && index+delta>=0 && index+delta<m_visibleClips.size()) open(m_visibleClips[index+delta]); }
void ClipPlayer::updateState() {
    m_error->setVisible(!m_fullscreen&&!m_error->text().isEmpty()); // an empty wrapped label still reserves a band under the editor
    m_player->setPlaybackRange(m_editingEnabled&&m_metadataReady?qRound64(m_in->value()*1000):0,
                              m_editingEnabled&&m_metadataReady?qRound64(m_out->value()*1000):0);
    const int index=m_visibleClips.indexOf(m_player->source()); m_next->setEnabled(index>0&&!m_exporting); m_previous->setEnabled(index>=0&&index+1<m_visibleClips.size()&&!m_exporting); m_recent->setEnabled(!m_exporting);
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
    const auto tracks=m_player->audioTracks(); QVector<QPair<int,QString>> sources;
    for(int i=0;i<tracks.size();++i) sources.append({tracks[i].id,i<m_audioTitles.size()&&!m_audioTitles[i].isEmpty()?m_audioTitles[i]:tracks[i].title});
    m_audio->setTracks(sources);
    m_audioScroll->setFixedHeight(qMin(m_audio->visibleRowCount(),6)*52+6);
    m_audioScroll->setVisible(m_editingEnabled&&!tracks.isEmpty()&&!m_fullscreen);
}
void ClipPlayer::showControls() { m_header->setVisible(!m_fullscreen); m_controls->show(); m_recentPanel->setVisible(!m_fullscreen&&!m_editingEnabled); m_edit->setVisible(!m_fullscreen); m_audioScroll->setVisible(m_editingEnabled&&!m_fullscreen&&!m_audio->empty()); m_player->unsetCursor(); if(m_fullscreen&&!m_player->paused()) m_idle.start(); else m_idle.stop(); }
void ClipPlayer::setFullscreen(bool enabled) {
    if(m_fullscreen==enabled) return;
    m_fullscreen=enabled;
    m_controls->layout()->setContentsMargins(enabled?12:0,enabled?8:0,enabled?12:0,enabled?8:0);
    m_controls->setStyleSheet(enabled?"QWidget#clipControls { background: rgba(16, 20, 22, 220); }":"");
    m_controls->raise(); layoutVideo();
    m_fullscreenButton->setAccessibleName(enabled?"Exit fullscreen":"Fullscreen"); m_fullscreenButton->setToolTip(enabled?"Exit fullscreen (Esc)":"Fullscreen (F)");
    m_editing->setVisible(!enabled&&m_editingEnabled); m_error->setVisible(!enabled&&!m_error->text().isEmpty()); showControls(); emit fullscreenChanged(enabled);
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
        request.audioTracks=m_audio->selectedOrdinals();
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
// Video and controls sit together, centered, so a tall window does not leave a gap between them.
void ClipPlayer::layoutVideo() {
    const QSize area=m_video->size(); const int gap=10, ch=m_controls->sizeHint().height();
    if(m_fullscreen) { m_player->setGeometry(0,0,area.width(),area.height()); m_controls->setGeometry(0,area.height()-ch,area.width(),ch); m_controls->raise(); return; }
    const double aspect=m_player->aspect(); const int availH=qMax(1,area.height()-ch-gap);
    int w=area.width(), h=qRound(w/aspect); if(h>availH) { h=availH; w=qRound(h*aspect); }
    const int top=(area.height()-(h+gap+ch))/2, cw=qMin(area.width(),qMax(w,m_controls->sizeHint().width()));
    m_player->setGeometry((area.width()-w)/2,top,w,h);
    m_controls->setGeometry((area.width()-cw)/2,top+h+gap,cw,ch);
}
bool ClipPlayer::eventFilter(QObject *watched,QEvent *event) {
    if(watched==m_video&&event->type()==QEvent::Resize) layoutVideo();
    auto *widget=qobject_cast<QWidget *>(watched); if(!isVisible()||!widget||(widget!=this&&!isAncestorOf(widget))) return QWidget::eventFilter(watched,event);
    if(event->type()==QEvent::MouseMove||event->type()==QEvent::MouseButtonPress) showControls();
    if(event->type()!=QEvent::KeyPress||m_pages->currentIndex()!=0) return QWidget::eventFilter(watched,event);
    if(qobject_cast<QLineEdit *>(widget)||qobject_cast<QAbstractSpinBox *>(widget)||qobject_cast<QComboBox *>(widget)) return QWidget::eventFilter(watched,event);
    const auto *key=static_cast<QKeyEvent *>(event); if(key->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier)) return false;
    if(key->key()==Qt::Key_Space && (qobject_cast<QCheckBox *>(widget)||widget==m_mute||qobject_cast<QAbstractItemView *>(widget))) return false;
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
