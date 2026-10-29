# Contrato de semántica e IR de RobotLang — v1

## Estado real y alcance

Inspección del 29 de octubre de 2026: el repositorio estaba limpio y contenía
`token.h/.cpp`, `scanner.h/.cpp`, `main.cpp`, `CMakeLists.txt` y
`examples/square.robot`. No existían Parser, AST ni interfaces Visitor del AST.

Este módulo pertenece al integrante 2. Implementa chequeo de tipos de dominio,
validación semántica y construcción de IR. No implementa ni modifica Scanner,
Parser, AST, optimizador, generador JavaScript, runtime o interfaz web.
El Visitor añadido recorre exclusivamente la IR; no impone una jerarquía al AST.

**Estado de integración:** biblioteca y pruebas integradas en CMake. `robotc`
sigue ejecutando exclusivamente el Scanner original. No existe todavía una
integración Parser → AST → semántica, ni se afirma que la demostración la pruebe.

## Gramática propuesta para coordinar con el integrante 1

El Scanner reconoce las palabras y símbolos necesarios, pero no valida una
gramática. Este módulo adopta el siguiente contrato provisional, basado en el
alcance del MVP. Debe ratificarse con el responsable del Parser:

```text
program     := worldDecl robotDecl statement* END
worldDecl   := WORLD NUM NUM
robotDecl   := ROBOT AT NUM NUM FACING direction
direction   := NORTH | EAST | SOUTH | WEST
statement   := FORWARD NUM | LEFT NUM | RIGHT NUM
             | REPEAT NUM LBRACE statement* RBRACE
```

Se requieren exactamente un `world` y un `robot`, en ese orden, antes de las
instrucciones y fuera de cualquier `repeat`. Se admiten bloques vacíos y cero
instrucciones después de las declaraciones. Se admiten repeticiones anidadas.
No se introducen variables, identificadores, declaraciones de tipos,
expresiones, signos, decimales ni nuevos tokens.

El Parser deberá validar `at`, `facing`, llaves, cantidad de operandos y sintaxis
completa. La semántica vuelve a comprobar unicidad, orden y ámbito para no
depender de que el AST los garantice. Los errores léxicos/sintácticos impiden
invocar el adaptador semántico sobre un programa incompleto.

## Tipos de dominio y restricciones de valores

`types.h` expone tipos fuertes distintos. No hay conversión implícita entre
`Distance`, `Angle`, `Count`, `Coordinate` y `Dimension`, ni desde un entero.
`Direction` es un `enum class`. No son tipos nuevos de token: el Scanner conserva
`NUM`; el contexto del operando determina su tipo semántico.

| Rol del operando | Tipo semántico | Validación del valor |
| --- | --- | --- |
| ancho y alto de `world` | `Dimension` | entero > 0 |
| x e y de `robot at` | `Coordinate` | entero >= 0; x < ancho, y < alto |
| argumento de `forward` | `Distance` | entero > 0 |
| argumento de `left`/`right` | `Angle` | exactamente 90, 180 o 270 |
| argumento de `repeat` | `Count` | entero >= 0 |
| argumento de `facing` | `Direction` | North, East, South o West |

**Chequeo de tipos de dominio:** cada posición del contrato requiere un token
de la familia apropiada (`NUM` o dirección), asigna el rol correspondiente y
produce un operando `Located<T>` con ese tipo. El resultado distingue, por
ejemplo, `Count(90)` de `Angle(90)`. No hay inferencia de variables, coerciones,
tabla de símbolos ni operadores aritméticos, porque la gramática no los tiene.

**Validación de valores:** comprobar positividad, pertenencia al conjunto de
ángulos, representabilidad y relación entre coordenadas y dimensiones. Un
`NUM` con lexema `45` sí tiene la familia numérica requerida para `Angle`, pero
falla su restricción de valor. `Angle(0)` es inválido; `Count(0)` es válido.

### Representación y overflow

- `Integer` es `std::int64_t`.
- El rango público del MVP es `0..MaxInteger`, donde
  `MaxInteger = 9007199254740991 = 2^53 - 1`.
- La cota conserva representación exacta cuando el integrante 3 use números
  JavaScript. Es una decisión explícita de este contrato pendiente de ratificar;
  cambiar a todo `int64_t` requeriría acordar otra representación en el backend.
- La conversión valida dígitos ASCII y comprueba el límite **antes** de cada
  multiplicación y suma. No usa `atoi`, `stoi`, conversiones con truncamiento ni
  conversiones intermedias a coma flotante.
- Un literal arbitrariamente largo fuera de rango produce un diagnóstico, sin
  desbordar el acumulador. Se aceptan ceros iniciales, incluso muchos ceros.
- Un `NUM` vacío, con signo o con caracteres no decimales indica un fallo del
  adaptador: el Scanner actual no emite esos lexemas como un único `NUM`.
  Por ejemplo, `-1` produce `ERR("-")` y `NUM("1")`; no se añade sintaxis negativa.
- Los constructores explícitos de los tipos fuertes son para productores de IR
  confiables; **no validan valores**. La garantía se obtiene mediante
  `SemanticAnalyzer::finish(...).ok()`. Un consumidor que construya o modifique
  IR manualmente debe mantener todas las invariantes del contrato.

## Comprobaciones estáticas y errores de ejecución

La semántica comprueba declaraciones, tipos, valores literales y posición
inicial. No simula estados posteriores, ni calcula posiciones finales, ni
expande repeticiones. Esto incluye comprobar estáticamente los cuerpos de
`repeat 0`: una distancia 0 en ese cuerpo sigue siendo un error semántico.

Un movimiento con distancia válida **no se rechaza por poder salir del mundo**,
incluso cuando resulte evidente a partir de los literales. El módulo tampoco
emite advertencias sobre ese caso. El futuro runtime deberá comprobarlo durante
la ejecución. Un `repeat 0` no ejecuta su cuerpo ni produce errores dinámicos de
ese cuerpo.

Coordenadas acordadas: origen `(0,0)` arriba a la izquierda, `north` resta y,
`east` suma x, `south` suma y y `west` resta x. El dominio espacial es
`0 <= x < ancho`, `0 <= y < alto`. Una dimensión 1 contiene la coordenada 0.

`examples/square.robot` comienza en `(2,2)` hacia el norte y ejecuta
`forward 3`. Pasa el análisis estático, pero ese movimiento intenta alcanzar
y = -1. El ejemplo se conserva sin cambios; no se certifica su ejecución exitosa.

### Contrato dinámico propuesto al integrante 3

Se propone que `forward d` represente d pasos unitarios observables, con
verificación antes de cada paso. Al intentar salir del mundo, se detiene con
error en el `MoveForward` original; los pasos previos válidos permanecen en la
trayectoria y el paso fallido no cambia el estado. Un giro cambia la orientación
de forma atómica. `right` avanza en el ciclo north → east → south → west;
`left` lo recorre en sentido contrario, una posición por cada 90 grados.

Esta granularidad y la política de estado al fallar **deben ratificarse con el
simulador**. No hay runtime implementado en este repositorio. Si el simulador
elige movimientos atómicos en lugar de pasos, debe actualizarse este contrato
antes de implementar optimizaciones que dependan de esa elección. La IR conserva
distancia, giro, orden y origen suficientes para ambas políticas.

En JavaScript, no debe calcularse primero `x + distancia` o `y + distancia`
sin comprobar límites: la suma podría superar el rango exacto. Se puede
comprobar cada paso, o calcular espacio disponible mediante diferencias entre
coordenadas válidas y dimensiones, conservando la trayectoria y el punto de
fallo acordados. Esta regla es para el consumidor; no se implementa ejecución.

## Interfaz pública para el integrante 1

`SemanticAnalyzer` es un receptor de llamadas, no un árbol de nodos ni un
reconocedor de texto. No recibe archivos, no consume un flujo de tokens y no
decide qué producción gramatical se ha encontrado. El futuro adaptador puede
ser un Visitor del AST real o un recorrido equivalente. No se requiere que el
compañero use nombres concretos de clases, herencia ni un método `accept` dado.

| Construcción del AST real | Llamada, en orden de fuente |
| --- | --- |
| declaración del mundo | `world(keyword, width, height)` |
| declaración del robot | `robot(keyword, x, y, facing)` |
| avance | `forward(keyword, distance)` |
| giro izquierdo | `left(keyword, angle)` |
| giro derecho | `right(keyword, angle)` |
| inicio de repetición | `beginRepeat(keyword, count)` |
| cuerpo de repetición | recorrer todas sus sentencias, una vez, en orden |
| fin de repetición | `endRepeat(closingBrace)` |
| fin del programa | `finish(endToken)` |

Todos los argumentos son `const Token&` del `token.h` existente. Los tokens se
leen durante la llamada; el analizador no guarda referencias al AST o al Scanner.
`finish` devuelve datos propietarios. Se puede destruir el AST después.

Datos mínimos que el AST/adaptador debe preservar:

1. Token de la palabra clave de cada instrucción, incluidos `left` y `right`
   por separado.
2. Token original de cada literal: `type`, **lexeme decimal sin convertir**,
   `line` y `col`. Guardar solo `int` pierde información y puede desbordar antes
   del análisis; en ese caso el compañero debe extender su contrato.
3. Token de orientación, con uno de los cuatro tipos cardinales existentes.
4. Orden de sentencias, anidamiento de los cuerpos y token de cierre de bloque.
5. Token `END` para ubicar declaraciones ausentes. Las ubicaciones se copian
   del Scanner; son base 1, y no se recalculan columnas o tabulaciones.

Cada programa usa una instancia nueva. El recorrido emite cada declaración e
instrucción exactamente una vez. No debe omitir cuerpos de contador cero, ni
desenrollar bucles. Debe balancear `beginRepeat`/`endRepeat`, llamar `finish`
una sola vez y detener la integración si el resultado tiene errores.
Cualquier llamada tras `finish` lanza `std::logic_error` por mal uso de la API.

### Resultado y diagnósticos

`AnalysisResult` contiene:

- `std::optional<ir::Program> program`: presente únicamente si no hubo errores.
- `std::vector<Diagnostic> diagnostics`: errores independientes acumulados.
- `bool ok() const`: indica IR válida disponible.

No se publica IR parcial. La declaración de un mundo inválido no dispara
errores derivados de coordenadas contra dimensiones desconocidas. Los
diagnósticos se acumulan al detectarse; no se garantiza ordenamiento por línea
(por ejemplo, un bloque sin cerrar se detecta al finalizar).

Cada `Diagnostic` incluye categoría, código estable, línea/columna, tipo de
dominio esperado cuando corresponde y mensaje. Los consumidores deben usar
el código, no analizar el texto del mensaje. Se imprime, por ejemplo:

```text
3:9: semantic[non_positive_value] Value must be greater than zero (Distance)
```

| Categoría | Códigos |
| --- | --- |
| `Semantic` | `IntegerOutOfRange`, `NonPositiveValue`, `InvalidAngle`, `DuplicateWorld`, `DuplicateRobot`, `DeclarationOrder`, `NestedDeclaration`, `MissingWorld`, `MissingRobot`, `InitialPositionOutOfBounds` |
| `Contract` | `InvalidTokenKind`, `InvalidIntegerLiteral`, `InvalidDirection`, `UnmatchedRepeatEnd`, `UnclosedRepeat` |

`Contract` indica un adaptador/AST que incumple la interfaz tras el análisis
léxico y sintáctico. Por eso una dirección que no sea un token cardinal se
clasifica aquí: el Parser debería haberla rechazado. Los errores de valores
válidamente tokenizados se clasifican como semánticos. Los errores dinámicos
no aparecen en `AnalysisResult`; corresponderán al runtime.

## Interfaz pública para el integrante 3

El consumidor incluye `ir.h` y enlaza `robotlang_semantic`. `ir.h` y `types.h`
no dependen de Token, Scanner, Parser, AST, JavaScript ni servicios web.
La IR es un árbol de valores propietario, copiable y movible, sin referencias
colgantes a nodos del frontend. Las estructuras son públicas para recorridos y
transformaciones confiables; no equivalen a un validador de IR arbitraria.

`ir::Program` contiene `instructions` y ubicación `end`. Cada
`ir::Instruction` contiene `Origin { id, source }` y una `Operation`
implementada mediante `std::variant`:

| Operación | Campos |
| --- | --- |
| `WorldInit` | `Located<Dimension> width, height` |
| `RobotInit` | `Located<Coordinate> x, y`; `Located<Direction> facing` |
| `MoveForward` | `Located<Distance> distance` |
| `Turn` | `TurnSense sense` (`Left`/`Right`); `Located<Angle> angle` |
| `Repeat` | `Located<Count> count`; `vector<Instruction> body`; `SourceLocation closingBrace` |

`Located<T>` contiene `value` y `source`. Para los tipos numéricos,
`operand.value.value()` obtiene el entero. La ubicación de la instrucción
corresponde a su palabra clave; cada operando conserva su ubicación propia.
No se guardan comentarios ni formato decimal original, porque la IR representa
valores tipados, no una copia del texto fuente.

Invariantes de una IR publicada por el analizador:

- `instructions[0]` es `WorldInit`, `instructions[1]` es `RobotInit`.
- Son las únicas declaraciones y nunca aparecen en cuerpos de `Repeat`.
- Todas las restricciones de la tabla de tipos se cumplen.
- El orden de instrucciones y el anidamiento coinciden con el recorrido fuente.
- Cada instrucción tiene un ID único dentro del programa, asignado desde 1 en
  preorden. El ID no pretende ser estable entre compilaciones tras editar fuente.
- Repeticiones conservan su contador y cuerpo; no se ejecutan ni se expanden.
- Los giros conservan su sentido y ángulo; no se normalizan ni se combinan.

### Recorridos y serialización

`ir::walk(program, visitor)` usa `ConstVisitor`, con una sobrecarga `visit`
por operación y `leave` opcional para `Repeat`. Recorre cada instrucción una
vez en preorden, incluido el cuerpo de `repeat 0`. Llama `leave` después del
cuerpo. No es un intérprete y no repite visitas según el contador.
También se pueden recorrer directamente los vectores con `std::visit`.

`ir::write(program, ostream)` y `ir::toString(program)` producen texto legible
determinista con tipos, valores, IDs, ubicaciones y bloques. `RobotIR v1` es
un formato de diagnóstico; no se implementa deserializador y el contrato del
backend es la API C++, no un parser de ese texto.

Ejemplo de fragmento generado por la demostración:

```text
#3 @4:1 Repeat count=Count(4)@4:8 {
  #4 @5:3 MoveForward distance=Distance(3)@5:11
  #5 @6:3 Turn sense=right angle=Angle(90)@6:9
} @7:1
```

### Preservación de trayectoria, estados intermedios y errores

La corrección de una optimización no se reduce a conservar la posición final.
Debe conservar la secuencia de movimientos y giros observables, los estados
intermedios, el primer error y su origen, y las acciones realizadas antes de él.

La IR conserva las operaciones originales, las distancias, los límites de
repeticiones, el orden, los IDs y las ubicaciones necesarios para ello. Una
ocurrencia dinámica puede identificarse mediante:

```text
(instructionId, [(repeatId, indiceIteracionBase0), ...], indicePasoBase0)
```

La pila de iteraciones y los pasos se derivan durante la ejecución; no se
almacena una trayectoria precalculada ni se expande un contador grande.
El índice de paso se aplica a avances si se ratifica ejecución por pasos.

Restricciones del contrato v1 para el optimizador:

1. Conservar IDs y ubicaciones de las operaciones originales que se ejecuten.
2. No reordenar operaciones ni adelantar un fallo dinámico frente a acciones
   que debían ocurrir antes.
3. No sustituir `forward a; forward b` por un único avance que pierda los dos
   orígenes y sus fronteras, ni eliminar giros solo porque se cancelen al final.
4. No desenrollar/compactar repeticiones perdiendo el contexto de iteración.
5. No eliminar errores semánticos en cuerpos de contador cero: la optimización
   recibe solamente resultados ya validados.
6. Si se quiere fusionar operaciones, extender primero el contrato con un mapa
   ordenado de acciones originales y puntos de fallo. Un único `Origin` no
   representa varias acciones. Esta versión no afirma soportar esa fusión.

Se puede optimizar el código emitido manteniendo esos eventos y comprobaciones.
Este módulo prepara y documenta la información; no implementa optimizaciones
ni demuestra equivalencia con un simulador que todavía no existe aquí.

## Demostración y verificación reproducible

Con CMake, un compilador C++17 y sus herramientas de construcción en PATH:

```sh
cmake -S . -B build/semantic -DBUILD_TESTING=ON
cmake --build build/semantic
ctest --test-dir build/semantic --output-on-failure
```

Para generadores multiconfiguración, seleccionar la misma configuración al
compilar y probar, por ejemplo `--config Debug` y `ctest -C Debug`.

Ejecutables:

- `robotc examples/square.robot --tokens`: Scanner original.
- `semantic_ir_tests`: pruebas unitarias, sin dependencias de frameworks.
- `semantic_contract_demo`: llamadas del contrato → IR validada legible.

La demostración usa el Scanner original sobre una fixture fija equivalente a
`square.robot` y posiciones de tokens conocidas. No reconoce programas
arbitrarios, no introduce un Parser y no construye un AST alternativo. Cuando
exista el AST acordado, se añadirá un adaptador que emita las mismas llamadas y
una prueba real de AST → IR; **esa demostración sigue pendiente**.

Verificación realizada en este entorno:

- CMake con Ninja y GCC 15.2.0, compilación Debug C++17 sin advertencias.
- 13 grupos unitarios aprobados: ejemplo válido/serialización, cuatro
  direcciones y giros válidos, valores cero, límites de coordenadas, ángulos
  inválidos, límite numérico en todos los operandos, entradas enormes,
  ceros iniciales, declaraciones, bloques, contrato, Visitor y errores múltiples.
- 3/3 pruebas CTest aprobadas: suite unitaria, demostración y ejemplo del Scanner.
- Las pruebas validan que posibles salidas del mundo no se rechazan estáticamente.
- No se ha probado MSVC ni un backend JavaScript/runtime. La condición de CMake
  para sus advertencias no equivale a certificar esa plataforma.

CMake y CTest no estaban en PATH. Para esta verificación se usaron los binarios
instalados en Visual Studio y `C:/w64devkit/bin/g++.exe`, sin instalar software.
Los artefactos locales están en `build/semantic-review`, ignorado por Git.

## Registro de archivos

| Estado | Archivo | Propósito |
| --- | --- | --- |
| Añadido | `src/types.h` | tipos de dominio, ubicaciones y rango numérico |
| Añadido | `src/semantic.h` | contrato de entrada, resultado y diagnósticos |
| Añadido | `src/semantic.cpp` | validación y construcción de IR |
| Añadido | `src/ir.h` | IR estructurada y Visitor |
| Añadido | `src/ir.cpp` | recorrido y serialización legible |
| Añadido | `tests/semantic_ir_tests.cpp` | pruebas válidas, inválidas y de límites |
| Añadido | `examples/semantic_contract_demo.cpp` | demostración sin AST ni Parser |
| Añadido | `docs/semantic_ir_contract.md` | este contrato y registro |
| Modificado | `CMakeLists.txt` | biblioteca, demostración, CTest y flags según compilador |

Se conservan sin cambios `src/main.cpp`, `src/scanner.h/.cpp`, `src/token.h/.cpp`,
`examples/square.robot`, README y `.gitignore`. No se realizan commits, push ni
merges.

## Coordinación pendiente y límites

- **Integrante 1:** ratificar el orden de declaraciones, bloques vacíos y
  anidamiento; entregar clases reales del AST, Visitor o recorrido público,
  tokens originales y EOF; entonces implementar/probar el adaptador. El módulo
  no exige una jerarquía de clases determinada.
- **Integrante 3:** consumir únicamente IR validada, mantener las invariantes de
  procedencia y eventos, ratificar rango numérico, granularidad de movimiento y
  estado al fallar antes de optimizar o implementar el runtime.
- **Profundidad y recursos:** no se fija una cota pedagógica de anidamiento,
  contador o tamaño de programa adicional al rango numérico. Los contadores no
  se ejecutan en compilación. Los recorridos/serialización usan recursión y
  memoria proporcional al programa; una política de límites de recursos para
  entradas extremas debe acordarse por separado. No hay garantías para
  profundidad arbitraria ni simulaciones de duración ilimitada.
- **Origen:** se conserva línea/columna, no nombre de archivo ni intervalos
  completos de texto. El llamador conoce el archivo asociado al programa.
- **Integración pendiente:** `robotc` aún no aplica estas reglas; enlazar una
  biblioteca no sustituye el adaptador AST ni una prueba de la cadena completa.
