## Haiku Generic Makefile v2.6 ##
# Builds the ElectroPaint screen saver add-on on Haiku (needs haiku_devel and
# mesa_devel). "make install" puts it into the user's non-packaged
# "Screen Savers" folder.

NAME = ElectroPaint
TYPE = SHARED
APP_MIME_SIG = application/x-vnd.ElectroPaint-Screensaver

SRCS = \
	src/ElectroPaintEngine.cpp \
	src/ElectroPaintSaver.cpp

RDEFS = src/ElectroPaint.rdef

LIBS = be screensaver GL $(STDCPPLIBS)

OPTIMIZE := FULL
WARNINGS = ALL
# Keep every floating point operation rounded like the original's
# (tests/reference compares the results bit for bit).
COMPILER_FLAGS = -ffp-contract=off -Werror
DEBUGGER :=

INSTALL_DIR = $(shell finddir B_USER_NONPACKAGED_ADDONS_DIRECTORY)/Screen Savers

DEVEL_DIRECTORY := /boot/system/develop
include $(DEVEL_DIRECTORY)/etc/makefile-engine
