APP_NAME     := PlutoGen
SOURCE_FILES := PlutoGen.cc
#INSTALL_DIR  := $(MYHADDIR)/bin

USES_RFIO    := no
USES_CERNLIB := no
USES_ORACLE  := no

include $(HADDIR)/hades.def.mk

INC_DIRS      += $(PLUTODIR)/include
LIB_DIRS      += $(PLUTODIR)/lib64

# override default list of linked Hydra libraries - before they can act on the rules
HYDRA_LIBS = -lPluto -lHydra

include $(HADDIR)/hades.app.mk
