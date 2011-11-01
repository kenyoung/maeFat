AM_CFLAGS = -g -Wall

bin_PROGRAMS = maefat
maefat_SOURCES = maeFat.c maeFatColors.h
maefat_CPPFLAGS = $(GTK_CFLAGS) $(HILDON_CFLAGS) $(FM_FLAGS) $(LIBOSSO_CFLAGS) $(DBUS_CFLAGS)
maefat_LDADD = $(GTK_LIBS) $(HILDON_LIBS) $(FM_LIBS) $(LIBOSSO_LIBS) $(DBUS_LIBS) -lrt
