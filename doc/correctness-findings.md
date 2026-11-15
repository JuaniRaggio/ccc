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

## Descartados (verificados, no eran bugs)

| ID | Sospecha | Veredicto |
| :- | :------- | :-------- |
| D1 | `static_cast<int32_t>(int8_t)` haría zero-extend en vez de sign-extend. | ⚪ Falso. `signed'(32'(y))` sign-extiende (probado: `-1` → `-1`). |

## Menores / conocidos (no bloquean)

- Colisión de numeración en tests (`reject/32` duplicado): higiene, el harness los corre igual.
- Construcción de listas O(n²) en `BisonActions` (append al final): irrelevante para tamaños reales.
- Profundidad de recursión del DFS de no-recursión: límite teórico de stack.
- Globales de call-nodes en el generador: use-after-free latente, hoy inalcanzable (bloqueado por un chequeo semántico).

## Pendiente (features, no bugs)

- Aliasing de slices solapados: el chequeo `unique`/`aliased` colapsa `a[i]`/`a[j]` a la base (conservador).
- FSM secuencial multi-bucle (anidados + llamadas); handshake `ready`/`valid`.
