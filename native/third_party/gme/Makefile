# Standalone rebuild of the replaceable Game Music Emu GBS library.
# Use `make` on Linux, or `make windows64` / `make windows32` with MinGW.
NAMES = Blip_Buffer Classic_Emu Data_Reader Dual_Resampler Effects_Buffer Fir_Resampler gme Gme_File M3u_Playlist Multi_Buffer Music_Emu Gb_Apu Gb_Cpu Gb_Oscs Gbs_Emu
SOURCES = $(addsuffix .cpp,$(NAMES))
COMMON = -std=gnu++11 -O2 -DBLARGG_BUILD_DLL

.PHONY: all windows64 windows32
all: librhhgme.so
librhhgme.so: $(SOURCES) $(wildcard *.h)
	$(CXX) $(COMMON) -fPIC -shared -Wl,-soname,librhhgme.so $(SOURCES) -o $@
windows64:
	x86_64-w64-mingw32-g++ $(COMMON) -shared -static-libgcc -static-libstdc++ $(SOURCES) -o rhhgme.dll
windows32:
	i686-w64-mingw32-g++ $(COMMON) -shared -static-libgcc -static-libstdc++ $(SOURCES) -o rhhgme.dll
