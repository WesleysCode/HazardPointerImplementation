# Hazard_Pointer_Implementation
An implementation of hazard Pointer

The original idea of hazard pointer is presented in paper ***Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects***[1]. 

### Usage

- Before a thread accesses shared data, set the hazard pointer first. After using the shared data, set the hazard pointer to nullptr. **target** is a pointer to that data.

```c++
set_hazard_ptr<T>(std::this_thread::get_id(), target);
set_hazard_ptr<T>(std::this_thread::get_id(), nullptr);
```

- use function reclaim_target to reclaim shared data.

```c++
reclaim_target<T>(target);
```



[1] M. M. Michael, "Hazard pointers: safe memory reclamation for lock-free objects," in IEEE Transactions on Parallel and Distributed Systems, vol. 15, no. 6, pp. 491-504, June 2004, doi: 10.1109/TPDS.2004.8.

