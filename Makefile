.DEFAULT_GOAL := all
PLUGIN_NAME = hypr-appmenu

WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

CXXFLAGS ?= -g -O2
CXXFLAGS += -shared -Wall -fPIC $(EXTRA_FLAGS) -std=c++23 $(shell pkg-config --cflags pixman-1 libdrm hyprland libsystemd)
LIBS = $(shell pkg-config --libs libsystemd hyprland)

OBJS = appMenuPlugin.o appmenu-protocol.o

all: $(PLUGIN_NAME).so

appmenu-protocol.h: appmenu.xml
	$(WAYLAND_SCANNER) server-header appmenu.xml $@

appmenu-protocol.c: appmenu.xml
	$(WAYLAND_SCANNER) private-code appmenu.xml $@

appmenu-protocol.o: appmenu-protocol.c appmenu-protocol.h
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

%.o: %.cpp appmenu-protocol.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(PLUGIN_NAME).so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(OBJS) $(LIBS)

clean:
	rm -f ./$(PLUGIN_NAME).so ./appmenuPlugin.so $(OBJS) appmenu-protocol.h appmenu-protocol.c

format:
	clang-format -i appMenuPlugin.cpp appMenuPlugin.hpp

load: all
	hyprctl plugin unload ${PWD}/$(PLUGIN_NAME).so
	hyprctl plugin load ${PWD}/$(PLUGIN_NAME).so

.PHONY: all clean load format
