# Simple Compiler in C++

A compiler implementation for a custom polynomial programming language, 
built as part of CSE445 at Arizona State University.

## Overview

This project implements a fully functional compiler pipeline from scratch in C++, 
including a lexical analyzer, recursive-descent parser, semantic analyzer, and 
interpreter/executor.

## Features

- **Lexical Analysis** — tokenizes raw input using a provided lexer
- **Recursive-Descent Parser** — parses a custom grammar with full syntax error detection
- **Semantic Analysis** — detects and reports 4 types of semantic errors:
  - Duplicate polynomial declarations
  - Invalid monomial/variable names
  - Evaluation of undeclared polynomials
  - Wrong number of arguments
- **Interpreter** — executes the parsed program and produces correct output
- **Static Analysis** — detects uninitialized variables and useless assignments
- **Polynomial Degree Calculator** — computes and outputs the degree of each polynomial

## Language Features Supported

The compiler supports a custom language with:
- Polynomial declarations with named parameters
- Multi-variable polynomial expressions with coefficients and exponents
- `INPUT`, `OUTPUT`, and assignment statements
- Nested polynomial evaluations as arguments

## Tech Stack

![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=cplusplus&logoColor=white)
![GCC](https://img.shields.io/badge/GCC-A8B9CC?style=flat&logo=gnu&logoColor=black)
![Ubuntu](https://img.shields.io/badge/Ubuntu_22.04-E95420?style=flat&logo=ubuntu&logoColor=white)

## How to Compile & Run

````bash
# Compile
g++ -std=c++11 parser.cpp lexer.cpp inputbuf.cpp -o a.out

# Run with input file
./a.out < input.txt

# Run and save output
./a.out < input.txt > output.txt
````

## Example

**Input:**
````
TASKS
1 2
POLY
F = x^2 + 1;
G = x + 1;
EXECUTE
X = F(4);
Y = G(2);
OUTPUT X;
OUTPUT Y;
INPUTS
1 2 3
````

**Output:**
````
17
3
````

## What I Learned

- How compilers work end-to-end — from tokenizing to execution
- Writing a recursive-descent parser from a formal grammar
- Implementing semantic analysis and static analysis passes
- Memory management and data structures in C++
