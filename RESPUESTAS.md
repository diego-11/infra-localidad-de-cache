# Respuestas

Nombre y código:

## Parte 3: el paso

¿Hasta qué paso el tiempo baja menos que las operaciones, y a partir de cuál
baja al mismo ritmo? Relacione ese punto con el tamaño de un `int` y de una
línea de caché.

## Parte 4: speedup, eficiencia y la ley de Amdahl

Con los tiempos de `amdahl.txt`:

| Hilos | Total medido | Speedup medido | Eficiencia | Speedup según Amdahl |
|---:|---:|---:|---:|---:|
| 1 |  | 1,00 | 1,00 | 1,00 |
| 2 |  |  |  |  |
| 4 |  |  |  |  |
| 8 |  |  |  |  |

Fracción paralelizable `p` (parte paralela sobre el total, con un hilo):

Techo del speedup con esa `p`, `1 / (1 - p)`:

¿Dónde se separa la columna medida de la que predice Amdahl, y qué lo
explica?
