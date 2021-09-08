#pragma once
#include <signal.h>
#include <atomic>
#include <vector>
#include <Arena.hpp>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

uintptr_t align_forward(uintptr_t ptr, size_t align) {
	uintptr_t p, a, modulo;
	p = ptr;
	a = (uintptr_t)align;
	// Same as (p % a) but faster as 'a' is a power of two
	modulo = p & (a-1);
	if (modulo != 0) {
		// If 'p' address is not aligned, push the address to the
		// next value which is aligned
		p += a - modulo;
	}
	return p;
}

// You can assume this as your page size. On some OSs (e.g. macOS), 
// it may in fact be larger and you'll waste memory due to internal 
// fragmentation as a result, but that's okay for this exercise.
constexpr size_t pageSize = 4096;

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
    MMapObject() = delete;

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

    /**
     * This function should call mmap to allocate a contiguous set of pages with
     * the passed size. If the caller is intending to use this region as an arena,
     * they should set arenaSize to the size of its items.
     * 
     * If this is a large allocation, the caller should set arenaSize to 0.
     */
    static MMapObject* alloc(size_t size, size_t arenaSize) {
        s_outstandingPages++;

        // TODO: mmap allocation code
        
        m_arenaSize = arenaSize;

       
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
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

                //mmap(0, size - 1, PROT_READ | PROT_WRITE, MAP_SHARED, 0, 0);
                
              //  offset = pageSize - (size + offset);
        
        m_mmapSize = size;
        return this;
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
    static void dealloc(void* obj) {
        size_t old = s_outstandingPages--;

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
                munmap((i * obj->mmapSize()), obj->mmapSize());
            }
        }
        s_outstandingPages = 0;
        m_mmapSize = 0;
        m_arenaSize = 0;
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

    char m_data[0];

public:
    BigAlloc(const BigAlloc& other) = delete;
    BigAlloc() = delete;

    /**
     * This method should allocate a single large contiguous block of memory using
     * MMapObject::alloc(). You then need to treat that pointer as a BigAlloc*
     * and return the address of the allocation *after* the header.
     * 
     * The returned address must be 64-bit aligned.
     */
 
     static void* alloc(size_t size) {
        // TODO: allocate the BigAlloc.
        void* p = MMapObject::alloc(size, 0);
        ALIGN(p);
        return p;
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

    std::vector<size_t> free_list;
   
    unsigned char *buf;
    size_t         buf_len;
    size_t         prev_offset; // This will be useful for later on
    size_t         curr_offset;
    size_t         allocated = 0;


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
        Arena* alloc = reinterpret_cast <Arena *>(pointer_variable);
        unsigned char backing_buf[feritemSize];
        buf = (unsigned char*)backing_buffer;
        buf_len = itemSize;
        curr_offset = 0;
        prev_offset = 0;
        return alloc;
    }

    ~Arena () { 

    }

    /**
     * Allocates an item in the arena and returns its address. Returns null if you
     * have already exceeded the bounds of the arena.
     */
    void* alloc() {
        // TODO: return a pointer to an item in this arena. We should return nullptr
        // if there are no more slots in which to allocate data.
        return arena_alloc_align(sizeof(char), ALIGNMENT);
    }

    void *arena_alloc_align(size_t size, size_t align) {
	// Align 'curr_offset' forward to the specified alignment
	uintptr_t curr_ptr = (uintptr_t)buf + (uintptr_t)curr_offset;
	uintptr_t offset = align_forward(curr_ptr, align);
	offset -= (uintptr_t)buf; // Change to relative offset

	// Check to see if the backing memory has space left
	if (offset+size <= buf_len) {
		void *ptr = &buf[offset];
		prev_offset = offset;
		curr_offset = offset+size;

		// Zero new memory by default
		memset(ptr, 0, size);
        allocated++;
        // remove from free list
        for (auto it = free_list.begin(); it != free_list.end(); ) {
                if (*it == allocated) {
                    it = free_list.erase(it);
                } else {
                    ++it;
                }
        }
		return ptr;
	}
	// Return nullptr if the arena is out of memory (or handle differently)
	return nullptr;
}

  void deallocate () {
      curr_offset = 0;
	  prev_offset = 0;
      allocated = 0;
  }

    /**
     * Marks one of the items in the arena as freed. Returns true if this arena
     * has no more allocation slots and everything is free'd.
     */
    bool free() {
        // TODO: actually free an item the arena.
        if(free_list.size() == itemSize)
        {
            curr_offset = 0;
	        prev_offset = 0;
            return true;
        }   
        free_list.push_back(allocated - 1);
        allocated--;
        if(free_list.size() == itemSize)
        {
            curr_offset = 0;
	        prev_offset = 0;
            return true;
        }
        return false;
    }

    /**
     * Whether or not this arena can hold more items.
     */
    bool full() {
        // TODO: acutally compute full()
        return allocated == itemSize;  
    }

    /**
     * Returns a pointer to the next free item in the arena.
     */
    char* next() {
        m_next = reinterpret_cast <char *>(&curr_offset);
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

public:
    /**
     * Allocates `bytes` bytes of data. If the data is too large to fit in an arena,
     * it will be allocated using BigAlloc.
     */
    void* alloc(size_t bytes) {
        // TODO: implement alloc
        void* alloc;
        if(bytes > 2048)
        {
            alloc = BigAlloc::alloc(bytes, 0);
        }
        else
        {
            alloc = Arena::create(bytes);
            switch(bytes)
            {
                case 8:
                 m_arenas[0] = alloc;
                case 16:
                 m_arenas[1] = alloc;
                case 32:
                 m_arenas[2] = alloc;
                case 64:
                 m_arenas[3] = alloc;
                case 128:
                 m_arenas[4] = alloc;
                case 256:
                 m_arenas[5] = alloc;
                case 512:
                 m_arenas[6] = alloc;
                case 1024:
                 m_arenas[7] = alloc;
                case 2048:
                 m_arenas[8] = alloc;

            }
        }
        return alloc;
    }

    /**
     * Determines the allocation type for the given pointer and calls
     * the appropriate free method.
     */
    void free(void* ptr) {
        // TODO: implement free.
        if(ptr->arenaSize() == 0)
        {
            MMapObject::dealloc(ptr);
        }
        else
        {
            MMapObject::dealloc(ptr);
            ptr->deallocate();
        }
        
    }

};

void* myMalloc(size_t n){
    return ArenaStore::alloc(n);
}
void myFree(void* ptr)
{
    if(ptr->arenaSize() == 0)
        {
            MMapObject::dealloc(ptr);
        }
        else
        {
            MMapObject::dealloc(ptr);
            ptr->deallocate();
        }
}