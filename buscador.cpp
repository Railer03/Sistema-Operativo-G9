#include <bits/stdc++.h>
using namespace std;
namespace fs = std::filesystem;

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a==string::npos)?"":s.substr(a, b-a+1);
}

string read_env_str(const string& key) {
    ifstream f(".env");
    if (!f.is_open()) return "";
    string line;
    while (getline(f, line)) {
        auto p = line.find('#'); if (p!=string::npos) line = line.substr(0,p);
        line = trim(line);
        if (line.rfind(key + "=", 0) == 0) {
            string val = trim(line.substr(key.size()+1));
            if (val.size()>=2 && ((val.front()=='\'' && val.back()=='\'') || (val.front()=='\"' && val.back()=='\"'))) val = val.substr(1,val.size()-2);
            return val;
        }
    }
    return "";
}

string run_cmd_capture(const string &cmd) {
    string out;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    pclose(pipe);
    return out;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: buscador <ruta_idx>\n";
        return 1;
    }
    string idxPath = argv[1];
    if (!fs::exists(idxPath)) {
        cerr << "El archivo índice no existe: " << idxPath << "\n";
        return 1;
    }

    string cacheApp = read_env_str("CACHE_APP");
    if (cacheApp.empty()) {
        const char* e = getenv("CACHE_APP");
        if (e) cacheApp = e;
    }
    if (cacheApp.empty()) {
        cerr << "CACHE_APP no configurado en .env\n";
        return 1;
    }

    cout << "BUSCADOR SistOpe (PID=" << getpid() << ")\n";
    cout << "Escriba consultas (ENTER). Escriba 'salir' para terminar.\n";

    while (true) {
        cout << "\nConsulta> ";
        string line;
        if (!std::getline(cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;
        if (line == "salir" || line == "exit" || line == "q") break;

        // ejecutar cache: cacheApp idxPath query
        // cuidado con comillas
        string cmd = cacheApp + " \"" + idxPath + "\" \"" + line + "\"";
        string response = run_cmd_capture(cmd);
        if (response.empty()) {
            cout << "Respuesta vacía o error al invocar cache/motor.\n";
            continue;
        }

        cout << "\n--- RESPUESTA (PID=" << getpid() << ") ---\n";
        cout << response << "\n";
    }

    cout << "Saliendo buscador.\n";
    return 0;
}