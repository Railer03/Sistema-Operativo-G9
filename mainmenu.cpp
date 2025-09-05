#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <map>
#include <algorithm>
#include <limits>

using namespace std;

struct Usuario {
    int id;
    char nombre[20];
    char username[20];
    char password[20];
    char perfil[8];
};

vector<Usuario> usuarios;
string user_file;
string atrivute;
string perfil_file;
map<string, vector<int>> opciones_perfil;

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

void cargar_env() {     //carga el archivo .env
    ifstream env(".env");
    if (!env.is_open()) { 
        cout << "ERROR: No se encontró el archivo .env.\n"; 
        exit(1); 
    }

    string linea;
    while (getline(env, linea)) {
        size_t pos_user = linea.find("USER_FILE=");
        size_t pos_perfil = linea.find("PERFIL_FILE=");
        if (pos_user != string::npos) {
            user_file = trim(linea.substr(pos_user + 10));
        }
        if (pos_perfil != string::npos) {
            perfil_file = trim(linea.substr(pos_perfil + 12));
        }
    }
}

void cargar_usuarios() {        //carga el archivo con los datos de los usuarios
    usuarios.clear();
    ifstream f(user_file);
    string linea;
    int line_count = 0;
    while (getline(f, linea)) {
        line_count++;
        Usuario u;
        char nombre[20], username[20], password[20], perfil[8];
        int id;
        int n = sscanf(linea.c_str(), "%d | %19[^|] | %19[^|] | %19[^|] | %7[^|]", &id, nombre, username, password, perfil);
        if (n == 5) {
            // Limpia espacios y saltos de línea de cada campo
            strncpy(u.nombre, trim(nombre).c_str(), 19); u.nombre[19] = '\0';
            strncpy(u.username, trim(username).c_str(), 19); u.username[19] = '\0';
            strncpy(u.password, trim(password).c_str(), 19); u.password[19] = '\0';
            strncpy(u.perfil, trim(perfil).c_str(), 7); u.perfil[7] = '\0';
            u.id = id;
            if (u.id > 0) {
                usuarios.push_back(u);
            }
        }
    }
}

void cargar_perfiles(const string& perfil_file) {
    opciones_perfil.clear();
    ifstream f(perfil_file);
    if (!f.is_open()) {
        cerr << "Error: no se pudo abrir " << perfil_file << endl;
        exit(1);
    }
    string linea;
    while (getline(f, linea)) {
        size_t sep = linea.find(':');
        if (sep == string::npos) continue;
        string perfil = trim(linea.substr(0, sep));
        string opciones = linea.substr(sep + 1);
        vector<int> ops;
        size_t start = 0, end;
        while ((end = opciones.find(',', start)) != string::npos) {
            ops.push_back(stoi(opciones.substr(start, end - start)));
            start = end + 1;
        }
        ops.push_back(stoi(opciones.substr(start)));
        opciones_perfil[perfil] = ops;
    }
}

void mostrar_menu_perfil(const string& perfil) {    //despliega las opciones para el usuario
    vector<string> menu = {
        "Salir",                // 0
        "Admin usuario",        // 1
        "Multiplicador Matrices NxN",      // 2
        "Juego",                // 3
        "Es palindromo?",       // 4
        "Calcular F(x)",        // 5
        "Conteo vocales"        // 6
    };
    cout << "\n--- MENU " << perfil << " ---\n";
    for (int op : opciones_perfil[perfil]) {
        if (op >= 0 && op < (int)menu.size())
            cout << op << ") " << menu[op] << "\n";
    }
    cout << "Elija una opción: ";
}

bool iniciar_sesion(int argc, char* argv[]) {
    string usuarioArg, passArg, archivoArg;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-u" && i + 1 < argc) usuarioArg = argv[++i];
        else if (arg == "-p" && i + 1 < argc) passArg = argv[++i];
        else if (arg == "-f" && i + 1 < argc) archivoArg = argv[++i];
    }

    if (usuarioArg.empty() || passArg.empty() || archivoArg.empty()) {
        cerr << "Uso: " << argv[0] << " -u <usuario> -p <password> -f <archivo>\n";
        return false;
    }

    Usuario usuarioLogueado;
    bool valido = false;

    for (auto &u : usuarios) {
        if (string(u.username) == usuarioArg && string(u.password) == passArg) {
            usuarioLogueado = u;
            valido = true;
            break;
        }
    }

    if (!valido) {
        cout << "Usuario o contraseña incorrectos.\n";
        return false;
    }

    atrivute = string(usuarioLogueado.perfil);
    cout << "Bienvenido " << usuarioLogueado.nombre
         << " (" << usuarioLogueado.perfil << ")\n";

    return true;
}
/// Funciones menu

void endesarrollo(){
    cout<<"En desarrollo.\n";
}

bool es_palindromo(const string &s) {   //funcion para checkear si un string es palindromo
    string tmp;
    for (char c : s) if (isalnum((unsigned char)c)) tmp.push_back(tolower((unsigned char)c));
    int i=0, j=(int)tmp.size()-1;
    while (i<j) { if (tmp[i]!=tmp[j]) return false; ++i; --j; }     //recorre ambos lados de un string  para confirmar si es palindromo o no
    return true;
}

void ui_palindromo() {  //texto en pantalla para el usuario a la hora de checkear palindromos
    while (true) {
        cout << "\n--- PALINDROMO ---\n1) Validar (ingresar texto)\n2) Cancelar\nElija una opción: ";
        int o;
        if (!(cin >> o)) { cin.clear(); cin.ignore(numeric_limits<streamsize>::max(),'\n'); continue; }
        cin.ignore(numeric_limits<streamsize>::max(),'\n');
        if (o == 2) return;
        if (o == 1) {
            cout << "Ingrese texto: ";
            string linea; getline(cin, linea);
            if (es_palindromo(linea)) cout << "Resultado: ES palíndromo.\n";
            else cout << "Resultado: NO es palíndromo.\n";
            cout << "Presione ENTER para continuar..."; cin.get();
        }
    }
}

void fx() {     //funcion matematica, calcula el valor de x*x + 2*x + 8
    while (true) {
        cout << "\n--- CALCULAR f(x) = x*x + 2*x + 8 ---\n";
        cout << "Ingrese X (o 'V' para VOLVER): ";
        string inp; getline(cin, inp);
        string t = trim(inp);
        if (t == "v" || t == "V") return;   //salir de la funcion
        try {       //intenta operar la funcion f(x), si la entrada no es valida, presenta un mensaje de error
            double x = stod(t);
            double fx = x*x + 2.0*x + 8.0;
            cout << "f(" << x << ") = " << x << "*" << x << " + 2*" << x << " + 8 = " << fx << "\n";
        } catch (...) {
            cout << "Entrada no válida. Intente de nuevo.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    cargar_env();
    cargar_usuarios();  

    if (!iniciar_sesion(argc, argv)) return 1;
    cargar_perfiles(perfil_file);
    int op;
    while (true) {
        mostrar_menu_perfil(atrivute);
        cin >> op; cin.ignore();
        if (find(opciones_perfil[atrivute].begin(), opciones_perfil[atrivute].end(), op) == opciones_perfil[atrivute].end()) {
            cout << "Opción inválida para su perfil.\n";
            continue;
        }
        switch (op) {   
            case 0:
                cout << "Saliendo...\n"; 
                return 0;
            case 1:
                endesarrollo();
                break;
            case 2:
                endesarrollo();
                break;
            case 3:
                endesarrollo();
                break;
            case 4:
                ui_palindromo();
                break;
            case 5:
                fx(); 
                break;
            case 6:
                cout << "Función para opción 6.\n"; break;
            default:
                cout << "Opción no implementada.\n"; break;
        }
    }

    return 0;
}
