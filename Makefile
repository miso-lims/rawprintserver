RawPrintServer.exe: RawPrintServer.cpp
	cl /O2 RawPrintServer.cpp kernel32.lib user32.lib wsock32.lib winmm.lib winspool.lib advapi32.lib

RawPrintServer.zip: RawPrintServer.exe
	zip -9 RawPrintServer.zip *.txt *.exe *.c* *.h Makefile
