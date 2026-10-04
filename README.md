[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/JuaniRaggio/ccc/releases)

[![✗](https://github.com/JuaniRaggio/ccc/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/JuaniRaggio/ccc/actions/workflows/pipeline.yaml)

# ccc

Compilador de un lenguaje derivado de C a SystemVerilog sintetizable (HLS), desarrollado con Flex y Bison sobre la base [Flex-Bison-Compiler](https://github.com/agustin-golmar/Flex-Bison-Compiler). La especificación del lenguaje se encuentra en [`doc/1er-entrega.typ`](doc/1er-entrega.typ).

* [Estado](#estado)
* [SystemVerilog](#systemverilog)
* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [CI/CD](#cicd)
* [Recommended Extensions](#recommended-extensions)

## Estado

**Stage II (Frontend)**: el analizador léxico (`FlexPatterns.l`) y sintáctico (`BisonGrammar.y`) construyen el AST completo del lenguaje (`AbstractSyntaxTree.h`). El backend (análisis semántico y generación de SystemVerilog) corresponde al Stage III.

El único conflicto de la gramática es el _dangling else_ (`%expect 1`), resuelto por Bison asociando el `else` al `if` más cercano.

Los siguientes casos de rechazo dependen del análisis semántico, por lo que el frontend los acepta momentáneamente (falsos positivos esperados):

| Caso                                     | Validación pendiente (Stage III)                         |
| :--------------------------------------- | :------------------------------------------------------- |
| `reject/06-recursive-function`           | Detección de recursión en el grafo de llamadas.          |
| `reject/11-dynamic-for-bound`            | Límite de `for` literal o `constexpr`.                   |
| `reject/12-parallel-dependency`          | Escrituras disjuntas dentro de `__parallel`.             |
| `reject/14-constexpr-recursive`          | Recursión en funciones `constexpr`.                      |
| `reject/15-constexpr-with-reference`     | Funciones `constexpr` sin parámetros por referencia.     |
| `reject/16-constexpr-calls-nonconstexpr` | Funciones `constexpr` solo llaman a otras `constexpr`.   |

Nota: `reject/08-stdlib-call` hoy se rechaza léxicamente (los literales de cadena no existen en el lenguaje); en el Stage III también se rechazará por invocar una función no declarada.

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
