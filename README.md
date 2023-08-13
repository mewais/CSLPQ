# Concurrent SkipList-based Priority Queue
CSLPQ is a concurrent priority queue based on skiplists that has started as a way to test out ChatGPT. 
I needed one for a project of mine and thought, given my sheer laziness and unwillingness to learn and write one myself, it would be a great opportunity to utilize ChatGPT. That said, ChatGPT quickly proved unable to create a functioning priority queue on its own, so I had to write one myself, with help from it of course.
The queue started with fine grained locks, then later switched to a lock-free CAS based implementation.

## Status
This is in working order. However, ~~popped nodes are marked deleted and removed from the list, but their memory is not freed yet.~~ deleting nodes post removal is not fully thread safe and causes races.

## Features
- Templated Key/Value priority queue
- Uses skiplists for faster insertion/deletion
- A C++ header only implementation
- Thread safe.
- Lock-free.

## License
This library is licensed under the MIT license.
