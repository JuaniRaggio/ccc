#set document(
  title: "1er Entrega TLA -- Diseño",
  author: ("Juan Ignacio Raggio", "Geronimo Naso Rodriguez", "Santiago Fernandez Pacheco", "Manuel Blacker"),
)

#set page(
  paper: "a4",
  margin: (
    top: 2.5cm,
    bottom: 2.5cm,
    left: 2cm,
    right: 2cm,
  ),
  numbering: "1",
  number-align: bottom + right,

  header: [
    #set text(size: 9pt, fill: gray)
    #grid(
      columns: (1fr, 1fr),
      align: (left, right),
      [Raggio · Naso Rodríguez · Fernández Pacheco · Blacker],
      [#datetime.today().display("[day]/[month]/[year]")]
    )
    #line(length: 100%, stroke: 0.5pt + gray)
  ],

  footer: context [
    #set text(size: 9pt, fill: gray)
    #line(length: 100%, stroke: 0.5pt + gray)
    #v(0.2em)
    #align(center)[
      Página #counter(page).display() / #counter(page).final().first()
    ]
  ]
)

#set text(
  font: "New Computer Modern",
  size: 11pt,
  lang: "es",
  hyphenate: true,
)

#set par(
  justify: true,
  leading: 0.65em,
  first-line-indent: 0em,
  spacing: 1.2em,
)

#set heading(numbering: "1.1")
#show heading.where(level: 1): it => {
  set text(size: 16pt, weight: "bold", hyphenate: false)
  v(0.5em)
  it
  v(0.3em)
}
#show heading.where(level: 2): it => {
  set text(size: 14pt, weight: "bold", hyphenate: false)
  v(0.5em)
  it
  v(0.3em)
}
#show heading.where(level: 3): it => {
  set text(size: 12pt, weight: "bold", hyphenate: false)
  v(0.5em)
  it
  v(0.3em)
}

#set list(indent: 1em, marker: ("•", "◦", "▪"))
#set enum(indent: 1em, numbering: "1.a.")

#show raw.where(block: false): box.with(
  fill: luma(240),
  inset: (x: 3pt, y: 0pt),
  outset: (y: 3pt),
  radius: 2pt,
)

#show raw.where(block: true): block.with(
  fill: luma(240),
  inset: 10pt,
  radius: 4pt,
  width: 100%,
)

#show link: underline

#set figure(supplement: "Figura")
#show figure.where(kind: table): set figure(supplement: "Tabla")
#show figure.caption: set text(hyphenate: false)

// ====================================
// PORTADA
// ====================================

#align(center)[
  #set par(justify: false)
  #v(1em)
  #text(size: 24pt, weight: "bold", hyphenate: false)[Autómatas, Teoría de Lenguajes y Compiladores]
  #v(0.5em)
  #text(size: 18pt)[Trabajo Práctico]
  #v(0.5em)
  #text(size: 12pt, fill: gray)[
    Primera Entrega: Diseño del Lenguaje \
    #datetime.today().display("[day]/[month]/[year]")
  ]
  #v(1em)
]

#line(length: 100%, stroke: 1pt)
#v(1em)


= Equipo

#align(center)[#table(
  columns: 4,
  fill: (_, row) => if row == 0 { luma(220) } else { white },
  [Nombres], [Apellidos], [Legajo], [E-mail],
  [Gerónimo], [Naso Rodríguez], [64177], [gnasorodriguez\@itba.edu.ar],
  [Juan Ignacio], [García Vautrin Raggio], [63319], [jgarciavautrinraggi\@itba.edu.ar],
  [Santiago], [Fernández Pacheco], [65868], [sfernandezpacheco\@itba.edu.ar],
  [Manuel], [Blacker], [65184], [mblacker\@itba.edu.ar],
)]

#align(center)[Repositorio: #link("https://github.com/JuaniRaggio/ccc")]

= Dominio

El proyecto consiste en un compilador que traduce un lenguaje derivado de C a SystemVerilog (IEEE 1800) sintetizable, con énfasis en la detección y explotación del paralelismo inherente al hardware. El objetivo no es solo traducir sintaxis, sino analizar dependencias de datos entre operaciones y generar hardware que aproveche la naturaleza paralela de los circuitos digitales.

El lenguaje de entrada no es un subconjunto estricto de C sino un derivado que toma prestadas algunas construcciones de C++ (como las referencias con `&`) e introduce anotaciones propias (`__parallel`, `unique`, `aliased`) y el mecanismo `constexpr` para evaluación en tiempo de compilación. El foco está en el subconjunto imperativo: no se soporta orientación a objetos ni manejo de excepciones.

El dominio de aplicación es la síntesis de alto nivel (High-Level Synthesis, HLS): el programador describe su algoritmo en un lenguaje imperativo familiar y el compilador genera un módulo de SystemVerilog que puede sintetizarse en hardware real. Esto elimina la necesidad de escribir SystemVerilog a mano, que requiere pensar directamente en términos de ciclos de reloj, registros y señales.

El compilador acepta como entrada un archivo fuente y produce como salida un módulo SystemVerilog sintetizable orientado a FPGAs (Field-Programmable Gate Arrays, circuitos integrados reprogramables en campo) o ASICs (Application-Specific Integrated Circuits, circuitos integrados de propósito específico).

== Alcance del lenguaje de entrada

El compilador soporta las siguientes construcciones:

- Tipos enteros con ancho de bits explícito: `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- Tipo booleano: `bool`
- Arreglos unidimensionales de tipos enteros con tamaño estático
- Expresiones aritméticas: `+`, `-`, `*`
- Expresiones lógicas y de comparación: `&&`, `||`, `!`, `==`, `!=`, `<`, `>`, `<=`, `>=`
- Estructuras de control: `if`/`else`, `for`, `while`
- Funciones con parámetros y valor de retorno (cada función se traduce a un módulo SystemVerilog independiente)
- Referencias como parámetros de función con calificadores de aliasing: `unique` (predeterminado) y `aliased`
- Anotación `__parallel` para marcar bloques de sentencias independientes
- Funciones intrínsecas del compilador: `__abs`, `__min`, `__max`
- Variables `constexpr`: constantes evaluadas en tiempo de compilación
- Funciones `constexpr`: funciones evaluadas completamente en tiempo de compilación

No se soportan: memoria dinámica, recursión, tipos de punto flotante, ni llamadas a funciones de la biblioteca estándar de C. La biblioteca estándar no tiene análogo en hardware sintetizable ya que asume la existencia de sistema operativo, heap y descriptores de archivo. Las funciones intrínsecas del compilador reemplazan las operaciones matemáticas más comunes con equivalentes directamente sintetizables.

Las referencias son válidas únicamente como parámetros de función. A diferencia de los punteros de C, no pueden ser nulas ni reasignadas. El concepto de aliasing y los calificadores asociados se definen en la @sec-refs.

= Construcciones

== Construcciones de entrada

=== Declaración de función <sec-funcion>

Cada función de nivel superior se traduce a un módulo SystemVerilog independiente. Los parámetros se convierten en puertos de entrada y el valor de retorno en un puerto de salida.

```c
int32_t suma(int32_t a, int32_t b) {
    return a + b;
}
```

=== Referencias y aliasing <sec-refs>

El _aliasing_ ocurre cuando dos o más nombres distintos refieren al mismo objeto en memoria. En C estándar, dos punteros pueden ser _aliased_ (apuntar al mismo objeto) sin que el compilador lo sepa, lo cual impide analizar dependencias de datos y por ende impide paralelizar de forma segura operaciones sobre ellos.

El lenguaje usa referencias en lugar de punteros (tomando la sintaxis de C++) y agrega calificadores explícitos de aliasing. El calificador va entre el `&` y el nombre del parámetro, en línea con cómo C trata `const`. `unique` es el predeterminado e indica que la referencia no se solapa con ninguna otra. `aliased` desactiva esa garantía.

Esta decisión también mejora la seguridad respecto a C estándar: los chequeos de `NULL` generan ramas adicionales que en hardware se traducen en lógica extra y ciclos potencialmente desperdiciados. Con referencias, la garantía de no-nulidad es estática y verificada en tiempo de compilación.

```c
// unique es el predeterminado; estas dos firmas son equivalentes
void escalar(int32_t & unique salida, int32_t & unique entrada, int32_t factor);
void escalar(int32_t &salida, int32_t &entrada, int32_t factor);

// aliased: el compilador no puede asumir que salida != entrada
void in_place(int32_t & aliased salida, int32_t &entrada, int32_t factor);
```

El compilador detecta en el sitio de llamada los casos obvios de aliasing (pasar el mismo símbolo dos veces a parámetros `unique`) y emite un error.

=== Funciones intrínsecas del compilador <sec-builtins>

Las funciones intrínsecas no pertenecen a ninguna biblioteca: el compilador las traduce directamente a la lógica de hardware correspondiente durante la compilación, sin generar una llamada a función. Esto es análogo a las _primitivas_ de otros lenguajes o a las `__builtin_*` de GCC.

#figure(
  caption: [Funciones intrínsecas del compilador y el hardware que generan.],
  table(
    columns: (auto, 1fr, 1fr),
    fill: (_, row) => if row == 0 { luma(220) } else { white },
    [*Función*], [*Descripción*], [*Hardware generado*],
    [`__abs(x)`], [Valor absoluto], [Lógica combinacional],
    [`__min(x, y)`], [Mínimo entre dos valores], [Comparador + multiplexor],
    [`__max(x, y)`], [Máximo entre dos valores], [Comparador + multiplexor],
  )
)

```c
int32_t normalizar(int32_t x, int32_t tope) {
    return __min(__abs(x), tope);
}
```

=== Bloque `__parallel` <sec-parallel>

El bloque `__parallel` indica al compilador que las sentencias contenidas no tienen dependencias de datos entre sí y pueden sintetizarse como lógica combinacional independiente dentro del mismo ciclo de reloj.

Es importante distinguir esta semántica de los hilos de software: en hardware, el paralelismo es inherente a la naturaleza de los circuitos. Una señal se propaga simultáneamente por todos los conductores conectados a ella sin coordinación explícita. El bloque `__parallel` no invoca procesos concurrentes; le indica al compilador que ambas operaciones son independientes y pueden por ende pertenecer al mismo ciclo de reloj como bloques `always_comb` separados, en lugar de secuenciarse en una FSM (Finite State Machine, Máquina de Estados Finitos).

```c
__parallel {
    resultado_a = f(x);
    resultado_b = g(y);
}
```

=== Bucle `for` con rango estático <sec-for>

Los bucles `for` cuyos límites son literales o expresiones `constexpr` se sintetizan como FSMs o se desenrollan completamente. Un bucle cuyo límite no sea estático es rechazado, ya que no es posible generar una FSM de tamaño fijo para él.

Una FSM modela la ejecución del bucle como un circuito que avanza de estado en estado con cada flanco de reloj. En cada estado se procesa un elemento del arreglo y se incrementa el índice hasta alcanzar el límite, momento en el que se transiciona al estado final. El _pipeline_ de una FSM es la técnica de superponer etapas de distintas iteraciones en ciclos de reloj consecutivos para aumentar el rendimiento.

```c
for (int32_t i = 0; i < 8; i++) {
    acum += datos[i];
}
```

=== Condicional `if`/`else` <sec-if>

Se traduce a un multiplexor en lógica combinacional (sin estado) o a una transición de estado en una FSM cuando aparece dentro de un bucle secuencial.

```c
if (x > umbral) {
    salida = x - umbral;
} else {
    salida = 0;
}
```

=== Variables y funciones `constexpr` <sec-constexpr>

Las variables `constexpr` son constantes evaluadas en tiempo de compilación. Se traducen a `localparam` en SystemVerilog y pueden usarse en cualquier lugar donde el hardware requiere un valor estático: límites de bucles, tamaños de arreglos, inicializadores.

La semántica es inductiva: una expresión `constexpr` válida es un literal entero (caso base), o una expresión formada exclusivamente por operadores aritméticos y lógicos sobre otras expresiones `constexpr` (paso inductivo). Las funciones `constexpr` extienden esto permitiendo cómputos iterativos en tiempo de compilación.

```c
constexpr int32_t N        = 8;
constexpr int32_t MAX_VAL  = (1 << N) - 1;
constexpr int32_t TABLA[4] = {1, 2, 4, 8};
```

Las funciones `constexpr` se evalúan completamente en tiempo de compilación y no generan ningún módulo de hardware. Solo pueden recibir y retornar valores `constexpr`, no pueden recibir referencias, y solo pueden llamar a otras funciones `constexpr`. No pueden ser recursivas: la terminación debe ser garantizable estáticamente mediante un bucle con límite `constexpr`.

```c
constexpr int32_t potencia(int32_t base, int32_t exp) {
    int32_t r = 1;
    for (int32_t i = 0; i < exp; i++)
        r *= base;
    return r;
}
```

== Construcciones de salida (SystemVerilog)

El lenguaje de salida es SystemVerilog (IEEE 1800), el sucesor de Verilog (IEEE 1364). A diferencia de Verilog, SystemVerilog distingue explícitamente entre lógica combinacional (`always_comb`) y secuencial (`always_ff`), lo que hace al código generado más legible y permite que las herramientas de síntesis detecten errores de diseño más fácilmente.

=== Módulo

Cada función del lenguaje de entrada se convierte en un módulo SystemVerilog con puertos `clk`, `rst`, `start`, `done`, los parámetros como entradas y el retorno como salida.

=== `always_comb`

Para expresiones puramente combinacionales sin estado: asignaciones directas, operaciones aritméticas, multiplexores generados por condicionales.

=== `always_ff`

Para lógica secuencial: bucles que requieren múltiples ciclos de reloj, acumuladores, FSMs de control.

=== Bloques `always_comb` independientes

Cuando el compilador detecta (o el programador declara con `__parallel`) que dos cómputos son independientes, genera dos bloques `always_comb` separados. Dado que ambos son lógica combinacional, la señal de entrada se propaga simultáneamente a ambos en el mismo ciclo de reloj sin necesidad de coordinación.

= Casos de Prueba

== Casos de aceptación

El compilador debe aceptar y traducir correctamente los siguientes programas.

#figure(
  caption: [Casos de prueba de aceptación.],
  table(
    columns: (auto, 1fr, 1fr),
    fill: (_, row) => if row == 0 { luma(220) } else { white },
    [*N*], [*Descripción*], [*Construcción cubierta*],
    [A1], [Suma de dos enteros de 32 bits], [Expresión aritmética, `always_comb`],
    [A2], [Clasificación de un valor respecto a un umbral], [`if`/`else`, multiplexor],
    [A3], [Suma acumulada de un arreglo de 8 elementos], [Bucle `for`, FSM, `always_ff`],
    [A4], [Cálculo de dos funciones independientes sobre la misma entrada], [`__parallel`, bloques combinacionales],
    [A5], [Pipeline: filtrar y luego acumular un arreglo], [Composición de módulos, latencia por etapas],
    [A6], [Función con condicional dentro de un bucle], [Combinación de FSM y multiplexor],
    [A7], [Dos referencias `unique` a variables distintas], [Paralelización correcta con `unique`],
    [A8], [Referencia `aliased` usada correctamente sin paralelismo], [`aliased` deshabilita paralelización],
    [A9], [Uso de `__abs`, `__min`, `__max` en expresión], [Funciones intrínsecas del compilador],
    [A10], [Bucle `for` con límite literal], [Rango estático, FSM o desenrollado],
    [A11], [Variable `constexpr` usada como límite de bucle], [`constexpr`, `localparam`, rango estático],
    [A12], [Función `constexpr` usada para inicializar un arreglo `constexpr`], [Evaluación en tiempo de compilación],
  )
)

== Casos de rechazo

El compilador debe rechazar los siguientes programas con un mensaje de error descriptivo.

#figure(
  caption: [Casos de prueba de rechazo.],
  table(
    columns: (auto, 1fr, 1fr),
    fill: (_, row) => if row == 0 { luma(220) } else { white },
    [*N*], [*Descripción*], [*Razón del rechazo*],
    [R1], [Función recursiva], [No se soporta recursión],
    [R2], [Variable de tipo `float` o `double`], [Tipos de punto flotante no soportados],
    [R3], [Llamada a `printf`, `malloc` u otra función de stdlib], [Funciones de biblioteca estándar no soportadas],
    [R4], [Referencia declarada como variable local], [Referencias válidas solo como parámetros],
    [R5], [Pasar el mismo símbolo dos veces a parámetros `unique`], [Aliasing detectado en sitio de llamada],
    [R6], [Bucle `for` con límite no estático], [Rango dinámico no sintetizable como FSM estática],
    [R7], [Bloque `__parallel` con sentencias que escriben la misma variable], [Dependencia de datos dentro del bloque],
    [R8], [Función sin tipo de retorno explícito], [Toda función debe declarar su tipo de retorno],
    [R9], [Función `constexpr` que recibe una referencia], [Las funciones `constexpr` no operan sobre referencias],
    [R10], [Función `constexpr` que llama a una función no `constexpr`], [Solo puede llamar otras funciones `constexpr`],
    [R11], [Función `constexpr` recursiva], [La recursión no garantiza terminación estática],
  )
)

= Ejemplos

== Operación combinacional con `__parallel`

Dos operaciones independientes sobre la misma entrada se sintetizan como lógica combinacional en el mismo ciclo de reloj. El compilador verifica que `cuadrado` y `absoluto` no comparten escrituras y genera dos bloques `always_comb` separados.

#figure(
  caption: [Bloque `__parallel`: entrada en el lenguaje (a) y salida en SystemVerilog (b).],
  grid(
    columns: (1fr, 1fr),
    gutter: 1em,
    [
      *(a) Entrada*
      ```c
      __parallel {
          cuadrado = x * x;
          absoluto = __abs(x);
      }
      ```
    ],
    [
      *(b) Salida (SystemVerilog)*
      ```verilog
      always_comb begin
          cuadrado = x * x;
      end

      always_comb begin
          if (x < 0)
              absoluto = -x;
          else
              absoluto = x;
      end
      ```
    ]
  )
)

== Condicional como multiplexor

Un `if`/`else` sin estado se traduce a lógica combinacional con un multiplexor implícito.

#figure(
  caption: [Condicional `if`/`else`: entrada en el lenguaje (a) y salida en SystemVerilog (b).],
  grid(
    columns: (1fr, 1fr),
    gutter: 1em,
    [
      *(a) Entrada*
      ```c
      int32_t clamp(int32_t x,
                    int32_t tope) {
          if (x > tope)
              return tope;
          else
              return x;
      }
      ```
    ],
    [
      *(b) Salida (SystemVerilog)*
      ```verilog
      module clamp (
        input  logic signed [31:0] x,
        input  logic signed [31:0] tope,
        output logic signed [31:0] out
      );
        always_comb begin
          if (x > tope)
            out = tope;
          else
            out = x;
        end
      endmodule
      ```
    ]
  )
)

== Bucle con acumulador (FSM)

Un bucle `for` con acumulador requiere retener estado entre ciclos de reloj. El compilador genera una FSM con estados `IDLE`, `COMPUTE` y `DONE`.

#figure(
  caption: [Bucle `for` acumulador: entrada en el lenguaje (a) y salida en SystemVerilog (b).],
  grid(
    columns: (1fr, 1fr),
    gutter: 1em,
    [
      *(a) Entrada*
      ```c
      int32_t sumar(int32_t datos[8]) {
          int32_t acum = 0;
          for (int32_t i = 0; i < 8; i++)
              acum += datos[i];
          return acum;
      }
      ```
    ],
    [
      *(b) Salida (SystemVerilog)*
      ```verilog
      // FSM: IDLE -> COMPUTE -> DONE
      always_ff @(posedge clk) begin
        case (estado)
          IDLE: begin
            acum <= 0; i <= 0;
            if (start) estado <= COMPUTE;
          end
          COMPUTE: begin
            acum  <= acum + datos[i];
            i     <= i + 1;
            if (i == 7) estado <= DONE;
          end
          DONE: begin
            done   <= 1;
            out    <= acum;
            estado <= IDLE;
          end
        endcase
      end
      ```
    ]
  )
)

== Hardware parametrizable con `constexpr`

Una función `constexpr` calcula una tabla de potencias de dos en tiempo de compilación. El arreglo `LUT` se convierte en un `localparam` con valores literales y la función `potencia` no genera ningún módulo de hardware.

#figure(
  caption: [Hardware parametrizable con `constexpr`: entrada en el lenguaje (a) y salida en SystemVerilog (b).],
  grid(
    columns: (1fr, 1fr),
    gutter: 1em,
    [
      *(a) Entrada*
      ```c
      constexpr int32_t N = 4;

      constexpr int32_t potencia(
          int32_t base,
          int32_t exp
      ) {
          int32_t r = 1;
          for (int32_t i = 0; i < exp; i++)
              r *= base;
          return r;
      }

      constexpr int32_t LUT[N] = {
          potencia(2, 0),
          potencia(2, 1),
          potencia(2, 2),
          potencia(2, 3),
      };

      int32_t buscar(int32_t idx) {
          return LUT[idx];
      }
      ```
    ],
    [
      *(b) Salida (SystemVerilog)*
      ```verilog
      // potencia y LUT: resueltos en
      // tiempo de compilación, sin
      // hardware generado para ellos.

      localparam int N = 4;
      localparam int LUT [0:3] = {
          1, 2, 4, 8
      };

      module buscar (
        input  logic signed [31:0] idx,
        output logic signed [31:0] out
      );
        always_comb begin
          out = LUT[idx];
        end
      endmodule
      ```
    ]
  )
)

== Rechazo: función `constexpr` recursiva

Las funciones `constexpr` no pueden ser recursivas. La evaluación en tiempo de compilación requiere que el compilador pueda determinar un orden de cómputo finito y estático; la recursión no garantiza terminación sin análisis adicional.

#figure(
  caption: [Función `constexpr` recursiva: caso rechazado (a) y versión iterativa válida (b).],
  grid(
    columns: (1fr, 1fr),
    gutter: 1em,
    [
      *(a) Rechazado*
      ```c
      // ERROR: constexpr function
      // 'factorial' is recursive
      constexpr int32_t factorial(
          int32_t n
      ) {
          if (n == 0) return 1;
          return n * factorial(n - 1);
      }
      ```
    ],
    [
      *(b) Correcto: iterativo*
      ```c
      constexpr int32_t factorial(
          int32_t n
      ) {
          int32_t r = 1;
          for (int32_t i = 2; i <= n; i++)
              r *= i;
          return r;
      }
      ```
    ]
  )
)
