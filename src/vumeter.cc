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
    virtual bool on_timeout();

    sigc::connection timeout_signal_connection;

public:
    virtual void pushData(const float *d, size_t l);
    virtual void showLevels();
};

MainWindow::MainWindow(unsigned nchan) :
    Gtk::Window(),
    table(1, 2) {

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

    levels = new float[nchan];
    for (unsigned c = 0; c < nchan; c++)
        levels[c] = 0;
    
    timeout_signal_connection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &MainWindow::on_timeout), 50);
    
    show_all();
}

MainWindow::~MainWindow() {
    while (channels.size() > 0) {
        ChannelInfo *i = channels.back();
        channels.pop_back();
        delete i;
    }

    delete[] levels;
    timeout_signal_connection.disconnect();
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
    progress->set_fraction(0.5);
    
    w.table.resize(w.channels.size()+1, 2);
    w.table.attach(*label, 0, 1, w.channels.size(), w.channels.size()+1, Gtk::FILL, (Gtk::AttachOptions) 0);
    w.table.attach(*progress, 1, 2, w.channels.size(), w.channels.size()+1, Gtk::EXPAND|Gtk::FILL, (Gtk::AttachOptions) 0);
}


void MainWindow::pushData(const float *d, unsigned samples) {
    unsigned nchan = channels.size();

    g_assert(levels);
    
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

void MainWindow::showLevels() {
    unsigned nchan = channels.size();

    g_assert(levels);
    
    for (unsigned c = 0; c < nchan; c++) {
        ChannelInfo *i = channels[c];
        i->progress->set_fraction(levels[c]);
    }

    for (unsigned c = 0; c < nchan; c++)
        levels[c] = 0;
}


bool MainWindow::on_timeout() {
    showLevels();
    return true;
}

static MainWindow *mainWindow = NULL;
static struct pa_context *context = NULL;
static struct pa_stream *stream = NULL;
static struct pa_sample_spec sample_spec = { (enum pa_sample_format) 0, 0, 0 };    
static char* sink_name = NULL;

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

static void context_get_sink_info_callback(struct pa_context *c, const struct pa_sink_info *si, int is_last, void *) {
    char t[256];
    
    if (is_last)
        return;
    
    if (!si) {
        g_message("Failed to get sink information: %s", pa_strerror(pa_context_errno(context)));
        Gtk::Main::quit();
        return;
    }

    sample_spec.format = PA_SAMPLE_FLOAT32;
    sample_spec.rate = si->sample_spec.rate;
    sample_spec.channels = si->sample_spec.channels;

    pa_sample_spec_snprint(t, sizeof(t), &sample_spec);
    g_message("Using sample format: %s", t);

    stream = pa_stream_new(c, "vumeter", &sample_spec);
    pa_stream_set_state_callback(stream, stream_state_callback, NULL);
    pa_stream_set_read_callback(stream, stream_read_callback, NULL);
    pa_stream_connect_record(stream, NULL, NULL);
}

static void context_get_server_info_callback(struct pa_context *c, const struct pa_server_info*si, void *) {
    if (!si) {
        g_message("Failed to get server information: %s", pa_strerror(pa_context_errno(context)));
        Gtk::Main::quit();
        return;
    }

    sink_name = g_strdup(si->default_sink_name);
    pa_operation_unref(pa_context_get_sink_info_by_name(c, sink_name, context_get_sink_info_callback, NULL));
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

            if (sink_name)
                pa_operation_unref(pa_context_get_sink_info_by_name(c, sink_name, context_get_sink_info_callback, NULL));
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
