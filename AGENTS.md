# AGENTS.md

## Build System

This is a C++ project using CMake. The codebase uses C++20.

### Requirements
- C++ compiler supporting C++20
- CMake >= 3.16
- Qt >= 5.15 (Core, Sql, Test modules)
- Botan >= 3.10.0
- ZLIB
- PostgreSQL (optional, enabled with ENABLE_PSQL)

### Build Commands

```bash
# Configure and build
cmake -B build
cmake --build build

# Run all tests
ctest --test-dir build

# Run a single test
cd build && ./bin/test_name --silent
# Or from build directory:
./bin/cflase --help  # for main app
```

### CMake Configuration Options

- `ENABLE_CCACHE=ON` - Use ccache for faster rebuilds (default)
- `ENABLE_PCH=ON` - Use precompiled headers (default)
- `ENABLE_PSQL=OFF` - Enable PostgreSQL support
- `BUILD_EXAMPLES=ON` - Build example applications

### Test Runner

Tests use QtTest framework. Each test file defines a test class with private slots.

**Test macros:**
- `QTEST_MAIN(TestClass)` + `#include "filename.moc"` at end of file
- `ADD_TEST(TestClass)` - Register test class (see `cflib/util/test.h`)
- Test methods are void functions with `void test_name()` signature
- Use `QCOMPARE`, `QVERIFY`, `QSKIP` for assertions

## Code Style Guidelines

### File Organization

- **Header files (.h):** Public interface, class definitions, declarations
- **Implementation files (.cpp):** Method implementations
- **Private headers (.h in impl/):** Internal implementation details
- **Test files:** *\_test.cpp naming convention

### Namespace Conventions

- All library code in `cflib::` namespace or sub-namespaces
- Common sub-namespaces:
  - `cflib::util` - Utilities
  - `cflib::serialize` - Serialization
  - `cflib::net` - Networking
  - `cflib::crypt` - Cryptography
  - `cflib::db` - Database
  - `cflib::dao` - Data Access Objects
- Use `}}    // namespace` comment for closing braces

### Header Guards

- Use `#pragma once` only (no traditional include guards)
- Copyright header at top:
  ```
  /* Copyright (C) 2013-2024 Christian Fischbach <cf@cflib.de>
   *
   * This file is part of cflib.
   *
   * Licensed under the MIT License.
   */
  ```

### Includes

- Qt includes first: `<QtCore>`, `<QtTest/QtTest>`, etc.
- Project includes with `<cflib/...>` path
- Sort by module, then alphabetically

### Naming Conventions

- **Classes:** CamelCase (e.g., `TCPConn`, `BERSerializer`)
- **Methods/Functions:** camelCase (e.g., `startReadWatcher`, `calcCRC32`)
- **Variables:** camelCase (e.g., `data_`, `tagNo`)
- **Member variables:** camelCase with trailing underscore (e.g., `data_`, `lenPos_`)
- **Constants:** CamelCase (e.g., `Q_UINT64_C`, `Q_INT64_C`)
- **Macros:** UPPERCASE (e.g., `SERIALIZE_CLASS`, `USE_LOG`)
- **Log categories:** CamelCase in `LogCat` namespace

### Types

- Use Qt types: `quint8`, `quint16`, `quint32`, `quint64`, `qint8`, `qint16`, `qint32`, `qint64`
- Use `QByteArray` for binary data
- Use `QString` for text
- Use `QSharedPointer` for shared ownership
- Use `QMap`/`QHash` for associative containers

### Formatting

- Line width: 120 characters (match `.cmake-format.yml`)
- Indentation: 4 spaces (no tabs)
- Braces: K&R style (opening brace on same line as statement)
- Space after `if`, `for`, `while` keywords
- No space before parentheses in function calls

### Serialization Framework

The project has a custom BER serialization framework:

- ** Macros:**
  - `SERIALIZE_CLASS` - Add serialization support to class
  - `SERIALIZE_IS_BASE(Class)` - Mark base class for polymorphic serialization
  - `SERIALIZE_BASE(Class)` - Mark base class implementation
  - `SERIALIZE_STDBASE(Class)` - For standard types
  - `SERIALIZE_SKIP(member)` - Skip member in serialization
  - `serialized` - Marker for serialized members

- **Classes:**
  - `BERSerializer` / `BERDeserializer` for binary serialization
  - `SerializeTypeInfo` for type information

### Logging

```cpp
USE_LOG(category)  // or USE_LOG_MEMBER(category)
logTrace, logDebug, logInfo, logWarn, logCritical  // Log levels
logFunctionTrace   // Automatic function entry/exit logging
```

Log categories defined in `LogCat` namespace: Trace, Debug, Info, Warn, Critical, plus domain categories (Etc, Network, Db, Http, Crypt, User, JS, Compute).

### Error Handling

- Use Qt's error reporting where appropriate
- C++ exceptions: Use sparingly, prefer error codes
- Invalid arguments: Return false or empty values
- Missing data: Use null/empty Qt types

### Thread Safety

- Use `QMutex`, `QMutexLocker` for locking
- Use `QSemaphore` for signaling
- Thread verification: `cflib::util::ThreadVerify`

### Memory Management

- Prefer stack allocation when possible
- Use `QSharedPointer` for shared ownership
- Use `QScopedPointer` for exclusive ownership
- Avoid raw `new`/`delete` in user code

## Example Test

```cpp
#include <cflib/util/test.h>

class MyTest: public QObject
{
    Q_OBJECT
private slots:
    void test_something()
    {
        QCOMPARE(value, expected);
        QVERIFY(condition);
    }
};
#include "my_test.moc"
ADD_TEST(MyTest)
```

Build and run:
```bash
ctest --test-dir build -R MyTest
```

## Additional Notes

- Precompiled headers enabled via `ENABLE_PCH`
- Code uses Qt's meta-object system (MOC) heavily
- Autogeneration via `ser serialize` for serialization code
- Use `directConnect` macro for `Qt::DirectConnection`
- Follow existing patterns in similar modules when adding code
