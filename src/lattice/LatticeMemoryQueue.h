#pragma once
#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

namespace lattice
{

class SharedMemoryQueue
{
  public:
    // Factory method to create a default shared memory queue with optional instance count increment
    static SharedMemoryQueue CreateDefaultInstanceTracker(bool incrementInstanceCount = false)
    {
        return SharedMemoryQueue("/default_instance", 100, 64, incrementInstanceCount);
    }

    // Constructor to initialize the shared memory queue
    SharedMemoryQueue(const std::string &shmName, size_t queueSize, size_t blockSize,
                      bool incrementInstanceCount = false)
        : shmName(shmName), queueSize(queueSize), blockSize(blockSize)
    {
        // Initialize the global shared memory for instance tracking
        initializeGlobalSharedMemory();

        // Assign a unique instance ID
        if (incrementInstanceCount)
        {
            instanceId = globalSharedMemory->nextInstanceId.fetch_add(1) + 1;
            globalSharedMemory->instanceCount.fetch_add(1);
        }
        else
        {
            instanceId = globalSharedMemory->nextInstanceId.load();
        }

        // Try opening existing shared memory
        shmFd = shm_open(shmName.c_str(), O_RDWR, 0666);
        std::cout << "SharedMemoryQueue ctor: Name of named memory:" << shmName << std::endl;
        bool isNew = false;

        // If it doesn't exist, create it
        if (shmFd == -1)
        {
            shmFd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
            if (shmFd == -1)
            {
                throw std::runtime_error("Failed to create shared memory object");
            }
            isNew = true;
        }

        // Calculate total memory size required for both directions
        size_t totalSize = 2 * (sizeof(SharedMemoryHeader) + queueSize * blockSize);
        
        // Set size of shared memory if new
        if (isNew)
        {
            if (ftruncate(shmFd, totalSize) == -1)
            {
                throw std::runtime_error("Failed to set size of shared memory object");
            }
        }

        // Map shared memory into process address space
        shmPtr = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
        if (shmPtr == MAP_FAILED)
        {
            throw std::runtime_error("Failed to map shared memory object");
        }

        // Pointers to the headers for child-to-host and host-to-child communication
        childToHostHeader = reinterpret_cast<SharedMemoryHeader *>(shmPtr);
        hostToChildHeader = reinterpret_cast<SharedMemoryHeader *>(reinterpret_cast<char *>(shmPtr) +
                                                                   sizeof(SharedMemoryHeader) + queueSize * blockSize);

        // Initialize headers if this is a new shared memory segment
        if (isNew)
        {
            childToHostHeader->head.store(0);
            childToHostHeader->tail.store(0);
            childToHostHeader->size.store(queueSize);
            childToHostHeader->refCount.store(1);

            hostToChildHeader->head.store(0);
            hostToChildHeader->tail.store(0);
            hostToChildHeader->size.store(queueSize);
            hostToChildHeader->refCount.store(1);
        }
        else
        {
            // Increment ref count if already exists
            childToHostHeader->refCount.fetch_add(1);
            hostToChildHeader->refCount.fetch_add(1);
        }
    }

    // Decrement the instance ID counter
    void decrementId()
    {
        if (globalSharedMemory)
        {
            instanceId = globalSharedMemory->nextInstanceId.fetch_sub(1) - 1;
        }
    }

    // Increment the instance ID counter
    void incrementId()
    {
        if (globalSharedMemory)
        {
            instanceId = globalSharedMemory->nextInstanceId.fetch_add(1);
        }
    }

    // Destructor to unmap and clean up shared memory
    ~SharedMemoryQueue()
    {
        // If this was the last user of the queue, unmap and unlink it
        if (childToHostHeader->refCount.fetch_sub(1) == 1)
        {
            size_t totalSize = 2 * (sizeof(SharedMemoryHeader) + queueSize * blockSize);
            munmap(shmPtr, totalSize);
            close(shmFd);
            shm_unlink(shmName.c_str());
            std::cout << "Shared memory freed (Process ID: " << getpid() << ")" << std::endl;
        }
        else
        {
            std::cout << "Process detached from shared memory (Process ID: " << getpid() << ")" << std::endl;
        }

        // Decrement global shared memory instance count
        decrementGlobalInstanceCount();
    }

    // Methods to send and receive messages from child/host
    bool sendToChild(const nlohmann::json &obj) { return push(hostToChildHeader, obj); }
    bool receiveFromChild(nlohmann::json &obj) { return pop(childToHostHeader, obj); }
    bool sendToHost(const nlohmann::json &obj) { return push(childToHostHeader, obj); }
    bool receiveFromHost(nlohmann::json &obj) { return pop(hostToChildHeader, obj); }

    // Returns the instance ID as a string
    std::string getInstanceId() const { return std::to_string(instanceId); }

  private:
    // Header metadata for circular buffer queues
    struct SharedMemoryHeader
    {
        std::atomic<size_t> head;
        std::atomic<size_t> tail;
        std::atomic<size_t> size;
        std::atomic<int> refCount;
    };

    // Structure to track global instance state
    struct GlobalSharedMemory
    {
        std::atomic<int> instanceCount;
        std::atomic<int> nextInstanceId;
    };

    // Decrement global instance count and clean up if last instance
    void decrementGlobalInstanceCount()
    {
        if (globalSharedMemory)
        {
            int remainingInstances = globalSharedMemory->instanceCount.fetch_sub(1) - 1;

            if (remainingInstances == 0)
            {
                munmap(globalShmPtr, sizeof(GlobalSharedMemory));
                close(globalShmFd);
                shm_unlink("/global_instance_tracker");

                std::cout << "Global shared memory freed (Process ID: " << getpid() << ")" << std::endl;
            }
        }
    }

    // Initializes global shared memory used for tracking instances
    void initializeGlobalSharedMemory()
    {
        globalShmFd = shm_open("/global_instance_tracker", O_CREAT | O_RDWR, 0666);
        if (globalShmFd == -1)
        {
            throw std::runtime_error("Failed to create global shared memory object");
        }

        if (ftruncate(globalShmFd, sizeof(GlobalSharedMemory)) == -1)
        {
            throw std::runtime_error("Failed to set size of global shared memory object");
        }

        globalShmPtr = mmap(nullptr, sizeof(GlobalSharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, globalShmFd, 0);
        if (globalShmPtr == MAP_FAILED)
        {
            throw std::runtime_error("Failed to map global shared memory object");
        }

        globalSharedMemory = reinterpret_cast<GlobalSharedMemory *>(globalShmPtr);

        // Initialize counters only if first process
        if (globalSharedMemory->instanceCount.load() == 0)
        {
            globalSharedMemory->instanceCount.store(0);
            globalSharedMemory->nextInstanceId.store(1); // IDs start at 1
        }
    }

    // Push a serialized JSON object into the circular queue
    bool push(SharedMemoryHeader *header, const nlohmann::json &obj)
    {
        size_t head = header->head.load();
        size_t nextHead = (head + 1) % header->size.load();

        if (nextHead == header->tail.load())
        {
            return false; // Queue is full
        }

        std::string serialized = obj.dump();
        if (serialized.size() >= blockSize)
        {
            throw std::runtime_error("JSON object too large for block size");
        }

        char *queueBase = reinterpret_cast<char *>(shmPtr) + sizeof(SharedMemoryHeader);
        std::memcpy(queueBase + head * blockSize, serialized.c_str(), serialized.size());
        queueBase[head * blockSize + serialized.size()] = '\0';

        header->head.store(nextHead);
        return true;
    }

    // Pop a serialized JSON object from the circular queue
    bool pop(SharedMemoryHeader *header, nlohmann::json &obj)
    {
        size_t tail = header->tail.load();

        if (tail == header->head.load())
        {
            return false; // Queue is empty
        }

        char *queueBase = reinterpret_cast<char *>(shmPtr) + sizeof(SharedMemoryHeader);
        char *block = queueBase + tail * blockSize;

        size_t messageLength = std::strlen(block);
        std::string serialized(block, messageLength);

        obj = nlohmann::json::parse(serialized);
        header->tail.store((tail + 1) % header->size.load());

        return true;
    }

    // Instance variables
    std::string shmName;
    size_t queueSize;
    size_t blockSize;
    int shmFd;
    void *shmPtr;
    SharedMemoryHeader *childToHostHeader;
    SharedMemoryHeader *hostToChildHeader;

    // Global shared memory for instance tracking
    int globalShmFd;
    void *globalShmPtr;
    GlobalSharedMemory *globalSharedMemory;
    int instanceId; // Unique ID for this instance
};

} // namespace lattice
