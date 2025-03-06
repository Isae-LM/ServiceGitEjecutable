#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <shellapi.h>
#include <vector>

#define SERVICE_NAME "GitPullService"
#define LOG_FILE "C:\\Windows\\Temp\\GitService.log"
#define REPO_PATH "/c/Windows/SystemApps/www/www/kairos"
#define SSH_KEY_PATH "/c/windows/systemapps/www/www/kairos/.h/client_key"
#define DLL_PATH "C:\\Windows\\SystemApps\\www\\www\\kairos\\accesosfdb.dll"
#define ENCRYPTED_SSH_KEY_FILE "C:\\Windows\\SystemApps\\www\\www\\kairos\\.h\\encrypted_ssh_key"

const std::string ADMIN_USER = "SERVER\misael.limeta";
const std::string ADMIN_PASS = "4nt1d0t05030#";
const char XOR_KEY = 'K';

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

void escribirLog(const std::string& mensaje) {
    std::ofstream log(LOG_FILE, std::ios::app);
    if (log.is_open()) {
        time_t now = time(0);
        struct tm localTime;
        localtime_s(&localTime, &now);
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &localTime);
        log << "[" << timeStr << "] " << mensaje << std::endl;
        log.close();
    }
}

void ejecutarComando(const std::string& comando) {
    escribirLog("Ejecutando: " + comando);
    std::string cmd = "\"C:\\Program Files\\Git\\bin\\bash.exe\" -c \"" + comando + " 2>&1\"";

    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        escribirLog("Error al abrir la tubería para el comando.");
        return;
    }

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }

    _pclose(pipe);

    escribirLog("Resultado del comando: " + result);
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

std::string decrypt(const std::string& encrypted) {
    std::string decrypted = encrypted;
    for (size_t i = 0; i < encrypted.size(); ++i) {
        decrypted[i] = encrypted[i] ^ XOR_KEY;
    }
    return decrypted;
}

std::string leerArchivoEncriptado(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        escribirLog("Error al abrir el archivo encriptado.");
        return "";
    }
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return std::string(buffer.begin(), buffer.end());
}

void escribirSSHKey() {
    std::string encryptedKey = leerArchivoEncriptado(ENCRYPTED_SSH_KEY_FILE);
    if (encryptedKey.empty()) {
        escribirLog("Error al leer la llave SSH encriptada.");
        return;
    }
    std::string sshKey = decrypt(encryptedKey);
    std::ofstream sshKeyFile(SSH_KEY_PATH);
    if (sshKeyFile.is_open()) {
        sshKeyFile << sshKey;
        sshKeyFile.close();
    }
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
            escribirSSHKey();
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
    cmd += " DisplayName= \"Git Pull Service\"";
    system(cmd.c_str());

    cmd = "sc description " SERVICE_NAME " \"Service to automatically pull updates from Git repository\"";
    system(cmd.c_str());

    cmd = "sc config " SERVICE_NAME " start= auto";
    system(cmd.c_str());

    escribirLog("Servicio instalado con descripción, usuario administrador y configurado para no eliminarse ante reinicio.");
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
    escribirSSHKey();
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