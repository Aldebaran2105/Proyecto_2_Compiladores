# RobotLang

> Compilador y simulador de un lenguaje para programar robots sobre una cuadrícula con terreno.

RobotLang es el proyecto de Compiladores (CS3402). El objetivo es compilar programas en un lenguaje propio y simular su ejecución de forma determinista. El programa se divide en una parte **descriptiva** (`world`) que define el tamaño, los muros, los obstáculos, los huecos y los robots, y una parte **declarativa** (`run`) que define el comportamiento de cada robot.

La implementación actual incluye análisis léxico, parser y validación de la extensión multirrobot, IR, simulación round robin, optimización y generación de assembly x86-64.

## Estado actual

| Componente | Estado |
| --- | --- |
| Análisis léxico | Implementado. Reconoce el lenguaje base y los tokens de la extensión: `obstacle`, `hole`, `wall`, `priority`, `run` e identificadores. |
| Semántica e IR | El visitante semántico baja el AST a IR y valida la extensión. |
| Parser y AST | Parser descendente y AST con clases y visitantes integrados en `robotc`. |
| Optimizador | Implementado: fusión de movimientos y simplificación de giros con verificación de estado final. |
| Codegen x86-64 | Implementado; genera assembly optimizado o sin optimizar con comentarios de comandos y transformaciones. |
| Extensión multirrobot | Parser y validación implementados. |
| Simulador, grilla y traza | Implementados. |
| Pruebas | CTest cubre el frontend, la semántica, el simulador, el optimizador y la generación y ensamblado de código. |

`robotc` utiliza una sola gramática y una sola cadena de compilación.

## Compilación y AST

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure

./build/robotc examples/square.robot --tokens
./build/robotc examples/patrol.robot --ast
./build/robotc examples/patrol.robot --ir
```

`--ast` muestra el árbol original con ubicaciones, antes del análisis semántico y la optimización. El visitante semántico transforma ese árbol en una IR separada.

Ejemplo de salida del AST:

```text
Program @1:1
  World 10 8 @1:1
    Wall 4 3 2 2 @2:3
    Hole 7 2 @3:3
    Robot r1 1 1 east priority 1 @4:3
    Robot r2 8 1 west priority 2 @5:3
  Run r1 @7:1
    Forward 3 @8:3
    Right 90 @9:3
    Forward 2 @10:3
  Run r2 @12:1
    Forward 6 @13:3
```

## Objetivo de la extensión

La extensión fijada es un mundo con varios robots con nombre, terreno, prioridad total y comportamiento declarativo por robot.

```robot
world 10 8 {
  wall 4 3 2 2
  hole at 7 2
  robot r1 at 1 1 facing east priority 1
  robot r2 at 8 1 facing west priority 2
}

run r1 {
  forward 3
  right 90
  forward 2
}

run r2 {
  forward 6
}
```

Salida actual:

```text
$ ./build/robotc examples/patrol.robot --run
r1: (4, 0) south
r2: (5, 1) west
```

### Código optimizado y sin optimizar

```sh
./build/robotc examples/optimization.robot --emit build/original.s
./build/robotc examples/optimization.robot --optimize --emit build/optimized.s
gcc -c build/original.s -o build/original.o
gcc -c build/optimized.s -o build/optimized.o
```

Los comentarios de cada archivo identifican el robot, el comando y su ubicación; el archivo optimizado también contiene comentarios con las transformaciones realizadas. No se usan directivas `.loc` ni símbolos por instrucción. El generador produce un programa x86-64 con un ciclo por ticks y estado de cada robot. Las pruebas actuales comprueban y ensamblan la salida; aún no ejecutan los binarios generados.

Por ejemplo, el archivo optimizado contiene:

```asm
# rewrite merge_moves: #1 + #2 -> #1
# rewrite simplify_turns: #3 + #4 -> -
```

El optimizador fusiona avances consecutivos, combina giros consecutivos y elimina repeticiones sin acciones. No cruza los límites de `repeat` y evita fusionar distancias que excedan el máximo numérico. Como estas transformaciones cambian el número de ticks, el compilador simula la versión original y la candidata y conserva la optimización únicamente si todos los robots terminan con la misma posición, orientación y estado. La traza por tick puede cambiar.

## Características

### Núcleo actual

- Scanner con tokens base y tokens de la extensión.
- Tipos de dominio fuertes: `Dimension`, `Coordinate`, `Distance`, `Angle`, `Count` y `Direction`.
- Validación semántica de declaraciones, valores, ángulos, direcciones y posición inicial.
- IR estructurada con IDs, ubicaciones y `repeat` anidados.
- Diagnósticos con código estable, línea y columna.
- Pruebas unitarias sin frameworks externos.

### Extensión fijada

- Mundo descriptivo con tamaño, muros, obstáculos y huecos.
- Varios robots con nombre, posición, orientación y prioridad.
- Comportamiento declarativo por robot con `run`.
- Simulación round robin determinista.
- Interacciones: bloqueo entre robots, caída en huecos y robots terminados que ocupan su celda.
- Prioridad total: orden de declaración por defecto, `priority` para sobrescribir y error si hay duplicados.
- Optimizaciones IR: fusión de movimientos consecutivos y simplificación de giros consecutivos, verificadas contra el simulador.
- Reporte final, grilla ASCII y traza por tick.
- Generación de assembly x86-64 con comentarios de instrucciones y optimizaciones.
- C++17 y CMake, sin dependencias externas.

## Lenguaje

### Gramática

```ebnf
Program   ::= World { Run }
World     ::= "world" NUM NUM "{" { Entity | Robot } "}"
Entity    ::= "obstacle" "at" NUM NUM
            | "hole" "at" NUM NUM
            | "wall" NUM NUM NUM NUM
Robot     ::= "robot" ID "at" NUM NUM "facing" Direction [ "priority" NUM ]
Run       ::= "run" ID "{" { Command } "}"
Command   ::= "forward" NUM
            | ( "right" | "left" ) NUM
            | "repeat" NUM "{" { Command } "}"
Direction ::= "north" | "south" | "east" | "west"
```

Reglas de la extensión:

- `(0,0)` está abajo a la izquierda; `north` sube en `y`, `east` sube en `x`.
- Giros válidos: 90, 180 y 270.
- Distancias y repeticiones mayores que 0.
- Una celda contiene como máximo un robot.
- El mundo debe contener al menos un robot. Los identificadores deben ser únicos; cada robot puede tener como máximo un bloque `run`.
- `wall x y w h` ocupa el rectángulo `[x, x+w)` × `[y, y+h)`.
- `obstacle at x y` es un muro de 1×1.
- El terreno debe estar dentro del mundo. Los muros pueden solaparse, pero un hueco no puede coincidir con terreno sólido y un robot no puede comenzar en terreno.
- Un robot que entra a un hueco cae y sale de la simulación.
- Los números se representan con enteros de 64 bits y no pueden superar `9007199254740991`.
- `repeat` es la única construcción de repetición; no existe `for`.

## Simulación

### Prioridad

Cada robot tiene una prioridad entera de `1` a `N`, donde `N` es la cantidad de robots. Menor número se ejecuta primero.

1. `priority p` es opcional.
2. Las prioridades explícitas deben ser únicas y estar en `[1, N]`.
3. Los robots sin prioridad reciben los números libres en orden de declaración.
4. El resultado siempre es una permutación de `1..N`; es imposible tener prioridades repetidas.

Ejemplo: con `r1 priority 3`, `r2` sin prioridad, `r3 priority 1` y `r4` sin prioridad, el orden es `r3, r2, r1, r4`.

### Turnos

- Un tick ejecuta un comando por robot activo, en orden de prioridad.
- `forward N` intenta avanzar hasta `N` celdas en la dirección actual; se detiene antes de un muro, un borde o un robot. Es atómico por tick.
- `right` y `left` consumen un tick.
- `repeat` y `run` expanden sus comandos; cada comando interno consume su tick.
- Un robot sin comandos permanece en su celda y sigue ocupándola.
- Los robots de menor prioridad ven el mundo ya actualizado por los de mayor prioridad.
- La simulación se corta con un límite de ticks para evitar bucles infinitos.
- El límite predeterminado es 100000 ticks y se puede cambiar con `--max-ticks N`.

### Interacciones

- Un robot bloquea a otro si está en la trayectoria.
- Un robot puede seguir a otro si este ya se movió en el mismo tick.
- No hay intercambio de celdas ni resolución de conflictos: el orden de prioridad decide.
- `--trace` reporta cada movimiento, bloqueo y caída.

## Compilar y ejecutar

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/robotc examples/square.robot --tokens
```

### CLI

```text
robotc <archivo.robot> [opciones]

--tokens        imprime los tokens
--ast           imprime el AST con sangría
--ir            imprime la representación intermedia
--run           simula y reporta el estado final
--grid          dibuja el mundo en ASCII
--trace         muestra la simulación tick por tick
--emit RUTA     genera assembly x86-64
--optimize      optimiza la IR al usar --ir o --emit
--max-ticks N   fija el límite de ticks
```

## Estructura

```text
src/token.*      tipos de token y nombres
src/scanner.*    análisis léxico
src/types.h      tipos de dominio, ubicaciones y rango numérico
src/ast.*         clases del AST y visitante de impresión
src/parser.*      parser descendente recursivo
src/ast_semantic.* visitante semántico: AST a IR
src/semantic.*    validación y serialización de IR
src/ir.h          representación intermedia
src/main.cpp      CLI
src/optimizer.*   optimizaciones IR
src/simulator.*   prioridades y simulación round robin
src/codegen.*     generación de assembly x86-64
examples/        programas de ejemplo
tests/           pruebas
docs/            contrato semántico y guion de demo
```


