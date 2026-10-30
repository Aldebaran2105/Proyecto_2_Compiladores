# Contrato semántico e IR de RobotLang

## Pipeline

`Scanner` produce tokens con ubicaciones. `Parser` construye el AST definido en `src/ast.h`, con una clase por construcción, propiedad mediante `std::unique_ptr` y un visitante de impresión. `SemanticVisitor` recorre ese árbol y construye la representación de `src/ir.h`; `SemanticAnalyzer` valida la representación, resuelve prioridades y asocia los bloques `run` con sus robots. Solo se entrega IR cuando no hay diagnósticos.

El AST y la IR son objetos distintos. El optimizador recibe una copia de la IR: las transformaciones no modifican el árbol mostrado por `--ast`.

## Lenguaje y validación

Un programa contiene un mundo con dimensiones positivas, terreno y al menos un robot, seguido por cero o más bloques `run`. Cada robot tiene un identificador único, posición inicial única y orientación. Un bloque `run` debe referirse a un robot declarado y no puede repetirse para el mismo robot. Un robot sin `run` permanece en su celda.

Las coordenadas son no negativas, `(0,0)` es la esquina inferior izquierda y `north` incrementa `y`. Los muros ocupan rectángulos semiabiertos y deben caber en el mundo. Un obstáculo se transforma en un muro de 1×1; los huecos no pueden coincidir con muros y los robots no pueden comenzar sobre terreno.

Los giros son 90, 180 o 270 grados. Las distancias y repeticiones son positivas. Los números se representan con `int64_t` y no pueden exceder `9007199254740991`; el parser informa la ubicación del literal que excede el límite. La posición inicial se comprueba estáticamente y los bloqueos durante la simulación.

Las prioridades explícitas son únicas y pertenecen a `1..N`. Las prioridades faltantes toman los números disponibles en orden de declaración; la prioridad menor ejecuta primero.

## IR y ejecución

La IR contiene dimensiones, muros, huecos y una tabla de robots con prioridades resueltas y comandos. Cada instrucción tiene un ID y una ubicación de origen; `repeat` conserva su cuerpo anidado. `robotc archivo.robot --ir` muestra esta representación como `RobotIR v2`.

El simulador ejecuta una acción por robot activo y por tick, en orden de prioridad. `forward` se detiene antes de un borde, muro o robot; entrar en un hueco produce una caída. Los robots terminados siguen bloqueando. El límite de ticks se puede cambiar con `--max-ticks`.

El optimizador fusiona avances y simplifica giros consecutivos dentro de cada lista de comandos, sin cruzar límites de `repeat`. Antes de aceptar la IR candidata compara el estado final de todos los robots con el de la IR original. La traza por tick no tiene por qué ser idéntica.

El generador produce assembly GNU x86-64 con comentarios para cada comando y, en modo optimizado, comentarios para las transformaciones aceptadas. No emite directivas `.loc` ni símbolos por instrucción. Las pruebas ensamblan y enlazan ejemplos sin ejecutar los binarios generados.
