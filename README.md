# Sequential_hashtable
Alternative implementation of C++ Associative Containers.

**Hash Tables** data structures are essential in many algorithms.
*Uniqueness* of the elements of the container and *constant time complexity* are two strong properties provided by this type of containers.
On the other hand, iterating over these container does not guarantee any specific sequencing, unless the Hash Table is ordered. Which will change the time complexity of this data structure from Amortized *O(1)* to *Log(n)*.

**note:** *Elements Sequencing*, here, does not mean *Elements Ordering*. Ordered Elements are Sequenced but Sequenced Elements are not necessarly ordered.

**Queues** and **Stacks** are two data structures which guarantee the *sequencing* of their elements.
The drawback is that finding an element in these type of containers is of *Linear time complexity*.

To combine strong properties like:
  * Elements Uniquness
  * Constant Time Complexity
  * Elements Sequencing

We need a different data structure.

Some mechanism that combines the above properties are well known. **LRU** is the most prominent.

However, the Implementation of **LRU** involves **2** containers.
  * *Doubly Linked List* : for sequencing.
  * *Set* : for uniqueness and O(1) time complexity.

On all major implementations of C++ Standard Library, Associative containers are implemented in terms of a `std::vector` and `std::forward_list` semantics. Which increments the total number of containers involved in the implementation of the **LRU** data structure to **3**.

While **LRU** is named after a specific purpose, yet it is not effecient enough for all algorithms usage case.

In this library, I'm trying to adapt C++ Associative Containers: `std::unordered_set`, `std::unordered_map` to produce `std::hashed_stack` and `std::hashed_queue`, which are two new data structures that combines the above listed properties without the drawbacks of **LRU** implementation.

The library is based on **gcc** *stdlibc++* implementation code for C++17.

### Hashed_Stack
`std::hashed_stack` : **LIFO** data structure with guaranteed uniquness of its elements.

You can:
* push
* pop
* test existance of an element
* visit elements sequentialy (`std::stack` behavior)
* iterate over element randomly (`std::unordered_set`, `std::unordered_map` behavior)
* extract an element without disturbing the sequencing.

### Hashed_Queue
`std::hashed_queue` : **FIFO** data structure with guaranteed uniquness of its elements.

You can:
* push
* pop
* test existance of an element
* visit elements sequentialy (`std::queue` behavior)
* iterate over element randomly (`std::unordered_set`, `std::unordered_map` behavior)
* extract an element without disturbing the sequencing.
