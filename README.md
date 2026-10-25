[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/JuaniRaggio/ccc/releases)

[![✗](https://github.com/JuaniRaggio/ccc/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/JuaniRaggio/ccc/actions/workflows/pipeline.yaml)

# ccc

Compilador de un lenguaje derivado de C a SystemVerilog sintetizable (HLS), desarrollado con Flex y Bison sobre la base [Flex-Bison-Compiler](https://github.com/agustin-golmar/Flex-Bison-Compiler). La especificación del lenguaje se encuentra en [`doc/1er-entrega.typ`](doc/1er-entrega.typ).

* [Estado](#estado)
* [Pasos del compilador](#pasos-del-compilador)
* [Estrategia de pruebas](#estrategia-de-pruebas)
* [SystemVerilog](#systemverilog)
* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [CI/CD](#cicd)
* [Recommended Extensions](#recommended-extensions)

## Estado

**Frontend completo**: léxico (`FlexPatterns.l`), sintáctico (`BisonGrammar.y` -> AST en `AbstractSyntaxTree.h`) y semántico (`frontend/semantic-analysis/SemanticAnalyzer.c`). El frontend acepta o rechaza el programa según las reglas estáticas del lenguaje. El backend (`backend/code-generation/`) se ocupa únicamente de generar SystemVerilog y corresponde al Stage III.

El único conflicto de la gramática es el _dangling else_ (`%expect 1`), resuelto por Bison asociando el `else` al `if` más cercano.

El análisis semántico recorre el AST completo y rechaza el programa si no se cumple alguna de estas reglas:

| Regla del lenguaje                                         | Implementación                                                        |
| :--------------------------------------------------------- | :-------------------------------------------------------------------- |
| No hay recursión (directa ni mutua).                       | Grafo de llamadas + detección de ciclos (DFS).                        |
| Las funciones `constexpr` no toman parámetros por referencia. | Inspección de los parámetros de cada declaración `constexpr`.      |
| Un contexto `constexpr` solo invoca funciones `constexpr`. | Recorrido de inicializadores y cuerpos `constexpr`.                   |
| El límite de un `for` es constante en tiempo de compilación. | Literal, `constexpr` global, o parámetro dentro de una `constexpr`. |
| Dentro de `__parallel` no se escribe dos veces la misma variable. | Detección de escrituras duplicadas por bloque.                 |

Pendiente para el Stage III: tabla de símbolos y chequeo de tipos (uso de nombres no declarados, compatibilidad de tipos), y la generación de código.

## Pasos del compilador

El compilador procesa un programa en cuatro fases. Las tres primeras forman el **frontend** (validan el lenguaje de entrada); la cuarta es el **backend** (genera el lenguaje de salida).

1. **Análisis léxico** (`frontend/lexical-analysis/FlexPatterns.l`). Convierte el texto fuente en una secuencia de tokens. Las palabras clave y los operadores son tokens dedicados; cualquier otro nombre es un único token `IDENTIFIER` que lleva su lexema como valor semántico; comentarios y espacios se descartan.

2. **Análisis sintáctico** (`frontend/syntactic-analysis/BisonGrammar.y`). Un parser LALR(1) _push_ consume los tokens y, mediante las acciones semánticas (`BisonActions.c`), construye el AST (`AbstractSyntaxTree.h`). Si la secuencia no deriva de la gramática, se rechaza.

3. **Análisis semántico** (`frontend/semantic-analysis/SemanticAnalyzer.c`). Una pasada sobre el AST ya completo que valida las reglas estáticas del lenguaje (ver tabla en [Estado](#estado)) y acepta o rechaza. Corre después del parser porque necesita el árbol entero: así puede razonar sobre todo el programa a la vez (por ejemplo, recursión mutua entre funciones declaradas en cualquier orden, imposible de detectar durante el parseo).

4. **Generación de código** (`backend/code-generation/Generator.c`). Recorre el AST ya validado y emite SystemVerilog sintetizable. _Pendiente (Stage III)._

## Estrategia de pruebas

Dos capas:

1. **Frontend: aceptación/rechazo por código de salida** (`src/main/bash/test.sh`). Cada programa en `src/test/c/accept/` debe salir con código `0`; cada uno en `src/test/c/reject/` debe salir con código distinto de `0`.
2. **Backend: lint y simulación del SystemVerilog generado** (`src/main/bash/verilog.sh`). Verilator verifica que la salida sea sintetizable; Icarus la simula contra un testbench cuando existe.

Opciones consideradas para la capa de frontend y por qué elegimos la actual:

| Opción                              | Decisión | Motivo                                                                                       |
| :---------------------------------- | :------: | :------------------------------------------------------------------------------------------- |
| Exit-code black-box                 | Elegida  | Prueba el contrato real (acepta válido / rechaza inválido), robusta a refactors internos.    |
| Diff contra AST/salida _golden_     | Descartada | Frágil: cualquier cambio de formato rompe los tests; costosa de mantener.                  |
| Unit tests en C de funciones internas | Descartada | Acopla los tests a la estructura interna y requiere un framework.                         |

El límite del black-box (no distingue "aceptado pero con AST incorrecto") lo cubre la capa de backend, que valida la salida con herramientas estándar (Verilator/Icarus) en vez de un verificador propio.

CI (`.github/workflows/pipeline.yaml`) ejecuta _build_ + `test.sh` en cada push o PR. La verificación de Verilog se corre localmente.

## SystemVerilog

La imagen de Docker incluye [Verilator](https://www.veripool.org/verilator/) (v5.032) e [Icarus Verilog](https://steveicarus.github.io/iverilog/) (v12.0), para verificar y simular el código SystemVerilog que produce el compilador. Los ejemplos del informe (Figuras 1 a 4) se encuentran completos en [`doc/examples`](doc/examples), cada uno con su _testbench_:

| Módulo         | Construcción                          |
| :------------- | :------------------------------------ |
| `procesar.sv`  | `__parallel` (bloques `always_comb`). |
| `clamp.sv`     | `if`/`else` como multiplexor.         |
| `sumar.sv`     | Bucle `for` como FSM (`always_ff`).   |
| `buscar.sv`    | `constexpr` como `localparam`.        |

Para verificar (_lint_) y simular todos los ejemplos, dentro del contenedor:

```bash
src/main/bash/verilog.sh
```

o un único módulo (si existe `<modulo>_tb.sv` en la misma carpeta, también se simula):

```bash
src/main/bash/verilog.sh doc/examples/sumar.sv
```

## Requirements

* [Docker v28.3.2](https://www.docker.com/)

## Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description                                                                                                                                                           |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`.                                                                 |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. To remove those logs from the console output set it to `false`.                    |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log in the console output. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. |

_Docker Compose_ can read the variables from an `.env` file too (see `compose.yaml` file).

## Commands

### Start

Rises an ephemeral container, ready to start development:

```bash
docker compose run --rm compiler
```

### Build

Builds or rebuilds the entire compiler:

```bash
src/main/bash/build.sh
```

### Run

Compiles a program:

```bash
src/main/bash/run.sh <program>
```

where `<program>` is the path to the file that represents its entry-point.

### Test

Executes every available unit-test under `src/test/c` folder:

```bash
src/main/bash/test.sh
```

### Stop

Logout, destroy the ephemeral containers and shutdowns the cluster:

```bash
exit
docker compose down
```

### Docker

| Command                                 | Description                                             |
| :-------------------------------------- | :------------------------------------------------------ |
| `docker builder prune --all`            | Removes all builds and complete build cache.            |
| `docker compose --progress=plain build` | Forces a build or rebuild of the images in the cluster. |
| `docker image prune`                    | Removes all of the dangling images from Docker.         |
| `docker network prune`                  | Removes unused networks from Docker.                    |
| `docker volume prune`                   | Removes unused volumes from Docker.                     |

## CI/CD

To trigger an automatic integration on every push or PR (_Pull Request_), you must activate _GitHub Actions_ in the _Settings_ tab. Use the following configuration:

| Key                                                        | Value                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |
