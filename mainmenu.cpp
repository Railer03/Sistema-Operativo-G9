#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <map>
#include <algorithm>
#include <limits>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>

using namespace std;

struct Usuario {
    int id;
    char nombre[20];
    char username[20];
    char password[20];
    char perfil[8];
};

struct Perfil {
    string nombre;
    vector<int> opciones;
};

vector<Usuario> usuarios;
vector<Perfil> perfiles;
string user_file;
string atrivute;
string perfil_file;
string admin_sys;
string mutli_m;
string create_index;
string create_index_paralelo;
string path_idx;
string game_path; // Añadir esta variable global al inicio del archivo junto con las otras
string buscador_app; 
string cache_app;    
string motor_app;
string benchmark_app; // Añadir variable para el benchmark

// Función para limpiar la pantalla
void limpiar_pantalla() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Función para ejecutar comandos de manera segura
int ejecutar_comando(const string& comando, const vector<string>& argumentos) {
    string cmd = comando;
    for(const auto& arg : argumentos) {
        cmd += " \"" + arg + "\"";
    }
    int resultado = system(cmd.c_str());
    if (resultado == -1) {
        cout << "Error al ejecutar el comando: " << cmd << "\n";
    } else if (resultado != 0) {
        cout << "El comando terminó con errores\n";
    }
    return resultado;
}


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
        // eliminar comentarios y trim
        auto posc = linea.find('#');
        if (posc != string::npos) linea = linea.substr(0, posc);
        linea = trim(linea);
        if (linea.empty()) continue;

        auto eq = linea.find('=');
        if (eq == string::npos) continue;

        string key = trim(linea.substr(0, eq));
        string val = trim(linea.substr(eq + 1));

        // quitar comillas si las hay
        if (val.size() >= 2 &&
            ((val.front() == '"' && val.back() == '"') ||
             (val.front() == '\'' && val.back() == '\'')))
        {
            val = val.substr(1, val.size() - 2);
        }

        if (key == "USER_FILE") user_file = val;
        else if (key == "PERFIL_FILE") perfil_file = val;
        else if (key == "ADMIN_SYS") admin_sys = val;
        else if (key == "MUTLI_M") mutli_m = val;
        else if (key == "CREATE_INDEX") create_index = val;
        else if (key == "INDICE_INVET_PARALELO") create_index_paralelo = val;
        else if (key == "PATH_IDX") path_idx = val;
        else if (key == "GAME_SPEED") {
            // export to environment so child processes inherit it
            setenv("GAME_SPEED", val.c_str(), 1);
        } else if (key == "GAME_ACCELERATION") {
            setenv("GAME_ACCELERATION", val.c_str(), 1);
        } else if (key == "GAME") {
            game_path = val; // Guardar directamente en game_path en lugar de admin_sys
        }
        else if (key == "BUSCADOR") buscador_app = val;
        else if (key == "CACHE_APP") cache_app = val;
        else if (key == "MOTOR_APP") motor_app = val;
        else if (key == "BENCHMARK") benchmark_app = val;
    }
    env.close();
}

void cargar_usuarios() {
    usuarios.clear();
    ifstream fileObj(user_file); // Abrir el archivo en modo texto
    if (!fileObj.is_open()) {
        cerr << "ERROR: No se pudo abrir el archivo de usuarios: " << user_file << "\n";
        return;
    }

    Usuario tempUsuario;
    string line;
    while (getline(fileObj, line)) { // Leer línea por línea
        size_t pos = 0;

        try {
            tempUsuario.id = stoi(line.substr(0, line.find(" | ", pos)));
            pos = line.find(" | ", pos) + 3;

            string nombre = line.substr(pos, line.find(" | ", pos) - pos);
            strncpy(tempUsuario.nombre, nombre.c_str(), sizeof(tempUsuario.nombre));
            pos = line.find(" | ", pos) + 3;

            string username = line.substr(pos, line.find(" | ", pos) - pos);
            strncpy(tempUsuario.username, username.c_str(), sizeof(tempUsuario.username));
            pos = line.find(" | ", pos) + 3;

            string password = line.substr(pos, line.find(" | ", pos) - pos);
            strncpy(tempUsuario.password, password.c_str(), sizeof(tempUsuario.password));
            pos = line.find(" | ", pos) + 3;

            string perfil = line.substr(pos);
            strncpy(tempUsuario.perfil, perfil.c_str(), sizeof(tempUsuario.perfil));

            usuarios.push_back(tempUsuario);
        } catch (...) {
            cerr << "ERROR: Formato inválido en la línea: " << line << "\n";
        }
    }

    fileObj.close();
}

void cargar_perfiles(const string& perfil_file) {
    perfiles.clear();
    ifstream f(perfil_file);
    if (!f.is_open()) {
        cerr << "Error: no se pudo abrir " << perfil_file << endl;
        exit(1);
    }

    string linea;
    while (getline(f, linea)) {
        size_t sep = linea.find(':');
        if (sep == string::npos) continue;

        Perfil perfil;
        perfil.nombre = trim(linea.substr(0, sep));
        string opciones = linea.substr(sep + 1);

        size_t start = 0, end;
        while ((end = opciones.find(',', start)) != string::npos) {
            perfil.opciones.push_back(stoi(opciones.substr(start, end - start)));
            start = end + 1;
        }
        perfil.opciones.push_back(stoi(opciones.substr(start)));

        perfiles.push_back(perfil);
    }
}

void mostrar_menu_perfil(const string& perfil_nombre) {
    vector<string> menu = {
        "Salir",                // 0
        "Admin usuario",        // 1
        "Multiplicador Matrices NxN",      // 2
        "Juego",                // 3
        "Es palindromo?",       // 4
        "Calcular F(x) = x^2+2x+8",        // 5
        "Conteo sobre texto",       // 6
        "Crea indice invertido",    // 7
        "Crea indice invertido paralelo",  // 8
        "Lanzar Buscador",        // 9
        "Benchmark Threads"       // 10
    };

    auto it = find_if(perfiles.begin(), perfiles.end(), [&](const Perfil& p) {
        return p.nombre == perfil_nombre;
    });

    if (it == perfiles.end()) {
        cout << "Perfil no encontrado.\n";
        return;
    }

    cout << "\n--- MENU " << perfil_nombre << " ---\n";
    for (int op : it->opciones) {
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

    // Verificar si el archivo es .env (exacto o termina en .env)
    if (archivoArg == ".env" || (archivoArg.size() > 4 && archivoArg.substr(archivoArg.size() - 4) == ".env")) {
        cout << "Ingreso no concendido, se uso un archivo de maxima sensibilidad en ingreso. Use un .txt" << endl;
        return false;
    }

    ifstream archivo(archivoArg);
    if (!archivo.is_open()) {
        cerr << "ERROR: No se pudo abrir el archivo especificado: " << archivoArg << "\n";
        return false;
    }
    archivo.close();

    user_file = archivoArg; // Guardar el archivo en la variable global

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
    sleep(2); // Pausa de 2 segundos para leer el mensaje
    limpiar_pantalla();

    return true;
}
void cargar_aplicacion(const string& app) {
    ejecutar_comando(app, {});
}

/// Funciones menu

void endesarrollo(){
    cout<<"En desarrollo.\n";
    sleep(2);
}

bool es_palindromo(const string &s) {   //funcion para checkear si un string es palindromo
    
    string tmp;
    for (char c : s) if (isalnum((unsigned char)c)) tmp.push_back(tolower((unsigned char)c));
    int i=0, j=(int)tmp.size()-1;
    while (i<j) { if (tmp[i]!=tmp[j]) return false; ++i; --j; }     //recorre ambos lados de un string  para confirmar si es palindromo o no
    return true;
}

void ui_palindromo() {
    limpiar_pantalla();
    while (true) {
        cout << "\n--- PALINDROMO ---\n1) Continuar\n2) Cancelar\nElija una opción: ";
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

void fx() {
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

void conteo_sobre_texto() {
    limpiar_pantalla();
    ifstream archivo(user_file);
    if (!archivo.is_open()) {
        cerr << "ERROR: No se pudo abrir el archivo: " << user_file << "\n";
        return;
    }

    int vocales = 0, consonantes = 0, especiales = 0, palabras = 0;
    bool enPalabra = false;
    string linea;

    cout << "Procesando archivo: " << user_file << "\n";

    while (getline(archivo, linea)) {
        for (char c : linea) {
            if (isalpha(c)) {
                c = tolower(c);
                if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
                    vocales++;
                else
                    consonantes++;
                enPalabra = true;
            } else if (isspace(c)) {
                if (enPalabra) {
                    palabras++;
                    enPalabra = false;
                }
            } else {
                especiales++;
                if (enPalabra) {
                    palabras++;
                    enPalabra = false;
                }
            }
        }
        if (enPalabra) {
            palabras++;
            enPalabra = false;
        }
    }

    cout << "\nConteo\n";
    cout << "Vocales: " << vocales << "\n";
    cout << "Consonantes: " << consonantes << "\n";
    cout << "Caracteres especiales: " << especiales << "\n";
    cout << "Palabras: " << palabras << "\n";
    cout << "Presione ENTER para continuar...";
    cin.get();
}

void ui_multiplicador_matrices() {
    while (true) {
        limpiar_pantalla();
        cout << "\n--- MULTIPLICADOR DE MATRICES ---\n";
        cout << "1) Ingresar datos\n";
        cout << "2) Volver\n";
        cout << "Elija una opción: ";
        int opcion;
        if (!(cin >> opcion)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (opcion == 2) return;

        if (opcion == 1) {
            int n;
            string pathA, pathB;

            // Solicitar el tamaño de la matriz
            cout << "Ingrese el tamaño de la matriz (N): ";
            while (!(cin >> n) || n <= 0) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Entrada inválida. Ingrese un número entero positivo para N: ";
            }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            // Solicitar los paths de las matrices
            cout << "Ingrese el path de la matriz A: ";
            getline(cin, pathA);
            cout << "Ingrese el path de la matriz B: ";
            getline(cin, pathB);

            // Validar existencia de los archivos
            ifstream archivoA(pathA), archivoB(pathB);
            if (!archivoA.is_open() || !archivoB.is_open()) {
                cout << "Error: Uno o ambos archivos no existen.\n";
                continue;
            }
            archivoA.close();
            archivoB.close();

            // Confirmar y lanzar la ejecución
            cout << "\nDatos ingresados:\n";
            cout << "Tamaño de la matriz (N): " << n << "\n";
            cout << "Path de la matriz A: " << pathA << "\n";
            cout << "Path de la matriz B: " << pathB << "\n";
            cout << "¿Desea lanzar la ejecución? (1: Sí, 2: No): ";
            int confirmar;
            while (!(cin >> confirmar) || (confirmar != 1 && confirmar != 2)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Opción inválida. Ingrese 1 para Sí o 2 para No: ";
            }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (confirmar == 1) {
                if (mutli_m.empty()) {
                    cout << "No está configurado el ejecutable MUTLI_M en .env (MUTLI_M)\n";
                } else {
                    vector<string> args = { to_string(n), pathA, pathB };
                    int rc = ejecutar_comando(mutli_m, args);
                    cout << "Presione ENTER para continuar..."; cin.get();
                }
            } else {
                cout << "Ejecución cancelada.\n";
            }
        }
    }
}

void ui_crear_indice_invertido() {
    limpiar_pantalla();
    while (true) {
        cout << "\n--- CREAR ÍNDICE INVERTIDO ---\n";
        cout << "1) Ingresar datos\n";
        cout << "2) Volver\n";
        cout << "Elija una opción: ";
        int opcion;
        if (!(cin >> opcion)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (opcion == 2) return;

        if (opcion == 1) {
            string nombreArchivo, pathCarpeta;

            // Solicitar el nombre del archivo
            cout << "Ingrese el nombre del archivo a crear (debe tener extensión .idx): ";
            getline(cin, nombreArchivo);
            if (nombreArchivo.size() < 4 || nombreArchivo.substr(nombreArchivo.size() - 4) != ".idx") {
                cout << "Error: El archivo debe tener la extensión .idx.\n";
                continue;
            }

            // Solicitar el path de la carpeta
            cout << "Ingrese el path de la carpeta donde están los libros: ";
            getline(cin, pathCarpeta);

            // Validar existencia de la carpeta
            if (access(pathCarpeta.c_str(), F_OK) != 0) {
                cout << "Error: La carpeta especificada no existe.\n";
                continue;
            }

            // Confirmar y lanzar la ejecución
            cout << "\nDatos ingresados:\n";
            cout << "Nombre del archivo: " << nombreArchivo << "\n";
            cout << "Path de la carpeta: " << pathCarpeta << "\n";
            // Verificar si el archivo ya existe
            if (std::filesystem::exists(path_idx + nombreArchivo)) {
                cout << "Ya existe un archivo con este nombre. Si continúa con la ejecución se sobrescribirá! \n";
            }
            cout << "¿Desea lanzar la ejecución? (1: Sí, 2: No): ";
            int confirmar;
            while (!(cin >> confirmar) || (confirmar != 1 && confirmar != 2)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Opción inválida. Ingrese 1 para Sí o 2 para No: ";
            }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (confirmar == 1) {
                ejecutar_comando(create_index, {nombreArchivo, pathCarpeta});
            } else {
                cout << "Ejecución cancelada.\n";
            }
        }
    }
}

void ui_crear_indice_invertido_paralelo() {
    limpiar_pantalla();
    while (true) {
        cout << "\n--- CREAR ÍNDICE INVERTIDO PARALELO ---\n";
        cout << "1) Ingresar datos\n";
        cout << "2) Volver\n";
        cout << "Elija una opción: ";
        int opcion;
        if (!(cin >> opcion)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (opcion == 2) return;

        if (opcion == 1) {
            string nombreArchivo, pathCarpeta;
            int N_THREADS = 0, N_LOTE = 0;

            // Nombre del archivo .idx
            cout << "Ingrese el nombre del archivo a crear (debe tener extensión .idx): ";
            getline(cin, nombreArchivo);
            if (nombreArchivo.size() < 4 || nombreArchivo.substr(nombreArchivo.size() - 4) != ".idx") {
                cout << "Error: El archivo debe tener la extensión .idx.\n";
                continue;
            }

            // Path de la carpeta con los libros
            cout << "Ingrese el path de la carpeta donde están los libros: ";
            getline(cin, pathCarpeta);
            if (access(pathCarpeta.c_str(), F_OK) != 0) {
                cout << "Error: La carpeta especificada no existe.\n";
                continue;
            }

            // N_THREADS
            cout << "Ingrese N_THREADS (número de hilos a usar): ";
            string s;
            getline(cin, s);
            try {
                N_THREADS = stoi(trim(s));
                if (N_THREADS <= 0) throw 0;
            } catch (...) {
                cout << "Entrada inválida para N_THREADS.\n";
                continue;
            }

            // N_LOTE
            cout << "Ingrese N_LOTE (cantidad de libros por lote en memoria): ";
            getline(cin, s);
            try {
                N_LOTE = stoi(trim(s));
                if (N_LOTE <= 0) throw 0;
            } catch (...) {
                cout << "Entrada inválida para N_LOTE.\n";
                continue;
            }

            // Confirmación y advertencia si ya existe
            cout << "\nDatos ingresados:\n";
            cout << "Nombre del archivo: " << nombreArchivo << "\n";
            cout << "Path de la carpeta: " << pathCarpeta << "\n";
            cout << "N_THREADS: " << N_THREADS << "\n";
            cout << "N_LOTE: " << N_LOTE << "\n";

            if (std::filesystem::exists(path_idx + nombreArchivo)) {
                cout << "Ya existe un archivo con este nombre en IDX. Si continúa se sobrescribirá.\n";
            }

            cout << "¿Desea lanzar la ejecución? (1: Sí, 2: No): ";
            int confirmar;
            while (!(cin >> confirmar) || (confirmar != 1 && confirmar != 2)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Opción inválida. Ingrese 1 para Sí o 2 para No: ";
            }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (confirmar == 1) {
                string exe = create_index_paralelo.empty() ? "./paralelo" : create_index_paralelo;
                vector<string> args = { nombreArchivo, pathCarpeta, to_string(N_THREADS), to_string(N_LOTE) };
                ejecutar_comando(exe, args);
            } else {
                cout << "Ejecución cancelada.\n";
            }
        }
    }
}

void ui_benchmark_threads() {
    limpiar_pantalla();
    cout << "\n=== BENCHMARK: Análisis de Rendimiento de Threads ===\n\n";
    
    cout << "Nombre del archivo de salida .idx: ";
    string nombreArchivo;
    getline(cin, nombreArchivo);
    if (nombreArchivo.empty()) {
        cout << "Nombre de archivo vacío. Operación cancelada.\n";
        return;
    }
    
    cout << "Path de la carpeta con libros: ";
    string pathCarpeta;
    getline(cin, pathCarpeta);
    if (pathCarpeta.empty()) {
        cout << "Path de carpeta vacío. Operación cancelada.\n";
        return;
    }
    
    if (std::filesystem::exists(path_idx + nombreArchivo)) {
        cout << "\n⚠️  Ya existe un archivo con este nombre en IDX.\n";
        cout << "    Se sobrescribirá en cada ejecución del benchmark.\n";
    }
    
    cout << "\n¿Desea iniciar el benchmark? (1: Sí, 2: No): ";
    int confirmar;
    while (!(cin >> confirmar) || (confirmar != 1 && confirmar != 2)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Opción inválida. Ingrese 1 para Sí o 2 para No: ";
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
    if (confirmar == 1) {
        cout << "\n";
        string exe = benchmark_app.empty() ? "./benchmark_threads" : benchmark_app;
        vector<string> args = { nombreArchivo, pathCarpeta };
        ejecutar_comando(exe, args);
        cout << "\nPresione ENTER para continuar...";
        cin.get();
    } else {
        cout << "Benchmark cancelado.\n";
    }
}

void ui_lanzar_buscador() {
    // validar existencia ruta idx (path_idx) y pedir nombre de archivo .idx
    limpiar_pantalla();
    cout << "\n--- BUSCADOR SistOpe (Interfaz) ---\n";
    cout << "Ingrese nombre del archivo índice (.idx) ubicado en PATH_IDX (ej: libros.idx): ";
    string idxname; getline(cin, idxname);
    idxname = trim(idxname); // quitar espacios
    if (idxname.size() < 4 || idxname.substr(idxname.size()-4) != ".idx") {
        cout << "Nombre inválido. Debe terminar en .idx\n";
        sleep(2); return;
    }
    // construir path de forma consistente -> ambas ramas son std::filesystem::path
    std::filesystem::path base = path_idx.empty() ? std::filesystem::path(".") : std::filesystem::path(path_idx);
    std::filesystem::path idxFull = base / idxname;
    if (!std::filesystem::exists(idxFull)) {
        cout << "No existe el archivo índice: " << idxFull << "\n";
        sleep(2); return;
    }
    // ejecutar buscador (le pasamos el PATH_IDX y el nombre del índice)
    if (buscador_app.empty()) {
        cout << "No está configurado el ejecutable BUSCADOR en .env (BUSCADOR)\n";
        sleep(2); return;
    }
    ejecutar_comando(buscador_app, { idxFull.string() });
}

int main(int argc, char* argv[]) {
    cargar_env();
    cargar_usuarios();  

    if (!iniciar_sesion(argc, argv)) return 1;
    cargar_perfiles(perfil_file);
    cout << "PID proceso principal: " << getpid() << '\n' << endl;
    while (true) {
        //limpiar_pantalla();
        mostrar_menu_perfil(atrivute);
        int op;
        cin >> op; cin.ignore();
        auto it = find_if(perfiles.begin(), perfiles.end(), [&](const Perfil& p) {
            return p.nombre == atrivute;
        });
        if (it == perfiles.end()) {
            cout << "Error: Perfil no encontrado.\n";
            return 1;
        }
        if (find(it->opciones.begin(), it->opciones.end(), op) == it->opciones.end()) {
            cout << "Opción inválida para su perfil.\n";
            continue;
        }

        switch (op) {
            case 0:
                cout << "Saliendo...\n";
                sleep(1);
                limpiar_pantalla();
                return 0;
            case 1:
                cargar_aplicacion(admin_sys);
                limpiar_pantalla();
                break;
            case 2:
                ui_multiplicador_matrices();
                limpiar_pantalla();
                break;
            case 3:
                // Usar game_path en lugar de admin_sys
                cargar_aplicacion(game_path);
                limpiar_pantalla();
                break;
            case 4:
                ui_palindromo();
                limpiar_pantalla();
                break;
            case 5:
                fx();
                limpiar_pantalla();
                break;
            case 6:
                conteo_sobre_texto();
                limpiar_pantalla();
                break;
            case 7:
                ui_crear_indice_invertido();
                limpiar_pantalla();
                break;
            case 8:
                ui_crear_indice_invertido_paralelo();
                limpiar_pantalla();
                break;
            case 9:
                ui_lanzar_buscador();
                limpiar_pantalla();
                break;
            case 10:
                ui_benchmark_threads();
                limpiar_pantalla();
                break;
            
            default:
                cout << "Opción no implementada.\n"; break;
        }
    }
    return 0;
}