#include <Malloc.hpp>
#include <sys/mman.h>
#include <mutex>          // std::mutex
/**
 * Your special drop-in replacement for malloc(). Should behave the same way.
 */

std::mutex mtx; 

void* myMalloc(size_t n) {
    // TODO: implement myMalloc
     mtx.lock();
    ArenaStore as;
    return as.alloc(n);
    mtx.unlock();
}

/**
 * Your special drop-in replacement for free(). Should behave the same way.
 */
void myFree(void* addr) {
    // TODO: implement myFree
     mtx.lock();
    MMapObject *mmpaObj = reinterpret_cast <MMapObject *>(addr);
    MMapObject::dealloc(mmpaObj);
    if(mmpaObj->arenaSize() != 0)
    {
        Arena *arena = reinterpret_cast <Arena *>(mmpaObj);
        arena->deallocate();
    }
    mtx.unlock();
}




std::atomic<size_t> MMapObject::s_outstandingPages = 0;