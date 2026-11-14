#set document(
  title: "Referencia del Lenguaje",
  author: ("Juan Ignacio Raggio", "Geronimo Naso Rodriguez", "Santiago Fernandez Pacheco", "Manuel Blacker"),
)

#set page(
  paper: "a4",
  margin: (top: 2.5cm, bottom: 2.5cm, left: 2cm, right: 2cm),
  numbering: "1",
  number-align: bottom + right,
)

#set text(font: "New Computer Modern", size: 11pt, lang: "es", hyphenate: true)
#set par(justify: true, leading: 0.65em, spacing: 1.0em)

#set heading(numbering: "1.1")
#show heading.where(level: 1): it => { set text(size: 15pt, weight: "bold", hyphenate: false); v(0.4em); it; v(0.2em) }
#show heading.where(level: 2): it => { set text(size: 12pt, weight: "bold", hyphenate: false); v(0.3em); it; v(0.15em) }

#show raw.where(block: false): box.with(fill: luma(240), inset: (x: 3pt, y: 0pt), outset: (y: 3pt), radius: 2pt)
#show raw.where(block: true): block.with(fill: luma(240), inset: 8pt, radius: 4pt, width: 100%)
#show link: underline

#align(center)[
  #text(size: 20pt, weight: "bold", hyphenate: false)[Referencia del Lenguaje]
  #v(0.3em)
  #text(size: 11pt, fill: gray)[Especificación normativa de la semántica]
]
#line(length: 100%, stroke: 1pt)

Referencia normativa: fija qué garantiza cada construcción. El _porqué_ de las decisiones vive
en el documento de diseño (`1er-entrega.typ`). Se formaliza por áreas; la primera es la
semántica de enteros.

#table(
  columns: (auto, 1fr),
  fill: (_, row) => if row == 0 { luma(220) } else { white },
  [*Decisión*], [*Elección*],
  [Overflow], [Error si es estáticamente detectable; envuelve (complemento a dos) en runtime.],
  [Ancho de operación], [Tipo común (estilo C), sin promoción a un ancho base.],
  [Conversiones], [Ensanchar implícito; estrechar o cambiar signedness requiere cast.],
  [Casts], [`static_cast<T>` (valor) y `bit_cast<T>` (bits, mismo ancho); sin `dynamic_cast`.],
  [Bucles], [`for` y `while` con límite/condición constante en compilación; se desenrollan.],
  [Referencias], [Escalares, no indexables; los buffers son parámetros arreglo.],
  [`__parallel`], [Sentencias independientes (sin dependencias RAW/WAW/WAR).],
  [Llamadas], [Instanciación de módulo.],
  [`constexpr`], [Evaluado en compilación; vars/arreglos a `localparam`; funciones sin hardware.],
  [Recursión], [Prohibida (directa y mutua).],
  [División / módulo], [Fuera del lenguaje.],
  [Hardware], [Combinacional por defecto; `__sequential` genera una FSM con handshake.],
)

= Semántica de enteros

== Tipos

Ocho enteros de ancho fijo: `int8_t`, `int16_t`, `int32_t`, `int64_t` (con signo, complemento a
dos) y sus variantes `uint*` (sin signo). El ancho (8/16/32/64) y la signedness son explícitos y
parte del tipo; no hay enteros de ancho dependiente de la plataforma.

== Literales

Decimal (`42`), hexadecimal (`0x2A`) o binario (`0b101010`); `_` es separador y se ignora
(`1_000`). Un literal es convertible a cualquier tipo entero que represente su valor
(`uint8_t x = 200;` es válido; `= 300` es error, @sec-int-overflow). Sin contexto, su tipo es
`int32_t`.

== Conversiones implícitas

Solo las que preservan el valor: ensanchamiento con la misma signedness (`intN` a `intM` o
`uintN` a `uintM`, con M > N). Estrechar o cambiar signedness —incluida la mezcla con/sin signo
en una operación— requiere cast explícito (@sec-int-cast).

== Casts <sec-int-cast>

Dos casts nombrados, ambos siempre definidos (nunca error de overflow):

`static_cast<T>(expr)` convierte el valor:

- Estrechar: conserva los bits bajos (truncamiento).
- Ensanchar: extensión de signo (con signo) o de ceros (sin signo).
- Mismo ancho, otra signedness: reinterpreta el patrón de bits.

`bit_cast<T>(expr)` reinterpreta el patrón de bits sin conversión; exige que `T` y el operando
tengan el mismo ancho (si no, error semántico).

`dynamic_cast` no aplica: requiere RTTI y polimorfismo, inexistentes en hardware.

== Ancho del resultado <sec-int-width>

En una operación binaria ambos operandos deben tener la misma signedness (si no, error que exige
cast). El más angosto se ensancha al más ancho (el _tipo común_); el resultado aritmético tiene
el tipo común, sin promoción a un ancho base, y puede desbordarlo (@sec-int-overflow). Para el
producto completo, ensanchar antes:

```c
uint16_t a, b;
uint32_t prod = (uint32_t)a * (uint32_t)b;  // 32 bits, no desborda
```

Las comparaciones producen `bool` cualquiera sea el ancho de los operandos.

== Overflow <sec-int-overflow>

- _Estáticamente detectable_ (operandos constantes y evaluación `constexpr`): error de
  compilación. Ej.: `uint8_t x = 300;`, `constexpr int8_t y = 100 + 100;`.
- _En runtime_: envuelve módulo $2^n$ (complemento a dos), comportamiento nativo del hardware.
  Saturación u otra política debe implementarse explícitamente.

== Desplazamientos

`a << b` y `a >> b` tienen el tipo de `a`; `b` debe ser no negativo y menor que el ancho de `a`
(excederlo, si es estáticamente determinable, es error). `>>` es aritmético si `a` es con signo,
lógico si es sin signo.

== División y módulo

`/` y `%` no forman parte del lenguaje (división genérica costosa en hardware; se posterga). Un
programa que los use es rechazado.

= Iteración y referencias

Los bucles `for` y `while` requieren que su límite/condición sea constante en tiempo de
compilación (literal, `constexpr` global, o parámetro dentro de una función `constexpr`); se
desenrollan. La iteración data-dependiente solo se admite dentro de una función `__sequential`
(@sec-sequential), que se traduce a una FSM en vez de a lógica combinacional.

Las referencias (`&`) son parámetros escalares de entrada/salida y no son indexables. Un buffer se
pasa como parámetro arreglo (`tipo nombre[tamaño]`).

= Mapeo a hardware

== Combinacional por defecto

Cada función se traduce a un módulo *combinacional*: los parámetros por valor son puertos de
entrada, las referencias son puertos de salida, el retorno es el puerto `out`, y el cuerpo es
lógica combinacional (`always_comb`). Los `for` de límite estático se desenrollan. No hay puertos
de reloj ni reset.

== Bloques `__parallel`

Cada sentencia del bloque se emite como un `always_comb` independiente; es válido porque el
análisis garantizó que no hay dependencias (RAW/WAW/WAR) entre ellas.

== Llamadas a función

Una llamada instancia el módulo invocado, conecta sus argumentos a los puertos de entrada y
expone el resultado en una señal que reemplaza a la llamada en la expresión.

== `constexpr`

Las variables y arreglos `constexpr` se evalúan en compilación y se emiten como `localparam` en un
_package_ compartido (los arreglos, con sus valores ya resueltos). Las funciones `constexpr` no
generan hardware.

== Lógica secuencial (`__sequential`) <sec-sequential>

Una función anotada con `__sequential` se traduce a una FSM con `always_ff`, puertos `clk`/`rst` y
handshake `start`/`done`, en vez de lógica combinacional. Reusa hardware por iteración a cambio de
latencia, y por eso admite bucles con límite data-dependiente.

El cuerpo se interpreta como preámbulo (declaraciones y asignaciones simples) + un bucle +
epílogo (el `return`). La FSM tiene tres estados:

- `CCC_IDLE`: con `done` en `0`, espera `start`; al llegar inicializa los registros y pasa a
  `CCC_RUN`.
- `CCC_RUN`: mientras la condición del bucle se cumple ejecuta una iteración por ciclo; cuando deja
  de cumplirse registra el resultado en `out` y pasa a `CCC_DONE`.
- `CCC_DONE`: asigna `done` en `1` por un ciclo y vuelve a `CCC_IDLE`.

Las actualizaciones de registro usan asignación no bloqueante (`<=`): cada iteración lee los
valores del flanco anterior. Los parámetros por valor son entradas que deben mantenerse estables
durante la ejecución. Limitaciones actuales: un único bucle no anidado y sin llamadas a función en
el cuerpo.
