# Smart Expression Evaluator Dashboard

A high-performance calculation dashboard that couples a raw C++17 evaluation module with a modern, responsive, glassmorphic HTML5 frontend. All parsing, mathematical validations, infix-to-postfix stack conversions, and recursive evaluations are processed entirely in client-side WebAssembly (WASM).

---

## Key Features

1. **WebAssembly Powered Engine**
   - High-speed mathematical calculation written in C++17 and compiled to WASM.
   - Low latency, secure, client-side execution.

2. **Stack-Based Expression Parsing**
   - Implements the Shunting-Yard algorithm (using standard library stacks `std::stack`) to convert infix math expressions to postfix notation.
   - Handles brackets `( )`, exponents `^`, operator precedence, and float operations.

3. **Special Functions and Constants**
   - Support for `sqrt(x)` (square root), `percentage(x)` (percentage conversion), and `factorial(x)` (recursive evaluation up to `20!`).
   - Integrated math constants such as `pi` (3.14159) and `e` (2.71828).

4. **Collapsible Navigation Menu**
   - Interactive hamburger menu to toggle the sidebar view.
   - Automatically hides and slides the sidebar out of bounds to maximize viewport real estate.
   - Saves sidebar toggle preference in browser local storage (`localStorage`).

5. **Scroll-Bound History Log**
   - Tracks calculation inputs, postfix outputs, results, and logs them in local browser memory.
   - The recent history container fits perfectly beside the calculator card and scrolls internally, preventing screen overflow.

---

## Directory Structure

```text
SmartCalculator/
│
├── .wasi-sdk/              # Local copy of WASI SDK toolchain (compiler & sysroot)
│
├── evaluator.hpp           # Header file containing the SmartExpressionEvaluator class
├── evaluator.cpp           # Main C++ source file implementing math parsing algorithms
├── wasm_bindings.cpp       # Interfacing functions mapping WebAssembly to JavaScript
│
├── calculator.wasm         # Compiled WebAssembly binary loaded by the browser
│
├── index.html              # Single Page Application layout, views, and overlays
├── index.css               # Styling rules, custom scrollbars, transitions, and themes
├── app.js                  # Frontend controller: initializes WASM, UI routing, history
│
├── build.ps1               # PowerShell build script for compiling C++ to WASM
├── setup_compiler.ps1      # Compiler installer downloads and sets up the WASI SDK
└── server.ps1              # PowerShell static web server script for local development
```

---

## Setup & Running Instructions

### 1. Prerequisites
- **Operating System**: Windows 10/11
- **Command Line**: PowerShell 5.1+ (runs natively on Windows)
- **Browser**: Modern web browser (Chrome, Edge, Firefox) supporting WebAssembly

### 2. Setup the Compiler (One-time Setup)
To compile the C++ source files into a WebAssembly module, you need the WASI SDK compiler. A script is provided to automate this:
1. Open your PowerShell terminal.
2. Navigate to the project root directory.
3. Run the setup script:
   ```powershell
   .\setup_compiler.ps1
   ```
   *Note: This script downloads the WASI SDK toolchain and unpacks it into the `.wasi-sdk` subdirectory inside your project.*

### 3. Compile the WebAssembly Module
If you modify the C++ evaluator logic or want to rebuild the binary, run the build script:
```powershell
.\build.ps1
```
The script will output:
```text
Compiling C++ evaluator to WebAssembly...
WebAssembly module successfully built: calculator.wasm (3387.84 KB)
```

### 4. Start the Development Server
Since WebAssembly requires fetching binary files over HTTP (due to browser security constraints on local `file:///` paths), you must run a web server.
A lightweight, zero-dependency PowerShell server is included:
```powershell
.\server.ps1
```
Upon running, the terminal will display:
```text
Server started and listening at http://localhost:8000/
Press Ctrl+C to stop the server.
```

### 5. Access the Dashboard
1. Open your web browser.
2. Navigate to: **`http://localhost:8000/`**
3. The Smart Calculator will load instantly. You can now type expressions or click keys to calculate.

---

## Architectural Workflow

The application operates as a Single Page Application (SPA). The diagram below illustrates how inputs are handled:

```text
[Browser Keypad / Input Box] ---> [app.js UI Controller]
                                        │ (Converts string to UTF-8 bytes)
                                        ▼
                               [calculator.wasm]
                                        │
                                        ├── [wasm_bindings.cpp Interop Layer]
                                        │       │ (Allocates heap memory)
                                        │       ▼
                                        └── [evaluator.cpp Engine Class]
                                                │ (Tokenizes input formula)
                                                ▼
                                            [std::stack Parser]
                                                │ (Evaluates operators and functions)
                                                ▼
                                        [Postfix & Result JSON]
                                        │
                                        ▼
[Browser History & Storage] <--- [Rendered UI Outputs]
```

1. **User Input**: The user types an expression (e.g. `(10 - 2) * 3`) in the display.
2. **String Transmission**: `app.js` allocates memory in the WASM heap using `wasm_malloc` and copies the string as null-terminated UTF-8 bytes.
3. **Calculation Phase**: The C++ engine tokenizes the expression, checks brackets, converts from infix to postfix notation, and computes the arithmetic result.
4. **JSON Serialization**: The engine returns a JSON string containing validation success state, parsed postfix notation, calculated answer, or descriptive error strings.
5. **State Persistency**: The Javascript controller parses the JSON output, updates the screen, and logs the entry to `localStorage` under `smart_calc_history`.

---

## Supported Syntax & Precedence

The engine respects traditional BODMAS algebraic rules:

- **Unary Operations**:
  - `sqrt(x)`: Square root of a positive float value.
  - `factorial(x)`: Evaluates recursive factorials. Limited to inputs $x \le 20$ to prevent integer overflows.
  - `percentage(x)`: Translates to $x / 100$.

- **Operators**:
  - `^` : Exponentiation (Right-Associative, highest precedence).
  - `*`, `/`, `%` : Multiplication, Division, Modulo (Left-Associative, medium precedence).
  - `+`, `-` : Addition, Subtraction (Left-Associative, lowest precedence).

- **Brackets**:
  - `( )` : Groups expressions and overrides standard operator hierarchy.

---

## Customizing the Dashboard

- **Theme Toggles**: Select Cyan Cyberpunk or Gold Royal schemes in the **Settings** view. Theme selections are stored locally.
- **Sidebar Collapse**: Click the three-line menu toggle button. The viewport automatically adjusts to fill the remaining horizontal area.
- **Memory Wiping**: Clear the entire calculation history from `localStorage` using the Reset Storage trigger in **Settings**.

---

## Custom C++ Implementation Details

The core evaluator implements custom standard templates:
- **Tokenization**: Parses input characters into numbers, operators, functions, and variables using string vectors.
- **Factorial Implementation**: Uses standard recursive loops protected by safe limits:
  ```cpp
  unsigned long long factorialRecursive(int n) {
      if (n <= 1) return 1;
      return n * factorialRecursive(n - 1);
  }
  ```
- **Error Handling**: Custom catches for division-by-zero, unmatched parentheses, illegal function arguments, and parsing invalid constants.
