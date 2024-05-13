TARGETS  += proximity
TARGETS  += allsensors
PACKAGES += sensors-glib
CFLAGS   += -std=c99 -Wall
CFLAGS   += $(shell pkg-config --cflags $(PACKAGES))
LDLIBS   += $(shell pkg-config --libs $(PACKAGES))

build:: $(TARGETS)

mostlyclean::
	$(RM) *.o *~ *.bak

clean:: mostlyclean
	$(RM) $(TARGETS)

distclean:: clean
