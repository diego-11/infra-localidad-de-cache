// False sharing: cuatro hilos que cuentan, con los contadores pegados y con
// los contadores separados.
//
// Las dos versiones hacen el mismo trabajo y dan el mismo resultado. La
// diferencia está en dónde caen los contadores en memoria.
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const int HILOS = 4;
const long VUELTAS = 50000000;
const int LINEA = 64;  // bytes de una línea de caché

// TODO: cada hilo suma VUELTAS veces sobre contadores[id], que están
// contiguos en memoria.
//
// Con -O2 el compilador guarda el contador en un registro y escribe una sola
// vez al final, y entonces las dos versiones tardan lo mismo. Para que cada
// vuelta vaya a memoria, incrementar a través de un puntero volatile:
//   volatile long *c = &contadores[id];  *c = *c + 1;
void pegados(vector<long> &contadores) {
}

// TODO: la misma cuenta, pero con los contadores separados lo suficiente para
// que cada uno caiga en su propia línea de caché. Sugerencia: reservar
// HILOS * (LINEA / sizeof(long)) posiciones y usar solo una de cada grupo.
void separados(vector<long> &contadores) {
}

int main() {
  vector<long> a(HILOS, 0);
  auto t0 = high_resolution_clock::now();
  pegados(a);
  auto t1 = high_resolution_clock::now();

  vector<long> b(HILOS * (LINEA / sizeof(long)), 0);
  separados(b);
  auto t2 = high_resolution_clock::now();

  long suma_a = 0;
  for (long x : a) suma_a += x;
  long suma_b = 0;
  for (long x : b) suma_b += x;

  printf("pegados %ld ms suma %ld\n",
         duration_cast<milliseconds>(t1 - t0).count(), suma_a);
  printf("separados %ld ms suma %ld\n",
         duration_cast<milliseconds>(t2 - t1).count(), suma_b);
  return 0;
}
