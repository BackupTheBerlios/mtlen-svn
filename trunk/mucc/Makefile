# Project: mtlen
# Makefile created by Dev-C++ 4.9.9.0

CPP  = g++.exe
CC   = gcc.exe
WINDRES = windres.exe
DLLWRAP = dllwrap.exe
RM = rm
UPX = upx -9 -q --compress-icons=0
SRC = AdminWindow.cpp ChatEvent.cpp ChatGroup.cpp ChatRoom.cpp ChatUser.cpp ChatWindow.cpp FontList.cpp HelperDialog.cpp ManagerWindow.cpp mucc.cpp Options.cpp mucc_services.cpp Utils.cpp
RES  = mucc.res
OBJ = $(SRC:.cpp=.o) $(RES)
BIN  = mucc.dll
INCS =  -I"../../include"
CFLAGS = $(INCS) -DBUILDING_DLL=1 -fno-exceptions -fexpensive-optimizations -O3 -Os -Wno-error -Wall
CXXFLAGS = $(INCS) -DBUILDING_DLL=1 -fno-exceptions -fno-rtti -fexpensive-optimizations -O3 -Os -Wno-error -Wall
# -save-temps
LIBS =  -lversion -lwsock32 -lole32 -lcomctl32
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
	$(CPP) -MM $(CFLAGS) $(SRC)>Makefile.dep

$(BIN): $(OBJ)
	$(CPP) $(CFLAGS) $(OBJ) $(LIBS) $(LFLAGS) -o $(BIN) -Wl

$(RES): $(RES:.res=.rc) resource.h
	$(WINDRES) $(RCFLAGS) -i $(RES:.res=.rc) -o $(RES)

-include Makefile.dep
