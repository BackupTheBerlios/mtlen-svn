# Project: mtlen
# Makefile created by Dev-C++ 4.9.9.0

CPP  = g++.exe
CC   = gcc.exe
WINDRES = windres.exe
RM = rm
UPX = upx -9 -q --compress-icons=0
SRC = codec/gsm_preprocess.c codec/gsm_long.c codec/gsm_short.c codec/gsm_rpe.c codec/gsm_lpc.c codec/gsm_codec.c tlen_file.c tlen_voice.c tlen_muc.c jabber_iq.c jabber_iqid.c jabber_list.c jabber_misc.c jabber_opt.c jabber_ssl.c jabber_svc.c jabber_thread.c jabber_userinfo.c jabber_util.c jabber_ws.c jabber_xml.c sha1.c tlen.c tlen_advsearch.c
RES  = tlen.res
OBJ = $(SRC:.c=.o) $(RES)
BIN  = tlen.dll
INCS =  -I"../../include"
CFLAGS = $(INCS) -DBUILDING_DLL=1 -DTLEN_PLUGIN -fno-exceptions -fexpensive-optimizations -O3 -Os -Wno-error -Wall
LIBS =  -lversion -lwsock32 -lwinmm
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
	$(CC) -MM $(CFLAGS) $(SRC)>Makefile.dep

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LIBS) $(LFLAGS) -o $(BIN) -Wl

$(RES): $(RES:.res=.rc) resource.h
	$(WINDRES) $(RCFLAGS) -i $(RES:.res=.rc) -o $(RES)

-include Makefile.dep
