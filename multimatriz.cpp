#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <unistd.h>

using namespace std;

string matriza, matrizb;

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

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        cout << "Uso:\n  " << argv[0] << " <archivo_matriz_A> <archivo_matriz_B>\n  " << argv[0] << " <N> <archivo_matriz_A> <archivo_matriz_B>\n";
        return 1;
    }
    int expected_n = 0;
    int idxA = 1, idxB = 2;
    if (argc == 4) {
        try {
            expected_n = stoi(argv[1]);
        } catch (...) {
            cout << "ERROR: Primer argumento no es un entero válido para N.\n";
            return 1;
        }
        idxA = 2; idxB = 3;
    }
    matriza = argv[idxA];
    matrizb = argv[idxB];
    int nA, nB;
    vector<vector<int>> A = leerMatrizDesdeArchivo(matriza, nA);
    vector<vector<int>> B = leerMatrizDesdeArchivo(matrizb, nB);
    if (expected_n > 0) {
        if (nA != expected_n || nB != expected_n) {
            cout << "ERROR: Las matrices no coinciden con N=" << expected_n << " ("
                 << nA << " vs " << nB << ").\n";
            return 1;
        }
    }
    if (nA != nB) {
        cout << "ERROR: Las matrices no tienen el mismo tamaño ("
             << nA << "x" << nA << " vs " << nB << "x" << nB << ").\n";
        return 1;
    }
    int n = nA;
    vector<vector<int>> C = multiplicarMatrices(A, B, n);
    cout << "Resultado de la multiplicación AxB:\n";
    cout << '\n';
    imprimirMatriz(C);

    return 0;
}
