#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <shellapi.h>
#define SERVICE_NAME "GitPullService"
#define LOG_FILE "C:\\Windows\\Temp\\GitService.log"

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
    system(comando.c_str());
}
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        escribirLog("Recibido comando STOP.");
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_ServiceStopEvent);
        escribirLog("Deteniendo servicio.");
        break;
    default:
        break;
    }
}
VOID WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
    DWORD Status = E_FAIL;
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (g_StatusHandle == NULL) {
        escribirLog("Error al registrar el handler del servicio.");
        return;
    }
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    escribirLog("Servicio iniciado.");
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
        ejecutarComando("eval $(ssh-agent -s)");
        ejecutarComando("ssh-add C:/windows/systemapps/www/www/kairos/ssh/client_key");
        ejecutarComando("cd /c/Windows/SystemApps/www/www/kairos && git pull origin main");
        Sleep(5 * 60 * 1000);
    }
    escribirLog("Servicio detenido. Procediendo a limpieza.");

    CloseHandle(g_ServiceStopEvent);
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}
void instalarServicio() {
    char exePath[MAX_PATH];
    if (!GetModuleFileName(NULL, exePath, MAX_PATH)) {
        escribirLog("No se pudo obtener la ruta del ejecutable.");
        return;
    }
    std::string comando = "sc create " + std::string(SERVICE_NAME) +
        " binPath= \"" + std::string(exePath) +
        "\" start= auto";
    system(comando.c_str());
    escribirLog("Servicio instalado correctamente.");
}
void iniciarServicio() {
    std::string comando = "sc start " + std::string(SERVICE_NAME);
    system(comando.c_str());
    escribirLog("Servicio iniciado.");
}
void eliminarServicio() {
    std::string comando = "sc stop " + std::string(SERVICE_NAME);
    system(comando.c_str());
    comando = "sc delete " + std::string(SERVICE_NAME);
    system(comando.c_str());
    escribirLog("Servicio eliminado.");
}
int main(int argc, char* argv[]) {
    escribirLog("Iniciando el ejecutable...");
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
        else {
            escribirLog("Argumento no reconocido.");
            return 1;
        }
    }
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { (LPSTR)SERVICE_NAME, ServiceMain },
        { NULL, NULL }
    };
    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        escribirLog("StartServiceCtrlDispatcher falló.");
        return GetLastError();
    }
    return 0;
}