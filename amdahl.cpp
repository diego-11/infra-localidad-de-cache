// Speedup, eficiencia y la ley de Amdahl, medidos.
//
// El programa tiene dos partes. La primera genera los datos con una cadena
// donde cada valor sale del anterior: no se puede repartir. La segunda
// procesa cada dato por separado y sí se reparte. El programa mide las dos
// con 1, 2, 4 y 8 hilos; el speedup, la eficiencia y la fracción paralela
// se calculan a mano a partir de la tabla.
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 10000000;

// Parte secuencial: cada valor depende del anterior.
void generar(vector<long> &v) {
  long s = 20260908;
  for (size_t i = 0; i < v.size(); i++) {
    for (int k = 0; k < 10; k++) s = (s * 1103515245 + 12345) % 2147483647;
    v[i] = s;
  }
}

// Parte paralelizable: procesar un dato no depende de los demás.
long procesar(long x) {
  long y = x;
  for (int k = 0; k < 40; k++) y = (y * 1103515245 + 12345) % 1000003;
  return y;
}

// TODO: cada hilo procesa su trozo [ini, fin) sumando procesar(v[i]) y deja
// el parcial en `salida`. Acumular en una variable local y escribir `salida`
// una sola vez.
void trozo(const vector<long> &v, size_t ini, size_t fin, long &salida) {
  long parcial = 0;
  for (size_t i = ini; i < fin; ++i) {
    parcial += procesar(v[i]);
  }
  salida = parcial;
}

// TODO: repartir [0, n) en k trozos, lanzar un hilo por trozo con `trozo`,
// unirlos y devolver la suma de los parciales.
long en_paralelo(const vector<long> &v, int k) {
  vector<long> parciales(k, 0);
  vector<thread> hilos;
  size_t n = v.size();
  for (int i = 0; i < k; ++i) {
    size_t ini = i * n / k;
    size_t fin = (i + 1) * n / k;
    hilos.emplace_back(trozo, cref(v), ini, fin, ref(parciales[i]));
    hilos.back().join();
    
  }

  return 0;
}

int main() {
  vector<long> v(N);
  for (int k : {1, 2, 4, 8}) {
    auto t0 = high_resolution_clock::now();
    generar(v);
    auto t1 = high_resolution_clock::now();
    long suma = en_paralelo(v, k);
    auto t2 = high_resolution_clock::now();
    double serial = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
    double paralelo = duration_cast<microseconds>(t2 - t1).count() / 1000.0;
    printf("hilos %d secuencial %.1f ms paralelo %.1f ms total %.1f ms suma %ld\n",
           k, serial, paralelo, serial + paralelo, suma);
  }
  return 0;
}
