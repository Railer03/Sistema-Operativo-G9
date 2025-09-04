#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <string>
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
int next_id = 1;
string user_file;

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

    string contenido((istreambuf_iterator<char>(env)), {});
    size_t pos = contenido.find("USER_FILE=");
    if (pos != string::npos) {
        size_t end = contenido.find('\n', pos);
        user_file = contenido.substr(pos + 10, end - pos - 10);
        user_file = trim(user_file); // Elimina espacios y saltos de línea
    }
}

void cargar_usuarios() {        //lee el archivo usuarios.txt y los pasa al vector usuarios
    usuarios.clear();
    ifstream f(user_file);
    string linea;
    int line_count = 0;
    while (getline(f, linea)) {
        line_count++;
        Usuario u;
        char nombre[20], username[20], password[20], perfil[7];
        int id;
        sscanf(linea.c_str(), "%d | %19[^|] | %19[^|] | %19[^|] | %6[^|]", &id, nombre, username, password, perfil);
        u.id = id;
        strncpy(u.nombre, nombre, 19); u.nombre[19] = '\0';
        strncpy(u.username, username, 19); u.username[19] = '\0';
        strncpy(u.password, password, 19); u.password[19] = '\0';
        strncpy(u.perfil, perfil, 6); u.perfil[6] = '\0';
        if (u.id > 0) {
            usuarios.push_back(u);
        }
    }
    next_id = line_count + 1;
}


void guardar_usuarios() {       //guarda los cambios hechos en agregar_usuarios al archivo .txt
    ofstream f(user_file);
    for (size_t i = 0; i < usuarios.size(); i++) {
        const Usuario& u = usuarios[i];
        f << u.id << " | " << u.nombre << " | " << u.username << " | " << u.password << " | " << u.perfil << "\n";
    }
}

void mostrar_menu() {       //imprime las opciones al usuario
    cout << "\n--- MENU USUARIOS ---\n";
    cout << "1) Agregar usuario\n";
    cout << "2) Listar usuarios\n";
    cout << "3) Eliminar usuario\n";
    cout << "0) Salir\n";
    cout << "Elija una opción: ";
}

const char PERFIL_ADMIN[] = "ADMIN";
const char PERFIL_GENERAL[] = "GENERAL";

void agregar_usuario() {
    Usuario u;      //se crea una instancia del struct, la cual se llenan los datos para luego ser agreagda a usuarios
    u.id = next_id++;
    cout << "Nombre: "; cin >> ws; cin.getline(u.nombre, 20);
    cout << "Username: "; cin.getline(u.username, 20);
    cout << "Password: "; cin.getline(u.password, 20);

    while (true) {
        cout << "Perfil (ADMIN o GENERAL): ";   //confirma si el usuario pertenece a admin o general
        cin.getline(u.perfil, 8);
        if (strcmp(u.perfil, PERFIL_ADMIN) == 0 || strcmp(u.perfil, PERFIL_GENERAL) == 0) {
            break;
        }
        cout << "Perfil inválido. Debe ser ADMIN o GENERAL.\n";
    }

    cout << "\n1) Guardar\n2) Cancelar\nElija una opción: ";
    int op;
    while (!(cin >> op) || (op != 1 && op != 2)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    cin.ignore();
    if (op == 1) {      //guarda los usuarios a el vector de struct y al .txt
        usuarios.push_back(u);
        guardar_usuarios();
        cout << "Usuario agregado con ID " << u.id << ".\n";
    } else {
        cout << "Operación cancelada.\n";
        next_id--; 
    }
}

void listar_usuarios() {
    if (usuarios.empty()) {
        cout << "No hay usuarios.\n";
    } else {
        cout << "ID | Nombre | Username | Password | Perfil\n";
        for (size_t i = 0; i < usuarios.size(); i++) {
            const Usuario& u = usuarios[i];
            cout << u.id << " | " << u.nombre << " | " << u.username << " | " << u.password << " | " << u.perfil << "\n";
        }
    }
    cout << "\n1) Volver\n Opción: \n";
    int op; 
    while (!(cin >> op) || op != 1) { 
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}
cin.ignore();
}

void eliminar_usuario() {
    cout << "ID a eliminar: ";
    int id;
    cin >> id; cin.ignore();
    size_t i;
    for (i = 0; i < usuarios.size(); i++) {     //busca el usuario a eliminar por la ID ingresada
        if (usuarios[i].id == id) break;
    }

    if (i == usuarios.size()) {
        cout << "No existe ese ID.\n";
        return;
    }

    cout << "\n1) Guardar\n2) Cancelar\nElija una opción: ";
    int op;
    while (!(cin >> op) || (op != 1 && op != 2)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    cin.ignore();
    if (op == 1) {
        usuarios.erase(usuarios.begin() + i);       //elimina al usuario de el vector, y guarda los cambios
        guardar_usuarios();
        cout << "Usuario eliminado.\n";
    } else {
        cout << "Operación cancelada.\n";
    }
}

int main() {
    cargar_env();
    cargar_usuarios();
    while (true) {
        mostrar_menu();
        int op;
        cin >> op; cin.ignore();            //recive la opcion escojida por el usuario
        if (op == 0) break;
        if (op == 1) agregar_usuario();
        else if (op == 2) listar_usuarios();
        else if (op == 3) eliminar_usuario();
        else cout << "Opción inválida.\n";
    }
    cout << "Fin del programa.\n";
    return 0;
}