#include <signal.h>

#include <gtkmm.h>
#include <polyp/polyplib-context.h>
#include <polyp/polyplib-stream.h>
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

    virtual void addChannel(const Glib::ustring &l);

public:
    virtual void pushData(const float *d, size_t l);
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
    
    show_all();
}

MainWindow::~MainWindow() {
    while (channels.size() > 0) {
        ChannelInfo *i = channels.back();
        channels.pop_back();
        delete i;
    }
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
    float *max;
    unsigned nchan = channels.size();

    max = (float*) g_malloc(sizeof(float)*nchan);
    g_assert(max);

    for (unsigned c = 0; c < nchan; c++)
        max[c] = 0;
    
    while (samples >= nchan) {

        for (unsigned c = 0; c < nchan; c++) {
            float v = fabs(d[c]);
            if (v > max[c])
                max[c] = v;
        }

        d += nchan;
        samples -= nchan;
    }

    for (unsigned c = 0; c < nchan; c++) {
        ChannelInfo *i = channels[c];
        i->progress->set_fraction(max[c]);
    }

    g_free(max);
}

static MainWindow *mainWindow = NULL;
static struct pa_context *context = NULL;
static struct pa_stream *stream = NULL;
static const struct pa_sample_spec sample_spec = {
    PA_SAMPLE_FLOAT32, 44100, 2
};    

static void stream_read_callback(struct pa_stream *, const void *p, size_t l, void *) {
    g_assert(mainWindow);

/*    mainWindow->pushData((const float*) p, l/sizeof(float));*/
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
        case PA_STREAM_TERMINATED:
            Gtk::Main::quit();
    }
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
            stream = pa_stream_new(c, "vumeter", &sample_spec);
            pa_stream_set_state_callback(stream, stream_state_callback, NULL);
            pa_stream_set_read_callback(stream, stream_read_callback, NULL);
            pa_stream_connect_record(stream, "input", NULL);
            break;
            
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
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
