#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

void to_lowercase(string &s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
}

vector<string> tokenize(const string &line) {
    vector<string> tokens;
    string token;
    for (char c : line) {
        if (isalnum(c)) {
            token += tolower(c);
        } else if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <nombre-archivo.idx> <path-carpeta>\n";
        return 1;
    }

    string nombreArchivo = argv[1];
    string pathCarpeta = argv[2];

    // Validar que la carpeta existe
    if (!fs::exists(pathCarpeta) || !fs::is_directory(pathCarpeta)) {
        cerr << "Error: La carpeta especificada no existe o no es válida.\n";
        return 1;
    }

    // Crear el índice invertido
    map<string, map<string, int>> indiceInvertido;

    for (const auto &entry : fs::directory_iterator(pathCarpeta)) {
        if (entry.is_regular_file()) {
            string nombreLibro = entry.path().filename().string();
            ifstream libro(entry.path());
            if (!libro.is_open()) {
                cerr << "Error: No se pudo abrir el archivo " << nombreLibro << ".\n";
                continue;
            }

            string linea;
            while (getline(libro, linea)) {
                vector<string> palabras = tokenize(linea);
                for (const string &palabra : palabras) {
                    indiceInvertido[palabra][nombreLibro]++;
                }
            }
        }
    }

    // Asegurar carpeta ./IDX y construir path de salida dentro de ella
    fs::path outdir = fs::current_path() / "IDX";
    if (!fs::exists(outdir)) {
        try {
            fs::create_directories(outdir);
        } catch (const fs::filesystem_error &e) {
            cerr << "Error: no se pudo crear la carpeta " << outdir.string() << ": " << e.what() << "\n";
            return 1;
        }
    }

    fs::path outpath = outdir / fs::path(nombreArchivo).filename(); // ignora rutas en el argumento

    // Guardar el índice en el archivo dentro de ./IDX
    ofstream archivoSalida(outpath);
    if (!archivoSalida.is_open()) {
        cerr << "Error: No se pudo crear el archivo " << outpath.string() << ".\n";
        return 1;
    }

    for (const auto &[palabra, documentos] : indiceInvertido) {
        archivoSalida << palabra;
        for (const auto &[doc, count] : documentos) {
            archivoSalida << ";(" << doc << "," << count << ")";
        }
        archivoSalida << "\n";
    }

    cout << "Índice invertido creado exitosamente en la carpeta /IDX" << ".\n";
    return 0;
}