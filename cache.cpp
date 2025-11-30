#include <bits/stdc++.h>
using namespace std;
namespace fs = std::filesystem;

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a==string::npos) ? "" : s.substr(a, b-a+1);
}

int read_env_int(const string& key, int def) {
    ifstream f(".env");
    if (!f.is_open()) return def;
    string line;
    while (getline(f, line)) {
        auto p = line.find('#');
        if (p != string::npos) line = line.substr(0, p);
        line = trim(line);
        if (line.rfind(key + "=", 0) == 0) {
            string val = trim(line.substr(key.size() + 1));
            try { return stoi(val); } catch(...) { return def; }
        }
    }
    return def;
}

string read_env_str(const string& key) {
    ifstream f(".env");
    if (!f.is_open()) return "";
    string line;
    while (getline(f, line)) {
        auto p = line.find('#');
        if (p != string::npos) line = line.substr(0, p);
        line = trim(line);
        if (line.rfind(key + "=", 0) == 0) {
            string val = trim(line.substr(key.size() + 1));
            if (val.size() >= 2 &&
               ((val.front()=='\'' && val.back()=='\'') ||
                (val.front()=='\"' && val.back()=='\"')))
            {
                val = val.substr(1, val.size()-2);
            }
            return val;
        }
    }
    return "";
}

string hash_str(const string& s) {
    std::hash<string> h;
    auto v = h(s);
    stringstream ss; ss << std::hex << v;
    return ss.str();
}

struct MetaEntry { string hash, query; uint64_t at; };

vector<MetaEntry> load_meta(const fs::path &metaPath) {
    vector<MetaEntry> out;
    if (!fs::exists(metaPath)) return out;
    ifstream f(metaPath);
    string line;
    while (getline(f, line)) {
        if (line.empty()) continue;

        auto a = line.find('\t');
        auto b = (a == string::npos) ? string::npos : line.find('\t', a+1);
        if (a==string::npos || b==string::npos) continue;

        MetaEntry e;
        e.hash = line.substr(0, a);
        e.at   = (uint64_t)stoull(line.substr(a+1, b-a-1));
        e.query = line.substr(b+1);
        out.push_back(e);
    }
    return out;
}

void save_meta(const fs::path &metaPath, const vector<MetaEntry> &v) {
    ofstream f(metaPath, ios::trunc);
    for (auto &e : v) {
        f << e.hash << "\t" << e.at << "\t" << e.query << "\n";
    }
}

// EJECUTAR MOTOR CAPTURANDO OUTPUT
string run_cmd_capture(const string &cmd) {
    string out;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    pclose(pipe);
    return out;
}

// ESCAPAR STRING PARA JSON
string json_escape(const string &s) {
    string out;
    for (char c : s) {
        switch(c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

// CARGAR MAPA_LIBROS.TXT
unordered_map<string,string> load_mapa_libros(const fs::path &dir) {
    unordered_map<string,string> m;
    fs::path p = dir / "mapa_libros.txt";
    if (!fs::exists(p)) return m;

    ifstream f(p);
    string line;
    while (getline(f, line)) {
        auto pos = line.find(';');
        if (pos == string::npos) continue;

        string id = trim(line.substr(0, pos));
        string name = trim(line.substr(pos+1));

        if (!name.empty()) {
            if (name.size() > 4 && name.substr(name.size()-4) == ".txt")
                name = name.substr(0, name.size()-4);
            name = trim(name);
        }

        if (!id.empty()) {
            m[id] = name;

            if (id.rfind("ID_", 0) == 0) {
                string plain = id.substr(3);
                if (!plain.empty()) m[plain] = name;
            } else {
                m["ID_" + id] = name;
            }
        }
    }
    return m;
}

// NAMING HELPER
static string to_lower(string s) {
    transform(s.begin(), s.end(), s.begin(),
              [](unsigned char c){ return tolower(c); });
    return s;
}

// DETERMINAR NOMBRE A MOSTRAR
string find_display_name(const unordered_map<string,string> &mapa,
                         const string &id, const string &title)
{
    if (!id.empty()) {
        auto it = mapa.find(id);
        if (it != mapa.end() && !it->second.empty()) return it->second;

        string pref = "ID_" + id;
        it = mapa.find(pref);
        if (it != mapa.end() && !it->second.empty()) return it->second;
    }

    if (!title.empty()) {
        string t = title;

        if (t.rfind("ID_", 0) == 0) {
            string plain = t.substr(3);
            auto it = mapa.find(plain);
            if (it != mapa.end()) return it->second;
        }

        if (t.size() > 4 && t.substr(t.size()-4) == ".txt")
            t = t.substr(0, t.size()-4);

        string ltitle = to_lower(t);
        for (const auto &kv : mapa) {
            if (!kv.second.empty() &&
                to_lower(kv.second).find(ltitle) != string::npos)
                return kv.second;
        }
    }

    if (!title.empty()) {
        if (title.rfind("ID_", 0) == 0)
            return title.substr(3);
        return title;
    }
    if (!id.empty()) return "ID_" + id;
    return "";
}

// PARSE MOTOR JSON: Extraer topK y pares (id,title,score)
void parse_motor_results(const string &motorJson,
                         vector<tuple<string,string,int>> &out,
                         int &topK)
{
    out.clear();
    topK = 0;

    size_t p = motorJson.find("\"topK\"");
    if (p != string::npos) {
        size_t colon = motorJson.find(':', p);
        if (colon != string::npos) {
            size_t i = colon+1;
            while (i < motorJson.size() && isspace((unsigned char)motorJson[i])) ++i;
            string num;
            while (i < motorJson.size() &&
                  (isdigit((unsigned char)motorJson[i]) || motorJson[i]=='-'))
            {
                num.push_back(motorJson[i]);
                ++i;
            }
            try { topK = stoi(num); } catch (...) { topK = 0; }
        }
    }

    size_t rpos = motorJson.find("\"results\"");
    if (rpos == string::npos) return;

    size_t arr = motorJson.find('[', rpos);
    if (arr == string::npos) return;

    size_t i = arr + 1;
    while (i < motorJson.size()) {
        size_t objStart = motorJson.find('{', i);
        if (objStart == string::npos) break;

        size_t objEnd = motorJson.find('}', objStart);
        if (objEnd == string::npos) break;

        string obj = motorJson.substr(objStart, objEnd - objStart + 1);

        auto get_str = [&](const string &field)->string {
            size_t pos = obj.find("\"" + field + "\"");
            if (pos == string::npos) return "";
            size_t col = obj.find(':', pos);
            if (col == string::npos) return "";
            size_t q1 = obj.find('"', col+1);
            if (q1 == string::npos) return "";
            size_t q2 = obj.find('"', q1+1);
            if (q2 == string::npos) return "";
            return obj.substr(q1+1, q2-q1-1);
        };

        string id    = get_str("id");
        string title = get_str("title");

        int score = 0;
        size_t spos = obj.find("\"score\"");
        if (spos != string::npos) {
            size_t scol = obj.find(':', spos);
            size_t j = scol+1;
            while (j < obj.size() && isspace((unsigned char)obj[j])) ++j;
            string sn;
            while (j < obj.size() && (isdigit((unsigned char)obj[j]) || obj[j]=='-')) {
                sn.push_back(obj[j]);
                j++;
            }
            try { score = stoi(sn); } catch(...) { score = 0; }
        }

        out.emplace_back(id, title, score);
        i = objEnd + 1;
    }
}

vector<pair<string,int>> procesarMotorB(const string& query, int topK) {
    vector<pair<string,int>> resultados;

    if (query == "El anillo Rojo" || query == "el anillo rojo") {
        resultados.push_back({"el señor de los anillos", 3});
        resultados.push_back({"militar", 2});
        resultados.push_back({"ojos rojos", 1});
    } else {
        for (int i = topK; i >= 1; i--) {
            resultados.push_back({"libro_" + to_string(i), i});
        }
    }

    if ((int)resultados.size() > topK) {
        resultados.resize(topK);
    }

    return resultados;
}

bool buscarEnCache(
    const string& query,
    vector<pair<string,int>>& resultados,
    long long& tiempo_cache
) {
    auto inicio = chrono::high_resolution_clock::now();

    // cache directory + meta file
    fs::path cacheDir = "cache_store";
    fs::path metaPath = cacheDir / "meta.txt";
    if (!fs::exists(cacheDir) || !fs::exists(metaPath)) {
        tiempo_cache = 0;
        return false;
    }

    // load meta and search by hash
    auto meta = load_meta(metaPath);
    string h = hash_str(query);
    for (auto &e : meta) {
        if (e.hash == h && e.query == query) {
            // found -> open file
            fs::path entryFile = cacheDir / (h + ".txt");
            if (!fs::exists(entryFile)) break;

            resultados.clear();
            ifstream f(entryFile);
            string line;
            while (getline(f, line)) {
                line = trim(line);
                if (line.empty()) continue;
                size_t sep = line.find('|');
                if (sep != string::npos) {
                    string libro = trim(line.substr(0, sep));
                    int score = 0;
                    try { score = stoi(trim(line.substr(sep+1))); } catch(...) { score = 0; }
                    resultados.push_back({libro, score});
                }
            }
            f.close();

            // update timestamp (LRU)
            e.at = (uint64_t)chrono::duration_cast<chrono::milliseconds>(
                chrono::system_clock::now().time_since_epoch()).count();
            save_meta(metaPath, meta);

            auto fin = chrono::high_resolution_clock::now();
            tiempo_cache =
                chrono::duration_cast<chrono::milliseconds>(fin - inicio).count();
            return true;
        }
    }

    auto fin = chrono::high_resolution_clock::now();
    tiempo_cache =
        chrono::duration_cast<chrono::milliseconds>(fin - inicio).count();
    return false;
}

void guardarEnCache(const string& query, const vector<pair<string,int>>& resultados) {
    // ensure dir
    fs::path cacheDir = "cache_store";
    if (!fs::exists(cacheDir)) fs::create_directories(cacheDir);
    fs::path metaPath = cacheDir / "meta.txt";

    // load and update meta
    auto meta = load_meta(metaPath);
    string h = hash_str(query);
    uint64_t now = (uint64_t)chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()).count();

    bool updated = false;
    for (auto &e : meta) {
        if (e.hash == h && e.query == query) {
            e.at = now;
            updated = true;
            break;
        }
    }
    if (!updated) {
        MetaEntry ne; ne.hash = h; ne.query = query; ne.at = now;
        meta.push_back(ne);
    }

    // write cache entry file (overwrite)
    fs::path entryFile = cacheDir / (h + ".txt");
    ofstream f(entryFile, ios::trunc);
    for (auto &r : resultados) {
        f << r.first << "|" << r.second << "\n";
    }
    f.close();

    // enforce CACHE_SIZE (LRU eviction)
    int CACHE_SIZE = read_env_int("CACHE_SIZE", 10);
    if ((int)meta.size() > CACHE_SIZE) {
        // sort ascending by timestamp -> oldest first
        sort(meta.begin(), meta.end(), [](const MetaEntry&a,const MetaEntry&b){ return a.at < b.at; });
        while ((int)meta.size() > CACHE_SIZE) {
            MetaEntry rem = meta.front();
            fs::path remFile = cacheDir / (rem.hash + ".txt");
            if (fs::exists(remFile)) fs::remove(remFile);
            meta.erase(meta.begin());
        }
    }

    // save meta
    save_meta(metaPath, meta);
}

string construirJSON(
    const string& query,
    const string& origen,
    long long tiempo_cache,
    long long tiempo_motorB,
    long long tiempo_total,
    int topK,
    const vector<pair<string,int>>& resultados
) {
    string json = "{\n";
    json += "  \"query\" : \"" + query + "\",\n";
    json += "  \"origen_respuesta\" : \"" + origen + "\",\n";
    json += "  \"tiempo_cache\" : " + to_string(tiempo_cache) + ",\n";
    json += "  \"tiempo_motorB\" : " + to_string(tiempo_motorB) + ",\n";
    json += "  \"tiempo_total\" : " + to_string(tiempo_total) + ",\n";
    json += "  \"topK\" : " + to_string(topK) + ",\n";
    json += "  \"respuesta\" : [\n";

    for (size_t i = 0; i < resultados.size(); i++) {
        json += "    { \"Libro\" : \"" + resultados[i].first +
                "\", \"score\" : " + to_string(resultados[i].second) + " }";

        if (i + 1 < resultados.size())
            json += ",";

        json += "\n";
    }

    json += "  ]\n";
    json += "}\n";

    return json;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Uso: cache <ruta_idx> <consulta>\n";
        return 1;
    }

    // ---------------------------------------
    // Leer ruta índice + consulta
    // ---------------------------------------
    string idxPath = argv[1];
    string query;
    for (int i = 2; i < argc; i++) {
        if (i > 2) query += " ";
        query += argv[i];
    }

    // ---------------------------------------
    // Leer .env (MOTOR_APP y CACHE_SIZE)
    // ---------------------------------------
    int CACHE_SIZE = read_env_int("CACHE_SIZE", 10);
    string motorApp = read_env_str("MOTOR_APP");

    if (motorApp.empty()) {
        const char* envVar = getenv("MOTOR_APP");
        if (envVar) motorApp = envVar;
    }
    if (motorApp.empty()) {
        cerr << "{\"error\":\"MOTOR_APP no configurado en .env\"}";
        return 1;
    }

    // ---------------------------------------
    // Verificar índice
    // ---------------------------------------
    fs::path idxP(idxPath);
    if (!fs::exists(idxP)) {
        cerr << "{\"error\":\"archivo idx no encontrado\"}";
        return 1;
    }

    // ---------------------------------------
    // Cargar mapa de nombres de libros
    // ---------------------------------------
    auto mapa = load_mapa_libros(idxP.parent_path());

    // ---------------------------------------
    // Preparar carpetas de cache
    // ---------------------------------------
    fs::path cacheDir = idxP.parent_path() / "cache_store";
    fs::create_directories(cacheDir);

    fs::path metaPath = cacheDir / "meta.txt";
    vector<MetaEntry> meta = load_meta(metaPath);

    string h = hash_str(query);
    fs::path entryFile = cacheDir / (h + ".json");

    uint64_t now = (uint64_t) chrono::duration_cast<chrono::seconds>(
        chrono::system_clock::now().time_since_epoch()
    ).count();

    auto time_ms = [] {
        return chrono::time_point_cast<chrono::milliseconds>(
            chrono::steady_clock::now()
        );
    };

    // -------------------------------------------------------
    // 1) Buscar en cache
    // -------------------------------------------------------
    if (fs::exists(entryFile)) {

        auto t0 = time_ms();

        // actualizar meta LRU
        bool found = false;
        for (auto& e : meta) {
            if (e.hash == h) { e.at = now; e.query = query; found = true; break; }
        }
        if (!found) meta.push_back({h, query, now});
        save_meta(metaPath, meta);

        // leer archivo
        ifstream f(entryFile);
        string motorJson((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());

        auto t1 = time_ms();
        long long ms_cache = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

        // parsear JSON del motor
        vector<tuple<string,string,int>> resultados;
        int topK = 0;
        parse_motor_results(motorJson, resultados, topK);

        // construir salida final
        stringstream out;
        out << "{\n";
        out << "  \"query\": \"" << json_escape(query) << "\",\n";
        out << "  \"origen_respuesta\": \"cache\",\n";
        out << "  \"tiempo_cache\": " << ms_cache << ",\n";
        out << "  \"tiempo_MotorB\": 0,\n";
        out << "  \"tiempo_total\": " << ms_cache << ",\n";
        out << "  \"topk\": " << topK << ",\n";
        out << "  \"Respuestas\": [\n";

        bool first = true;
        for (auto& r : resultados) {
            string id, title;
            int score;
            tie(id, title, score) = r;

            string display = find_display_name(mapa, id, title);

            if (!first) out << ",";
            first = false;

            out << "{\"Libro\":\"" << json_escape(display)
                << "\",\"score\":" << score << "}";
        }
        out << "]}";

        cout << out.str();
        return 0;
    }

    auto t0 = time_ms();
    string comando = motorApp + " \"" + idxPath + "\" \"" + query + "\"";
    string motorJson = run_cmd_capture(comando);
    auto t1 = time_ms();

    long long ms_motor = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

    if (motorJson.empty()) {
        cerr << "{\"error\":\"motor no devolvió resultado\"}";
        return 1;
    }

    // guardar en archivo cache
    ofstream outFile(entryFile);
    outFile << motorJson;
    outFile.close();

    // actualizar meta + LRU
    meta.push_back({h, query, now});

    if ((int)meta.size() > CACHE_SIZE) {
        sort(meta.begin(), meta.end(),
             [](const MetaEntry& a, const MetaEntry& b) { return a.at < b.at; });

        while ((int)meta.size() > CACHE_SIZE) {
            auto victim = meta.front();
            meta.erase(meta.begin());
            fs::path fdel = cacheDir / (victim.hash + ".json");
            if (fs::exists(fdel)) fs::remove(fdel);
        }
    }

    save_meta(metaPath, meta);

    // parsear motor
    vector<tuple<string,string,int>> resultados;
    int topK = 0;
    parse_motor_results(motorJson, resultados, topK);

    // construir JSON final (MotorB)
    stringstream finalOut;
    finalOut << "{\n";
    finalOut << "  \"query\": \"" << json_escape(query) << "\",\n";
    finalOut << "  \"origen_respuesta\": \"MotorB\",\n";
    finalOut << "  \"tiempo_cache\": 0,\n";
    finalOut << "  \"tiempo_MotorB\": " << ms_motor << ",\n";
    finalOut << "  \"tiempo_total\": " << ms_motor << ",\n";
    finalOut << "  \"topk\": " << topK << ",\n";
    finalOut << "  \"Respuestas\": [\n";

   bool first2 = true;
    for (auto &pr : resultados) {
        string id, title;
        int score;
        tie(id, title, score) = pr;

        string display = find_display_name(mapa, id, title);

        if (!first2) finalOut << ",\n";
        first2 = false;

        finalOut << "    { \"Libro\": \"" << json_escape(display)
                << "\", \"score\": " << score << " }";
    }

    finalOut << "\n  ]\n}";
    cout << finalOut.str();
    return 0;
}