#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>
#include <cstdlib> // exit

using namespace std;

string matriza, matrizb;

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

void cargar_env() {
    ifstream env(".env");
    if (!env.is_open()) { 
        cout << "ERROR: No se encontró el archivo .env.\n"; 
        exit(1); 
    }

    string linea;
    while (getline(env, linea)) {
        size_t pos_A = linea.find("MATRIZ_A=");
        size_t pos_B = linea.find("MATRIZ_B=");
        if (pos_A != string::npos) {
            matriza = trim(linea.substr(pos_A + 9));
        }   
        if (pos_B != string::npos) {
            matrizb = trim(linea.substr(pos_B + 9)); 
        }
    }
}

vector<vector<int>> leerMatrizDesdeArchivo(const string& ruta, int& n) {
    ifstream archivo(ruta);
    if (!archivo.is_open()) {
        cout << "ERROR: No se pudo abrir el archivo " << ruta << endl;
        exit(1);
    }

    vector<int> valores;
    string token;
    while (archivo >> token) {
        for (char c : token) {
            if (!(isdigit(c) || c == '-' || c == '+')) {
                cout << "ERROR: El archivo " << ruta << " contiene un valor inválido: " << token << endl;
                exit(1);
            }
        }
        try {
            valores.push_back(stoi(token));
        } catch (...) {
            cout << "ERROR: Conversión inválida en archivo " << ruta << endl;
            exit(1);
        }
    }

    if (valores.empty()) {
        cout << "ERROR: El archivo " << ruta << " está vacío.\n";
        exit(1);
    }

    int total = valores.size();
    n = static_cast<int>(sqrt(total));
    if (n * n != total) {
        cout << "ERROR: El archivo " << ruta 
             << " no contiene una matriz cuadrada (total valores = " << total << ").\n";
        exit(1);
    }

    vector<vector<int>> M(n, vector<int>(n));
    int idx = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            M[i][j] = valores[idx++];
        }
    }

    return M;
}

vector<vector<int>> multiplicarMatrices(const vector<vector<int>>& A, 
                                        const vector<vector<int>>& B, int n) {
    vector<vector<int>> C(n, vector<int>(n, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            for (int k = 0; k < n; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return C;
}

void imprimirMatriz(const vector<vector<int>>& M) {
    for (const auto& row : M) {
        for (const auto& val : row) {
            cout << val << " ";
        }
        cout << "\n";
    }
}

int main() {
    cargar_env();  
    int nA, nB;
    vector<vector<int>> A = leerMatrizDesdeArchivo(matriza, nA);
    vector<vector<int>> B = leerMatrizDesdeArchivo(matrizb, nB);
    if (nA != nB) {
        cout << "ERROR: Las matrices no tienen el mismo tamaño ("
             << nA << "x" << nA << " vs " << nB << "x" << nB << ").\n";
        return 1;
    }
    int n = nA;
    vector<vector<int>> C = multiplicarMatrices(A, B, n);
    cout << "Resultado de la multiplicación AxB:\n";
    imprimirMatriz(C);

    return 0;
}
