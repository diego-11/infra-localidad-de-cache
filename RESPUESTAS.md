# Respuestas

Nombre y código:

## Parte 3: el paso

¿Hasta qué paso el tiempo baja menos que las operaciones, y a partir de cuál
baja al mismo ritmo? Relacione ese punto con el tamaño de un `int` y de una
línea de caché.

El tiempo baja menos que el número de operaciones hasta un paso de **16**;
a partir de ese paso baja aproximadamente al mismo ritmo. Un `int` ocupa
normalmente 4 bytes y una línea de caché 64 bytes, por lo que caben 16 `int`
en una línea. Hasta `paso = 16` todavía se aprovecha la localidad espacial:
al acceder a un elemento se trae también el resto de la línea. Desde `paso =
16`, cada acceso cae, en general, en una línea distinta y desaparece ese
beneficio, de modo que el tiempo queda dominado por el número de accesos.



## Parte 4: speedup, eficiencia y la ley de Amdahl


Con los tiempos de `amdahl.txt`:

| Hilos | Total medido | Speedup medido | Eficiencia | Speedup según Amdahl |
|---:|---:|---:|---:|---:|
| 1 | `T₁ = ___` | 1,00 | 1,00 | 1,00 |
| 2 | `T₂ = ___` | `T₁/T₂` | `(T₁/T₂)/2` | `1/(s + (1-s)/2)` |
| 4 | `T₄ = ___` | `T₁/T₄` | `(T₁/T₄)/4` | `1/(s + (1-s)/4)` |
| 8 | `T₈ = ___` | `T₁/T₈` | `(T₁/T₈)/8` | `1/(s + (1-s)/8)` |

Fracción paralelizable `p` (parte paralela sobre el total, con un hilo):

`p = ___` (se obtiene de la configuración o del ajuste de los datos; si se
conoce el tiempo con un hilo y el tiempo con `n` hilos, puede estimarse con
`p = (1 - 1/Sₙ) / (1 - 1/n)`, donde `Sₙ = T₁/Tₙ`).

Techo del speedup con esa `p`, `1 / (1 - p)`:

`1 / (1 - p) = ___`

¿Dónde se separa la columna medida de la que predice Amdahl, y qué lo
explica?
