#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <queue>

using namespace std;
namespace fs = std::filesystem;

map<string, map<string, int>> indiceInvertido;      // palabra -> (idLibro -> count)
mutex indiceMutex;
mutex queueMutex;
mutex logMutex;

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

void cargar_env(string &out_path_idx, string &out_log_file) {     //carga el archivo .env (devuelve rutas, vacío si no existe)
    out_path_idx.clear();
    out_log_file.clear();
    ifstream env(".env");
    if (!env.is_open()) {
        return;
    }

    string linea;
    while (getline(env, linea)) {
        // eliminar comentarios y trim
        auto posc = linea.find('#');
        if (posc != string::npos) linea = linea.substr(0, posc);
        linea = trim(linea);
        if (linea.empty()) continue;

        const string key1 = "PATH_IDX=";
        const string key2 = "LOG_FILE=";
        if (linea.rfind(key1, 0) == 0) {
            string val = trim(linea.substr(key1.size()));
            // quitar comillas si las hay
            if (val.size() >= 2 && ((val.front()=='"' && val.back()=='"') || (val.front()=='\'' && val.back()== '\'')))
                val = val.substr(1, val.size()-2);
            out_path_idx = val;
        } else if (linea.rfind(key2, 0) == 0) {
            string val = trim(linea.substr(key2.size()));
            if (val.size() >= 2 && ((val.front()=='"' && val.back()=='"') || (val.front()=='\'' && val.back()== '\'')))
                val = val.substr(1, val.size()-2);
            out_log_file = val;
        }
    }
    env.close();
}

// tokenize (convierte a minusculas)
vector<string> tokenize(const string &line) {
    vector<string> tokens;
    string token;
    for (char c : line) {
        if (isalnum(static_cast<unsigned char>(c))) {
            token += static_cast<char>(tolower(static_cast<unsigned char>(c)));
        } else if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

string timestamp_now_iso() {
    using namespace chrono;
    auto now = system_clock::now();
    time_t t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << put_time(localtime(&t), "%Y-%m-%d %H:%M:%S") << '.' << setfill('0') << setw(3) << ms.count();
    return ss.str();
}

string timestamp_now_filename() {
    using namespace chrono;
    auto now = system_clock::now();
    time_t t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << put_time(localtime(&t), "%Y%m%d_%H%M%S") << '_' << setfill('0') << setw(3) << ms.count();
    return ss.str();
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        cerr << "Uso: " << argv[0] << " <salida.idx> <path-carpeta-libros> <N_THREADS> <N_LOTE>\n";
        cerr << "      Nota: Si N_LOTE=0, se calculará automáticamente como total_libros/N_THREADS\n";
        return 1;
    }

    string nombreArchivo = argv[1];
    string pathCarpeta = argv[2];
    int N_THREADS = stoi(argv[3]);
    int N_LOTE = stoi(argv[4]);

    if (N_THREADS <= 0) {
        cerr << "N_THREADS debe ser > 0\n";
        return 1;
    }

    if (N_LOTE < 0) {
        cerr << "N_LOTE debe ser >= 0 (0 = automático)\n";
        return 1;
    }

    if (!fs::exists(pathCarpeta) || !fs::is_directory(pathCarpeta)) {
        cerr << "Error: La carpeta especificada no existe o no es válida.\n";
        return 1;
    }

    // Cargar rutas desde .env (si existen) para decidir dónde guardar mapa y logs/idx
    string env_idx_dir, env_log_dir;
    cargar_env(env_idx_dir, env_log_dir);

    fs::path idxDir = env_idx_dir.empty() ? fs::path(".") : fs::path(env_idx_dir);
    fs::path logsDir = env_log_dir.empty() ? fs::path("logs") : fs::path(env_log_dir);

    std::error_code ec;
    fs::create_directories(idxDir, ec);
    fs::create_directories(logsDir, ec);

    // El mapa de libros se guarda siempre con nombre fijo "mapa_libros.txt" dentro de PATH_IDX (o .)
    fs::path mapaLibrosPath = "mapa_libros.txt";

    // Recolectar archivos y asignar ids numéricos
    vector<pair<string,string>> archivos; // (ruta, nombre)
    for (const auto &entry : fs::directory_iterator(pathCarpeta)) {
        if (entry.is_regular_file()) {
            archivos.emplace_back(entry.path().string(), entry.path().filename().string());
        }
    }

    // Si N_LOTE es 0, calcularlo automáticamente de forma equitativa
    if (N_LOTE == 0) {
        size_t total_libros = archivos.size();
        if (total_libros == 0) {
            cerr << "Error: No se encontraron archivos en la carpeta especificada.\n";
            return 1;
        }
        // N_LOTE = ceil(total_libros / N_THREADS) para distribución equitativa
        N_LOTE = (total_libros + N_THREADS - 1) / N_THREADS;
        cout << "N_LOTE calculado automáticamente: " << N_LOTE 
             << " (total libros: " << total_libros 
             << ", threads: " << N_THREADS << ")\n";
    }

    // Crear MAPA-LIBROS (id;nombre) en mapa_libros.txt
    ofstream mapaOut(mapaLibrosPath.string());
    if (!mapaOut.is_open()) {
        cerr << "No se pudo crear " << mapaLibrosPath.string() << "\n";
        return 1;
    }
    // ids empiezan en 1
    vector<pair<string,string>> archivosConId; // (idStr, ruta)
    for (size_t i = 0; i < archivos.size(); ++i) {
        string id = to_string(i+1);
        mapaOut << id << ";" << archivos[i].second << "\n";
        archivosConId.emplace_back(id, archivos[i].first);
    }
    mapaOut.close();

    // preparar paths finales: .idx en PATH_IDX con mismo nombre; log en LOG_FILE con base + timestamp
    fs::path idxPath = idxDir / fs::path(nombreArchivo).filename();
    string base = fs::path(nombreArchivo).stem().string();
    string logFilename = (logsDir / (base + "_proceso_" + timestamp_now_filename() + ".log")).string();

    ofstream logOut(logFilename, ios::app);
    if (!logOut.is_open()) {
        cerr << "No se pudo abrir " << logFilename << " para escribir\n";
        return 1;
    }
    cout << "Log de proceso: " << logFilename << "\n";

    // Procesamiento por lotes
    for (size_t start = 0; start < archivosConId.size(); start += N_LOTE) {
        size_t end = min(start + N_LOTE, archivosConId.size());
        queue<size_t> q;
        for (size_t i = start; i < end; ++i) q.push(i);

        // Lanzar N_THREADS (o menos si batch pequeño)
        int threadsToStart = min((size_t)N_THREADS, end - start);
        vector<thread> workers;
        workers.reserve(threadsToStart);

        auto worker = [&](int workerIdx) {
            while (true) {
                size_t idx;
                {
                    lock_guard<mutex> lg(queueMutex);
                    if (q.empty()) break;
                    idx = q.front(); q.pop();
                }

                string idLibro = archivosConId[idx].first;
                string ruta = archivosConId[idx].second;

                string t_ini = timestamp_now_iso();
                int palabrasTot = 0;

                // ÍNDICE LOCAL para este libro (sin mutex)
                map<string, map<string, int>> indiceLocal;

                ifstream infile(ruta);
                if (infile.is_open()) {
                    string linea;
                    while (getline(infile, linea)) {
                        vector<string> toks = tokenize(linea);
                        palabrasTot += (int)toks.size();
                        // Construir índice LOCAL (SIN MUTEX - rápido)
                        for (const string &tok : toks) {
                            indiceLocal[tok][idLibro]++;
                        }
                    }
                    infile.close();
                } else {
                    // archivo no pudo abrirse: registrar con 0 palabras
                }

                // MERGE: Una sola vez al final (con mutex)
                {
                    lock_guard<mutex> lg(indiceMutex);
                    for (const auto &[palabra, docs] : indiceLocal) {
                        for (const auto &[docId, count] : docs) {
                            indiceInvertido[palabra][docId] += count;
                        }
                    }
                }

                string t_fin = timestamp_now_iso();

                // Registrar log (thread id, id libro, cantidad palabras, t_ini, t_fin)
                {
                    lock_guard<mutex> lg(logMutex);
                    logOut << "thread=" << this_thread::get_id()
                           << "; worker=" << workerIdx
                           << "; libro=" << idLibro
                           << "; palabras=" << palabrasTot
                           << "; inicio=" << t_ini
                           << "; fin=" << t_fin << "\n";
                    logOut.flush();
                }
            }
        };

        for (int i = 0; i < threadsToStart; ++i) {
            workers.emplace_back(worker, i);
        }
        for (auto &t : workers) t.join();
    }

    // al final: guardar índice en archivo .idx en idxPath
    ofstream archivoSalida(idxPath.string());
    if (!archivoSalida.is_open()) {
        cerr << "Error: No se pudo crear el archivo " << idxPath.string() << ".\n";
        return 1;
    }

    for (const auto &[palabra, documentos] : indiceInvertido) {
        archivoSalida << palabra;
        for (const auto &[doc, count] : documentos) {
            archivoSalida << ";(" << doc << "," << count << ")";
        }
        archivoSalida << "\n";
    }

    cout << "Índice invertido creado exitosamente en " << idxPath.string() << ".\n";
    return 0;
}