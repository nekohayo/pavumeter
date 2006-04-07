/* $Id$ */

/***
  This file is part of pavumeter.
 
  pavumeter is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.
 
  pavumeter is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public License
  along with pavumeter; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <signal.h>

#include <gtkmm.h>
#include <polyp/polypaudio.h>
#include <polyp/glib-mainloop.h>

#define LOGARITHMIC 1


class MainWindow : public Gtk::Window {

public:
    MainWindow(const pa_channel_map &map, const char *source_name);
    virtual ~MainWindow();
    
protected:

    class ChannelInfo {
    public:
        ChannelInfo(MainWindow &w, const Glib::ustring &l);
        Gtk::Label *label;
        Gtk::ProgressBar *progress;
    };

    Gtk::VBox vbox, titleVBox;
    Gtk::Table table;
    std::vector<ChannelInfo*> channels;
    Gtk::Label titleLabel;
    Gtk::Label subtitleLabel;
    Gtk::HSeparator separator;
    Gtk::EventBox eventBox;

    float *levels;

    virtual void addChannel(const Glib::ustring &l);
     virtual bool on_delete_event(GdkEventAny* e);
    virtual bool on_display_timeout();
    virtual bool on_calc_timeout();
    virtual void decayLevels();

    sigc::connection display_timeout_signal_connection;
    sigc::connection calc_timeout_signal_connection;
    pa_usec_t latency;

    class LevelInfo {
    public:
        LevelInfo(float *levels, pa_usec_t l);
        virtual ~LevelInfo();
        bool elapsed();

        struct timeval tv;
        float *levels;
    };

    std::deque<LevelInfo *> levelQueue;

public:
    virtual void pushData(const float *d, size_t l);
    virtual void showLevels(const LevelInfo& i);
    virtual void updateLatency(pa_usec_t l);
};

MainWindow::MainWindow(const pa_channel_map &map, const char *source_name) :
    Gtk::Window(),
    table(1, 2),
    latency(0) {

    char t[256];
    int n;

    set_title("Volume Meter");

    add(vbox);

    Gdk::Color c("white");
    eventBox.modify_bg(Gtk::STATE_NORMAL, c);

    vbox.pack_start(eventBox, false, false);

    eventBox.add(titleVBox);
    titleVBox.add(titleLabel);
    titleVBox.add(subtitleLabel);
    titleVBox.set_border_width(12);
    titleVBox.set_spacing(6);

    titleLabel.set_markup("<span size=\"18000\" color=\"black\"><b>Polypaudio Volume Meter</b></span>");
    titleLabel.set_alignment(0);
    snprintf(t, sizeof(t), "Showing signal levels of source <b>%s</b>.", source_name);
    subtitleLabel.set_markup(t);
    subtitleLabel.set_alignment(0);
    
    vbox.pack_start(separator, false, false);

    table.set_border_width(12);
    table.set_row_spacings(6);
    table.set_col_spacings(12);
    vbox.pack_start(table, true, true);

    for (n = 0; n < map.channels; n++) {
        snprintf(t, sizeof(t), "<b>%s:</b>", pa_channel_position_to_string(map.map[n]));
        addChannel(t);
    }

    g_assert(channels.size() == map.channels);

    levels = NULL;
    display_timeout_signal_connection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &MainWindow::on_display_timeout), 10);
    calc_timeout_signal_connection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &MainWindow::on_calc_timeout), 50);
    
    show_all();
}

MainWindow::~MainWindow() {
    while (channels.size() > 0) {
        ChannelInfo *i = channels.back();
        channels.pop_back();
        delete i;
    }

    while (levelQueue.size() > 0) {
        LevelInfo *i = levelQueue.back();
        levelQueue.pop_back();
        delete i;
    }
    
    if (levels)
        delete[] levels;
    
    display_timeout_signal_connection.disconnect();
    calc_timeout_signal_connection.disconnect();
}

bool MainWindow::on_delete_event(GdkEventAny*) {
    Gtk::Main::quit();
    return true;
}

void MainWindow::addChannel(const Glib::ustring &l) {



    channels.push_back(new ChannelInfo(*this, l));
}

MainWindow::ChannelInfo::ChannelInfo(MainWindow &w, const Glib::ustring &l) {
    label = Gtk::manage(new Gtk::Label(l, 1.0, 0.5));
    label->set_markup(l);

    progress = Gtk::manage(new Gtk::ProgressBar());
    progress->set_fraction(0);
    
    w.table.resize(w.channels.size()+1, 2);
    w.table.attach(*label, 0, 1, w.channels.size(), w.channels.size()+1, Gtk::FILL, (Gtk::AttachOptions) 0);
    w.table.attach(*progress, 1, 2, w.channels.size(), w.channels.size()+1, Gtk::EXPAND|Gtk::FILL, (Gtk::AttachOptions) 0);
}

void MainWindow::pushData(const float *d, unsigned samples) {
    unsigned nchan = channels.size();

    if (!levels) {
        levels = new float[nchan];
        for (unsigned c = 0; c < nchan; c++)
            levels[c] = 0;
    }
    
    while (samples >= nchan) {

        for (unsigned c = 0; c < nchan; c++) {
            float v = fabs(d[c]);
            if (v > levels[c])
                levels[c] = v;
        }

        d += nchan;
        samples -= nchan;
    }
}

void MainWindow::showLevels(const LevelInfo &i) {
    unsigned nchan = channels.size();

    g_assert(i.levels);
    
    for (unsigned n = 0; n < nchan; n++) {
        double level;
        ChannelInfo *c = channels[n];

        level = i.levels[n];

#ifdef LOGARITHMIC            
        level = log10(level*9+1);
#endif
        
        c->progress->set_fraction(level > 1 ? 1 : level);
    }

}

#define DECAY_LEVEL (0.005)

void MainWindow::decayLevels() {
    unsigned nchan = channels.size();

    for (unsigned n = 0; n < nchan; n++) {
        double level;

        ChannelInfo *c = channels[n];

        level = c->progress->get_fraction();

        if (level <= 0)
            continue;

        level = level > DECAY_LEVEL ? level - DECAY_LEVEL : 0;
        c->progress->set_fraction(level);
    }
}

bool MainWindow::on_display_timeout() {
    LevelInfo *i = NULL;

    if (levelQueue.empty()) {
        decayLevels();
        return true;
    }
    
    while (levelQueue.size() > 0) {
        if (i)
            delete i;
        
        i = levelQueue.back();
        levelQueue.pop_back();

        if (!i->elapsed())
            break;
    }

    if (i) {
        showLevels(*i);
        delete i;
    }
    
    return true;
}

bool MainWindow::on_calc_timeout() {
    if (levels) {
        levelQueue.push_front(new LevelInfo(levels, latency));
        levels = NULL;
    }

    return true;
}

void MainWindow::updateLatency(pa_usec_t l) {
    latency = l;
}

static void timeval_add_usec(struct timeval *tv, pa_usec_t v) {
    uint32_t sec = v/1000000;
    tv->tv_sec += sec;
    v -= sec*1000000;
    
    tv->tv_usec += v;

    while (tv->tv_usec >= 1000000) {
        tv->tv_sec++;
        tv->tv_usec -= 1000000;
    }
}

MainWindow::LevelInfo::LevelInfo(float *l, pa_usec_t latency) {
    levels = l;
    gettimeofday(&tv, NULL);
    timeval_add_usec(&tv, latency);
}

MainWindow::LevelInfo::~LevelInfo() {
    delete[] levels;
}

bool MainWindow::LevelInfo::elapsed() {
    struct timeval now;
    gettimeofday(&now, NULL);

    if (now.tv_sec != tv.tv_sec)
        return now.tv_sec > tv.tv_sec;

    return now.tv_usec >= tv.tv_usec;
}

static MainWindow *mainWindow = NULL;
static struct pa_context *context = NULL;
static struct pa_stream *stream = NULL;
static struct pa_sample_spec sample_spec = { (enum pa_sample_format) 0, 0, 0 };    
static struct pa_channel_map channel_map;
static char* source_name = NULL;

static void stream_update_timing_info_callback(struct pa_stream *s, int success, void *) {
    pa_usec_t t;
    int negative = 0;
    
    if (!success || pa_stream_get_latency(s, &t, &negative) < 0) {
        g_message("Failed to get latency information: %s", pa_strerror(pa_context_errno(context)));
        Gtk::Main::quit();
        return;
    }

    if (!mainWindow)
        return;

    mainWindow->updateLatency(negative ? 0 : t);
}

static gboolean latency_func(gpointer) {
    pa_operation *o;
    
    if (!stream)
        return false;

    if (!(o = pa_stream_update_timing_info(stream, stream_update_timing_info_callback, NULL)))
        g_message("pa_stream_update_timing_info() failed: %s", pa_strerror(pa_context_errno(context)));
    else
        pa_operation_unref(o);
    
    return true;
}

static void stream_read_callback(struct pa_stream *s, size_t l, void *) {
    const void *p;
    g_assert(mainWindow);

    if (pa_stream_peek(s, &p, &l) < 0) {
        g_message("pa_stream_peek() failed: %s", pa_strerror(pa_context_errno(context)));
        return;
    }
    
    mainWindow->pushData((const float*) p, l/sizeof(float));

    pa_stream_drop(s);
}

static void stream_state_callback(struct pa_stream *s, void *) {
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;

        case PA_STREAM_READY:
            g_assert(!mainWindow);
            mainWindow = new MainWindow(channel_map, source_name);

            g_timeout_add(100, latency_func, NULL);
            pa_operation_unref(pa_stream_update_timing_info(stream, stream_update_timing_info_callback, NULL));
            break;
            
        case PA_STREAM_FAILED:
            g_message("Connection failed: %s", pa_strerror(pa_context_errno(context)));
            
        case PA_STREAM_TERMINATED:
            Gtk::Main::quit();
    }
}

static void context_get_source_info_callback(struct pa_context *c, const struct pa_source_info *si, int is_last, void *) {
    char t[256];

    if (is_last < 0) {
        g_message("Failed to get source information: %s", pa_strerror(pa_context_errno(context)));
        Gtk::Main::quit();
        return;
    }

    if (!si)
        return;

    sample_spec.format = PA_SAMPLE_FLOAT32;
    sample_spec.rate = si->sample_spec.rate;
    sample_spec.channels = si->sample_spec.channels;

    channel_map = si->channel_map;
    
    g_message("Using sample format: %s", pa_sample_spec_snprint(t, sizeof(t), &sample_spec));
    g_message("Using channel map: %s", pa_channel_map_snprint(t, sizeof(t), &channel_map));

    stream = pa_stream_new(c, "vumeter", &sample_spec, &channel_map);
    pa_stream_set_state_callback(stream, stream_state_callback, NULL);
    pa_stream_set_read_callback(stream, stream_read_callback, NULL);
    pa_stream_connect_record(stream, source_name, NULL, (enum pa_stream_flags) 0);
}

static void context_get_server_info_callback(struct pa_context *c, const struct pa_server_info*si, void *) {
    if (!si) {
        g_message("Failed to get server information: %s", pa_strerror(pa_context_errno(context)));
        Gtk::Main::quit();
        return;
    }

    if (!*si->default_source_name) {
        g_message("No default source set.");
        Gtk::Main::quit();
        return;
    }    
    
    source_name = g_strdup(si->default_source_name);
    pa_operation_unref(pa_context_get_source_info_by_name(c, source_name, context_get_source_info_callback, NULL));
}

static void context_state_callback(struct pa_context *c, void *) {
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            g_assert(!stream);

            if (source_name)
                pa_operation_unref(pa_context_get_source_info_by_name(c, source_name, context_get_source_info_callback, NULL));
            else
                pa_operation_unref(pa_context_get_server_info(c, context_get_server_info_callback, NULL));
            
            break;
            
        case PA_CONTEXT_FAILED:
            g_message("Connection failed: %s", pa_strerror(pa_context_errno(context)));
            
        case PA_CONTEXT_TERMINATED:
            Gtk::Main::quit();
    }
}

int main(int argc, char *argv[]) {
    struct pa_glib_mainloop *m;

    signal(SIGPIPE, SIG_IGN);

    Gtk::Main kit(argc, argv);

    if (argc > 1)
        source_name = g_strdup(argv[1]);
    else {
        char *e = getenv("POLYP_SOURCE");
        if (e)
            source_name = g_strdup(e);
    }

    if (source_name)
        g_message("Using source '%s'", source_name);

    m = pa_glib_mainloop_new(g_main_context_default());
    g_assert(m);

    context = pa_context_new(pa_glib_mainloop_get_api(m), "vumeter");
    g_assert(m);

    pa_context_set_state_callback(context, context_state_callback, NULL);
    pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

    Gtk::Main::run();

    if (stream)
        pa_stream_unref(stream);
    if (context)
        pa_context_unref(context);

    if (mainWindow)
        delete mainWindow;

    if(source_name)
        g_free(source_name);
    
    pa_glib_mainloop_free(m);
    
    return 0;
}
