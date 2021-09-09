#pragma once
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <atomic>
#include <vector>
#include <deque>
#include <map>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>


#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

class MMapObject;
class ArenaStore;
class Arena;

//MMapObject* This = nullptr;

//class Arena;
//Arena* ArenaThis  = nullptr;
// You can assume this as your page size. On some OSs (e.g. macOS), 
// it may in fact be larger and you'll waste memory due to internal 
// fragmentation as a result, but that's okay for this exercise.
constexpr size_t pageSize = 4096;
 const int JOB_SCHEDULER_SIZE = 256;
  const int COMPLEX_SIZE = 128;
  const int COORDINATE_SIZE = 64;
  const int POOL_SIZE = 1024; //number of elements in a single pool
            //can be chosen based on application requirements 

  const int MAX_BLOCK_SIZE = 36; //depending on the application it may change 
                //In above case it came as 36


class MMapObject {

    // The size of the allocated contiguous pages (i.e. the size passed to mmap)
    size_t m_mmapSize;

    // If the type is an arena, the size of each item in the arena. If a big alloc,
    // should be zero.
    size_t m_arenaSize;

    // Debug counter for asserting we freed all the pages we were supposed to.
    // Thread safe and you can ignore it. It's for tests and seeing how many
    // outstanding pages there are.
    static std::atomic<size_t> s_outstandingPages;
public:
    
    MMapObject(const MMapObject& other) = delete;
    MMapObject(){}
    
    
    /**
     * The number of contiguous bytes in this mmap allocation.
     */
    size_t mmapSize() {
        return m_mmapSize;
    }

    /**
     * If the type of this mmap overlay is an arena, this is the size of its items.
     * If a single allocation, this is zero.
     */
    size_t arenaSize() {
        return m_arenaSize;
    }

    void setmmapSize(size_t s)
    {
        m_mmapSize = s;
    }

    void setarenaSize(size_t s)
    {
        m_arenaSize = s;
    }

    /**
     * This function should call mmap to allocate a contiguous set of pages with
     * the passed size. If the caller is intending to use this region as an arena,
     * they should set arenaSize to the size of its items.
     * 
     * If this is a large allocation, the caller should set arenaSize to 0.
     */
    static MMapObject* alloc(size_t size, size_t arenaSize) {
        s_outstandingPages++;

        MMapObject* sd;
        sd->setmmapSize(size);
        sd->setarenaSize(arenaSize);

        // TODO: mmap allocation code
        
        //m_arenaSize = arenaSize;

       
        // if (offset >= getpagesize()) {
        //     fprintf(stderr, "offset is past end of file\n");
        //     exit(EXIT_FAILURE);
        // }

        // if (offset + size > getpagesize())
        //     size = getpagesize() - offset;
        //     /* Can't display bytes past end of file */
        // }
        // offset = getpagesize() - (size + offset);

          //size = size + (unsigned long)pageSize - (size % (unsigned long)pageSize);
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);

                //mmap(0, size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, 0, 0);
                
              //  offset = pageSize - (size + offset);
        
       // m_mmapSize = size;
        return sd;
    }

    /**
     * This function should deallocate the passed pointer by calling munmap.
     * The passed pointer may not be at the start of the memory region, but will
     * be within it, so you'll need to calculate the start of the MMapObject* ptr,
     * passing that as start of the region to unmap and ptr->mmapSize() as its length.
     * 
     * Recall that Arenas will never be larger than the OS page size and BigAllocs
     * always return a pointer to just after the MMapObject header, so you can
     * jump back to the nearest multiple of page size and that will be the MMapObject*.
     */
    static void dealloc(void* ptr) {
        size_t old = s_outstandingPages--;
        MMapObject *obj = reinterpret_cast <MMapObject *>(ptr);

        // If there previously 0 pages, then we goofed and tried to free more pages
        // than we allocated. This is a serious bug, so sigtrap and your debugger
        // can break on this line. If not debugging, you'll get a SIGTRAP message
        // and your program will exit.
         // TODO: munmap deallocation code
        if (old == 0) {
            //raise(SIGTRAP);
            munmap(0, obj->mmapSize());
        }
        else{
            for(size_t i = 0; i <= old; i++)
            {
                void *addr = reinterpret_cast <void *>(i * obj->mmapSize());
                munmap(addr, obj->mmapSize());
            }
        }
        s_outstandingPages = 0;
        obj->setmmapSize(0);
        obj->setarenaSize(0);
    }

    /**
     * Returns the number of pages outstanding that have not been collected.
     * Don't touch this.
     */
    static size_t outstandingPages() {
        return s_outstandingPages.load();
    }
};

class BigAlloc : public MMapObject {
    // This inherits from MMapObject, so it also has the mmapSize and arenSize
    // members as well.

    struct FreeStore
    {
     FreeStore* next;
    }; 
    void expandPoolSize()
    {
            size_t size = (sizeof(volatile char*) > sizeof(FreeStore*)) ?
            sizeof(volatile char*) : sizeof(FreeStore*);
            FreeStore* head = reinterpret_cast <FreeStore*> (new char[size]);
            freeStoreHead = head;

            for (int i = 0; i < pageSize; i++) {
                head->next = reinterpret_cast <FreeStore*> (new char[size]);
                head = head->next;
            }
            head->next = 0;
    }

    void cleanUp()
    {
        FreeStore* nextPtr = freeStoreHead;
        for (; nextPtr; nextPtr = freeStoreHead) {
            freeStoreHead = freeStoreHead->next;
            delete[] nextPtr; // remember this was a char array
        }
    }
    FreeStore* freeStoreHead;
    char m_data[0];

public:
    BigAlloc(const BigAlloc& other) = delete;

    BigAlloc(){
        freeStoreHead = 0;
        expandPoolSize();
    } 
      
    
    virtual ~BigAlloc() { 
      cleanUp();
    }

    

    /**
     * This method should allocate a single large contiguous block of memory using
     * MMapObject::alloc(). You then need to treat that pointer as a BigAlloc*
     * and return the address of the allocation *after* the header.
     * 
     * The returned address must be 64-bit aligned.
     */

     
 
     static void* alloc(size_t size) {
        // TODO: allocate the BigAlloc.
       // pthread_mutex_lock (&lock); 
       MMapObject* p = MMapObject::alloc(size, 0);
        BigAlloc* j;
         if (0 == j->freeStoreHead)
            j->expandPoolSize();

        FreeStore* head = j->freeStoreHead;
        j->freeStoreHead = head->next;
        void *v = reinterpret_cast <void*>(head);
        return v;
       // pthread_mutex_unlock (&lock);
    }
    
};

// This is the data overlay for your Arena allocator.
// It inherits from MMapObject, and thus has a size_
class Arena : public MMapObject {
    // This inherits from MMapObject, so it also has the mmapSize and arenSize
    // members as well.

    // You'll need some means of tracking the number of freed items in this arena
    // in a thread-safe manner. That should go here.
    // mything thing = stuff;
    uint8_t *region;
    size_t size;
    size_t current;
    Arena *nextArena = nullptr;
    size_t allocations = 0;
    size_t expectedAllocations;

    // A pointer to the next free address in the arena.
    char* m_next;

    // This might look kind of weird as it's size is zero, but this serves as a surrogate
    // location to start of the arena's allocation slots. That is &this->m_data[0] is a pointer
    // to the first allocation slot, &this->m_data[arenaSize()] is a pointer to the second
    // and so forth.
    //
    // Note, if you put any data members in this class, you must put them *before* this.
    // Additionally, you need to ensure this address is 64-bit aligned, so you need appropriate
    // padding or to ensure the sizes of your previous members ensures this happens before this.
    //
    // If sizeof(Arena) % 8 == 0, you should be good.
    char* m_data[0];

public:
    
    /**
     * Creates an arena with items of the given size. You should allocate with
     * MMapObject::alloc() and coerce the result into an Arena*.
     */
    static Arena* create(uint32_t itemSize) {
        // TODO: create and initialize the arena.
        MMapObject* pointer_variable = MMapObject::alloc(pageSize, itemSize);
        Arena* arena = reinterpret_cast <Arena*>(pointer_variable); 
        if(!arena) return nullptr;
        arena->region = (uint8_t *) ::calloc(itemSize, sizeof(uint8_t));
        if(!arena->region) { 
            ::free(arena); 
            return nullptr; 
        }

        arena->expectedAllocations = (pageSize - sizeof(Arena)) / itemSize;

        arena->size = itemSize;
        return arena;
    }

    void deallocate(){
        Arena *nextptr, *last = this;
        do {
            nextptr = last->nextArena;
            ::free(last->region);
            ::free(last);
            allocations--;
            last = nextptr;
        } while(nextptr != nullptr);
    }

    /**
     * Allocates an item in the arena and returns its address. Returns null if you
     * have already exceeded the bounds of the arena.
     */
    void* alloc() {
        // TODO: return a pointer to an item in this arena. We should return nullptr
        // if there are no more slots in which to allocate data.
       // return arena_alloc_align(sizeof(char), ALIGNMENT);
       if(allocations == expectedAllocations)
       {
           return nullptr;
       }
       else
       {
           Arena *last, *loop = this;

            do {
            if((size - current) >= pageSize){
                current += pageSize;
                return region + (current - pageSize);
            }
            } while((loop = loop->nextArena) != nullptr);

            Arena *nextptr  = Arena::create(pageSize);
            last->nextArena     = nextptr;
            loop->current += pageSize;

            allocations++;

            return loop->region;
       }
    }

   
    /**
     * Marks one of the items in the arena as freed. Returns true if this arena
     * has no more allocation slots and everything is free'd.
     */
    bool free() {
        // TODO: actually free an item the arena.
        
        Arena *nextptr = nullptr, *last = this;
        if((nextptr != nullptr)){
            nextptr = last->nextArena;
            ::free(last->region);
            ::free(last);
            last = nextptr;
        }
        
        allocations--;
        if(allocations == 0)
        {
            return true;
        }
        return false;
    }

    /**
     * Whether or not this arena can hold more items.
     */
    bool full() {
        // TODO: acutally compute full()
        return allocations == expectedAllocations;
    }

    /**
     * Returns a pointer to the next free item in the arena.
     */
    char* next() {
        m_next = reinterpret_cast <char *>(nextArena);
        return m_next;
    }
};

class ArenaStore {
    /**
     * A set of arenas with the following sizes:
     * 0: 8 bytes
     * 1: 16 bytes
     * 2: 32 bytes
     * ...
     * 8: 1024 bytes
     */
    Arena* m_arenas[9]; // Default initializer for pointer is nullptr

     std::deque<void*>     Byte8PtrList;
    std::deque<void*>     Byte16PtrList;
    std::deque<void*>     Byte24PtrList;
    std::deque<void*>     Byte32PtrList;
    std::deque<void*>     Byte40PtrList;
    std::deque<void*>    MemoryPoolList;

public:
    /**
     * Allocates `bytes` bytes of data. If the data is too large to fit in an arena,
     * it will be allocated using BigAlloc.
     */
    void* alloc(size_t size) {
        // TODO: implement alloc
         
            void *base;
            void *blockPtr =  Byte32PtrList.front();
            switch(size)
                {
                case JOB_SCHEDULER_SIZE :  
                {
                if(Byte32PtrList.empty())
                    {
                    base = new char [32 & POOL_SIZE];
                    MemoryPoolList.push_back(base);
                   // InitialiseByte32List(base);
                    }
                
                ((static_cast<char*>(blockPtr)) + 30); //size of block set
                ((static_cast<char*>(blockPtr)) + 31); //block is no longer free
                Byte32PtrList.pop_front();
                return blockPtr;
                }         
                case COORDINATE_SIZE :  
                {
                if(Byte40PtrList.empty())
                    {
                    base = new char [40 & POOL_SIZE];
                    MemoryPoolList.push_back(base);
                    //InitialiseByte40List(base);
                    }
                void *blockPtr =  Byte40PtrList.front();
                ((static_cast<char*>(blockPtr)) + 38); //size of block set
                ((static_cast<char*>(blockPtr)) + 39); //block is no longer free
                Byte40PtrList.pop_front();
                return blockPtr;
                }         
                case COMPLEX_SIZE : 
                {
                if(Byte24PtrList.empty())
                    {
                    base = new char [24 & POOL_SIZE];
                    MemoryPoolList.push_back(base);
                   // InitialiseByte24List(base);
                    }
                void* blockPtr =  Byte24PtrList.front();
                ((static_cast<char*>(blockPtr)) + 22); //size of block set
                ((static_cast<char*>(blockPtr)) + 23); //block is no longer free
                Byte24PtrList.pop_front();
                return blockPtr;
                }
                default : break;
                }
            return 0;
            }
        
    

    /**
     * Determines the allocation type for the given pointer and calls
     * the appropriate free method.
     */
    void free(void* object) {
        // TODO: implement free.
        //MMapObject *mmpaObj = reinterpret_cast <MMapObject *>(ptr);
        //MMapObject::dealloc(mmpaObj);
        char* init = static_cast<char*>(object);

        while(1)
            {
            int count = 0;
            while(*init != static_cast<char>(0xde))  
                //this loop shall never iterate more than 
            {                 // MAX_BLOCK_SIZE times and hence is O(1)
            init++;
            if(count > MAX_BLOCK_SIZE)
                {
               // printf ("runtime heap memory corruption near %d", object);
                exit(1);
                } 
            count++; 
            }
            if(*(++init) == static_cast<char>(0xad))  // we have hit the guard bytes
            break;  // from the outer while 
            }
        init++;
        int blockSize = static_cast<int>(*init);
        switch(blockSize)
            {
            case 24: Byte24PtrList.push_front(object); break;
            case 32: Byte32PtrList.push_front(object); break;
            case 40: Byte40PtrList.push_front(object); break;
            default: break;
            }
        init++;
        *init = 1; // set free/available byte
            }

};

void* myMalloc(size_t n);
void myFree(void* ptr);

