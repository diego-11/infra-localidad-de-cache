// El experimento del paso: el mismo arreglo recorrido con paso creciente.
//
// Al duplicar el paso se hace la mitad de las operaciones. Si el tiempo no
// baja a la mitad, lo que cuesta no son las operaciones sino las líneas de
// caché que hay que traer.
#include <chrono>
#include <cstdio>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 32 * 1024 * 1024;  // 32 Mi enteros: 128 MiB

// TODO: multiplicar por 3 una de cada `paso` posiciones, empezando en la 0,
// y devolver cuántas posiciones se tocaron.
size_t recorrer(vector<int> &a, size_t paso) {
  size_t operaciones = 0;
  return operaciones;
}

int main() {
  vector<int> a(N, 1);
  printf("%6s %12s %10s %14s %12s\n", "paso", "operaciones", "tiempo", "ns/operacion", "suma");
  for (size_t paso = 1; paso <= 1024; paso *= 2) {
    fill(a.begin(), a.end(), 1);
    auto t0 = high_resolution_clock::now();
    size_t ops = recorrer(a, paso);
    auto t1 = high_resolution_clock::now();
    long suma = 0;
    for (int x : a) suma += x;
    double ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
    printf("%6zu %12zu %7.1f ms %11.2f ns %12ld\n", paso, ops, ms,
           ops ? ms * 1e6 / ops : 0.0, suma);
  }
  return 0;
}
