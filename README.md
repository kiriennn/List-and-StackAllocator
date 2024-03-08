# List and StackAllocator

### StackAllocator

&emsp; Implementation of the STL-compatible StackAllocator<typename T, size_t N> class that meets [allocator requirements] and allows you to create standard containers on the stack without accessing dynamic memory.

&emsp; Objects of the StackAllocator class are lightweight, easy to copy and assign.

### List

&emsp; The implementation of the List class is a doubly linked list with [correct usage of the allocator].

&emsp; Supports [bidirectional], constant, reverse iterators; insert(iterator, const T&) and erase(iterator) methods for adding and removing single elements to a list, and others.

   [allocator requirements]: <https://en.cppreference.com/w/cpp/named_req/Allocator>
   [correct usage of the allocator]: <https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer>
  [bidirectional]: <https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator>
