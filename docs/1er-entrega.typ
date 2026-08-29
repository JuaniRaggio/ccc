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
      [Raggio · Naso Rodriguez · Fernandez Pacheco · Blacker],
      [#datetime.today().display("[day]/[month]/[year]")]
    )
    #line(length: 100%, stroke: 0.5pt + gray)
  ],

  footer: context [
    #set text(size: 9pt, fill: gray)
    #line(length: 100%, stroke: 0.5pt + gray)
    #v(0.2em)
    #align(center)[
      Pagina #counter(page).display() / #counter(page).final().first()
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
#show heading.where(level: 1): set text(size: 16pt, weight: "bold")
#show heading.where(level: 2): set text(size: 14pt, weight: "bold")
#show heading.where(level: 3): set text(size: 12pt, weight: "bold")

#show heading: it => {
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
// ====================================
// PORTADA
// ====================================

#align(center)[
  #v(1em)
  #text(size: 24pt, weight: "bold")[Automatas, Teoria de Lenguajes y Compiladores]
  #v(0.5em)
  #text(size: 18pt)[Trabajo Practico]
  #v(0.5em)
  #text(size: 12pt, fill: gray)[
    Primera Entrega -- Diseño del Lenguaje \
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
  [Nombre], [Apellido], [Legajo], [E-mail],
  [Geronimo], [Naso Rodriguez], [64177], [gnasorodriguez\@itba.edu.ar],
  [Juan Ignacio], [Garcia Vautrin Raggio], [63319], [jgarciavautrinraggi\@itba.edu.ar],
  [Santiago], [Fernandez Pacheco], [], [],
  [Manuel], [Blacker], [], [],
)]


= Dominio

El proyecto consiste en un compilador que traduce un subconjunto de C a SystemVerilog sintetizable, con enfasis en la deteccion y explotacion de paralelismo a nivel de hardware. El objetivo no es solo traducir sintaxis, sino analizar dependencias de datos entre operaciones y generar hardware paralelo cuando las operaciones son independientes entre si.

El dominio de aplicacion es la sintesis de alto nivel (High-Level Synthesis): el programador describe su algoritmo en C, un lenguaje secuencial que ya conoce, y el compilador se encarga de determinar que partes pueden ejecutarse en paralelo en hardware real (FPGAs o ASICs). Esto elimina la necesidad de escribir Verilog a mano, que es considerablemente mas verboso y requiere pensar directamente en terminos de ciclos de clock, registros y señales.

El compilador acepta como entrada un archivo `.c` con anotaciones opcionales y produce como salida un modulo SystemVerilog sintetizable con su correspondiente testbench.

== Alcance del subconjunto de C soportado

El compilador soporta las siguientes construcciones del lenguaje C:

- Tipos enteros con ancho de bits explicito: `int8_t`, `int16_t`, `int32_t`, `uint8_t`, `uint16_t`, `uint32_t`
- Tipo booleano: `bool`
- Arreglos unidimensionales de tipos enteros
- Expresiones aritmeticas: `+`, `-`, `*`
- Expresiones logicas y de comparacion: `&&`, `||`, `!`, `==`, `!=`, `<`, `>`, `<=`, `>=`
- Estructuras de control: `if`/`else`, `for`, `while`
- Funciones con parametros y valor de retorno (cada funcion se traduce a un modulo Verilog independiente)
- Punteros como parametros de funcion con qualifiers de aliasing: `unique` (default, garantia de no-aliasing) y `aliased` (puede solaparse con otro puntero)
- Anotacion `__parallel` para marcar bloques que el programador garantiza como independientes

No se soportan: memoria dinamica, recursion, tipos de punto flotante, ni llamadas a funciones de biblioteca estandar.

Los punteros son validos unicamente como parametros de funcion. Dentro del cuerpo de una funcion no se pueden declarar punteros locales ni hacer aritmetica de punteros mas alla de indexacion de arreglos. Por defecto todos los punteros son `unique`: el compilador asume no-aliasing y puede paralelizar operaciones sobre ellos. Si el programador necesita expresar que dos punteros pueden apuntar a la misma memoria, debe anotarlos con `aliased`, lo que deshabilita la paralelizacion automatica entre esas variables.

= Construcciones

== Construcciones de entrada (C)

=== Declaracion de funcion

Cada funcion de nivel superior se traduce a un modulo SystemVerilog independiente. Los parametros se convierten en puertos de entrada y el valor de retorno en un puerto de salida.

```c
int32_t suma(int32_t a, int32_t b) {
    return a + b;
}
```

=== Punteros con qualifier de aliasing

Los punteros solo son validos como parametros de funcion. El qualifier va entre el `*` y el nombre del parametro, consistente con como C trata `const` y `restrict`.

`unique` es el default e indica que el compilador puede asumir que ese puntero no se solapa con ningun otro parametro. `aliased` desactiva esa garantia.

```c
// unique es el default, estas dos firmas son equivalentes
void escalar(int32_t * unique salida, int32_t * unique entrada, int32_t factor);
void escalar(int32_t *salida, int32_t *entrada, int32_t factor);

// aliased: el compilador no puede asumir que salida != entrada
void in_place(int32_t * aliased salida, int32_t *entrada, int32_t factor);
```

El compilador detecta en el call site los casos obvios de aliasing (pasar el mismo simbolo dos veces a parametros `unique`) y emite un error.

=== Bloque paralelo

La anotacion `__parallel` indica al compilador que las sentencias dentro del bloque no tienen dependencias entre si y pueden sintetizarse como logica combinacional concurrente.

```c
__parallel {
    resultado_a = f(x);
    resultado_b = g(y);
}
```

=== Bucle `for` con rango estatico

Los bucles `for` con limites conocidos en tiempo de compilacion se pueden desenrollar o sintetizar como maquinas de estados finitos (FSM) con pipeline.

```c
for (int i = 0; i < 8; i++) {
    acum += datos[i];
}
```

=== Condicional `if`/`else`

Se traduce a un multiplexor en logica combinacional o a una transicion de estado en una FSM.

```c
if (x > umbral) {
    salida = x - umbral;
} else {
    salida = 0;
}
```

== Construcciones de salida (SystemVerilog)

=== Modulo

Cada funcion C se convierte en un modulo con puertos `clk`, `rst`, `start`, `done`, los parametros como entradas y el retorno como salida.

=== `always_comb`

Para expresiones puramente combinacionales sin estado: asignaciones directas, operaciones aritmeticas simples, multiplexores.

=== `always_ff`

Para logica secuencial: bucles que requieren multiples ciclos, acumuladores, FSMs de control.

=== Bloques `always` paralelos

Cuando el compilador detecta (o el programador anota con `__parallel`) que dos computos son independientes, genera dos bloques `always_comb` separados que el sintetizador ejecuta en paralelo en hardware.

= Casos de Prueba

A continuacion se describen los casos de prueba que cubren las construcciones principales del lenguaje.

#v(0.5em)

#table(
  columns: (auto, 1fr, 1fr),
  fill: (_, row) => if row == 0 { luma(220) } else { white },
  [*N*], [*Descripcion*], [*Construccion cubierta*],
  [1], [Suma de dos enteros de 32 bits], [Expresion aritmetica, `always_comb`],
  [2], [Clasificacion de un valor respecto a un umbral], [`if`/`else`, multiplexor],
  [3], [Suma acumulada de un arreglo de 8 elementos], [Bucle `for`, FSM, `always_ff`],
  [4], [Calculo de dos funciones independientes sobre la misma entrada], [`__parallel`, bloques concurrentes],
  [5], [Pipeline: filtrar y luego acumular un arreglo], [Composicion de modulos, latencia],
  [6], [Funcion con condicional dentro de un bucle], [Combinacion de FSM y multiplexor],
)

= Ejemplos

== Ejemplo 1: operacion combinacional simple

Dos operaciones independientes sobre la misma entrada se sintetizan en paralelo. El compilador detecta que `cuadrado` y `valor_absoluto` no comparten escrituras y genera dos bloques `always_comb` separados.

#grid(
  columns: (1fr, 1fr),
  gutter: 1em,
  [
    *Entrada (C)*
    ```c
    __parallel {
        cuadrado = x * x;
        absoluto = x < 0 ? -x : x;
    }
    ```
  ],
  [
    *Salida (SystemVerilog)*
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

== Ejemplo 2: condicional como multiplexor

Un `if`/`else` simple sin estado se traduce directamente a logica combinacional con un multiplexor implicito.

#grid(
  columns: (1fr, 1fr),
  gutter: 1em,
  [
    *Entrada (C)*
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
    *Salida (SystemVerilog)*
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

== Ejemplo 3: bucle con acumulador (FSM)

Un bucle `for` con un acumulador requiere estado entre ciclos de clock. El compilador genera una FSM con estados `IDLE`, `COMPUTE` y `DONE`.

#grid(
  columns: (1fr, 1fr),
  gutter: 1em,
  [
    *Entrada (C)*
    ```c
    int32_t sumar(
        int32_t datos[8]
    ) {
        int32_t acum = 0;
        for (int i = 0; i < 8; i++)
            acum += datos[i];
        return acum;
    }
    ```
  ],
  [
    *Salida (SystemVerilog)*
    ```verilog
    // FSM con estados:
    // IDLE -> COMPUTE -> DONE
    always_ff @(posedge clk) begin
      case (estado)
        IDLE: begin
          acum  <= 0;
          i     <= 0;
          if (start) estado <= COMPUTE;
        end
        COMPUTE: begin
          acum  <= acum + datos[i];
          i     <= i + 1;
          if (i == 7) estado <= DONE;
        end
        DONE: begin
          done <= 1;
          out  <= acum;
          estado <= IDLE;
        end
      endcase
    end
    ```
  ]
)

