# Revisión de correctness

Bitácora de los bugs encontrados en 4 rondas de revisión (code review + especialista en
compiladores/HLS) y su resolución. Cada bug tiene su propio header; si en el futuro aparece algo
relacionado, extender bajo el header correspondiente. Todo se verificó empíricamente, con un test de
aceptación o rechazo cuando aplica. "Reject N" / "Accept N" refieren a `src/test/c/{reject,accept}/`.

## Semántica de enteros

### F1 — Mezcla de signedness aceptada

`int32_t a + uint32_t b` se aceptaba y emitía `(a + b)`, que SystemVerilog evalúa unsigned.
Fix: se rechaza mezclar signedness en aritmética, relacionales e igualdad; una constante que cabe en
el otro tipo se exime; los shifts no se tocan. Reject 34, PR #7.

### F3 — Ancho del resultado vs spec

El SV generado usa el sizing por contexto de SystemVerilog, no el "tipo común con wrap" que decía la
spec. Fix: se enmendó la spec (el ancho sigue el contexto; para envolver a un ancho hay que castear).
PR #8.

### F4 — Shift-count sin validar

`uint8_t x << 10` (o `<< -1`) se aceptaba. Fix: si el conteo es constante, debe cumplir
`0 <= b < ancho(a)`. Reject 36, PR #8.

### F6 — Overflow de literales y constexpr sin guardia

Un literal mayor a 64 bits y la aritmética `constexpr` en `int64` eran UB. Fix: el lexer rechaza el
literal que no cabe en 64 bits (reject 38); el intérprete usa builtins con chequeo de overflow. PR #8.

### F11 — Tipo del resultado aritmético = operando izquierdo

`int8 a + int32 b` asignado a `int8` narrowaba en silencio; `1 + b` se rechazaba aunque `b + 1` se
aceptaba. Fix: el resultado es el tipo común (una constante que se adapta toma el tipo del otro
operando, si no gana el más ancho). Accept 34, reject 44, PR #15.

### F14 — Overflow de `-INT64_MIN` en constexpr

El unario `-` y `__abs` sobre `INT64_MIN` eran UB. Fix: guardia de overflow en neg y abs del
intérprete. PR #15.

### F18 — Compound-assign constexpr sin guardia de overflow

`+=` / `-=` / `*=` en `constexpr` no chequeaban overflow, inconsistente con `_evalBinary`. Fix:
builtins con chequeo de overflow en los compound-assign. PR #17.

### F19 — Tipo de resultado de intrínsecos fijo en `int32_t`

`__min` / `__max` / `__abs` ignoraban los operandos: `__min(int64,int64)` narrowaba en silencio,
mezcla de signo no se rechazaba, `__min(uint,uint)` se over-rechazaba (misma clase que F11/F1). Fix:
el resultado sigue a los operandos (`__abs` conserva el tipo; `__min`/`__max` el tipo común y rechazan
mezcla de signo). Accept 36, reject 49/50, PR #19.

## Referencias, arreglos y drivers

### F9 — Dirección de puerto de una referencia escrita vía llamada

Una referencia escrita sólo pasándola a otra función se emitía como `input` en vez de `output`. Fix:
`AccessSets` atribuye un argumento por referencia según lo que el callee lee/escribe del parámetro.
Accept 32, PR #11.

### F10 — Señal multi-driver

Un local manejado a la vez por una asignación `always_comb` y por la instancia de una llamada (ref)
se aceptaba (sólo lo atrapaba el lint). Fix: chequeo de driver único en el frontend (un nombre no
puede ser escrito por asignación y por un call, ni por dos calls). Reject 41, PR #12.

### F12 — Parámetro arreglo escrito emitido como `input`

Escribir un parámetro arreglo producía un puerto `input` asignado en el cuerpo, no sintetizable
(`ASSIGNIN`); era la forma canónica de producir un buffer de salida. Fix: la dirección de un
`PARAM_ARRAY` se infiere de lecturas/escrituras como las referencias. Accept 35, PR #15.

### F13 — Referencia leída y escrita en función combinacional

Un puerto `inout` manejado por `always_comb` que se lee a sí mismo forma un bucle combinacional
(`UNOPTFLAT`). Fix: se rechaza una referencia read-write en función no-`__sequential` (requiere
registro). Reject 45, PR #15.

### F17 — Arreglo leído y escrito en función combinacional

Mismo bucle que F13 pero con un parámetro arreglo; el chequeo de read-write no cubría arreglos. Fix:
se extiende el chequeo a `PARAM_ARRAY`. Reject 48, PR #17.

## Flujo de control combinacional

### F5 — `out` no asignado en todo camino

Una función no-void sin return en todo camino infería un latch. Fix: chequeo de retorno en todo
camino (if/else exhaustivo o return terminal). Reject 37, PR #8.

### F15 — Local leído antes de asignarse

Un local asignado sólo en una rama `if` y leído después infería un latch; sólo lo atrapaba el lint.
Fix: pase de asignación definida que rechaza leer un local no asignado en todo camino. Reject 46,
PR #16.

### F16 — Return temprano miscompilado

Cada return emitía `out = expr;` sin guarda, así que con varios returns ganaba el último (p. ej.
`clamp` con guard-clauses daba resultados erróneos). Silencioso, ni el lint lo atrapaba. Fix: se
exige que los returns estén en posición tail; el idioma soportado es `if/else` con un return por
rama. Reject 47, PR #17.

### F26 — Llamada dentro de un bucle combinacional

`for (...) s += g(a[i]);` se aceptaba y generaba una sola instancia de `g` conectada a `a[i]` fuera
del `always_comb`, así que el circuito no calculaba la suma; ni el lint lo detectaba. Fix: se rechaza
una llamada dentro de un bucle de una función combinacional. Reject 51.

## Backend secuencial

### F2 — RAW intra-ciclo en `__sequential`

Con asignación no bloqueante, una sentencia que lee lo que otra anterior del mismo ciclo escribió lee
el valor viejo; el epílogo `result = acc+1; return result;` devolvía basura. Fix: detección de RAW por
grupo de ciclo (IDLE/iteración/epílogo) reusando `AccessSets`, con rechazo. Reject 35, PR #7.

### F7 — `for` con step data-dependiente

`for(i=0;i<8;i+=n)` con `n` runtime se aceptaba (trip count dinámico, no sintetizable, y el lint no lo
atrapaba). Fix: el update también debe ser constante, salvo en `__sequential`. Reject 39, PR #9.

### F20 — Return o bucle dentro de un bloque `{ }` suelto en `__sequential`

Un `return` dentro de un bloque suelto se aceptaba y el generador no emitía nada para él: `out` nunca
se asignaba, sin error ni warning del lint. Un bloque suelto de sentencias simples también se
descartaba en silencio. Un bucle dentro de un bloque suelto se rechazaba, pero con un mensaje de RAW
engañoso. Fix: se rechazan los bucles y `return` dentro de un bloque suelto, y el generador emite los
bloques de sentencias simples en el ciclo que los contiene. Reject 55 y 56, accept 41 (simulado).

### F21 — Locales con el mismo nombre declarados dos veces

Todo local se declara a nivel de módulo, así que dos `for (uint32_t i ...)` emitían `logic [31:0] i;`
dos veces y Verilator e Icarus fallaban por declaración duplicada (también en funciones
combinacionales). Con tipos distintos el registro quedaba mal tipado. Fix: se rechaza ocultar o
redeclarar un nombre visible y reusar un nombre con otro tipo o tamaño; los locales hermanos del mismo
tipo comparten una única declaración. Reject 58 y 59, accept 40 (simulado) y 42.

### F22 — `for` sin condición

`for (init; ; update)` se aceptaba y la FSM emitía `if ()`, que no compila. Fix: todo `for` debe
tener condición. Reject 57.

### F23 — Referencia leída y escrita en `__sequential` emitida como `inout`

Se emitía un puerto `inout` asignado desde `always_ff`. Verilator lo rechaza (`Unsupported tristate
construct`) e Icarus también (`not a valid l-value`). Fix: la referencia se divide en la entrada
`<nombre>_in`, que se carga al recibir `start`, y el registro de salida `<nombre>`; vale también para
arreglos. Se rechaza un nombre que choque con los que agrega el generador. Accept 39 (simulado),
reject 61 y 62.

### F24 — RAW dentro de un `if` en un ciclo de `__sequential`

El chequeo de RAW trataba cada `if` como una sola sentencia, así que `if (c) { a = i; b = a; }` no se
detectaba y `b` leía el valor viejo de `a`. Fix: el chequeo recorre el ciclo en orden, entrando en las
ramas del `if` y en los bloques. Reject 60.

### F25 — Scheduler duplicado entre análisis y generador

El análisis de hazards reimplementaba la segmentación en ciclos del generador, así que podían
discrepar sobre qué sentencias comparten un ciclo. Fix: ambos usan `SeqSchedule.c`. El ancho del
registro de estado (antes fijo en 16 bits, sin chequeo) se deriva de la cantidad de estados.

## Constexpr

### F8 — Constexpr no evaluable emitía SV basura

Una función `constexpr` con `a[0]++` no evaluaba y el generador emitía `localparam X = __call-1`.
Además había un misread de unión en `a[i]++`. Fix: toda declaración `constexpr` debe evaluar a
constante (reject 40); el intérprete falla limpio en lvalues no escalares. PR #10.

## Descartados (verificados, no eran bugs)

### D1 — static_cast con signo

Sospecha: `static_cast<int32_t>(int8_t)` haría zero-extend en vez de sign-extend. Falso: emite
`signed'(32'(y))`, que sign-extiende (probado: `-1` da `-1`).

## Limitaciones conocidas (sound, no bugs)

### Asignación definida a nivel de nombre, no de elemento

Escribir `a[0]` marca todo el arreglo `a` como asignado, así que `a[0]=1; return a[1];` se acepta
aunque `a[1]` quede sin manejar (Verilator no detecta latch por-elemento en arreglos unpacked). Igual
aplica a buffers de salida parcialmente manejados. Rastrear por-elemento es un trabajo aparte.

### Read-write de arreglo a nivel de nombre

El chequeo de F17 es por nombre base, así que `a[1] = a[0] + 1` (elementos distintos, sin self-loop)
también se rechaza. Conservador y sound; no usa la maquinaria de slices del aliasing.

### Over-rejection de la asignación definida en bucles

Un local asignado en toda iteración de un bucle que siempre corre, y leído después, se rechaza (el
pase descarta las asignaciones del cuerpo porque el bucle podría no correr). Conservador y sound;
inicializar la variable lo evita.

### Otros menores

- Construcción de listas O(n^2) en `BisonActions` (append al final): irrelevante para tamaños reales.
- Profundidad de recursión del DFS de no-recursión: límite teórico de stack.
- Globales de call-nodes en el generador: use-after-free latente, hoy inalcanzable (bloqueado por un
  chequeo semántico).

## Features implementadas durante la revisión

### Aliasing de slices disjuntos

Dos referencias a elementos con índice constante distinto del mismo arreglo (`a[0]`, `a[1]`) ya no se
rechazan; el chequeo de aliasing y el de driver único son slice-aware. Accept 33, reject 43.

### FSM secuencial multi-bucle

`__sequential` admite múltiples bucles secuenciales y bucles anidados: emisor de FSM general con
estados numerados (desazucara `for` a init + `while`, emisión recursiva por regiones) y chequeo de
hazard RAW recursivo. Restricciones: sin llamadas en el cuerpo, un bucle no puede estar dentro de un
`if`, el `return` debe ser la última sentencia top-level, y un bloque `{ }` suelto no puede contener
bucles ni `return` (cada una con su propio mensaje de error; reject 52 a 56). Accept 37 (anidado),
38 (dos bucles), ambos simulados con Icarus. Una iteración de `while` toma 2 ciclos y una de `for`, 3
(ver `doc/language-reference.typ`).

## Pendiente (features, no bugs)

Documentadas con su diseño y decisiones abiertas en la sección _Trabajo futuro_ de
`doc/language-reference.typ`; no implementadas.

- Llamadas a función dentro de `__sequential` (sub-FSM con handshake). Los bucles múltiples y anidados ya están implementados.
- Handshake `ready`/`valid` con backpressure.
