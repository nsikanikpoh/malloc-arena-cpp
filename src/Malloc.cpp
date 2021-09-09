#include <Malloc.hpp>
#include <sys/mman.h>

/**
 * Your special drop-in replacement for malloc(). Should behave the same way.
 */
void* myMalloc(size_t n) {
    // TODO: implement myMalloc
    ArenaStore as;
    return as.alloc(n);
}

/**
 * Your special drop-in replacement for free(). Should behave the same way.
 */
void myFree(void* addr) {
    // TODO: implement myFree
    MMapObject *mmpaObj = reinterpret_cast <MMapObject *>(addr);
    MMapObject::dealloc(mmpaObj);
    if(mmpaObj->arenaSize() != 0)
    {
        Arena *arena = reinterpret_cast <Arena *>(mmpaObj);
        arena->deallocate();
    }
}




std::atomic<size_t> MMapObject::s_outstandingPages = 0;