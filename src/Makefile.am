desktopdir = $(datadir)/applications

bin_PROGRAMS=pavumeter

pavumeter_SOURCES=vumeter.cc

pavumeter_LDADD=$(AM_LDADD) $(GUILIBS_LIBS) $(POLYP_LIBS)
pavumeter_CXXFLAGS=$(AM_CXXFLAGS) $(GUILIBS_CFLAGS) $(POLYP_CFLAGS) 

desktop_DATA=pavumeter.desktop
