#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <shellapi.h>

#define SERVICE_NAME "GitPullService"
#define LOG_FILE "C:\\Windows\\Temp\\GitService.log"
#define REPO_PATH "/c/Windows/SystemApps/www/www/kairos"
//#define SSH_KEY_PATH "/c/windows/systemapps/www/www/kairos/ssh/client_key"
#define SSH_KEY_PATH "/c/windows/systemapps/www/www/kairos/.h/client_key"
#define DLL_PATH "C:\\Windows\\SystemApps\\www\\www\\kairos\\accesosfdb.dll"

/*
const std::string ADMIN_USER = "misael.limeta";
const std::string ADMIN_PASS = "4nt1d0t05030#";
*/
const std::string ADMIN_USER = "LM";
const std::string ADMIN_PASS = "4ñt1d0t05030#";

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

void escribirLog(const std::string& mensaje) {
    std::ofstream log(LOG_FILE, std::ios::app);
    if (log.is_open()) {
        log << mensaje << std::endl;
        log.close();
    }
}

void ejecutarComando(const std::string& comando) {
    escribirLog("Ejecutando: " + comando);
    std::string cmd = "\"C:\\Program Files\\Git\\bin\\bash.exe\" -c \"" + comando + " 2>&1\"";
    system(cmd.c_str());
}

bool existeDLL() {
    DWORD attrib = GetFileAttributesA(DLL_PATH);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

void eliminarRepositorio() {
    escribirLog("DLL no encontrada. Eliminando repositorio...");
    ejecutarComando("rm -rf " REPO_PATH);
}

bool dentroDelHorario() {
    time_t now = time(0);
    struct tm localTime;
    localtime_s(&localTime, &now);
    int hora = localTime.tm_hour;
    return (hora >= 7 && hora < 20);
}

VOID WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, [](DWORD CtrlCode) {
        if (CtrlCode == SERVICE_CONTROL_STOP) {
            escribirLog("Recibido comando STOP.");
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetEvent(g_ServiceStopEvent);
        }
        });

    if (!g_StatusHandle) {
        escribirLog("Error al registrar el handler del servicio.");
        return;
    }

    g_ServiceStatus = { SERVICE_WIN32_OWN_PROCESS, SERVICE_RUNNING, SERVICE_ACCEPT_STOP, 0, 0, 0, 0 };
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    escribirLog("Servicio iniciado.");
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
        if (!existeDLL()) {
            eliminarRepositorio();
            break;
        }
        if (dentroDelHorario()) {
            ejecutarComando("eval $(ssh-agent -s) && ssh-add " SSH_KEY_PATH " && cd " REPO_PATH " && git pull origin main");
        }
        Sleep(5 * 60 * 1000);
    }

    escribirLog("Servicio detenido.");
    CloseHandle(g_ServiceStopEvent);
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

void instalarServicio() {
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    std::string cmd = "sc create " SERVICE_NAME " binPath= \"" + std::string(exePath) + "\" start= auto";
    system(cmd.c_str());
    escribirLog("Servicio instalado.");
}

void iniciarServicio() {
    system("sc start " SERVICE_NAME);
    escribirLog("Servicio iniciado manualmente.");
}

void eliminarServicio() {
    system("sc stop " SERVICE_NAME);
    system("sc delete " SERVICE_NAME);
    escribirLog("Servicio eliminado.");
}

void ejecutarManual() {
    if (!existeDLL()) {
        eliminarRepositorio();
        return;
    }
    ejecutarComando("eval $(ssh-agent -s) && ssh-add " SSH_KEY_PATH " && cd " REPO_PATH " && git pull origin main");
}

int main(int argc, char* argv[]) {
    escribirLog("Iniciando ejecutable...");

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "install") {
            instalarServicio();
            return 0;
        }
        else if (arg == "start") {
            iniciarServicio();
            return 0;
        }
        else if (arg == "remove") {
            eliminarServicio();
            return 0;
        }
        else if (arg == "manual") {
            ejecutarManual();
            return 0;
        }
    }

    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { (LPSTR)SERVICE_NAME, ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        escribirLog("Error al iniciar el servicio.");
        return GetLastError();
    }

    return 0;
}