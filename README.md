# Localidad de caché y false sharing

Infraestructuras Paralelas y Distribuidas
Escuela de Ingeniería de Sistemas y Computación, Universidad del Valle
Carlos Andrés Delgado Saavedra

[![Pruebas](../../actions/workflows/pruebas.yml/badge.svg)](../../actions/workflows/pruebas.yml)

Lo que cada parte necesita de las bibliotecas y herramientas está en
[DOCUMENTACION.md](DOCUMENTACION.md), con ejemplos que corren y los enlaces
a la documentación oficial.

Cuatro programas cortos. En los dos primeros la misma cuenta se hace de dos
maneras, el resultado es idéntico y el tiempo no, y la explicación está en
cómo viajan los datos entre la memoria y la caché. El tercero mide cuánto
cuesta traer una línea, y el cuarto pone números a la ley de Amdahl.

| Parte | Archivo | Qué se mide |
|---|---|---|
| 1 | `matriz.cpp` | El orden del recorrido |
| 2 | `falso_compartir.cpp` | Dos hilos sobre la misma línea |
| 3 | `paso.cpp` | El costo de una línea, aislado |
| 4 | `amdahl.cpp` | Speedup, eficiencia y el techo de la parte secuencial |

## Requisitos

| Qué | Linux (Debian/Ubuntu) | macOS | Windows |
|---|---|---|---|
| `g++` con C++17, `make` y `pthread` | `sudo apt install build-essential` | `xcode-select --install` | WSL2 con Ubuntu y el comando de Linux |

En macOS `g++` es Apple clang y compila estos cuatro programas sin cambios.
En los Mac con Apple Silicon la línea de caché es de 128 bytes y no de 64,
así que la separación de la parte 2 puede no mostrar la diferencia en el
portátil; lo que se entrega sigue siendo la versión de 64 bytes, que es la
que mide el servidor. En Windows, MSYS2 con MinGW-w64 también sirve, pero
WSL2 evita sorpresas con `make` y con los tiempos.

Cómo dejar cada sistema listo, paso a paso, está en
[DOCUMENTACION.md](DOCUMENTACION.md), al final.

## Parte 1: recorrer una matriz

`matriz.cpp` guarda una matriz de 2048 por 2048 en un solo vector, fila tras
fila: el elemento de la fila `i` y la columna `j` está en la posición
`i * N + j`. Hay que completar dos funciones que sumen todos los elementos:

- `por_filas`: recorre fila por fila.
- `por_columnas`: recorre columna por columna.

```bash
make matriz
```

La salida queda también en `matriz.txt`. Las dos sumas tienen que dar
`2048 * 2048`; si una da otra cosa, el recorrido dejó elementos por fuera.

## Parte 2: false sharing

`falso_compartir.cpp` lanza cuatro hilos que incrementan su propio contador
cincuenta millones de veces. Hay que escribir dos versiones:

- `pegados`: los cuatro contadores viven en posiciones contiguas del vector.
- `separados`: cada contador queda en su propia línea de caché. Una línea son
  64 bytes y un `long` ocupa 8, así que basta con dejar ocho posiciones entre
  contador y contador.

```bash
make falso
```

Ninguna de las dos versiones necesita cerrojos: cada hilo escribe en su propia
posición. La diferencia de tiempo no viene de la corrección sino del protocolo
de coherencia entre núcleos.

Si las dos versiones tardan 0 ms, el compilador guardó el contador en un
registro y escribió una sola vez al final: aplicó por su cuenta la corrección.
Incrementar a través de un puntero `volatile long *` obliga a ir a memoria en
cada vuelta, y ahí la diferencia aparece.

## Parte 3: el paso

`paso.cpp` recorre un arreglo de 128 MiB multiplicando por 3 una de cada
`paso` posiciones, con el paso en 1, 2, 4, ... 1024. Al duplicar el paso se
hace la mitad de las operaciones. Hay que completar `recorrer`, que hace ese
recorrido y devuelve cuántas posiciones tocó.

```bash
make paso
```

La tabla trae, para cada paso, las operaciones, el tiempo y el tiempo por
operación. La suma de la última columna comprueba que se tocaron las
posiciones correctas: con `N` unos y `k` operaciones tiene que dar `N + 2k`.

Un `int` ocupa 4 bytes y una línea 64, así que hasta el paso 16 cada
duplicación sigue trayendo las mismas líneas. La pregunta va en
`RESPUESTAS.md`: dónde empieza el tiempo a caer al ritmo de las operaciones
y por qué justo ahí.

## Parte 4: speedup, eficiencia y la ley de Amdahl

`amdahl.cpp` tiene dos partes. `generar` produce diez millones de valores en
una cadena donde cada uno sale del anterior, y por eso no se puede repartir.
`procesar` trabaja cada valor por separado y sí se reparte. Hay que completar
`trozo`, lo que hace cada hilo sobre su tramo, y `en_paralelo`, que parte el
arreglo en `k` trozos, lanza los hilos, los une y suma los parciales.

```bash
make amdahl
```

El programa mide las dos partes con 1, 2, 4 y 8 hilos e imprime los tiempos.
Lo demás se calcula a mano en `RESPUESTAS.md`: el speedup `T(1) / T(k)`, la
eficiencia `S(k) / k`, la fracción paralelizable `p` a partir de los tiempos
con un hilo, y el speedup que predice Amdahl para cada `k` con esa `p`. La
tabla medida y la predicha se separan en algún punto, y la explicación de ese
punto es la parte que importa.

## Qué revisa el flujo de Actions

- Parte 1: que las dos sumas den el valor correcto y que el recorrido por
  columnas no salga más rápido que el de filas.
- Parte 2: que las dos cuentas den el valor correcto, que la versión con los
  contadores pegados no tarde 0 ms, y que separarlos baje el tiempo.
- Parte 3: que cada paso toque las posiciones que le tocan.
- Parte 4: que la suma sea la misma con cualquier número de hilos y que con
  cuatro la parte paralela baje al menos a la mitad.
- Que `RESPUESTAS.md` tenga la tabla y las explicaciones.

Cada parte es un job aparte: la lista de verificaciones del commit dice cuál
quedó en verde y cuál no, y la pestaña del run trae un resumen con la salida
de cada programa y el conteo de partes en verde. Cuando una verificación de
tiempos falla, el flujo repite la corrida una vez antes de marcar rojo, y el
error queda anotado sobre el archivo de esa parte. Un push nuevo cancela el
run anterior.

## Lo que hay que poder explicar

Cuánto más lento resultó el recorrido por columnas y por qué, con la línea de
caché en la explicación. Cuánto costó tener los contadores pegados, y por qué
separarlos arregla algo que no era un error de programación. Si en su máquina
la diferencia es menor que en el servidor, vale la pena mirar el tamaño de la
caché de su procesador.

En qué paso la tabla de la parte 3 cambia de régimen y qué tiene que ver con
los 64 bytes de la línea. Y con qué `p` salió su programa, hasta dónde puede
llegar el speedup con esa `p` por más núcleos que tenga, y por qué la
medición con ocho hilos queda por debajo de lo que predice la fórmula.
