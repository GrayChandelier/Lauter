# Lauter

**Lauter** is an experimental programming language and compiler currently under active development.

> !!! The project is currently in a major refactoring stage and does not compile at the moment. The compiler is being rewritten to support symbol overloading.

## Current status

The following components are currently implemented:

* Lexer
* Parser
* Symbol table
* Partially implemented semantic analyzer

Currently in development:

* Module system
* Symbol overloading
* Further semantic analysis

The source code currently contains comments in Russian.

## Syntax

An example of Lauter syntax can be found in:

```text
LauterSyntaxExample.laut
```

## Tests

The `tests` directory contains Lauter source code used to test different stages of the compilation process and verify their correctness.

## Planned backends

Lauter is planned to support two compilation backends:

* **LLVM** — for native code generation
* **Custom virtual machine** — primarily intended for integration of Lauter into C++ projects

## Project status

Lauter is an experimental project and its architecture may change significantly during development.
