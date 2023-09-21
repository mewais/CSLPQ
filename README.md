# Concurrent SkipList-based Priority Queue
CSLPQ is a concurrent priority queue based on skiplists that has started as a way to test out ChatGPT. 
I needed one for a project of mine and thought, given my sheer laziness and unwillingness to learn and write one myself, it would be a great opportunity to utilize ChatGPT. That said, ChatGPT quickly proved unable to create a functioning priority queue on its own, so I had to write one myself, with help from it of course.
The queue started with fine grained locks, then later switched to a lock-free CAS based implementation.

## Status
The library is in working order, and currently being used in the [DCSim simulator](https://github.com/DCArch/DCSim). If you encounter any bugs, please open an issue.

## Features
- A C++ header only implementation
- Templated Key or Key/Value priority queues
- Uses skiplists for faster insertion/deletion
- Thread safe.
- Lock-free.
- Uses delayed deletion to avoid stack overflow on chain deletions.

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
    GIT_REPOSITORY https://github.com/mewais/CSLPQ
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
)

target_include_directories(${PROJECT_NAME} PUBLIC ${EXTERNAL_INSTALL_LOCATION}/include)
```

Then in your code simply include the following:
```cpp
#include "CSLPQ/Queue.hpp"

// Example usage
CSLPQ::KVQueue<KeyType, ValueType> kvqueue(max_levels = 4, max_size = 0);               // If max_size is set to anything other than 0, the queue will be approximately bounded to that size, any pushes beyond that will stall
kvqueue.Push(key);          // Inserts default value
kvqueue.Push(key, value);   // Inserts value
bool success = kvqueue.TryPop(key, value);       // Fills key and value and returns true if queue is not empty
std::string str = kvqueue.ToString(bool all_levels = false);   // Returns a string representation of the queue. enabling all levels will print all levels of the skiplist, otherwise only the first level is printed
uint64_t size = kvqueue.GetSize();     // Returns the number of elements in the queue, this is only an approximate count due to the concurrent nature of the queue

CSLPQ::KQueue<KeyType> queue(max_levels = 4, max_size = 0);               // If max_size is set to anything other than 0, the queue will be approximately bounded to that size, any pushes beyond that will stall
queue.Push(key);
bool success = queue.TryPop(key);       // Fills key and returns true if queue is not empty
std::string str = queue.ToString(bool all_levels = false);   // Returns a string representation of the queue. enabling all levels will print all levels of the skiplist, otherwise only the first level is printed
uint64_t size = queue.GetSize();     // Returns the number of elements in the queue, this is only an approximate count due to the concurrent nature of the queue
```

Because of dependency on Atomic128, you must compile with the `-Wno-strict-aliasing` flag enabled.

## License
The atomic_shared_ptr library is licensed under the BSD license. The rest is licensed under the MIT license.
