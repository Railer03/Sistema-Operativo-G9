#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <thread>

using namespace std;
namespace fs = std::filesystem;

string py_script;

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

        if (key == "PY_SCRIPT") py_script = val;
    }
    env.close();
}

string timestamp_now() {
    using namespace chrono;
    auto now = system_clock::now();
    time_t t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    ostringstream ss;
    ss << put_time(localtime(&t), "%Y%m%d_%H%M%S") << '_' << setfill('0') << setw(3) << ms.count();
    return ss.str();
}

int main(int argc, char* argv[]) {
    cargar_env();
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <salida.idx> <path-carpeta-libros>\n";
        cerr << "Este programa ejecutará createindexparalelo con diferentes cantidades de threads.\n";
        return 1;
    }

    string nombreIdx = argv[1];
    string pathCarpeta = argv[2];

    // Detectar cantidad de núcleos disponibles
    unsigned int num_cores = thread::hardware_concurrency();
    if (num_cores == 0) num_cores = 1; // fallback si no se puede detectar

    cout << "\n========================================\n";
    cout << "  Benchmark de Threads\n";
    cout << "========================================\n";
    cout << "Núcleos de CPU detectados: " << num_cores << "\n";
    cout << "========================================\n\n";

    // Solicitar tamaño del arreglo de threads
    int tamano_arreglo;
    cout << "¿Cuántas configuraciones de threads desea probar? ";
    cin >> tamano_arreglo;

    if (tamano_arreglo <= 0 || tamano_arreglo > 100) {
        cerr << "Error: El tamaño debe ser entre 1 y 100.\n";
        return 1;
    }

    // Solicitar cantidad de threads para cada configuración (sin repetidos)
    vector<int> CANT_THREADS;
    set<int> threads_usados;

    cout << "\nIngrese la cantidad de threads para cada configuración:\n";
    cout << "(Nota: No se permiten valores repetidos)\n\n";

    for (int i = 0; i < tamano_arreglo; i++) {
        int threads;
        while (true) {
            cout << "Configuración " << (i + 1) << " - Threads: ";
            cin >> threads;

            if (cin.fail()) {
                cin.clear();
                cin.ignore(10000, '\n');
                cout << "❌ Error: Ingrese un número válido.\n";
                continue;
            }

            if (threads <= 0) {
                cout << "❌ Error: La cantidad de threads debe ser mayor a 0.\n";
                continue;
            }

            if (threads_usados.find(threads) != threads_usados.end()) {
                cout << "❌ Error: El valor " << threads << " ya fue ingresado. Ingrese un valor diferente.\n";
                continue;
            }

            // Valor válido
            CANT_THREADS.push_back(threads);
            threads_usados.insert(threads);
            break;
        }
    }

    // Crear estructura de directorios para benchmarks (logs y gráficos)
    error_code ec;
    fs::path benchmarkLogDir = "logs/benchmark";
    fs::path latestLogDir = benchmarkLogDir / "latest";
    fs::path allLogDir = benchmarkLogDir / "all";
    
    fs::path benchmarkGraficoDir = "graficos/Benchmark";
    fs::path latestGraficoDir = benchmarkGraficoDir / "latest";
    fs::path allGraficoDir = benchmarkGraficoDir / "all";
    
    fs::create_directories(latestLogDir, ec);
    fs::create_directories(allLogDir, ec);
    fs::create_directories(latestGraficoDir, ec);
    fs::create_directories(allGraficoDir, ec);

    // Si latest tiene contenido, moverlo a all con timestamp
    string timestamp_move = timestamp_now();
    bool hayContenido = false;
    
    // Mover logs de latest a all
    if (!fs::is_empty(latestLogDir)) {
        hayContenido = true;
        fs::path destLogDir = allLogDir / timestamp_move;
        fs::create_directories(destLogDir, ec);
        
        for (const auto& entry : fs::directory_iterator(latestLogDir)) {
            fs::path destPath = destLogDir / entry.path().filename();
            fs::rename(entry.path(), destPath, ec);
        }
    }
    
    // Mover gráficos de latest a all
    if (!fs::is_empty(latestGraficoDir)) {
        hayContenido = true;
        fs::path destGraficoDir = allGraficoDir / timestamp_move;
        fs::create_directories(destGraficoDir, ec);
        
        for (const auto& entry : fs::directory_iterator(latestGraficoDir)) {
            fs::path destPath = destGraficoDir / entry.path().filename();
            fs::rename(entry.path(), destPath, ec);
        }
    }

    // Archivo de log para benchmark (se guardará en latest)
    string logFile = (latestLogDir / ("benchmark_threads_" + timestamp_now() + ".log")).string();
    ofstream logOut(logFile);
    if (!logOut.is_open()) {
        cerr << "Error: No se pudo crear el archivo de log: " << logFile << "\n";
        return 1;
    }

    cout << "\n========================================\n";
    cout << "Configuración del Benchmark:\n";
    cout << "========================================\n";
    cout << "  - Archivo de salida: " << nombreIdx << "\n";
    cout << "  - Carpeta de libros: " << pathCarpeta << "\n";
    cout << "  - Configuraciones de threads: ";
    for (size_t i = 0; i < CANT_THREADS.size(); i++) {
        cout << CANT_THREADS[i];
        if (i < CANT_THREADS.size() - 1) cout << ", ";
    }
    cout << "\n";
    cout << "  - Log de resultados: " << logFile << "\n";
    cout << "========================================\n";
    cout << "NOTA: N_LOTE se calculará automáticamente como:\n";
    cout << "      N_LOTE = total_libros / N_THREADS\n";
    cout << "========================================\n\n";

    // Escribir header del log
    logOut << "threads,tiempo_ms,tiempo_sec\n";

    // Ejecutar benchmark para cada cantidad de threads
    for (int threads : CANT_THREADS) {
        cout << "Ejecutando con " << threads << " thread(s)... " << flush;

        // N_LOTE se calcula automáticamente en createindexparalelo (pasamos 0)
        string cmd = "./paralelo \"" + nombreIdx + "\" \"" + pathCarpeta + "\" " 
                     + to_string(threads) + " 0 > /dev/null 2>&1";

        // Medir tiempo de ejecución
        auto inicio = chrono::high_resolution_clock::now();
        int resultado = system(cmd.c_str());
        auto fin = chrono::high_resolution_clock::now();

        if (resultado != 0) {
            cout << "ERROR (código " << resultado << ")\n";
            logOut << threads << ",-1,-1\n";
            continue;
        }

        // Calcular tiempo transcurrido
        auto duracion_ms = chrono::duration_cast<chrono::milliseconds>(fin - inicio).count();
        double duracion_sec = duracion_ms / 1000.0;

        cout << "OK (" << duracion_ms << " ms = " << fixed << setprecision(2) << duracion_sec << " s)\n";

        // Registrar en log
        logOut << threads << "," << duracion_ms << "," << fixed << setprecision(3) << duracion_sec << "\n";
        logOut.flush();
    }

    logOut.close();

    cout << "\n========================================\n";
    cout << "  Benchmark completado\n";
    cout << "========================================\n";
    cout << "Generando gráfico de rendimiento...\n";

    // Llamar al script de Python para generar el gráfico
    string pythonCmd = "python3 ";
    pythonCmd += py_script;
    pythonCmd += " \"" + logFile + "\"";
    int py_result = system(pythonCmd.c_str());

    if (py_result == 0) {
        // Extraer nombre del archivo sin extensión
        string logBasename = fs::path(logFile).filename().string();
        string graficoFilename = logBasename.substr(0, logBasename.rfind(".log")) + ".png";
        string graficoPath = "graficos/Benchmark/latest/" + graficoFilename;
        
        cout << "\n✅ Proceso completado exitosamente\n";
        cout << "   - Log: " << logFile << "\n";
        cout << "   - Gráfico: " << graficoPath << "\n\n";
        cout << "   - Log: " << logFile << "\n";
        cout << "   - Gráfico: " << graficoPath << "\n\n";
    } else {
        cout << "\n⚠️ El benchmark se completó, pero hubo un error generando el gráfico.\n";
        cout << "   - Log disponible en: " << logFile << "\n\n";
    }

    return 0;
}
