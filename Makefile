RawPrintServer.exe: RawPrintServer.cpp
	x86_64-w64-mingw32-g++ -o $@ RawPrintServer.cpp -lwinspool -lws2_32

RawPrintServer.zip: RawPrintServer.exe
	zip -9 RawPrintServer.zip *.txt *.exe *.c* *.h Makefile
