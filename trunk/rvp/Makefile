# Project: mtlen
# Makefile created by Dev-C++ 4.9.9.0

CPP  = g++.exe
CC   = gcc.exe
WINDRES = windres.exe
RM = rm
UPX = upx -9 -q --compress-icons=0
SRC = ./xmllib/XMLObject.cpp ./xmllib/JString.cpp ./httplib/MD5.cpp ./httplib/HTTPLib.cpp JLogger.cpp Utils.cpp List.cpp ThreadManager.cpp RVPUtils.cpp RVPImpl.cpp rvp_opt.cpp rvp_svc.cpp rvp.cpp
RES  = rvp.res
OBJ = $(SRC:.cpp=.o) $(RES)
BIN  = rvp.dll
INCS =  -I"../../include"
CFLAGS = $(INCS) -DBUILDING_DLL=1 -DTLEN_PLUGIN -fno-exceptions -fexpensive-optimizations -O3 -Os -Wno-error -Wall
CXXFLAGS = $(INCS) -DBUILDING_DLL=1 -DTLEN_PLUGIN -fno-exceptions -fexpensive-optimizations -O3 -Os -Wno-error -Wall
LIBS =  -lversion -lwsock32 -lwinmm -ldnsapi
LFLAGS = -s -mdll -mwindows --no-export-all-symbols --add-stdcall-alias
RCFLAGS = -I rc -O coff
# --no-export-all-symbols --add-stdcall-alias

.PHONY: all all-before all-after clean clean-custom

all: all-before $(BIN) all-after
ifdef PACK
	$(UPX) $(BIN)
endif

clean: clean-custom
	$(RM) -f $(OBJ) $(BIN)

depend :
	$(CPP) -MM $(CXXFLAGS) $(SRC)>Makefile.dep

$(BIN): $(OBJ)
	$(CPP) $(CFLAGS) $(OBJ) $(LIBS) $(LFLAGS) -o $(BIN) -Wl

$(RES): $(RES:.res=.rc) resource.h
	$(WINDRES) $(RCFLAGS) -i $(RES:.res=.rc) -o $(RES)

-include Makefile.dep
