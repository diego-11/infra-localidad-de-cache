// Recorrido de una matriz por filas y por columnas.
//
// La matriz se guarda en un solo vector, fila tras fila: el elemento (i, j)
// está en la posición i * N + j. Las dos funciones deben sumar exactamente los
// mismos elementos; lo único que cambia es el orden en que los visitan.
#include <chrono>
#include <cstdio>
#include <vector>

using namespace std;
using namespace std::chrono;

const int N = 2048;

// TODO: sumar todos los elementos recorriendo fila por fila.
long por_filas(const vector<long> &m) {
  long suma = 0;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      suma += m[i * N + j];
  
    }
    
  }
  return suma;
}

// TODO: sumar todos los elementos recorriendo columna por columna.
long por_columnas(const vector<long> &m) {
  long suma = 0;
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      suma += m[i * N + j];
      
    }
  }return suma;
  
}

int main() {
  vector<long> m(static_cast<size_t>(N) * N, 1);

  auto t0 = high_resolution_clock::now();
  long s1 = por_filas(m);
  auto t1 = high_resolution_clock::now();
  long s2 = por_columnas(m);
  auto t2 = high_resolution_clock::now();

  printf("filas %ld ms suma %ld\n",
         duration_cast<milliseconds>(t1 - t0).count(), s1);
  printf("columnas %ld ms suma %ld\n",
         duration_cast<milliseconds>(t2 - t1).count(), s2);
  return 0;
}
