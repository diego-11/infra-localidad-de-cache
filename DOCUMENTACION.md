# Documentación de apoyo: localidad de caché y false sharing

Aquí está lo que hace falta saber para completar los cuatro programas del
README: qué función o instrucción se usa en cada parte, un ejemplo completo
que resuelve un problema vecino al que se pide, los errores que más se ven y
los enlaces a la documentación de referencia. Hay una sección por parte, en
el mismo orden y con el mismo nombre que en el README; los ejemplos se
compilan y corren tal cual, y las salidas son las de un Ryzen 5 3600 con
seis núcleos y doce hilos, pegadas como las imprime `printf`, con punto
decimal. En el texto los números van con coma decimal.

## Parte 1: recorrer una matriz

### Lo que se usa

- `std::vector<long> m(n, valor)`: construye un vector de `n` posiciones,
  todas con `valor`, contiguas en memoria. `m.size()` devuelve `n` y `m[k]`
  accede a la posición `k` sin comprobar límites. Va en `<vector>`.
- `m[i * N + j]`: el elemento de la fila `i` y la columna `j` de una matriz
  de `N` columnas guardada fila tras fila. Dos vecinos de una misma fila
  están a 8 bytes (un `long`); dos vecinos de una misma columna están a
  `N * 8` bytes, 16.384 con `N = 2048`. Una línea de caché trae 64 bytes,
  es decir ocho `long` seguidos de la misma fila.
- `static_cast<size_t>(i) * N + j`: `i` y `N` son `int`, y el producto se
  hace en `int`. Con 2048 cabe (4.194.304), pero a partir de `N = 46.341`
  desborda; el cast hace la cuenta en 64 bits.
- `std::chrono::steady_clock::now()`: devuelve un instante; la resta de dos
  instantes es una duración. Es el reloj que no salta si el sistema ajusta
  la hora, el que se usa para cronometrar. Va en `<chrono>`.
- `duration_cast<milliseconds>(t1 - t0).count()`: convierte la duración a
  milisegundos enteros (trunca) y `count()` saca el número. Con
  `microseconds` da microsegundos; dividir entre `1000.0` deja milisegundos
  con decimales.
- `high_resolution_clock`: es el que usa el `main` del repositorio. En
  libstdc++ es un alias de `system_clock`, y para intervalos dentro de una
  misma corrida sirve igual. En código propio conviene `steady_clock`.

### Ejemplo

Producto de una matriz por un vector, `y = M x`, con los dos órdenes de
ciclo. La matriz de 4.096 por 4.096 `double` ocupa 128 MiB y se guarda fila
tras fila. Las dos funciones hacen las mismas multiplicaciones y dan el mismo
vector; lo que cambia es qué ciclo va adentro.

```cpp
// matvec.cpp
// Producto matriz por vector, y = M x, con los dos órdenes de ciclo.
// La matriz de N por N va en un solo vector, fila tras fila: el elemento
// (i, j) está en la posición i * N + j.
#include <chrono>
#include <cstdio>
#include <vector>

using namespace std;
using namespace std::chrono;

const int N = 4096;

// El ciclo interno avanza por j: recorre una fila, posiciones contiguas.
void filas_afuera(const vector<double> &m, const vector<double> &x,
                  vector<double> &y) {
  for (int i = 0; i < N; i++) {
    double s = 0;
    for (int j = 0; j < N; j++) s += m[static_cast<size_t>(i) * N + j] * x[j];
    y[i] = s;
  }
}

// El ciclo interno avanza por i: salta de fila en fila, N * 8 bytes cada vez.
void columnas_afuera(const vector<double> &m, const vector<double> &x,
                     vector<double> &y) {
  for (int i = 0; i < N; i++) y[i] = 0;
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) y[i] += m[static_cast<size_t>(i) * N + j] * x[j];
}

int main() {
  vector<double> m(static_cast<size_t>(N) * N, 1.0);
  vector<double> x(N, 2.0);
  vector<double> y1(N), y2(N);

  auto t0 = steady_clock::now();
  filas_afuera(m, x, y1);
  auto t1 = steady_clock::now();
  columnas_afuera(m, x, y2);
  auto t2 = steady_clock::now();

  // Cada y[i] tiene que dar N * 1.0 * 2.0 = 8192.
  printf("filas afuera    %4ld ms  y[0] = %.0f\n",
         duration_cast<milliseconds>(t1 - t0).count(), y1[0]);
  printf("columnas afuera %4ld ms  y[0] = %.0f\n",
         duration_cast<milliseconds>(t2 - t1).count(), y2[0]);
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -o matvec matvec.cpp && ./matvec
```

```
filas afuera      13 ms  y[0] = 8192
columnas afuera  541 ms  y[0] = 8192
```

Con las filas afuera cada línea de 64 bytes que llega a la caché aporta ocho
`double` seguidos y se usa completa. Con las columnas afuera cada acceso cae
32.768 bytes más adelante, en otra línea; cuando el ciclo vuelve a la misma
fila, esa línea ya salió de la caché, porque la matriz mide 128 MiB y la L3 de
esta máquina 32 MiB. Los mismos 16,8 millones de multiplicaciones tardan
cuarenta veces más. `perf stat -e cache-misses,cache-references ./matvec`
cuenta los fallos de las dos funciones juntas: 94.907.932 fallos sobre
172.232.194 accesos.

### Lo que suele fallar

- La suma da 0 o un número distinto de 4.194.304. El flujo dice
  `la suma por filas dio X y debia dar 4194304`. Casi siempre es un límite
  del ciclo (`j < N - 1`, o el ciclo interno que no entra) o un índice que
  no visita todas las posiciones.
- Las dos funciones tardan casi lo mismo. Se copió el cuerpo de una en la
  otra y los dos ciclos quedaron en el mismo orden. Como el ruido decide cuál
  sale más rápida, el flujo a veces pasa y a veces marca
  `el recorrido por columnas salio mas rapido que por filas`.
- `por_filas` sale más lenta que `por_columnas`. Se cambió la fórmula del
  índice (`m[j * N + i]`) en vez del orden de los ciclos, y la función que
  se llama `por_filas` es la que salta de fila en fila. Las sumas dan bien y
  el flujo queda en rojo.
- Compilar a mano sin `-O2`. Los tiempos suben varias veces y la relación
  entre los dos cambia. `make matriz` compila con las banderas del
  repositorio.
- Índice en `int` cuando la matriz crece. Con 2048 no pasa nada; con
  `N = 65536` el producto `i * N + j` desborda y el programa lee fuera del
  vector. El cast a `size_t` no cuesta nada y evita la sorpresa.

### Enlaces

- [std::vector](https://en.cppreference.com/w/cpp/container/vector):
  constructores, `size()`, `operator[]` y la garantía de memoria contigua.
- [std::chrono::steady_clock](https://en.cppreference.com/w/cpp/chrono/steady_clock):
  el reloj monótono; `now()` y el tipo del instante que devuelve.
- [std::chrono::duration_cast](https://en.cppreference.com/w/cpp/chrono/duration/duration_cast):
  conversión entre unidades de duración y qué hace con los decimales.
- [std::chrono::high_resolution_clock](https://en.cppreference.com/w/cpp/chrono/high_resolution_clock):
  el reloj que usa el `main` del repositorio y por qué es un alias.
- [Row- and column-major order](https://en.wikipedia.org/wiki/Row-_and_column-major_order):
  la fórmula `i * N + j` y su espejo en otros lenguajes.
- [Opciones de optimización de GCC](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html):
  qué activa `-O2` y qué diferencia hay con `-O0` y `-O3`.

## Parte 2: false sharing

### Lo que se usa

- `std::thread t(f, arg1, arg2, ...)`: arranca un hilo que ejecuta
  `f(arg1, arg2, ...)` de inmediato. Los argumentos se copian al hilo; para
  que `f` reciba una referencia hay que envolverla con `std::ref(x)`, o con
  `std::cref(x)` si es `const`. Va en `<thread>`; `std::ref` en
  `<functional>`.
- `t.join()`: bloquea hasta que el hilo termina. Todo `std::thread` tiene
  que unirse antes de destruirse; si no, el programa aborta.
- `std::vector<std::thread> hilos; hilos.emplace_back(f, args...)`:
  construye el hilo dentro del vector, y al final
  `for (auto &t : hilos) t.join();` los espera a todos.
- Lambda `[&, h] { ... }`: captura todo por referencia menos `h`, que se
  copia. La variable del ciclo se copia siempre, porque el ciclo la sigue
  cambiando mientras el hilo arranca.
- `volatile long *c = &v[k]; *c = *c + 1;`: `volatile` le dice al compilador
  que cada lectura y cada escritura a través de ese puntero cuentan y no se
  pueden juntar ni guardar en un registro. Sin él, `-O2` ve un contador que
  nadie lee hasta el final y escribe el resultado una sola vez, o lo calcula
  sin dar una vuelta.
- `alignas(64)` delante de un `struct` o una variable: su dirección es
  múltiplo de 64 y su tamaño se rellena hasta 64. Cada objeto así ocupa una
  línea para él solo.
- `std::hardware_destructive_interference_size`: la separación mínima entre
  dos objetos para que no compartan línea, 64 en x86-64. Va en `<new>`,
  desde GCC 12 (Ubuntu 24.04 la trae; 22.04 viene con GCC 11 y no).
- `sizeof(long)` es 8 en Linux de 64 bits, así que `64 / sizeof(long)` da
  ocho posiciones por línea. Dos `long` a ocho posiciones de distancia están
  a 64 bytes, y ninguna ventana de 64 bytes los contiene a los dos, sin
  importar dónde empiece el vector.

Lo que hace `-O2` con un contador se ve en tres variantes del mismo ciclo:

```cpp
// contador.cpp
// El mismo contador, tres veces: uno normal, uno volatile y uno atómico.
// Con -O2 el primero no da vueltas: el compilador sabe el resultado.
#include <atomic>
#include <chrono>
#include <cstdio>

using namespace std;
using namespace std::chrono;

const long VUELTAS = 200000000;

int main() {
  auto t0 = steady_clock::now();
  long normal = 0;
  for (long k = 0; k < VUELTAS; k++) normal++;
  auto t1 = steady_clock::now();

  volatile long lento = 0;  // cada lectura y escritura va a memoria
  for (long k = 0; k < VUELTAS; k++) lento = lento + 1;
  auto t2 = steady_clock::now();

  atomic<long> atomico(0);  // cada incremento es una instrucción lock
  for (long k = 0; k < VUELTAS; k++) atomico++;
  auto t3 = steady_clock::now();

  printf("normal   %4ld ms  %ld\n", duration_cast<milliseconds>(t1 - t0).count(), normal);
  printf("volatile %4ld ms  %ld\n", duration_cast<milliseconds>(t2 - t1).count(), (long)lento);
  printf("atomic   %4ld ms  %ld\n", duration_cast<milliseconds>(t3 - t2).count(), atomico.load());
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -o contador contador.cpp && ./contador
g++ -std=c++17 -O0 -o contador0 contador.cpp && ./contador0
```

```
normal      0 ms  200000000
volatile   53 ms  200000000
atomic    929 ms  200000000
```

```
normal    149 ms  200000000
volatile  119 ms  200000000
atomic   1129 ms  200000000
```

Con `-O0` las tres versiones dan vueltas. Con `-O2` el primer ciclo
desaparece: en el ensamblador (`g++ -std=c++17 -O2 -S contador.cpp`) las dos
llamadas a `now()` quedan seguidas, sin nada en medio, y el `volatile` sí
deja un ciclo que suma sobre memoria en cada vuelta:

```
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	movq	%rax, %rbx
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	...
.L2:
	addq	$1, 8(%rsp)
	subq	$1, %rax
	jne	.L2
```

### Ejemplo

Dos hilos cuentan cuántos pares hay en su mitad de un vector de cien
millones de enteros y anotan cada hallazgo en su propia casilla de memoria.
Las casillas se declaran de dos formas: dos `long` seguidos en un arreglo,
que caben en una línea, y dos `struct` con `alignas(64)`, cada uno en la
suya. Los hilos reciben la casilla como puntero `volatile`.

```cpp
// pares.cpp
// Dos hilos cuentan cuántos pares hay en su mitad de un vector. Cada hilo
// anota cada hallazgo en su propia casilla de memoria. Las casillas se
// declaran de dos formas: pegadas en un arreglo y separadas con alignas.
#include <chrono>
#include <cstdio>
#include <new>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 100000000;

// Una casilla que ocupa una línea de caché completa.
struct alignas(64) Casilla {
  long valor;
};

// Cuenta los pares de v en [ini, fin) escribiendo en *cuenta cada vez.
void contar_pares(const vector<int> &v, size_t ini, size_t fin,
                  volatile long *cuenta) {
  for (size_t i = ini; i < fin; i++)
    if (v[i] % 2 == 0) *cuenta = *cuenta + 1;
}

int main() {
  vector<int> v(N);
  for (size_t i = 0; i < N; i++) v[i] = static_cast<int>(i % 7);  // 0..6

  long pegadas[2] = {0, 0};        // dos long seguidos: 16 bytes, una línea
  Casilla separadas[2] = {{0}, {0}};  // dos líneas distintas

  printf("sizeof(long) = %zu, sizeof(Casilla) = %zu, interferencia = %zu\n",
         sizeof(long), sizeof(Casilla),
         static_cast<size_t>(hardware_destructive_interference_size));

  auto t0 = steady_clock::now();
  thread a(contar_pares, cref(v), 0, N / 2, &pegadas[0]);
  thread b(contar_pares, cref(v), N / 2, N, &pegadas[1]);
  a.join();
  b.join();
  auto t1 = steady_clock::now();

  thread c(contar_pares, cref(v), 0, N / 2, &separadas[0].valor);
  thread d(contar_pares, cref(v), N / 2, N, &separadas[1].valor);
  c.join();
  d.join();
  auto t2 = steady_clock::now();

  printf("pegadas   %4ld ms  pares %ld\n",
         duration_cast<milliseconds>(t1 - t0).count(), pegadas[0] + pegadas[1]);
  printf("separadas %4ld ms  pares %ld\n",
         duration_cast<milliseconds>(t2 - t1).count(),
         separadas[0].valor + separadas[1].valor);
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -pthread -o pares pares.cpp && ./pares
```

```
sizeof(long) = 8, sizeof(Casilla) = 64, interferencia = 64
pegadas    130 ms  pares 57142857
separadas   52 ms  pares 57142857
```

Los dos hilos escriben en direcciones distintas y el resultado es el mismo.
Con las casillas pegadas, cada escritura de un hilo invalida la línea en la
caché del otro núcleo, que tiene que volver a pedirla antes de su siguiente
escritura; la línea viaja de un núcleo al otro decenas de millones de veces.
Con `alignas(64)` cada núcleo se queda con su línea y el tiempo cae a menos
de la mitad. En el README los contadores van en un `vector<long>`, así que
la separación se logra con el índice, no con `alignas`.

### Lo que suele fallar

- `pegados 0 ms`. El flujo lo dice tal cual:
  `la version con contadores pegados tardo 0 ms: el compilador dejo el
  contador en un registro`. El ciclo incrementa `contadores[id]` a secas o
  una copia local, y `-O2` escribe una vez al final. Hay que incrementar a
  través de un puntero `volatile long *`.
- `terminate called without an active exception` y el programa aborta con
  código 134. Un `std::thread` se destruyó sin `join()`: falta el ciclo de
  `join`, o el hilo se creó como variable local dentro del ciclo que lo
  lanza y murió en la siguiente vuelta.
- `static assertion failed: std::thread arguments must be invocable after
  conversion to rvalues`. Se pasó una variable a una función que recibe
  `long &` sin envolverla en `std::ref`.
- La suma sale distinta de 200.000.000 y cambia entre corridas, con alguna
  posición en cero. La lambda capturó la variable del ciclo por referencia
  (`[&]`) y varios hilos leyeron el mismo índice; `[&, h]` la copia.
- `separar los contadores no bajo el tiempo`. La separación quedó en bytes
  cuando el vector se indexa en `long` (`h + 8` en vez de `h * 8`), o las dos
  funciones escriben en `contadores[h]`. Y con `h * 64` el índice se sale del
  vector, que tiene 32 posiciones: `Segmentation fault` o una suma
  corrupta.
- En el portátil la diferencia es menor que en el servidor. Con dos núcleos
  los cuatro hilos se turnan y comparten menos; conviene correr dos veces y
  mirar `lscpu` para saber cuántos núcleos hay y de qué tamaño son las
  cachés.

### Enlaces

- [std::thread](https://en.cppreference.com/w/cpp/thread/thread): la clase,
  su constructor, cómo se pasan los argumentos y qué pasa si se destruye sin
  unir.
- [std::thread::join](https://en.cppreference.com/w/cpp/thread/thread/join):
  la espera por el hilo y las condiciones en que lanza excepción.
- [std::ref y std::cref](https://en.cppreference.com/w/cpp/utility/functional/ref):
  cómo pasar referencias a través de `std::thread`.
- [volatile](https://en.cppreference.com/w/cpp/language/cv): qué garantiza y
  qué no; no sirve para sincronizar hilos, solo para obligar el acceso a
  memoria.
- [alignas](https://en.cppreference.com/w/cpp/language/alignas): el
  especificador de alineación y sus reglas.
- [std::hardware_destructive_interference_size](https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size):
  la constante, el ejemplo de dos contadores y la nota sobre su valor según
  el compilador.

## Parte 3: el paso

### Lo que se usa

- `size_t`: el tipo sin signo de los tamaños e índices; `a.size()` lo
  devuelve y `recorrer` lo retorna. Mezclarlo con `int` en la condición del
  ciclo produce el aviso `comparison of integer expressions of different
  signedness`, y un `int` que baja de cero nunca termina el ciclo.
- Un ciclo que avanza `paso` posiciones por vuelta: la variable de control
  se incrementa con `i += paso` en vez de `i++`, y arranca en 0. Cuántas
  posiciones toca es lo que hay que contar y devolver.
- `a[i] *= 3`: leer, multiplicar y volver a escribir. La línea llega a la
  caché, se modifica ahí, y más tarde vuelve a memoria.
- `std::fill(a.begin(), a.end(), 1)`: el `main` deja el arreglo en unos
  antes de cada paso, así la suma final comprueba qué posiciones se
  tocaron.
- `duration_cast<microseconds>(t1 - t0).count() / 1000.0`: milisegundos con
  decimales; `ms * 1e6 / ops` los convierte en nanosegundos por operación.
  Un milisegundo son 1.000.000 de nanosegundos.
- `sizeof(int)` es 4: una línea de 64 bytes trae dieciséis `int`. Para saber
  el tamaño de la línea y de las cachés del procesador:

```bash
getconf LEVEL1_DCACHE_LINESIZE
lscpu | grep -iE '^L[123]'
```

```
64
L1d cache:                               192 KiB (6 instances)
L1i cache:                               192 KiB (6 instances)
L2 cache:                                3 MiB (6 instances)
L3 cache:                                32 MiB (2 instances)
```

### Ejemplo

El costo de una línea visto al revés: en vez de hacer menos operaciones
sobre el mismo arreglo, se hace siempre la misma cantidad y lo que crece es
la distancia entre un acceso y el siguiente. El programa suma la primera
columna de una matriz de 1.048.576 filas guardada fila tras fila, y
ensancha la fila en cada corrida: 1, 2, 4, ... 32 `long`, es decir de 8 a
256 bytes entre un elemento y el que sigue. Las sumas son siempre
1.048.576; las líneas que hay que traer, no.

```cpp
// columna.cpp
// La primera columna de una matriz guardada fila tras fila, sumada con la
// fila cada vez más ancha. Las sumas son siempre FILAS; lo que cambia es
// cuántos bytes separan un elemento del siguiente.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t FILAS = 1024 * 1024;  // 1 Mi filas
const size_t ANCHO_MAX = 32;           // 32 long por fila: 256 bytes

// Suma m[0], m[ancho], m[2 * ancho], ... una por fila.
long primera_columna(const vector<long> &m, size_t ancho) {
  long s = 0;
  for (size_t f = 0; f < FILAS; f++) s += m[f * ancho];
  return s;
}

int main() {
  vector<long> m(FILAS * ANCHO_MAX);
  printf("%6s %6s %10s %9s %9s\n", "ancho", "bytes", "tiempo", "ns/suma", "suma");
  for (size_t ancho = 1; ancho <= ANCHO_MAX; ancho *= 2) {
    fill(m.begin(), m.end(), 1);  // saca la matriz de la caché entre corridas
    auto t0 = steady_clock::now();
    long s = primera_columna(m, ancho);
    auto t1 = steady_clock::now();
    double ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
    printf("%6zu %6zu %7.2f ms %6.2f ns %9ld\n", ancho, ancho * sizeof(long),
           ms, ms * 1e6 / FILAS, s);  // la suma tiene que dar FILAS
  }
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -o columna columna.cpp && ./columna
```

```
 ancho  bytes     tiempo   ns/suma      suma
     1      8    0.62 ms   0.59 ns   1048576
     2     16    1.11 ms   1.06 ns   1048576
     4     32    1.58 ms   1.51 ns   1048576
     8     64    2.98 ms   2.85 ns   1048576
    16    128    4.02 ms   3.83 ns   1048576
    32    256    3.66 ms   3.49 ns   1048576
```

La suma da 1.048.576 en todas las filas: se leyó un elemento por fila. De
8 a 64 bytes el tiempo sube con cada duplicación del ancho (0,62 ms; 1,11 ms;
1,58 ms; 2,98 ms) aunque las sumas sean las mismas. Cada línea de 64 bytes
rinde cada vez menos: con la fila de 8 bytes una línea trae ocho elementos de
la columna y se traen 131.072 líneas; con la fila de 64 bytes cada elemento
cae en su propia línea y se traen 1.048.576. De 64 en adelante el tiempo deja
de subir (2,98 ms; 4,02 ms; 3,66 ms): ya cada suma paga una línea completa, y
ensanchar más la fila solo deja líneas sin leer en el medio. Los 3,5 ns por
suma del final son lo que cuesta traer una línea desde memoria en esta
máquina. En el ejercicio el arreglo está fijo y lo que crece es el paso, así
que la tabla sale al revés: las operaciones bajan y el tiempo no las sigue
hasta que cada operación queda en su propia línea.

### Lo que suele fallar

- `paso 2: se tocaron X posiciones y debian ser 16777216`. El ciclo arranca
  en 1 en vez de 0, o la condición es `i <= a.size()`, o `operaciones` se
  calcula como `a.size() / paso` sin contar lo que de verdad se tocó.
- `paso 4: la suma dio X y debia dar Y`. Se tocó la cantidad correcta de
  posiciones pero no las correctas: `a[i]` con `i` avanzando de uno en uno
  mientras el contador avanza con el paso, o `a[i * paso]` con `i` hasta
  `a.size()`, que se sale del vector.
- `Segmentation fault` o `free(): invalid pointer`. Un índice pasó de
  `a.size() - 1`; `operator[]` no comprueba límites. Mientras se depura,
  `a.at(i)` lanza `std::out_of_range` con un mensaje y señala la posición.
- La tabla tiene menos de once filas: `faltan filas en paso.txt`. El
  programa se cayó a mitad de camino; la fila que falta dice en qué paso.
- `RESPUESTAS.md tiene N palabras: falta la tabla o las explicaciones`. El
  flujo cuenta las palabras de todo el archivo y pide 220 como mínimo; la
  tabla de la parte 4 cuenta, y la respuesta de esta parte tiene que decir
  en qué paso cambia el ritmo y relacionarlo con los 4 bytes del `int` y los
  64 de la línea.

### Enlaces

- [std::chrono::duration](https://en.cppreference.com/w/cpp/chrono/duration):
  las unidades (`microseconds`, `milliseconds`) y `count()`.
- [std::fill](https://en.cppreference.com/w/cpp/algorithm/fill): cómo deja
  un rango en un valor.
- [sizeof](https://en.cppreference.com/w/cpp/language/sizeof): el tamaño de
  un tipo en bytes y por qué `sizeof(int)` no es lo mismo en toda
  plataforma.
- [getconf](https://man7.org/linux/man-pages/man1/getconf.1p.html): las
  variables del sistema que se pueden consultar, entre ellas
  `LEVEL1_DCACHE_LINESIZE`.
- [lscpu](https://man7.org/linux/man-pages/man1/lscpu.1.html): núcleos,
  hilos por núcleo y tamaño de cada caché.
- [Cachegrind](https://valgrind.org/docs/manual/cg-manual.html): la
  herramienta de Valgrind que simula la caché y cuenta fallos por línea de
  código; sirve con arreglos más pequeños que el del ejercicio.

## Parte 4: speedup, eficiencia y la ley de Amdahl

### Lo que se usa

- `std::thread(trozo, std::cref(v), ini, fin, std::ref(parciales[h]))`: la
  función recibe el vector por referencia constante y el parcial por
  referencia; sin `std::cref` el vector de diez millones de `long` se copia
  en cada hilo, y sin `std::ref` el programa no compila.
- `std::vector<long> parciales(k, 0)`: una casilla de salida por hilo. Están
  contiguas, así que si cada hilo escribiera la suya en cada vuelta habría
  false sharing; por eso se acumula en una variable local y se escribe una
  sola vez.
- El reparto de `[0, n)` en `k` tramos: cada hilo recibe `n / k` posiciones,
  y cuando `n` no es múltiplo de `k` el residuo tiene que caer en algún
  tramo. Con `n = 10.000.000` los cuatro valores de `k` dividen exacto; aun
  así, el último tramo termina en `n` y no en `ini + n / k`, para que el
  reparto sirva con cualquier tamaño. La comprobación es que los tamaños
  sumen `n` y que ningún índice se repita.
- `for (auto &t : hilos) t.join();` después de lanzar todos. Unir cada hilo
  justo después de crearlo los pone en fila y el tiempo no baja.
- `std::thread::hardware_concurrency()`: cuántos hilos de hardware hay; 12
  en esta máquina, 4 en el runner de Actions para un repositorio público y
  2 para uno privado.
- Las fórmulas, con `T(k)` el tiempo total con `k` hilos:
  - speedup `S(k) = T(1) / T(k)`;
  - eficiencia `E(k) = S(k) / k`;
  - fracción paralelizable `p = T_paralelo(1) / T(1)`, medida con un hilo;
  - predicción de Amdahl `S(k) = 1 / ((1 - p) + p / k)`;
  - techo cuando `k` crece sin límite: `1 / (1 - p)`.

### Ejemplo

Cuántos primos distintos hay en una lista de cuatro millones de números. Dos
fases: quitar los repetidos (ordenar y compactar, en un solo hilo) y contar
los primos por división de prueba (cada número por su lado, repartido entre
`k` hilos). El reparto es por turnos: el hilo `id` toma las posiciones
`id`, `id + k`, `id + 2k`, ... y así ninguno recibe más trabajo que otro.
El programa imprime los tiempos de las dos fases, el speedup y la
eficiencia; `p` y la predicción de Amdahl se calculan después a mano.

```cpp
// primos.cpp
// Cuántos primos distintos hay en una lista de números. Dos fases: quitar
// los repetidos (ordenar y compactar, en un solo hilo) y contar los primos
// (cada número por su lado, repartido entre k hilos).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 4000000;

bool es_primo(int x) {
  if (x < 2) return false;
  for (int d = 2; d * d <= x; d++)
    if (x % d == 0) return false;
  return true;
}

// Fase que no se reparte: ordenar y quitar duplicados.
void sin_repetidos(vector<int> &v) {
  sort(v.begin(), v.end());
  v.erase(unique(v.begin(), v.end()), v.end());
}

// Lo que hace el hilo `id` de `k`: toma las posiciones id, id + k, id + 2k...
// Acumula en una variable local y escribe `cuenta` una sola vez al final.
void contar_primos(const vector<int> &v, int id, int k, long &cuenta) {
  long c = 0;
  for (size_t i = id; i < v.size(); i += k)
    if (es_primo(v[i])) c++;
  cuenta = c;
}

// Lanza k hilos, los espera y suma lo que contó cada uno.
long primos_en_paralelo(const vector<int> &v, int k) {
  vector<long> parciales(k, 0);
  vector<thread> hilos;
  for (int h = 0; h < k; h++)
    hilos.emplace_back(contar_primos, cref(v), h, k, ref(parciales[h]));
  for (thread &t : hilos) t.join();
  long total = 0;
  for (long p : parciales) total += p;
  return total;
}

int main() {
  mt19937 gen(2026);
  uniform_int_distribution<int> dist(2, 9999999);
  vector<int> original(N);
  for (int &x : original) x = dist(gen);

  double total_1 = 0;  // tiempo total con un hilo, base del speedup
  printf("%5s %10s %10s %10s %8s %8s %8s\n",
         "hilos", "serie ms", "paral ms", "total ms", "primos", "S", "E");
  for (int k : {1, 2, 4, 8}) {
    vector<int> v = original;
    auto t0 = steady_clock::now();
    sin_repetidos(v);
    auto t1 = steady_clock::now();
    long primos = primos_en_paralelo(v, k);
    auto t2 = steady_clock::now();
    double serie = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
    double paral = duration_cast<microseconds>(t2 - t1).count() / 1000.0;
    double total = serie + paral;
    if (k == 1) total_1 = total;
    double s = total_1 / total;  // speedup
    printf("%5d %10.1f %10.1f %10.1f %8ld %8.2f %8.2f\n",
           k, serie, paral, total, primos, s, s / k);
  }
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -pthread -o primos primos.cpp && ./primos
```

```
hilos   serie ms   paral ms   total ms   primos        S        E
    1      273.9     3047.9     3321.9   218616     1.00     1.00
    2      275.4     1626.7     1902.1   218616     1.75     0.87
    4      273.1      800.4     1073.6   218616     3.09     0.77
    8      258.0      605.5      863.5   218616     3.85     0.48
```

La cuenta de primos es la misma con cualquier número de hilos, y la fase de
ordenar tarda lo mismo siempre, porque nadie la reparte. Con estos tiempos,
la fracción paralelizable sale de la fila de un hilo:
`p = 3.047,9 / 3.321,9 = 0,918`. La predicción de Amdahl con esa `p`:

| Hilos | Total medido (ms) | Speedup medido | Eficiencia | Speedup según Amdahl |
|---:|---:|---:|---:|---:|
| 1 | 3.321,9 | 1,00 | 1,00 | 1,00 |
| 2 | 1.902,1 | 1,75 | 0,87 | 1,85 |
| 4 | 1.073,6 | 3,09 | 0,77 | 3,21 |
| 8 | 863,5 | 3,85 | 0,48 | 5,07 |

Por ejemplo, con cuatro hilos `1 / ((1 - 0,918) + 0,918 / 4) = 1 / 0,312 =
3,21`. El techo con esa `p` es `1 / (1 - 0,918) = 12,2`: por más núcleos
que se agreguen, el programa nunca va más de doce veces más rápido, porque
los 273,9 ms de ordenar no se mueven. Hasta cuatro hilos la medición se
queda apenas por debajo de la fórmula; la diferencia es el costo de crear
los hilos y esperarlos. Con ocho se separa de verdad: la máquina tiene seis
núcleos, y los dos hilos que sobran comparten núcleo con otros dos, así que
la parte paralela baja de 800,4 a 605,5 ms en vez de a la mitad.

### Lo que suele fallar

- `con 4 hilos la suma dio X y debia dar 4999872888854`, con la fila de un
  hilo correcta. O los parciales se suman antes del ciclo de `join`, cuando
  los hilos todavía no han escrito, y la suma sale en 0 o incompleta; o dos
  tramos se pisan o dejan un hueco porque el `fin` de uno no es el `ini`
  del siguiente. Con un solo hilo nada de eso se nota.
- `con cuatro hilos la parte paralela no bajo ni a la mitad`. O el `join`
  está dentro del ciclo que lanza, y los hilos corren de a uno; o `trozo`
  escribe `salida` en cada vuelta y los parciales, contiguos, comparten
  línea. Se acumula en una variable local y `salida` se escribe al final.
- `static assertion failed: std::thread arguments must be invocable after
  conversion to rvalues`. Falta `std::ref(parciales[h])`.
- La parte paralela sube con más hilos en vez de bajar. Se pasó `v` sin
  `std::cref` y cada hilo recibió una copia de 80 MB antes de empezar.
- `terminate called without an active exception`: algún hilo quedó sin
  `join`, porque el ciclo que une no recorre todos los que se lanzaron o
  porque hay un `return` antes de unirlos.
- Con ocho hilos el speedup no llega ni a cinco, en la máquina propia o en
  el servidor. No hay ocho núcleos: el runner tiene cuatro procesadores en un
  repositorio público. El flujo solo exige que con cuatro la parte paralela
  baje a la mitad; la explicación de la fila de ocho va en `RESPUESTAS.md`.

### Enlaces

- [Constructor de std::thread](https://en.cppreference.com/w/cpp/thread/thread/thread):
  cómo se copian los argumentos y por qué hacen falta `std::ref` y
  `std::cref`.
- [std::thread::hardware_concurrency](https://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency):
  cuántos hilos soporta el hardware, y cuándo devuelve 0.
- [std::vector::emplace_back](https://en.cppreference.com/w/cpp/container/vector/emplace_back):
  construir el hilo directamente dentro del vector.
- [Amdahl's law](https://en.wikipedia.org/wiki/Amdahl%27s_law): la fórmula,
  su deducción y la gráfica del techo según `p`.
- [std::sort](https://en.cppreference.com/w/cpp/algorithm/sort) y
  [std::unique](https://en.cppreference.com/w/cpp/algorithm/unique): la fase
  que en el ejemplo no se reparte.
- [Runners de GitHub Actions](https://docs.github.com/en/actions/reference/runners/github-hosted-runners):
  procesadores y memoria de `ubuntu-latest`, donde corre el flujo.

## Cómo compilar y ejecutar en la máquina propia

### Debian y Ubuntu

```bash
sudo apt install build-essential make
g++ --version    # Ubuntu 24.04 trae GCC 13; 22.04, GCC 11
nproc            # cuántos procesadores ve el sistema
```

El `Makefile` tiene un objetivo por parte (`matriz`, `falso`, `paso`,
`amdahl`), `todo` para las cuatro y `limpiar` para borrar binarios y
salidas. Cada objetivo compila, ejecuta con `./programa | tee programa.txt`
y borra el binario. El `tee` muestra la salida en pantalla y la guarda en el
`.txt`. Ese archivo es el que lee el flujo de Actions, y por eso las líneas
de `printf` del `main` no se tocan. El binario se borra porque cada regla
lleva el nombre del ejecutable que produce, y si ese archivo existe `make`
responde `make: 'matriz' is up to date.` y no hace nada: cuando se compila a
mano hay que borrar el ejecutable antes de volver a `make`.

Las banderas son `-std=c++17 -O2`, y `-pthread` en las dos partes con hilos:

- `-std=c++17` fija el estándar que pide el repositorio; es el que trae
  `std::hardware_destructive_interference_size`.
- `-O2` es la optimización con la que se miden todos los tiempos; sin ella
  los ciclos son varias veces más lentos y la relación entre versiones
  cambia, y con ella aparece el efecto del contador en registro de la parte
  2.
- `-pthread` compila y enlaza con la biblioteca de hilos. En glibc 2.34 o
  posterior (Ubuntu 22.04 en adelante) el programa enlaza aunque falte; en
  sistemas más viejos sin la bandera el enlazador dice
  `undefined reference to 'pthread_create'`.

Para compilar a mano con las mismas banderas y el aviso de tipos mezclados:

```bash
g++ -std=c++17 -O2 -Wall -pthread -o falso falso_compartir.cpp && ./falso
```

Los tiempos se miden con el portátil conectado a la corriente y sin otra
cosa pesada abierta; en un portátil con el ahorro de energía activo la
primera corrida suele salir más lenta que la segunda.

### Windows con WSL2

Se instala Ubuntu desde WSL2 y todo lo anterior aplica igual, con dos
diferencias:

- El repositorio se clona dentro del sistema de archivos de Linux (`~/`),
  no en `/mnt/c/...`; el acceso a discos de Windows desde WSL es más lento
  y los tiempos salen con más ruido.
- `nproc` muestra los procesadores que Windows le asignó a la máquina
  virtual, que pueden ser menos que los del equipo; se ajustan en el archivo
  `.wslconfig`.

Compilar con MinGW o con Visual Studio directamente en Windows no sirve para
este ejercicio: ahí `long` mide 4 bytes, la cuenta de la parte 2 queda en
dieciséis posiciones por línea en vez de ocho, y la suma de la parte 4
(4.999.872.888.854) no cabe en 32 bits.

### macOS

```bash
xcode-select --install    # g++ (que es Apple clang), make y las cabeceras
sysctl hw.cachelinesize   # tamaño de la línea
sysctl hw.physicalcpu hw.logicalcpu
```

`make todo` funciona igual: Apple clang acepta `-std=c++17 -O2 -pthread`.
Lo que cambia:

- `std::hardware_destructive_interference_size` está en libc++ desde LLVM
  19; con un Xcode anterior el compilador dice que no está declarada y se
  escribe el número a mano.
- En los procesadores Apple Silicon (M1 en adelante) la línea de caché mide
  128 bytes, no 64. Los ocho `long` de separación del README pueden dejar
  dos contadores en la misma línea, y `separados` puede tardar casi lo mismo que
  `pegados` en el portátil aunque en el servidor sí baje. Para verlo en la
  propia máquina, separar dieciséis posiciones; lo que se entrega sigue
  siendo la versión de 64 bytes, la del servidor.
- Los tiempos cambian de una máquina a otra; la forma de las curvas y el
  paso donde cambia el régimen dependen de la línea, no del reloj.

Con `brew install gcc` queda disponible `g++-14` (o la versión que instale
Homebrew), que es GCC de verdad y se comporta como en Linux; para usarlo con
el `Makefile` basta `make CXX=g++-14 todo`.

### Enlaces

- [make(1)](https://man7.org/linux/man-pages/man1/make.1.html): la
  página de manual de GNU Make; qué archivo busca, cómo se nombran los
  objetivos y las opciones de la línea de comandos.
- [Opciones de enlace de GCC](https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html):
  qué hace `-pthread`.
- [Opciones de dialecto de GCC](https://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html):
  `-std=c++17` y los demás estándares.
- [tee](https://man7.org/linux/man-pages/man1/tee.1.html): la orden que
  duplica la salida a un archivo.
- [Instalar WSL](https://learn.microsoft.com/en-us/windows/wsl/install) y
  [sistemas de archivos en WSL](https://learn.microsoft.com/en-us/windows/wsl/filesystems):
  la instalación y por qué el repositorio va en el disco de Linux.
- [Building from the command line with Xcode](https://developer.apple.com/library/archive/technotes/tn2339/_index.html):
  las herramientas de línea de comandos de macOS.
- [Estado de C++17 en libc++](https://libcxx.llvm.org/Status/Cxx17.html):
  desde qué versión de LLVM está cada pieza, entre ellas la constante de
  interferencia.
