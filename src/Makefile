
all: pavumeter

pavumeter: vumeter.cc
	$(CXX) $^ -o $@ `pkg-config gtkmm-2.4 --cflags --libs` -I ../../polypaudio -lpolyp -lpolyp-mainloop-glib -L../../polypaudio/polyp/.libs -Wall -W -pipe -O0 -g

clean:
	rm -f *.o pavumeter
