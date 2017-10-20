/*
PrintServer Copyright (c) 2006, Henk Jonas
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. The name of the author may not be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (
INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (
INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <windows.h>
#include <stdio.h>
#include <winspool.h>

#define SLEEP_TIME 5000
#define LOGFILE "C:\\PrintServer.log"

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

typedef enum {
  INSTALL = 0,
  REMOVE,
  STANDALONE,
  BACKGROUND,
  PRIVATE_SERVICE,
  INVALID
} CommandType;

/* Same order as CommandType */
struct {
  char *commandName;
  int maxCommandArgs;
} commands[] = {{"INSTALL", 2},
                {"REMOVE", 1},
                {"STANDALONE", 2},
                {"BACKGROUND", 2},
                {"PRIVATE_SERVICE", 1}};

#define NUM_COMMANDS (sizeof commands / sizeof *commands)

char serviceKey[] = "System\\CurrentControlSet\\Services\\%s";
char standaloneKey[] = "SOFTWARE\\Alexander_Pruss\\RawPrintServer\\%s";
char *regKey = serviceKey;
char printerName[256] = {0};
char strServiceName[64];
DWORD serverPort;
DWORD startPort;

int InnerLoop(DWORD port, int service);
void ServiceMain(int argc, char **argv);
void ControlHandler(DWORD request);
int InitService();

int WriteToLog(char *str) {
  FILE *log;
  log = fopen(LOGFILE, "a+");
  if (log == NULL)
    return -1;
  fprintf(log, "%s\n", str);
  fclose(log);
  return 0;
}

/*
Z:\Programme\Microsoft Visual Studio\MyProjects\PrintServer\Debug>sc create
PrintServer binPath= "Z:\Programme\Microsoft Visual
Studio\MyProjects\PrintServer\Debug\PrintServer.exe"
*/

VOID CreatePrintServer(char *strMyPath, char *strPrinter, DWORD port,
                       int service) {
  char strTemp[1024];
  sprintf(strTemp, "\"%s\" PRIVATE_SERVICE %d", strMyPath, port);

  if (service) {
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) {
      printf("Error: ServiceManager %d\n", GetLastError());
      return;
    }

    SC_HANDLE schService = CreateService(
        schSCManager,   // SCManager database
        strServiceName, // name of service
        strServiceName, // service name to display (Attention: we better use the
                        // same name here, or you will never find it...)
        SERVICE_ALL_ACCESS,        // desired access
        SERVICE_WIN32_OWN_PROCESS, // service type
        SERVICE_AUTO_START, // SERVICE_DEMAND_START,      // start type
        SERVICE_ERROR_NORMAL, // error control type
        strTemp, // lpszBinaryPathName,        // service's binary
        NULL,  // no load ordering group
        NULL,  // no tag identifier
        NULL,  // no dependencies
        NULL,  // LocalSystem account
        NULL); // no password

    if (schService == NULL) {
      printf("Error: CreateService %d\n", GetLastError());
      return;
    } else
      printf("CreateService SUCCESS.\n");

    if (StartService(schService, 0, NULL))
      printf("Service started.\n");
    else
      printf("Error starting Service: %d, please do it by hand.\n",
             GetLastError());
    CloseServiceHandle(schService);
  }

  HKEY hdlKey;
  sprintf(strTemp, regKey, strServiceName);
  if (!service) {
    RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Alexander_Pruss", 0, "",
                   REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, NULL, NULL);
  }
  RegCreateKeyEx(HKEY_LOCAL_MACHINE, strTemp, 0, "", REG_OPTION_NON_VOLATILE,
                 KEY_ALL_ACCESS, NULL, &hdlKey, NULL);
  RegSetValueEx(hdlKey, "Description", 0, REG_SZ,
                (const unsigned char *)"Routes all traffic from port 910x to a "
                                       "local printer",
                53);
  RegSetValueEx(hdlKey, "Printer", 0, REG_SZ, (const unsigned char *)strPrinter,
                strlen(strPrinter) + 1);
  RegSetValueEx(hdlKey, "Port", 0, REG_DWORD, (const unsigned char *)&port,
                sizeof(port));
  RegCloseKey(hdlKey);
}

VOID DeletePrintServerService(DWORD port) {
  SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) {
    printf("Error: ServiceManager %d\n", GetLastError());
    return;
  }

  SC_HANDLE schService =
      OpenService(schSCManager,        // SCManager database
                  strServiceName,      // name of service
                  SERVICE_ALL_ACCESS); // only need DELETE access

  if (schService == NULL) {
    printf("Error: OpenService (%s) %d\n", strServiceName, GetLastError());
    return;
  }

  SERVICE_STATUS ss;
  if (ControlService(schService, SERVICE_CONTROL_STOP, &ss))
    printf("Server stopped.\n");
  else {
    int err = GetLastError();
    if (err != ERROR_SERVICE_NOT_ACTIVE) {
      printf("Error stopping Service: %d, please do it by hand.\n", err);
      return;
    }
  }

  if (!DeleteService(schService)) {
    printf("Error: DeleteService %d\n", GetLastError());
    return;
  } else
    printf("DeleteService SUCCESS\n");

  CloseServiceHandle(schService);
}

int main(int argc, char **argv) {
  int command;

  remove(LOGFILE);

  WriteToLog("RawPrintServer 1.00 created by Henk Jonas (www.metaviewsoft.de)");
  WriteToLog("PrintServer start");

  command = INVALID;

  if (1 < argc) {
    for (command = 0; command < NUM_COMMANDS; command++) {
      if (0 == stricmp(argv[1], commands[command].commandName))
        break;
    }

    if (NUM_COMMANDS <= command)
      command = INVALID;
  }

  if (command != INVALID && argc == 2 + commands[command].maxCommandArgs - 1) {
    serverPort = 9100;
  } else if (command != INVALID &&
             argc == 2 + commands[command].maxCommandArgs) {
    int p = atoi(argv[2 + commands[command].maxCommandArgs - 1]);

    if (0 < p)
      serverPort = p;
    else
      serverPort = 9100;
  } else {
    char *lastPart = argv[0];
    char *p = argv[0];

    while (*p) {
      if (*p == ':' || *p == '/' || *p == '\\') {
        lastPart = p + 1;
      }
      p++;
    }

    fprintf(stderr, "%s INSTALL \"Printer Name\" [port]\n"
                    "%s REMOVE [port]\n"
                    "%s STANDALONE \"Printer Name\" [port]\n"
                    "%s BACKGROUND \"Printer Name\" [port]\n"
                    "If port is unspecified, 9100 is assumed.\n",
            lastPart, lastPart, lastPart, lastPart);
    return 1;
  }

  startPort = serverPort;
  sprintf(strServiceName, "RawPrintServer_%d", serverPort);

  switch (command) {
  case INSTALL:
    CreatePrintServer(argv[0], argv[2], serverPort, 1);
    break;
  case STANDALONE:
  case BACKGROUND: {
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(2, 2);
    regKey = standaloneKey;

    CreatePrintServer(argv[0], argv[2], serverPort, 0);

    if (command == BACKGROUND)
      FreeConsole();

    WSAStartup(wVersionRequested, &wsaData);

    while (InnerLoop(serverPort, 0))
      ;

    WSACleanup();
    break;
  }
  case REMOVE:
    DeletePrintServerService(serverPort);
    break;
  case PRIVATE_SERVICE:
    SERVICE_TABLE_ENTRY ServiceTable[2];
    ServiceTable[0].lpServiceName = strServiceName;
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;

    // Start the control dispatcher thread for our service
    StartServiceCtrlDispatcher(ServiceTable);

    WriteToLog("PrintServer exit");
    break;
  default:
    break;
  }

  return 0;
}

int InnerLoop(DWORD port, int service) {
  char strTemp[256];
  DWORD valueSize;

  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    sprintf(strTemp, "Error: no socket: %d", WSAGetLastError());
    WriteToLog(strTemp);
    if (service) {
      ServiceStatus.dwCurrentState = SERVICE_STOPPED;
      ServiceStatus.dwWin32ExitCode = 3;
      SetServiceStatus(hStatus, &ServiceStatus);
    }
    return 0;
  }
  sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(serverPort);
  if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
    sprintf(strTemp, "Error: couldn't bind: %d", WSAGetLastError());
    WriteToLog(strTemp);
    closesocket(sock);
    if (service) {
      ServiceStatus.dwCurrentState = SERVICE_STOPPED;
      ServiceStatus.dwWin32ExitCode = 4;
      SetServiceStatus(hStatus, &ServiceStatus);
    }
    return 0;
  }
  sockaddr_in client = {0};
  int size = sizeof(client);
  while (listen(sock, 2) != SOCKET_ERROR) {
    SOCKET sock2 = accept(sock, (struct sockaddr *)&client, &size);
    if (sock2 != INVALID_SOCKET) {
      HANDLE printer = NULL;

      HKEY hdlKey;
      sprintf(strTemp, regKey, strServiceName);
      RegOpenKeyEx(HKEY_LOCAL_MACHINE, strTemp, 0, KEY_ALL_ACCESS, &hdlKey);
      valueSize = sizeof(printerName);
      RegQueryValueEx(hdlKey, "Printer", NULL, NULL, (BYTE *)printerName,
                      &valueSize);
      RegCloseKey(hdlKey);

      sprintf(
          strTemp, "Accept print job for %s from %d.%d.%d.%d", printerName,
          client.sin_addr.S_un.S_un_b.s_b1, client.sin_addr.S_un.S_un_b.s_b2,
          client.sin_addr.S_un.S_un_b.s_b3, client.sin_addr.S_un.S_un_b.s_b4);
      WriteToLog(strTemp);

      DOC_INFO_1 info;
      info.pDocName = "Forwarded Job";
      info.pOutputFile = NULL;
      info.pDatatype = "RAW";

      // char *printerName = "Test;//"Brother HL-1430 series";
      if (!OpenPrinter(printerName, &printer, NULL) ||
          !StartDocPrinter(printer, 1, (LPBYTE)&info)) {
        WriteToLog("Error opening print job.");
      } else {
        char buffer[1024];
        DWORD wrote;
        while (1) {
          int result = recv(sock2, buffer, sizeof(buffer), 0);
          if (result <= 0)
            break;
          WritePrinter(printer, buffer, result, &wrote);
          if (wrote != (DWORD)result) {
            WriteToLog("Couldn't print all data.");
            break;
          }
        }
        ClosePrinter(printer);
      }
    }
    closesocket(sock2);
  }
  if (service) {
    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    ServiceStatus.dwWin32ExitCode = 5;
    SetServiceStatus(hStatus, &ServiceStatus);
  }
  closesocket(sock);
  return 1;
}

void ServiceMain(int argc, char **argv) {
  int error;
  char strTemp[256];

  ServiceStatus.dwServiceType = SERVICE_WIN32;
  ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
  ServiceStatus.dwControlsAccepted =
      SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  ServiceStatus.dwWin32ExitCode = 1;
  ServiceStatus.dwServiceSpecificExitCode = 0;
  ServiceStatus.dwCheckPoint = 0;
  ServiceStatus.dwWaitHint = 0;

  hStatus = RegisterServiceCtrlHandler(strServiceName,
                                       (LPHANDLER_FUNCTION)ControlHandler);
  if (hStatus == (SERVICE_STATUS_HANDLE)0) {
    // Registering Control Handler failed
    sprintf(strTemp, "Registering Control Handler failed %d", GetLastError());
    WriteToLog(strTemp);
    return;
  }

  // Initialize Service
  error = InitService();
  if (error) {
    // Initialization failed
    sprintf(strTemp, "Initialization failed %d", GetLastError());
    WriteToLog(strTemp);
    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    ServiceStatus.dwWin32ExitCode = 2;
    SetServiceStatus(hStatus, &ServiceStatus);
    return;
  }

  // We report the running status to SCM.
  ServiceStatus.dwCurrentState = SERVICE_RUNNING;
  SetServiceStatus(hStatus, &ServiceStatus);

  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

  wVersionRequested = MAKEWORD(2, 2);
  err = WSAStartup(wVersionRequested, &wsaData);

  HKEY hdlKey;
  DWORD valueSize;
  sprintf(strTemp, regKey, strServiceName);
  RegOpenKeyEx(HKEY_LOCAL_MACHINE, strTemp, 0, KEY_ALL_ACCESS, &hdlKey);
  valueSize = sizeof(printerName);
  RegQueryValueEx(hdlKey, "Printer", NULL, NULL, (BYTE *)printerName,
                  &valueSize);
  valueSize = sizeof(serverPort);
  RegQueryValueEx(hdlKey, "Port", NULL, NULL, (BYTE *)&serverPort, &valueSize);
  RegCloseKey(hdlKey);

  sprintf(strTemp, "%s on %d (%d)", printerName, serverPort, startPort);
  WriteToLog(strTemp);

  // The worker loop of a service
  while (ServiceStatus.dwCurrentState == SERVICE_RUNNING &&
         InnerLoop(serverPort, 1))
    ;

  WSACleanup();

  return;
}

// Service initialization
int InitService() { return 0; }

// Control handler function
void ControlHandler(DWORD request) {
  switch (request) {
  case SERVICE_CONTROL_STOP:
    WriteToLog("PrintServer stopped.");

    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hStatus, &ServiceStatus);
    return;

  case SERVICE_CONTROL_SHUTDOWN:
    WriteToLog("PrintServer stopped.");

    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hStatus, &ServiceStatus);
    return;

  default:
    break;
  }

  // Report current status
  SetServiceStatus(hStatus, &ServiceStatus);

  return;
}
