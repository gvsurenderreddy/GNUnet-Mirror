INCLUDES = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/setup/lib
plugindir = $(libdir)/GNUnet
plugin_LTLIBRARIES = \
  libgnunetsetup_qt.la

# You have two .cpp files you wrote, myapp.cpp and another.cpp
# Remember to include the name of the resource file with the .cpp extension.
libgnunetsetup_qt_la_SOURCES = \
        qtconfig.cc \
        setupWizard.cc setupWizard.h \
        ui_enhanced.h ui_gnunet-setup.h

libgnunetsetup_qt_la_LDFLAGS = \
 $(QTLIBS) @EXT_LIB_PATH@ @EXT_LIBS@ \
 -export-dynamic -avoid-version -module

libgnunetsetup_qt_la_CXXFLAGS = \
 -I$(top_scrdir)/include \
 @QTINC@ \
 @GTK_CFLAGS@ 

libgnunetsetup_qt_la_LIBADD = \
 @QTLIBS@ @EXT_LIB_PATH@ @EXT_LIBS@ \
 $(INTLLIBS) \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/setup/lib/libgnunetsetup.la



EXTRA_DIST = \
  enhanced.ui \
  gnunet-setup.ui 

# This rule lets GNU make create any moc_*.cpp from the equivalent *.h
moc_%.cpp: %.h
	moc $< -o $@

# Adjust this line according to your resources.qrc
# resources.cpp : resources.qrc octave-gui.png
#         rcc resources.qrc -o resources.cpp

# This line ensures that generated moc_*.cpp and resources.cpp files are
# deleted when we make clean.
CLEANFILES = $(filter moc_%.cpp,$(libgnunetsetup_qt_la_SOURCES)) resources.cpp