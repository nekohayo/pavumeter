/* $Id$ */

#include <signal.h>

#include <gtkmm.h>
#include <polyp/polyplib-context.h>
#include <polyp/polyplib-stream.h>
#include <polyp/polyplib-error.h>
#include <polyp/polyplib-introspect.h>
#include <polyp/glib-mainloop.h>

class MainWindow : public Gtk::Window {

public:
    MainWindow(unsigned chan);
    virtual ~MainWindow();
    
protected:

    class ChannelInfo {
    public:
        ChannelInfo(MainWindow &w, const Glib::ustring &l);
        Gtk::Label *label;
        Gtk::ProgressBar *progress;
    };
    
    Gtk::Table table;
    std::vector<ChannelInfo*> channels;

    float *levels;

    virtual void addChannel(const Glib::ustring &l);
    virtual bool on_delete_event(GdkEventAny* e);
    virtual bool on_display_timeout();
    virtual bool on_calc_timeout();

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

MainWindow::MainWindow(unsigned nchan) :
    Gtk::Window(),
    table(1, 2),
    latency(0) {

    g_assert(nchan > 0);
    
    set_border_width(12);
    set_title("Volume Meter");
    table.set_row_spacings(6);
    table.set_col_spacings(12);
    add(table);

    if (nchan == 2) {
        addChannel("<b>Left:</b>");
        addChannel("<b>Right:</b>");
    } else if (nchan == 1)
        addChannel("<b>Level:</b>");
    else {
        for (unsigned i = 1; i <= nchan; i++) {
            char t[40];
            snprintf(t, sizeof(t), "<b>Channel #%u:</b>", i);
            addChannel(t);
        }
    }

    g_assert(channels.size() == nchan);

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
    return false;
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
        ChannelInfo *c = channels[n];
        c->progress->set_fraction(i.levels[n]);
    }

}

bool MainWindow::on_display_timeout() {
    LevelInfo *i = NULL;

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
static char* source_name = NULL;
static uint32_t sink_index = PA_INVALID_INDEX;

static void context_get_sink_info_callback(struct pa_context *c, const struct pa_sink_info *si, int is_last, void *) {
    if (is_last < 0) {
        g_message("Failed to get latency information: %s", pa_strerror(pa_context_errno(c)));
        Gtk::Main::quit();
        return;
    }

    if (!si)
        return;

    if (mainWindow)
        mainWindow->updateLatency(si->latency);
}

static gboolean latency_func(gpointer) {
    if (!stream)
        return false;

    pa_operation_unref(pa_context_get_sink_info_by_index(context, sink_index, context_get_sink_info_callback, NULL));
    return true;
}

static void stream_read_callback(struct pa_stream *, const void *p, size_t l, void *) {
    g_assert(mainWindow);

    mainWindow->pushData((const float*) p, l/sizeof(float));
}

static void stream_state_callback(struct pa_stream *s, void *) {
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_DISCONNECTED:
        case PA_STREAM_CREATING:
            break;

        case PA_STREAM_READY:
            g_assert(!mainWindow);
            mainWindow = new MainWindow(sample_spec.channels);
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

    if (si->monitor_of_sink != PA_INVALID_INDEX) {
        g_timeout_add(100, latency_func, NULL);
        sink_index = si->monitor_of_sink;
        pa_operation_unref(pa_context_get_sink_info_by_index(context, sink_index, context_get_sink_info_callback, NULL));
    }

    pa_sample_spec_snprint(t, sizeof(t), &sample_spec);
    g_message("Using sample format: %s", t);

    stream = pa_stream_new(c, "vumeter", &sample_spec);
    pa_stream_set_state_callback(stream, stream_state_callback, NULL);
    pa_stream_set_read_callback(stream, stream_read_callback, NULL);
    pa_stream_connect_record(stream, source_name, NULL);
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

    m = pa_glib_mainloop_new(g_main_context_default());
    g_assert(m);

    context = pa_context_new(pa_glib_mainloop_get_api(m), "vumeter");
    g_assert(m);

    pa_context_set_state_callback(context, context_state_callback, NULL);
    pa_context_connect(context, NULL);

    Gtk::Main::run();

    if (stream)
        pa_stream_unref(stream);
    if (context)
        pa_context_unref(context);

    if (mainWindow)
        delete mainWindow;

    pa_glib_mainloop_free(m);
    
    return 0;
}
