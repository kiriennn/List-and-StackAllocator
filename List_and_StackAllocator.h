#include <algorithm>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <tuple>
#include <type_traits>

template<std::size_t N>
class StackStorage {
public:
  char _bytes[N];
  size_t _bytes_used;

  StackStorage() : _bytes_used(0) {}
  StackStorage(const StackStorage &) = delete;
  StackStorage &operator=(const StackStorage &) = delete;
};

template<typename T, std::size_t N>
class StackAllocator {
  StackStorage<N> &_storage;

public:
  using value_type = T;

  StackAllocator(StackStorage<N> &storage) noexcept : _storage(storage) {}

  StackAllocator(const StackAllocator &rhs) noexcept : _storage(rhs._storage) {}
  StackAllocator(StackAllocator &&rhs) noexcept : _storage(rhs._storage) {}

  template<typename U>
  StackAllocator(const StackAllocator<U, N> &rhs) noexcept : _storage(rhs._storage) {}
  template<typename U>
  StackAllocator(StackAllocator<U, N> &&rhs) noexcept : _storage(rhs._storage) {}

  template<typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  T *allocate(size_t count) {
    uintptr_t address = reinterpret_cast<uintptr_t>(_storage._bytes + _storage._bytes_used);
    if (address % alignof(T) != 0) {
      address = (address / alignof(T) + 1) * alignof(T);
    }
    size_t new_bytes_used = reinterpret_cast<char *>(address) - _storage._bytes + count * sizeof(T);
    if (new_bytes_used > N) {
      throw std::bad_alloc();
    }
    _storage._bytes_used = new_bytes_used;
    return reinterpret_cast<T *>(address);
  }

  void deallocate(T *data, size_t count) noexcept {
    std::ignore = data;
    std::ignore = count;
  }

  size_t max_size() const noexcept {
    return N / sizeof(T);
  }

  bool operator==(const StackAllocator &rhs) const noexcept {
    return &_storage == &rhs._storage;
  }

  bool operator!=(const StackAllocator &rhs) const noexcept {
    return &_storage != &rhs._storage;
  }

  StackAllocator select_on_container_copy_construction() const noexcept {
    return *this;
  }

  template<typename T1, size_t N1>
  friend class StackAllocator;
};

template <typename T, typename Allocator = std::allocator<T>> class List {
  struct Node {
    Node *prev;
    Node *next;
    T value;
  };

  Node *_head;
  Node *_tail;
  size_t _size;
  using NodeAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  Allocator _allocator;
  NodeAllocator _node_allocator;

  NodeAllocator _get_node_allocator() const {
    return _node_allocator;
  }

public:
  List() noexcept : _head(nullptr), _tail(nullptr), _size(0) {}
  List(const Allocator &allocator)
      : _head(nullptr), _tail(nullptr), _size(0), _allocator(allocator), _node_allocator(allocator) {
  }
  List(size_t count, const T &value, const Allocator &allocator = Allocator{})
      : List(allocator) {
    try {
      for (size_t i = 0; i < count; i++) {
        push_back(value);
      }
    } catch (...) {
      while (size() > 0) {
        pop_back();
      }
      throw;
    }
  }
  List(size_t count, const Allocator &allocator = Allocator{})
      : List(allocator) {
    for (size_t i = 0; i < count; i++) {
      Node *new_tail = _get_node_allocator().allocate(1);
      try {
        new (new_tail) Node{_tail, nullptr, T{}};
      } catch (...) {
        _get_node_allocator().deallocate(new_tail, 1);
        while (size() > 0) {
          pop_back();
        }
        throw;
      }
      if (_tail == nullptr) {
        _head = new_tail;
      } else {
        _tail->next = new_tail;
      }
      _tail = new_tail;
      _size++;
    }
  }

  List(const List &rhs)
      : List(std::allocator_traits<Allocator>::
                 select_on_container_copy_construction(rhs._allocator)) {
    try {
      for (const T &value : rhs) {
        push_back(value);
      }
    } catch (...) {
      while (size() > 0) {
        pop_back();
      }
      throw;
    }
  }
  List(List &&rhs) noexcept
      : _head(rhs._head), _tail(rhs._tail), _size(rhs._size),
        _allocator(std::move(rhs._allocator)) {
    rhs._head = nullptr;
    rhs._tail = nullptr;
    rhs._size = 0;
  }
  ~List() {
    while (size() > 0) {
      pop_back();
    }
  }

  List &operator=(const List& rhs) {
    if (this == &rhs) {
      return *this;
    }
    List copy = rhs;
    swap(copy);
    if constexpr (std::allocator_traits<Allocator>::
                      propagate_on_container_copy_assignment::value) {
      _allocator = rhs._allocator;
    }
    return *this;
  }

  static void _destruct_list(Node *head, Allocator &allocator) {
    typename std::allocator_traits<Allocator>::template rebind_alloc<Node>
        node_allocator{allocator};
    while (head != nullptr) {
      Node *next_head = head->next;
      head->~Node();
      node_allocator.deallocate(head, 1);
      head = next_head;
    }
  }

  List &operator=(List &&rhs) noexcept {
    if (this == &rhs) {
      return *this;
    }
    _head = rhs._head;
    _tail = rhs._tail;
    _size = rhs._size;
    if constexpr (std::allocator_traits<Allocator>::
                      propagate_on_container_move_assignment ::value) {
      _allocator = std::move(rhs._allocator);
    }
    rhs._head = nullptr;
    rhs._tail = nullptr;
    rhs._size = 0;
    return *this;
  }

  void swap(List &rhs) noexcept {
    std::swap(_head, rhs._head);
    std::swap(_tail, rhs._tail);
    std::swap(_size, rhs._size);
    if constexpr (std::allocator_traits<
                      Allocator>::propagate_on_container_swap::value) {
      std::swap(_allocator, rhs._allocator);
    }
  }

  using allocator_type = NodeAllocator;

  NodeAllocator get_allocator() const { return _allocator; }

  size_t size() const noexcept { return _size; }

  void push_back(const T &value) {
    Node *new_tail = _get_node_allocator().allocate(1);
    try {
      new (new_tail) Node{_tail, nullptr, value};
    } catch (...) {
      _get_node_allocator().deallocate(new_tail, 1);
      throw;
    }
    if (_tail == nullptr) {
      _head = new_tail;
    } else {
      _tail->next = new_tail;
    }
    _tail = new_tail;
    _size++;
  }

  void push_front(const T &value) {
    Node *new_head = _get_node_allocator().allocate(1);
    try {
      new (new_head) Node{nullptr, _head, value};
    } catch (...) {
      _get_node_allocator().deallocate(new_head, 1);
      throw;
    }
    if (_head == nullptr) {
      _tail = new_head;
    } else {
      _head->prev = new_head;
    }
    _head = new_head;
    _size++;
  }

  void pop_back() {
    Node *new_tail = _tail->prev;
    _tail->~Node();
    _get_node_allocator().deallocate(_tail, 1);
    _tail = new_tail;
    if (_tail == nullptr) {
      _head = nullptr;
    } else {
      _tail->next = nullptr;
    }
    _size--;
  }

  void pop_front() {
    Node *new_head = _head->next;
    _head->~Node();
    _get_node_allocator().deallocate(_head, 1);
    _head = new_head;
    if (_head == nullptr) {
      _tail = nullptr;
    } else {
      _head->prev = nullptr;
    }
    _size--;
  }

  template <bool _IsConst> class base_iterator {
    using _NodePtr = std::conditional_t<_IsConst, const Node *, Node *>;
    using _ListPtr = std::conditional_t<_IsConst, const List *, List *>;
    _NodePtr _node;
    _ListPtr _list;

  public:
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = std::conditional_t<_IsConst, const T *, T *>;
    using reference = std::conditional_t<_IsConst, const T &, T &>;
    using iterator_category = std::bidirectional_iterator_tag;

    base_iterator() : _node(nullptr), _list(nullptr) {}
    base_iterator(_NodePtr node, _ListPtr list)
        : _node(node), _list(list) {}

    template <bool _IsConst2,
              typename = std::enable_if_t<_IsConst || !_IsConst2>>
    base_iterator(const base_iterator<_IsConst2> &rhs)
        : _node(rhs._node), _list(rhs._list) {}

    template <bool _IsConst2,
              typename = std::enable_if_t<_IsConst || !_IsConst2>>
    base_iterator<_IsConst> &
    operator=(const base_iterator<_IsConst2> &rhs) {
      _node = rhs._node;
      _list = rhs._list;
      return *this;
    }

    base_iterator<_IsConst> &operator++() noexcept {
      _node = _node->next;
      return *this;
    }
    base_iterator<_IsConst> operator++(int) noexcept {
      base_iterator<_IsConst> copy = *this;
      ++*this;
      return copy;
    }

    base_iterator<_IsConst> &operator--() noexcept {
      if (_node == nullptr) {
        _node = _list->_tail;
      } else {
        _node = _node->prev;
      }
      return *this;
    }
    base_iterator<_IsConst> operator--(int) noexcept {
      base_iterator<_IsConst> copy = *this;
      --*this;
      return copy;
    }

    reference operator*() const noexcept { return _node->value; }

    template <bool _IsConst2>
    bool operator==(const base_iterator<_IsConst2> &rhs) const noexcept {
      return _node == rhs._node;
    }
    template <bool _IsConst2>
    bool operator!=(const base_iterator<_IsConst2> &rhs) const noexcept {
      return _node != rhs._node;
    }

    friend class List<T, Allocator>;
  };
  using iterator = base_iterator<false>;
  using const_iterator = base_iterator<true>;

  iterator begin() noexcept { return {_head, this}; }
  iterator end() noexcept { return {nullptr, this}; }

  const_iterator begin() const noexcept { return {_head, this}; }
  const_iterator end() const noexcept { return {nullptr, this}; }

  const_iterator cbegin() noexcept { return {_head, this}; }
  const_iterator cend() noexcept { return {nullptr, this}; }

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  reverse_iterator rbegin() noexcept { return reverse_iterator{end()}; }
  reverse_iterator rend() noexcept { return reverse_iterator{begin()}; }

  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator{end()};
  }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator{begin()};
  }

  const_reverse_iterator crbegin() noexcept {
    return const_reverse_iterator{end()};
  }
  const_reverse_iterator crend() noexcept {
    return const_reverse_iterator{begin()};
  }

  iterator insert(const_iterator where, const T &value) {
    const_iterator prev_where = where;
    --prev_where;
    Node *new_node = _get_node_allocator().allocate(1);
    try {
      new (new_node) Node{const_cast<Node *>(prev_where._node),
                          const_cast<Node *>(where._node), value};
    } catch (...) {
      _get_node_allocator().deallocate(new_node, 1);
      throw;
    }
    if (new_node->next == nullptr) {
      _tail = new_node;
    } else {
      new_node->next->prev = new_node;
    }
    if (new_node->prev == nullptr) {
      _head = new_node;
    } else {
      new_node->prev->next = new_node;
    }
    _size++;
    return {new_node, this};
  }

  iterator erase(const_iterator where) {
    Node *prev = const_cast<Node *>(where._node->prev);
    Node *next = const_cast<Node *>(where._node->next);
    const_cast<Node *>(where._node)->~Node();
    _get_node_allocator().deallocate(const_cast<Node *>(where._node), 1);
    if (prev == nullptr) {
      _head = next;
    } else {
      prev->next = next;
    }
    if (next == nullptr) {
      _tail = prev;
    } else {
      next->prev = prev;
    }
    _size--;
    return {next, this};
  }
};