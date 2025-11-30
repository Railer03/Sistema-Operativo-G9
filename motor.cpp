#include <bits/stdc++.h>
using namespace std;
namespace fs = std::filesystem;

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a==string::npos)?"":s.substr(a, b-a+1);
}

vector<string> tokenize(const string &line) {
    vector<string> out; string cur;
    for (char c : line) {
        if (isalnum((unsigned char)c)) cur.push_back(tolower((unsigned char)c));
        else if (!cur.empty()) { out.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int read_env_int(const string& key, int def) {
    ifstream f(".env");
    if (!f.is_open()) return def;
    string line;
    while (getline(f, line)) {
        auto p = line.find('#'); if (p!=string::npos) line = line.substr(0,p);
        line = trim(line);
        if (line.rfind(key + "=", 0) == 0) {
            string val = trim(line.substr(key.size()+1));
            try { return stoi(val); } catch(...) { return def; }
        }
    }
    return def;
}

map<string,string> load_mapa_libros(const fs::path &dir) {
    map<string,string> m;
    fs::path p = dir / "mapa_libros.txt";
    if (!fs::exists(p)) return m;
    ifstream f(p);
    string line;
    while (getline(f, line)) {
        auto pos = line.find(';');
        if (pos==string::npos) continue;
        string id = trim(line.substr(0,pos));
        string name = trim(line.substr(pos+1));
        m[id]=name;
    }
    return m;
}

unordered_map<string, unordered_map<string,int>> load_idx(const fs::path &idxPath) {
    unordered_map<string, unordered_map<string,int>> idx;
    ifstream f(idxPath);
    string line;
    while (getline(f, line)) {
        if (line.empty()) continue;
        size_t pos = line.find(';');
        if (pos==string::npos) {
            continue;
        }
        string term = trim(line.substr(0,pos));
        size_t i = pos+1;
        while (i < line.size()) {
            if (line[i] == '(') {
                size_t comma = line.find(',', i);
                size_t close = line.find(')', i);
                if (comma==string::npos || close==string::npos) break;
                string id = trim(line.substr(i+1, comma-(i+1)));
                string cnt = trim(line.substr(comma+1, close-(comma+1)));
                try {
                    int c = stoi(cnt);
                    idx[term][id] = c;
                } catch(...) {}
                i = close + 1;
            } else ++i;
        }
    }
    return idx;
}

string json_escape(const string &s) {
    string out;
    for (char c: s) {
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

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Uso: motor <ruta_idx> <consulta>\n";
        return 1;
    }
    string idxPathStr = argv[1];
    // combine remaining argv as query
    string query;
    for (int i=2;i<argc;i++) { if (i>2) query += " "; query += argv[i]; }
    fs::path idxPath(idxPathStr);
    if (!fs::exists(idxPath)) {
        cerr << "{\"error\":\"archivo idx no encontrado\"}\n";
        return 1;
    }
    int TOPK = read_env_int("TOPK", 5);

    // load index and mapa
    auto idx = load_idx(idxPath);
    auto mapa = load_mapa_libros(idxPath.parent_path());

    // tokenize query
    vector<string> tokens = tokenize(query);
    if (tokens.empty()) {
        cout << "{\"pid\":" << getpid() << ",\"query\":\"" << json_escape(query) << "\",\"results\":[]}\n";
        return 0;
    }

    // scoring: sum of counts across tokens
    unordered_map<string, int> score;
    unordered_map<string, unordered_map<string,int>> details; // doc -> token->count
    for (const auto &t : tokens) {
        auto it = idx.find(t);
        if (it==idx.end()) continue;
        for (const auto & [doc, cnt] : it->second) {
            score[doc] += cnt;
            details[doc][t] = cnt;
        }
    }

    // vector results
    vector<pair<string,int>> vec;
    for (auto &p : score) vec.emplace_back(p.first, p.second);
    sort(vec.begin(), vec.end(), [](const auto &a, const auto &b){
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    if ((int)vec.size() > TOPK) vec.resize(TOPK);

    // build JSON
    stringstream ss;
    ss << "{";
    ss << "\"pid\":" << getpid() << ",";
    ss << "\"query\":\"" << json_escape(query) << "\",";
    ss << "\"timestamp\":\"" << chrono::system_clock::to_time_t(chrono::system_clock::now()) << "\",";
    ss << "\"topK\":" << TOPK << ",";
    ss << "\"results\":[";
    bool first = true;
    for (auto &r : vec) {
        if (!first) ss << ",";
        first=false;
        string id = r.first;
        int sc = r.second;
        string title = mapa.count(id) ? mapa[id] : string("ID_") + id;
        ss << "{";
        ss << "\"id\":\"" << json_escape(id) << "\",";
        ss << "\"title\":\"" << json_escape(title) << "\",";
        ss << "\"score\":" << sc << ",";
        ss << "\"counts\":{";
        bool f2 = true;
        for (auto &tc : details[id]) {
            if (!f2) ss << ",";
            f2=false;
            ss << "\"" << json_escape(tc.first) << "\":" << tc.second;
        }
        ss << "}";
        ss << "}";
    }
    ss << "]";
    ss << "}";
    cout << ss.str() << "\n";
    return 0;
}