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
#set par(justify: true, leading: 0.65em, spacing: 1.2em)

#set heading(numbering: "1.1")
#show heading.where(level: 1): it => { set text(size: 16pt, weight: "bold", hyphenate: false); v(0.5em); it; v(0.3em) }
#show heading.where(level: 2): it => { set text(size: 13pt, weight: "bold", hyphenate: false); v(0.4em); it; v(0.2em) }

#show raw.where(block: false): box.with(fill: luma(240), inset: (x: 3pt, y: 0pt), outset: (y: 3pt), radius: 2pt)
#show raw.where(block: true): block.with(fill: luma(240), inset: 10pt, radius: 4pt, width: 100%)
#show link: underline

#align(center)[
  #v(1em)
  #text(size: 22pt, weight: "bold", hyphenate: false)[Referencia del Lenguaje]
  #v(0.5em)
  #text(size: 12pt, fill: gray)[Especificación normativa de la semántica]
  #v(1em)
]
#line(length: 100%, stroke: 1pt)
#v(1em)

Este documento es la referencia normativa del lenguaje: fija sin ambigüedad la semántica de
cada construcción, de modo que un programa válido tenga un único significado y una única
traducción a hardware. Complementa al documento de diseño (`1er-entrega.typ`), que justifica
las decisiones; aquí se describe _qué_ garantiza el lenguaje, no _por qué_. Se construye por
áreas; la primera en formalizarse es la semántica de enteros.

= Semántica de enteros

== Tipos enteros

El lenguaje provee ocho tipos enteros de ancho fijo: `int8_t`, `int16_t`, `int32_t`, `int64_t`
(con signo, en complemento a dos) y `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t` (sin signo).
El ancho en bits es explícito y parte del tipo; no existe un tipo entero cuyo ancho dependa de
la plataforma. La _signedness_ (con o sin signo) y el _ancho_ (8, 16, 32 o 64 bits) son
atributos independientes de cada tipo.

== Literales enteros

Un literal entero se escribe en decimal (`42`), hexadecimal (`0x2A`) o binario (`0b101010`).
El carácter `_` puede intercalarse entre dígitos como separador visual y se ignora
(`0xFF_00`, `1_000_000`).

Un literal entero no tiene un tipo fijo: es convertible implícitamente a cualquier tipo entero
capaz de representar su valor. `uint8_t x = 200;` es válido porque 200 entra en `uint8_t`;
`uint8_t y = 300;` es un error de compilación porque 300 no entra (ver @sec-int-overflow). En
ausencia de un contexto que lo restrinja, un literal tiene tipo `int32_t`.

== Conversiones implícitas

Una conversión implícita entre tipos enteros se permite únicamente cuando preserva el valor
para todo valor del tipo origen, es decir, cuando es un _ensanchamiento con la misma
signedness_:

- `intN` a `intM` con M > N (extensión de signo).
- `uintN` a `uintM` con M > N (extensión con ceros).

Toda otra conversión —estrechamiento (M < N) o cambio de signedness— requiere una conversión
explícita (@sec-int-cast). En particular, mezclar un operando con signo y uno sin signo en una
misma operación es un error: el programador debe convertir explícitamente uno de los dos. Esta
regla elimina la clase de errores silenciosos más común de C (el truncamiento implícito y las
comparaciones signed/unsigned inesperadas).

== Conversiones explícitas (casts) <sec-int-cast>

La conversión explícita usa la sintaxis `(tipo)expresión`:

```c
uint8_t  low  = (uint8_t)value;        // estrecha: conserva los 8 bits bajos
int32_t  wide = (int32_t)small;        // ensancha con extensión de signo
uint32_t bits = (uint32_t)signedValue; // reinterpreta el patrón de bits
```

La semántica de `(T)x` según la relación entre el tipo de `x` y `T`:

- _Estrechar_ (T más angosto): se conservan los bits bajos del origen (truncamiento).
- _Ensanchar_ (T más ancho): extensión de signo si el origen es con signo, extensión con ceros
  si es sin signo.
- _Mismo ancho, distinta signedness_: reinterpretación del patrón de bits; la representación no
  cambia.

Un cast nunca produce un error de overflow: es una operación explícita cuyo resultado está
siempre definido.

_Dirección futura._ La sintaxis `(T)x` hereda de C el problema de que un mismo operador realiza
operaciones distintas según los tipos involucrados (truncar, extender o reinterpretar bits), sin
que la intención quede visible en el código. Se prevé migrar a casts nombrados al estilo de C++,
que separan la intención en construcciones distintas y greppables: `static_cast<T>(x)` para una
conversión de valor (extensión o truncamiento) y `bit_cast<T>(x)` para la reinterpretación del
patrón de bits (mismo ancho, cambio de signedness). No se contempla `dynamic_cast`: depende de
información de tipos en tiempo de ejecución (RTTI) y de polimorfismo, que no tienen contraparte
en hardware sintetizable. Si en el futuro se necesitara un estrechamiento verificado, sería una
construcción propia (por ejemplo un `checked_cast` que satura o expone una señal de error), con
un costo en lógica explícito, no un `dynamic_cast`.

== Ancho del resultado de una operación <sec-int-width>

En una operación binaria (`+`, `-`, `*`, `<<`, `>>`, comparaciones) ambos operandos deben tener
la misma signedness; en caso contrario es un error que exige un cast. El operando más angosto se
ensancha implícitamente al tipo del más ancho, el _tipo común_. Para los operadores aritméticos
(`+`, `-`, `*`) el resultado tiene el tipo común: no hay promoción automática a un ancho base,
de modo que el ancho del hardware generado corresponde exactamente al de los operandos.

```c
int16_t a; int32_t b;
a + b    // a se ensancha a int32_t; el resultado es int32_t
```

Como el resultado conserva el ancho del tipo común, una operación puede desbordarlo (ver
@sec-int-overflow). Para obtener el producto completo de dos valores de N bits, el programador
ensancha explícitamente antes de operar:

```c
uint16_t a, b;
uint32_t prod = (uint32_t)a * (uint32_t)b;  // 32 bits, no desborda
```

Las comparaciones (`<`, `<=`, `==`, `!=`, `>`, `>=`) producen `bool`, cualquiera sea el ancho de
los operandos.

== Overflow <sec-int-overflow>

El desborde se trata según pueda determinarse en tiempo de compilación:

- _Detectable estáticamente_ (operandos constantes: literales, variables `constexpr`, y toda la
  evaluación de un contexto `constexpr`): el desborde del tipo resultante es un *error de
  compilación*. Ejemplos: `uint8_t x = 300;`, `constexpr int8_t y = 100 + 100;`.
- _No detectable_ (al menos un operando es un valor de runtime): la aritmética *envuelve* módulo
  $2^n$ (complemento a dos, con $n$ el ancho del tipo), que es el comportamiento nativo del
  hardware y no genera lógica adicional.

La regla captura en compilación todo desborde demostrable y define sin ambigüedad el resto. Un
programa que requiera saturación u otra política debe implementarla explícitamente.

== Desplazamientos

`a << b` y `a >> b` tienen el tipo de `a` (el operando izquierdo). `b` se interpreta como la
cantidad de posiciones; debe ser no negativo y menor que el ancho de `a`. Si estáticamente se
determina que `b` alcanza o excede el ancho, es un error de compilación. `<<` descarta los bits
que exceden el ancho. `>>` es aritmético (extiende el signo) cuando `a` es con signo, y lógico
(introduce ceros) cuando `a` es sin signo.

== División y módulo

Los operadores `/` y `%` no forman parte del lenguaje: la división entera genérica es costosa en
hardware (lógica iterativa de varios ciclos) y su inclusión se posterga. Una división por una
potencia de dos constante puede expresarse con un desplazamiento. Un programa que use `/` o `%`
es rechazado.
