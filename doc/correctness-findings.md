# Revisión de correctness — hallazgos y resolución

Bitácora de los problemas de correctness detectados (dos revisiones: un code review y un
análisis de un especialista en compiladores/HLS) y cómo se resuelven. Cada hallazgo se verifica
empíricamente antes y después del fix, con un test de rechazo cuando aplica.

## Bugs de correctness

| ID | Problema | Sev. | Estado | Resolución | PR |
| :- | :------- | :--- | :----- | :--------- | :- |
| F1 | Mezcla de signedness aceptada: `int32_t a + uint32_t b` emitía `(a + b)`, que SV evalúa unsigned. | Crítico | ✅ | Rechazo en arithmetic/relational/equality; literales que caben en el otro tipo se eximen; shifts no se tocan. Reject 34. | #7 |
| F2 | RAW intra-ciclo en `__sequential`: con NBA, una sentencia lee el valor viejo; el epílogo `result=…; return result;` devolvía basura. | Crítico | ✅ | Detección de RAW por grupo de ciclo (IDLE/iteración/epílogo) reusando `AccessSets`; rechazo. Reject 35. | #7 |
| F3 | Ancho de resultado: el SV usa el sizing por contexto, no el "tipo común con wrap" de la spec. | Alto | ✅ | Se enmendó la spec (el ancho sigue el contexto de SV; para envolver hay que castear). | #8 |
| F4 | Shift-count sin validar: `uint8_t x << 10` (o `<< -1`) se aceptaba. | Medio | ✅ | Si el conteo es constante, debe ser `0 ≤ b < ancho(a)`. Reject 36. | #8 |
| F5 | Asignación no definida de `out`: función no-void sin return en todo camino → latch inferido. | Medio | ✅ | Chequeo de retorno en todo camino (if/else exhaustivo o return terminal). Reject 37. | #8 |
| F6 | Overflow sin guardia: literal > 64 bits (UB) y aritmética `constexpr` en `int64` (UB). | Medio | ✅ | Lexer rechaza literal que no cabe en 64 bits (reject 38); constexpr usa builtins con chequeo de overflow. | #8 |
| F7 | `for` con step data-dependiente: `for(i=0;i<8;i+=n)` con `n` runtime se aceptaba (trip count dinámico, no sintetizable, ni el lint lo atrapa). | Alto | ✅ | El update también debe ser constante (salvo en `__sequential`). Reject 39. | #9 |
| F8 | `constexpr` emite SV basura: una función constexpr con `a[0]++` no evalúa y el generador emitía `localparam X = __call-1`. Además, misread de unión en `a[i]++`. | Alto | ✅ | Toda declaración `constexpr` debe evaluar a constante (reject 40); el intérprete falla limpio en lvalues no escalares. | #10 |
| F9 | Dirección de puerto incorrecta: una referencia escrita sólo vía una llamada anidada se emite como `input` en vez de `output`. | Alto | ✅ | `AccessSets` atribuye un argumento por referencia según lo que el callee lee/escribe del parámetro. Accept 32 (`output`, lint-clean). | #11 |
| F10 | Señal multi-driver: un local manejado a la vez por una asignación `always_comb` y por la instancia de una llamada (ref) se aceptaba (lo atrapaba sólo el lint). | Medio | ✅ | Chequeo de driver único en el frontend: un nombre no puede ser escrito por asignación y por un call (ref), ni por dos calls. Reject 41. | #12 |
| F11 | Tipo del resultado aritmético = operando izquierdo: `int8 a + int32 b` asignado a `int8` narrowa en silencio; `1 + b` (literal a la izq) se rechazaba aunque `b + 1` se aceptaba. | Alto | ✅ | Tipo común: una constante que se adapta toma el tipo del otro operando, si no gana el más ancho. Accept 34, reject 44. | #15 |
| F12 | Parámetros arreglo escritos se emitían como puerto `input` (no sintetizable, `ASSIGNIN`); era la forma canónica de producir un buffer de salida. | Alto | ✅ | La dirección de un `PARAM_ARRAY` se infiere de lecturas/escrituras como las referencias. Accept 35 (`output`, lint-clean). | #15 |
| F13 | Referencia leída y escrita en función combinacional: puerto `inout` manejado por `always_comb` que se lee a sí mismo → bucle combinacional (`UNOPTFLAT`). | Medio | ✅ | Se rechaza una referencia read-write en función no-`__sequential` (requiere registro). Reject 45. | #15 |
| F14 | Overflow de `-INT64_MIN` en `constexpr` (unario `-` e `__abs`) sin guardia (UB en el compilador). | Bajo | ✅ | Guardia de overflow en neg/abs del intérprete. | #15 |
| F15 | Local leído antes de asignarse en algún camino (p. ej. asignado sólo en una rama `if`) infería un latch; sólo lo atrapaba el lint. | Medio | ✅ | Pase de asignación definida: rechaza leer un local que no está asignado en todo camino (if/else exhaustivo, decl con init, etc.). Reject 46. | #16 |
| F16 | `return` temprano en función combinacional se miscompila en silencio: cada return emite `out = expr;` sin guarda, gana el último (p. ej. `clamp` con guard-clauses daba resultados erróneos). | Crítico | ✅ | Se exige que los returns estén en posición tail (sin código después); el idioma soportado es `if/else` con un return por rama. Reject 47. | #17 |
| F17 | Parámetro arreglo leído y escrito en función combinacional → puerto `inout` con bucle (`UNOPTFLAT`); el chequeo de ref read-write no cubría arreglos. | Medio | ✅ | El chequeo de read-write se extiende a `PARAM_ARRAY`. Reject 48. | #17 |
| F18 | Compound-assign (`+=`/`-=`/`*=`) en `constexpr` sin guarda de overflow (UB), inconsistente con `_evalBinary`. | Bajo | ✅ | Builtins con chequeo de overflow en los compound-assign del intérprete. | #17 |
| F19 | Intrínsecos `__min`/`__max`/`__abs` fijaban el tipo de resultado a `int32_t`: `__min(int64,int64)` narrowaba en silencio, mezcla de signo no se rechazaba, y `__min(uint,uint)` se over-rechazaba. Misma clase que F11/F1. | Alto | ✅ | El resultado sigue a los operandos: `__abs` conserva el tipo, `__min`/`__max` el tipo común y rechazan mezcla de signo. Accept 36, reject 49/50. | #19 |

## Descartados (verificados, no eran bugs)

| ID | Sospecha | Veredicto |
| :- | :------- | :-------- |
| D1 | `static_cast<int32_t>(int8_t)` haría zero-extend en vez de sign-extend. | ⚪ Falso. `signed'(32'(y))` sign-extiende (probado: `-1` → `-1`). |

## Limitaciones conocidas (sound, no bugs)

- *Asignación definida a nivel de nombre, no de elemento*: escribir `a[0]` marca todo el arreglo `a` como asignado, así que `a[0]=1; return a[1];` se acepta aunque `a[1]` quede sin manejar (el lint de Verilator no detecta latch por-elemento en arreglos unpacked). Igual aplica a buffers de salida parcialmente manejados. Rastrear por-elemento es un trabajo aparte.
- *Over-rejection de la asignación definida en bucles*: un local asignado en todo iteración de un bucle que siempre corre, y leído después, se rechaza (el pase descarta las asignaciones del cuerpo porque el bucle "podría no correr"). Conservador/sound; inicializar la variable lo evita.
- *Read-write de arreglo a nivel de nombre*: el chequeo de read-write combinacional (F17) es por nombre base, así que `a[1] = a[0] + 1` (elementos distintos, sin self-loop) también se rechaza. Conservador/sound; no usa la maquinaria de slices del aliasing.
- Construcción de listas O(n²) en `BisonActions` (append al final): irrelevante para tamaños reales.
- Profundidad de recursión del DFS de no-recursión: límite teórico de stack.
- Globales de call-nodes en el generador: use-after-free latente, hoy inalcanzable (bloqueado por un chequeo semántico).

## Hecho (features)

- Aliasing de slices: dos referencias a elementos con índice constante distinto del mismo arreglo (`a[0]`, `a[1]`) ya no se rechazan; el chequeo de aliasing y el de driver único son slice-aware. Accept 33, reject 43.

## Pendiente (features, no bugs)

Documentadas con su diseño y decisiones abiertas en la sección _Trabajo futuro_ de
`doc/language-reference.typ`; no implementadas.

- FSM secuencial multi-bucle (bucles anidados + llamadas dentro de `__sequential`).
- Handshake `ready`/`valid` con backpressure.
