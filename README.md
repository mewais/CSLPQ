# Concurrent SkipList-based Priority Queue
CSLPQ is a concurrent priority queue based on skiplists that has started as a way to test out ChatGPT. 
I needed one for a project of mine and thought, given my sheer laziness and unwillingness to learn and write one myself, it would be a great opportunity to utilize ChatGPT. That said, ChatGPT quickly proved unable to create a functioning priority queue on its own, so I had to write one myself, with help from it of course.
The queue started with fine grained locks, then later switched to a lock-free CAS based implementation.

## Status
The library is in working order, and currently being used in the [DCSim simulator](https://github.com/DCArch/DCSim). If you encounter any bugs, please open an issue.

## Features
- Templated Key/Value priority queue
- Uses skiplists for faster insertion/deletion
- A C++ header only implementation
- Thread safe.
- Lock-free.

## Dependencies
- [Atomic128 library](https://github.com/mewais/Atomic128) (included)
- A modified version of [atomic_shared_ptr](https://github.com/anthonywilliams/atomic_shared_ptr) (included)

## Usage
Recommended use is through CMake's ExternalProject. Add the following (modify to your specific use) to your CMakeLists.txt:
```cmake
include(ExternalProject)
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
ExternalProject_Add(
    CSLPQ
    GIT_REPOSITORY https://github.com/mewais/ConcurrentSkipListPriorityQueue
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
)

target_include_directories(${PROJECT_NAME} PUBLIC ${EXTERNAL_INSTALL_LOCATION}/include)
```

Then in your code simply include the following:
```cpp
#include "CSLPQ/Queue.hpp"
```

## License
The atomic_shared_ptr library is licensed under the BSD license. The rest is licensed under the MIT license.
