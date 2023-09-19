# ----------------------------------------------------------- -*- mode: sh -*-
# Package Configuration
# ----------------------------------------------------------------------------

NAME    ?= sensors-glib
VERSION ?= 1.0.0
DESTDIR ?= test-install-$(NAME)

# Library SONAME

SOMAJOR     := .1
SOMINOR     := .0
SORELEASE   := .0

EXT_LINKLIB := .so
EXT_SONAME  := .so$(SOMAJOR)
EXT_LIBRARY := .so$(SOMAJOR)$(SOMINOR)$(SORELEASE)

# ----------------------------------------------------------------------------
# Installation directories
# ----------------------------------------------------------------------------

# Standard directories
_PREFIX         ?= /usr#                         # /usr
_INCLUDEDIR     ?= $(_PREFIX)/include#           # /usr/include
_EXEC_PREFIX    ?= $(_PREFIX)#                   # /usr
_BINDIR         ?= $(_EXEC_PREFIX)/bin#          # /usr/bin
_SBINDIR        ?= $(_EXEC_PREFIX)/sbin#         # /usr/sbin
_LIBEXECDIR     ?= $(_EXEC_PREFIX)/libexec#      # /usr/libexec
_LIBDIR         ?= $(_EXEC_PREFIX)/lib#          # /usr/lib
_SYSCONFDIR     ?= /etc#                         # /etc
_DATADIR        ?= $(_PREFIX)/share#             # /usr/share
_MANDIR         ?= $(_DATADIR)/man#              # /usr/share/man
_INFODIR        ?= $(_DATADIR)/info#             # /usr/share/info
_DEFAULTDOCDIR  ?= $(_DATADIR)/doc#              # /usr/share/doc
_LOCALSTATEDIR  ?= /var#                         # /var
_UNITDIR        ?= /lib/systemd/system#
_TESTSDIR       ?= /opt/tests#                   # /opt/tests

# Package directories
VARDIR          := $(_LOCALSTATEDIR)/lib/$(NAME)
RUNDIR          := $(_LOCALSTATEDIR)/run/$(NAME)
CACHEDIR        := $(_LOCALSTATEDIR)/cache/$(NAME)
CONFDIR         := $(_SYSCONFDIR)/$(NAME)
MODULEDIR       := $(_LIBDIR)/$(NAME)/modules
DBUSDIR         := $(_SYSCONFDIR)/dbus-1/system.d
HELPERSCRIPTDIR := $(_DATADIR)/$(NAME)
TESTSDESTDIR    := $(_TESTSDIR)/$(NAME)

# ----------------------------------------------------------------------------
# Targets to build / install
# ----------------------------------------------------------------------------

TARGETS_DSO    += lib$(NAME)$(EXT_LIBRARY)
TARGETS_DSO    += lib$(NAME)$(EXT_SONAME)
TARGETS_DSO    += lib$(NAME)$(EXT_LINKLIB)

TARGETS_ALL    += $(TARGETS_DSO)

INSTALL_HDR    += sfwlogging.h
INSTALL_HDR    += sfwplugin.h
INSTALL_HDR    += sfwreporting.h
INSTALL_HDR    += sfwsensor.h
INSTALL_HDR    += sfwservice.h
INSTALL_HDR    += sfwtypes.h

INSTALL_PC     += pkg-config/$(NAME).pc

# ----------------------------------------------------------------------------
# Standard top level targets
# ----------------------------------------------------------------------------

.PHONY: build install clean distclean mostlyclean

build:: $(TARGETS_ALL)

install:: build

mostlyclean::
	$(RM) *.o *~ *.bak */*.o */*~ */*.bak

clean:: mostlyclean
	$(RM) $(TARGETS_ALL)

distclean:: clean

# ----------------------------------------------------------------------------
# Default flags
# ----------------------------------------------------------------------------

CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64
CPPFLAGS += -DVERSION='"$(VERSION)"'

COMMON   += -Wall
COMMON   += -Wextra
COMMON   += -Os
COMMON   += -g

CFLAGS   += $(COMMON)
CFLAGS   += -std=c99
CFLAGS   += -Wmissing-prototypes
CFLAGS   += -Wno-missing-field-initializers

CXXFLAGS += $(COMMON)

LDFLAGS  += -g

LDLIBS   += -Wl,--as-needed

# Options that might be useful during development time
#COMMON += -Werror

# Options that are useful for weeding out unused functions
#CFLAGS += -O0 -ffunction-sections -fdata-sections
#LDLIBS += -Wl,--gc-sections,--print-gc-sections

# ----------------------------------------------------------------------------
# Flags from pkg-config
# ----------------------------------------------------------------------------

PKG_NAMES += glib-2.0
PKG_NAMES += gio-2.0

# intersection of two sets -> $(call intersection, set1, set2)
intersection = $(strip $(foreach w, $1, $(filter $w, $2)))

maintenance += clean distclean mostlyclean
maintenance += normalize
maintenance += update-protos
maintenance += archive
maintenance += graphs graphs-pdf graphs-png
maintenance += cppcheck

ifneq ($(call intersection, $(maintenance), $(MAKECMDGOALS)),)
PKG_CONFIG   ?= true
endif

ifneq ($(strip $(PKG_NAMES)),)
PKG_CONFIG   ?= pkg-config
PKG_CFLAGS   := $(shell $(PKG_CONFIG) --cflags $(PKG_NAMES))
PKG_LDLIBS   := $(shell $(PKG_CONFIG) --libs   $(PKG_NAMES))
PKG_CPPFLAGS := $(filter -D%, $(PKG_CFLAGS)) $(filter -I%, $(PKG_CFLAGS))
PKG_CFLAGS   := $(filter-out -I%, $(filter-out -D%, $(PKG_CFLAGS)))
endif

CPPFLAGS += $(PKG_CPPFLAGS)
CFLAGS   += $(PKG_CFLAGS)
LDLIBS   += $(PKG_LDLIBS)

# ----------------------------------------------------------------------------
# Implicit rules
# ----------------------------------------------------------------------------

.SUFFIXES: %.c %.o
.SUFFIXES: %.pic.o %$(EXT_LIBRARY) %$(EXT_SONAME) %$(EXT_LINKLIB)

%.o : %.c
	$(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

%.pic.o : %.c
	$(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS) -fPIC -fvisibility=hidden

%$(EXT_LIBRARY) :
	$(CC) -o $@ -shared -Wl,-soname,$*$(EXT_SONAME) $^ $(LDFLAGS) $(LDLIBS)

%$(EXT_SONAME) : %$(EXT_LIBRARY)
	ln -sf $< $@

%$(EXT_LINKLIB) : %$(EXT_SONAME)
	ln -sf $< $@

.SUFFIXES: %.dot %.png %.pdf

%.pdf : %.dot
	dot -Tpdf $< -o $@

%.png : %.dot
	dot -Tpng $< -o $@

# ----------------------------------------------------------------------------
# Rules for libsensors-glib.so
# ----------------------------------------------------------------------------

libsensors-glib_src += sfwlogging.c
libsensors-glib_src += sfwplugin.c
libsensors-glib_src += sfwreporting.c
libsensors-glib_src += sfwsensor.c
libsensors-glib_src += sfwservice.c
libsensors-glib_src += sfwtypes.c
libsensors-glib_src += utility.c

libsensors-glib_obj += $(patsubst %.c, %.pic.o, $(libsensors-glib_src))

libsensors-glib$(EXT_LIBRARY) : $(libsensors-glib_obj)

EXPAND_TEMPLATE = sed -i\
 -e 's:%VERSION%:$(VERSION):g'\
 -e 's:%LIBDIR%:$(_LIBDIR):g'\
 -e 's:%INCLUDEDIR%:$(_INCLUDEDIR):g'

install::
	# dynamic libraries
	install -d -m 755 $(DESTDIR)$(_LIBDIR)
	tar -cf- $(TARGETS_DSO) | tar -C$(DESTDIR)$(_LIBDIR) -xf-
	# headers
	install -d -m 755 $(DESTDIR)$(_INCLUDEDIR)/sensors-glib
	install -m 644 $(INSTALL_HDR) $(DESTDIR)$(_INCLUDEDIR)/sensors-glib/
	# pkg config
	install -d -m 755 $(DESTDIR)$(_LIBDIR)/pkgconfig
	install -m 644 $(INSTALL_PC) $(DESTDIR)$(_LIBDIR)/pkgconfig
	for f in $(DESTDIR)$(_LIBDIR)/pkgconfig/*.pc; do\
	  $(EXPAND_TEMPLATE) $$f;\
	done

# ----------------------------------------------------------------------------
# Generate snapshot tarball
# ----------------------------------------------------------------------------

.PHONY: archive

archive::
	git archive -o "$(NAME)-$(VERSION)-$(shell git-current-branch).tar.gz" --prefix=$(NAME)-$(VERSION)/ HEAD

# ----------------------------------------------------------------------------
# Update header dependecies
# ----------------------------------------------------------------------------

.PHONY: depend

depend::
	$(CC) -MM -MG $(CPPFLAGS) $(wildcard *.c) | util/depend_filter.py > .depend

ifeq ($(call intersection,depend,$(MAKECMDGOALS)),) # not while: make depend
ifneq ($(wildcard .depend),)                        # only if: .depend exists
include .depend
endif
else
endif

# ----------------------------------------------------------------------------
# Generate graphs from graphviz dot files
# ----------------------------------------------------------------------------

GRAPHS_DOT := $(wildcard *.dot)
GRAPHS_PNG = $(patsubst %.dot, %.png, $(GRAPHS_DOT))
GRAPHS_PDF = $(patsubst %.dot, %.pdf, $(GRAPHS_DOT))

.PHONY: graphs graphs-pdf graphs-png

graphs: graphs-pdf graphs-png

graphs-pdf: $(GRAPHS_PDF)
graphs-png: $(GRAPHS_PNG)

distclean::
	$(RM) $(GRAPHS_PDF) $(GRAPHS_PNG)

# ----------------------------------------------------------------------------
# Generate / update prototypes
# ----------------------------------------------------------------------------

PROTO_SOURCES := $(wildcard *.c)
PROTO_HEADERS := $(wildcard $(patsubst %.c, %.h, $(PROTO_SOURCES)))

.SUFFIXES: %.q %.p %.g
.PRECIOUS: %.q

PROTO_CPPFLAGS += -D_Float32="float"
PROTO_CPPFLAGS += -D_Float64="double"
PROTO_CPPFLAGS += -D_Float128="long double"
PROTO_CPPFLAGS += -D_Float32x="float"
PROTO_CPPFLAGS += -D_Float64x="double"
PROTO_CPPFLAGS += -D_Float128x="long double"

%.q : CPPFLAGS += $(PROTO_CPPFLAGS)
%.q : %.c ; $(CC) -o $@ -E $< $(CPPFLAGS) -O
%.p : %.q prettyproto.groups ; cproto -s < $< | prettyproto.py > $@ --local
%.g : %.q prettyproto.groups ; cproto    < $< | prettyproto.py > $@ --global

clean::
	$(RM) *.[qpg]

.PHONY: protos-q protos-p protos-g

protos-q : $(patsubst %.c, %.q, $(PROTO_SOURCES))
protos-p : $(patsubst %.c, %.p, $(PROTO_SOURCES))
protos-g : $(patsubst %.c, %.g, $(PROTO_SOURCES))

protos-pre: protos-q
	touch $@
protos-post: protos-p protos-g
	touch $@
clean::
	$(RM) protos-pre protos-post

.PHONY:  protos-update

protos-update: protos-pre | protos-post
	updateproto.py $(PROTO_SOURCES) $(PROTO_HEADERS)

# ----------------------------------------------------------------------------
# Check for excess include statements
# ----------------------------------------------------------------------------

.PHONY: headers

.SUFFIXES: %.checked

headers:: c_headers c_sources

%.checked : %
	find_unneeded_includes.py $(CPPFLAGS) $(CFLAGS) -- $<
	@touch $@

clean::
	$(RM) *.checked *.order

c_headers:: $(patsubst %, %.checked, $(wildcard *.h))
c_sources:: $(patsubst %, %.checked, $(wildcard *.c))

order::
	find_unneeded_includes.py -- $(wildcard *.h) $(wildcard *.c)

# ----------------------------------------------------------------------------
# Source code normalization
# ----------------------------------------------------------------------------

.PHONY: normalize

normalize::
	normalize_whitespace -M Makefile
	normalize_whitespace -a $(wildcard *.[ch] */*.[ch])
	normalize_whitespace -a $(wildcard */*.spec */*.pc *.md)
	normalize_whitespace -a $(wildcard *.py */*.py)

# ----------------------------------------------------------------------------
# Static analysis
# ----------------------------------------------------------------------------

.PHONY: cppcheck

CPPCHECK_FLAGS += --quiet -UDEAD_CODE --enable=all --std=c99
CPPCHECK_EXTRA := $(wildcard cppcheck-extra/*.c)

cppcheck::
	cppcheck $(CPPCHECK_FLAGS) $(libsensors-glib_src) $(CPPCHECK_EXTRA)
