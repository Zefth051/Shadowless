#pragma once
#include <QPainter>
#include <QMessageBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMap>
#include <QSet>
#include <QSignalBlocker>
#include <functional>

namespace Shadowless {
class ClipWaveform : public QWidget {
public:
    QVector<float> peaks;
    explicit ClipWaveform(QWidget *parent=nullptr):QWidget(parent) { setFixedHeight(32); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); painter.setPen(Qt::NoPen);
        painter.setBrush(window()->palette().color(QPalette::Base)); painter.drawRoundedRect(rect(),5,5);
        painter.setPen(isEnabled()?window()->palette().color(QPalette::Link):window()->palette().color(QPalette::Mid));
        painter.drawLine(0,height()/2,width(),height()/2);
        for(int i=0;i<peaks.size();++i) {
            const int x=i*width()/peaks.size();
            const int height=qRound(qBound(0.f,peaks[i]*3.f,1.f)*(this->height()/2.-1));
            painter.drawLine(x,this->height()/2-height,x,this->height()/2+height);
        }
    }
};
class ClipAudioTrack : public QWidget {
    class TrackButton : public QCheckBox {
    public:
        using QCheckBox::QCheckBox;
    protected:
        bool hitButton(const QPoint &point) const override { return rect().contains(point); }
        void nextCheckState() override {
            if(guard&&!guard(checkState())) return;
            setCheckState(checkState()==Qt::Checked?Qt::Unchecked:Qt::Checked);
        }
    public:
        std::function<bool(Qt::CheckState)> guard;
    };
public:
    TrackButton *selector;
    ClipWaveform *waveform;
    explicit ClipAudioTrack(const QString &title,QWidget *parent=nullptr):QWidget(parent) {
        setObjectName("clipAudioRow");
        auto *line=new QHBoxLayout(this); line->setContentsMargins(6,4,6,4); line->setSpacing(12);
        selector=new TrackButton(title); selector->setProperty("clipAudioTrack",true);
        selector->setFixedSize(190,32); selector->setCursor(Qt::PointingHandCursor); selector->setChecked(true);
        line->addWidget(selector);
        waveform=new ClipWaveform; line->addWidget(waveform,1);
        line->addStretch();
    }
};
// Group controls project the same physical stream selection used by playback and export.
class ClipAudioTracks : public QWidget {
public:
    std::function<void()> selectionChanged, layoutChanged;
    // A group whose members were changed individually asks before re-enabling every member.
    std::function<bool(const QString &)> confirmEnableAll=[](const QString &group) {
        QMessageBox box(QMessageBox::Question,"Enable all tracks",
            QString("Some %1 tracks are turned off. Clicking %1 will turn all of its tracks on.").arg(group),
            QMessageBox::Ok|QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel); return box.exec()==QMessageBox::Ok;
    };
    explicit ClipAudioTracks(QWidget *parent=nullptr):QWidget(parent) {
        layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(6); layout->setAlignment(Qt::AlignTop);
        for(const auto &name:{QString("PC"),QString("Game"),QString("Microphone")}) {
            auto *row=new ClipAudioTrack(name); groups.insert(name,row); layout->addWidget(row);
            row->selector->setObjectName("clipGroup"+name); row->selector->setAccessibleName(name);
            row->selector->setTristate(true);
            row->selector->guard=[this,name](Qt::CheckState state) { return state!=Qt::PartiallyChecked||confirmEnableAll(name); };
            connect(row->selector,&QCheckBox::clicked,this,[this,name,sel=row->selector](bool) {
                if(sel->checkState()==Qt::PartiallyChecked) return; // confirmation was cancelled
                const bool checked=sel->checkState()==Qt::Checked;
                for(const auto &track:order) if(group(track.second)==name) {
                    touched.insert(track.first); const QSignalBlocker blocker(rows[track.first]->selector);
                    rows[track.first]->selector->setChecked(checked);
                }
                refresh(); if(selectionChanged) selectionChanged();
            });
        }
        more=new QPushButton("More tracks"); more->setObjectName("clipMoreTracks"); more->setCheckable(true);
        more->setAccessibleName("More tracks"); more->setToolTip("Show individual recorded sources. Applications sharing a name share one source.");
        layout->addWidget(more);
        connect(more,&QPushButton::toggled,this,[this] { refresh(); });
        refresh();
    }
    void clear() {
        for(auto *row:rows) delete row;
        rows.clear(); order.clear(); touched.clear(); more->setChecked(false); refresh();
    }
    void setTracks(const QVector<QPair<int,QString>> &tracks) {
        const auto before=selectedIds(); order=tracks;
        for(int id:rows.keys()) {
            bool found=false; for(const auto &track:tracks) if(track.first==id) found=true;
            if(!found) { delete rows.take(id); touched.remove(id); }
        }
        for(int i=0;i<tracks.size();++i) {
            const auto &track=tracks[i]; auto *row=rows.value(track.first);
            if(!row) {
                row=new ClipAudioTrack(track.second); rows.insert(track.first,row); layout->addWidget(row);
                connect(row->selector,&QCheckBox::toggled,this,[this,id=track.first] {
                    touched.insert(id); refresh(); if(selectionChanged) selectionChanged();
                });
            }
            auto *check=row->selector; const QSignalBlocker blocker(check);
            check->setObjectName(QString("clipTrack%1").arg(i)); check->setAccessibleName(track.second);
            check->setText(fontMetrics().elidedText(track.second,Qt::ElideRight,150));
            check->setToolTip(track.second+"\nInclude in playback and export");
            if(!touched.contains(track.first)) check->setChecked(!track.second.startsWith("App (off): ")&&!track.second.startsWith("Game (off): "));
        }
        refresh(); if(before!=selectedIds() && selectionChanged) selectionChanged();
    }
    QVector<int> selectedIds() const {
        QVector<int> ids; for(const auto &track:order) if(rows.value(track.first)&&rows[track.first]->selector->isChecked()&&!ids.contains(track.first)) ids.append(track.first);
        return ids;
    }
    QVector<int> selectedOrdinals() const {
        QVector<int> ordinals; const auto selected=selectedIds();
        for(int i=0;i<order.size();++i) if(selected.contains(order[i].first)) ordinals.append(i);
        return ordinals;
    }
    void setEditing(bool enabled) { editing=enabled; refresh(); }
    void setPeaks(int id,const QVector<float> &peaks) { if(rows.contains(id)) { rows[id]->waveform->peaks=peaks; refresh(); } }
    int visibleRowCount() const {
        int count=dynamic?3+(editing?1:0):0; for(const auto &track:order) if(!dynamic||group(track.second).isEmpty()||(editing&&more->isChecked()&&groupSize(group(track.second))>1&&track.second!="PC (other)")) ++count;
        return count;
    }
    bool empty() const { return order.isEmpty(); }
private:
    QVBoxLayout *layout;
    QMap<int,ClipAudioTrack *> rows;
    QMap<QString,ClipAudioTrack *> groups;
    QVector<QPair<int,QString>> order;
    QSet<int> touched;
    QPushButton *more;
    bool dynamic=false,editing=false;
    int groupSize(const QString &g) const { int n=0; for(const auto &t:order) if(group(t.second)==g) ++n; return n; }
    static QString group(const QString &title) {
        if(title=="PC (other)"||title.startsWith("App: ")||title.startsWith("App (off): ")) return "PC";
        if(title.startsWith("Game: ")||title.startsWith("Game (off): ")) return "Game";
        if(title=="Microphone") return "Microphone";
        return {};
    }
    void refresh() {
        dynamic=false;
        for(const auto &track:order) if(!group(track.second).isEmpty()&&track.second!="Microphone") dynamic=true;
        for(const auto &name:groups.keys()) {
            auto *row=groups[name]; int count=0,selected=0; QVector<float> peaks;
            for(const auto &track:order) if(group(track.second)==name) {
                ++count; auto *child=rows.value(track.first); if(!child||!child->selector->isChecked()) continue;
                ++selected; const auto &source=child->waveform->peaks; if(peaks.size()<source.size()) peaks.resize(source.size());
                for(int i=0;i<source.size();++i) peaks[i]=qMin(1.f,peaks[i]+source[i]);
            }
            const QSignalBlocker blocker(row->selector);
            row->selector->setCheckState(selected==0?Qt::Unchecked:selected==count?Qt::Checked:Qt::PartiallyChecked);
            row->selector->setEnabled(count>0); row->selector->setToolTip(count?"Include this group in playback and export":"No audio recorded for this group");
            row->setVisible(dynamic); row->waveform->setVisible(true); row->waveform->setEnabled(selected>0); row->waveform->peaks=peaks; row->waveform->update();
        }
        more->setVisible(dynamic&&editing);
        for(const auto &track:order) if(auto *row=rows.value(track.first)) {
            const auto g=group(track.second); int siblings=0; for(const auto &other:order) if(group(other.second)==g) ++siblings;
            // A group with a single source is already represented by its group row; do not list it twice.
            row->setVisible(!dynamic||g.isEmpty()||(editing&&more->isChecked()&&siblings>1&&track.second!="PC (other)"));
            row->waveform->setVisible(true); row->waveform->setEnabled(row->selector->isChecked()); row->waveform->update();
        }
        if(layoutChanged) layoutChanged();
    }
};
}
