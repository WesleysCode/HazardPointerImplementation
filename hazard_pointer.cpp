#include <atomic>
#include <unordered_set>
#include <thread>
#define RECLAIM_LIST_THRES 1000

// Hazard Pointer inplemented for Hashtable
// -------------------data structures-------------------
// as each thread only operate one entry to the hashtable at a time so each thread has one hazard pointer block in the global struct
// the amount of hazard pointers need to be extended in other uses
template<typename T>
struct HazardPointerBlock {
    std::thread::id tid; // tid of the owner thread
    std::atomic<T *> ptr;
    std::atomic<HazardPointerBlock *> next;
    
    HazardPointerBlock() : tid(0), ptr(nullptr), next(nullptr) {}
    HazardPointerBlock(std::thread::id tid_) : tid(tid_), ptr(nullptr),next(nullptr) {}
    HazardPointerBlock(std::thread::id tid_, T *pointer) : tid(tid_), ptr(pointer),next(nullptr) {}
};

template<typename T>
struct reclaim_list_node {
    T * target;
    reclaim_list_node *next;
    reclaim_list_node(T * to_reclaim): target(to_reclaim), next(nullptr) {}
    reclaim_list_node() : target(nullptr), next(nullptr) {}
};

// hazard pointer global struct
template<typename T>
static std::atomic< HazardPointerBlock<T> *> hazard_pointer_global;

template<typename T>
static std::atomic<reclaim_list_node<T> *> reclaim_list;

static std::atomic<unsigned int> reclaim_list_count(0);


// -------------------algorithm implementation-------------------
template<typename T>
void set_hazard_ptr(std::thread::id tid, T *ptr_to_set) {
    HazardPointerBlock<T> *tmp = hazard_pointer_global<T>.load();
    HazardPointerBlock<T> *tail = nullptr;
    T *n_ptr = nullptr;

    while(tmp){
        if(tmp->ptr.compare_exchange_strong(n_ptr, ptr_to_set)){
            tmp->tid = tid;
            return;
        }
        if(tmp->next.load()){ tmp = tmp->next.load(); }
        else{
            HazardPointerBlock<T> *new_node = new HazardPointerBlock<T>(tid,ptr_to_set);
            if(tmp->next.compare_exchange_strong(tail, new_node)){ return; }
            else{ tmp = tmp->next.load(); }
        }
    }
}

template<typename T>
void clean_reclaim_list() {
    std::unordered_set<T *> occupied;
    HazardPointerBlock<T> *hp_tmp = hazard_pointer_global<T>.load();
    T *ptr;
    
    while (hp_tmp) {
        ptr = hp_tmp->ptr.load();
        if (ptr) { occupied.insert(ptr); }
        hp_tmp = hp_tmp->next.load();
    }
    reclaim_list_node<T> *to_reclaim = reclaim_list<T>.load();
    reclaim_list_node<T> *tmp = nullptr;
    while(!reclaim_list<T>.compare_exchange_strong(to_reclaim, tmp)){
        to_reclaim = reclaim_list<T>.load();
    }
    while (to_reclaim) {
        if (occupied.find(to_reclaim->target) != occupied.end()) {
            tmp = to_reclaim;
            to_reclaim = to_reclaim->next;
            tmp->next = reclaim_list<T>.load();
            while(!reclaim_list<T>.compare_exchange_strong(tmp->next, tmp)){
                tmp->next = reclaim_list<T>.load();
            }
            reclaim_list_count++;
        }
        else {
            tmp = to_reclaim;
            to_reclaim = to_reclaim->next;
            free(tmp->target);
        }
    }
}

template<typename T>
void reclaim_target(T *target) {
    reclaim_list_node<T> *new_node = new reclaim_list_node<T>(target);
    new_node->next = reclaim_list<T>.load();
    while(!reclaim_list<T>.compare_exchange_strong(new_node->next, new_node)){
        new_node->next = reclaim_list<T>.load();
    }
    reclaim_list_count++;
    if (reclaim_list_count.load() >= RECLAIM_LIST_THRES) {
        reclaim_list_count.store(0);
        clean_reclaim_list<T>();
    }
}
