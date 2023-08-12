#ifndef __CSLPQ_MARKEDPOINTER_HPP__
#define __CSLPQ_MARKEDPOINTER_HPP__

#include <cstdint>
#include <atomic>
#include <sstream>
#include <thread>

namespace CSLPQ
{
    template<typename T>
    class MarkedSharedPointer;

    template<typename T>
    class alignas(16) SharedPointer
    {
        private:
            std::atomic<uint64_t>* reference_count;
            T* pointer;

            void Release()
            {
                if (this->reference_count)
                {
                    auto count = this->reference_count->fetch_sub(1);
                    if (count == 1)
                    {
                        delete this->reference_count;
                        delete this->pointer;
                        this->reference_count = 0;
                        this->pointer = 0;
                    }
                }
            }

        public:
            explicit SharedPointer(T* pointer = nullptr) : reference_count(nullptr), pointer(pointer)
            {
                if (pointer)
                {
                    this->reference_count = new std::atomic<uint64_t>(1);
                }
            }

        protected:
            SharedPointer(T* pointer, std::atomic<uint64_t>* reference_count) : reference_count(reference_count),
                          pointer(pointer)
            {
                if (this->reference_count)
                {
                    this->reference_count->fetch_add(1);
                }
            }

        public:
            SharedPointer(const SharedPointer& other)
            {
                this->reference_count = other.reference_count;
                if (this->reference_count)
                {
                    this->reference_count->fetch_add(1);
                }
                this->pointer = other.pointer;
            }

            SharedPointer(SharedPointer&& other)
            {
                this->reference_count = other.reference_count;
                this->pointer = other.pointer;
                other.reference_count = nullptr;
                other.pointer = nullptr;
            }

            SharedPointer(const MarkedSharedPointer<T>& other)
            {
                this->reference_count = other.reference_count;
                if (this->reference_count)
                {
                    this->reference_count->fetch_add(1);
                }
                this->pointer = other.pointer;
            }

            SharedPointer& operator=(const SharedPointer& other)
            {
                if (this != &other)
                {
                    this->Release();
                    this->reference_count = other.reference_count;
                    if (this->reference_count)
                    {
                        this->reference_count->fetch_add(1);
                    }
                    this->pointer = other.pointer;
                }
                return *this;
            }

            SharedPointer& operator=(SharedPointer&& other)
            {
                if (this != &other)
                {
                    this->Release();
                    this->reference_count = other.reference_count;
                    this->pointer = other.pointer;
                    other.reference_count = nullptr;
                    other.pointer = nullptr;
                }
                return *this;
            }

            ~SharedPointer()
            {
                this->Release();
            }

            T& operator*()
            {
                return *this->pointer;
            }

            T* operator->()
            {
                return this->pointer;
            }

            explicit operator bool() const
            {
                return this->pointer != nullptr;
            }

            bool operator==(const SharedPointer& other) const
            {
                return this->pointer == other.pointer;
            }

            bool operator!=(const SharedPointer& other) const
            {
                return this->pointer != other.pointer;
            }

            friend class MarkedSharedPointer<T>;
    };

    template<typename T>
    class alignas(16) MarkedSharedPointer
    {
        private:
            std::atomic<uint64_t>* reference_count;
            std::atomic<uintptr_t> pointer;
            static const uintptr_t mask = (uintptr_t)1 << ((sizeof(uintptr_t) * 8) - 1);

            void Release()
            {
                if (this->reference_count)
                {
                    auto count = this->reference_count->fetch_sub(1);
                    if (count == 1)
                    {
                        uintptr_t val = this->pointer.load();
                        val &= ~MarkedSharedPointer::mask;
                        T* ptr = (T*)val;
                        this->pointer.store(0);
                        delete this->reference_count;
                        this->reference_count = 0;
                        delete ptr;
                    }
                }
            }

        public:
            explicit MarkedSharedPointer(T* pointer = nullptr, bool marked = false) : reference_count(nullptr),
                                         pointer((uintptr_t)pointer)
            {
                if (pointer)
                {
                    this->reference_count = new std::atomic<uint64_t>(1);
                    if (marked)
                    {
                        // Set the MSB
                        auto val = (uintptr_t) pointer;
                        val |= MarkedSharedPointer::mask;
                        this->pointer.store((val));
                    }
                }
            }

            MarkedSharedPointer(const MarkedSharedPointer& other)
            {
                this->reference_count = other.reference_count;
                if (this->reference_count)
                {
                    this->reference_count->fetch_add(1);
                }
                this->pointer.store(other.pointer.load());
            }

            MarkedSharedPointer(MarkedSharedPointer&& other)
            {
                this->reference_count = other.reference_count;
                this->pointer.store(other.pointer.load());
                other.reference_count = nullptr;
                other.pointer.store(0);
            }

            MarkedSharedPointer(const SharedPointer<T>& other)
            {
                this->reference_count = other.reference_count;
                if (this->reference_count)
                {
                    this->reference_count->fetch_add(1);
                }
                this->pointer.store((uintptr_t)other.pointer);
            }

            MarkedSharedPointer(SharedPointer<T>&& other)
            {
                this->reference_count = other.reference_count;
                this->pointer.store((uintptr_t)other.pointer);
                other.reference_count = nullptr;
                other.pointer = nullptr;
            }

            MarkedSharedPointer& operator=(const MarkedSharedPointer& other)
            {
                if (this != &other)
                {
                    this->Release();
                    this->reference_count = other.reference_count;
                    if (this->reference_count)
                    {
                        this->reference_count->fetch_add(1);
                    }
                    this->pointer.store(other.pointer.load());
                }
                return *this;
            }

            MarkedSharedPointer& operator=(MarkedSharedPointer&& other) noexcept
            {
                if (this != &other)
                {
                    this->Release();
                    this->reference_count = other.reference_count;
                    this->pointer.store(other.pointer.load());
                    other.reference_count = nullptr;
                    other.pointer.store(0);
                }
                return *this;
            }

            MarkedSharedPointer& operator=(const SharedPointer<T>& other)
            {
                this->Release();
                this->reference_count = other.reference_count;
                if (this->reference_count)
                {
                    this->reference_count->fetch_add(1);
                }
                this->pointer.store((uintptr_t)other.pointer);
                return *this;
            }

            MarkedSharedPointer& operator=(SharedPointer<T>&& other) noexcept
            {
                if (this != &other)
                {
                    this->Release();
                    this->reference_count = other.reference_count;
                    this->pointer.store((uintptr_t)other.pointer);
                    other.reference_count = nullptr;
                    other.pointer = nullptr;
                }
                return *this;
            }

            ~MarkedSharedPointer()
            {
                this->Release();
            }

            bool IsMarked() const
            {
                // Check if the MSB is set
                uintptr_t val = this->pointer.load();
                return val & MarkedSharedPointer::mask;
            }

            SharedPointer<T> GetPointer() const
            {
                uintptr_t val = this->pointer.load();
                val &= ~MarkedSharedPointer::mask;
                auto sp = SharedPointer<T>((T*)val, this->reference_count);
                return sp;
            }

            std::pair<SharedPointer<T>, bool> GetPointerAndMark() const
            {
                uintptr_t val = this->pointer.load();
                T* ptr = (T*)(val & ~MarkedSharedPointer::mask);
                auto sp = SharedPointer<T>(ptr, this->reference_count);
                bool mark = val & MarkedSharedPointer::mask;
                return std::make_pair(sp, mark);
            }

            T& operator*() const
            {
                uintptr_t val = this->pointer.load();
                val &= ~MarkedSharedPointer::mask;
                return *(T*)val;
            }

            T* operator->() const
            {
                uintptr_t val = this->pointer.load();
                val &= ~MarkedSharedPointer::mask;
                return (T*)val;
            }

            explicit operator bool() const
            {
                uintptr_t val = this->pointer.load();
                val &= ~MarkedSharedPointer::mask;
                return (T*)val != nullptr;
            }

            bool operator==(const MarkedSharedPointer& other) const
            {
                uintptr_t val = this->pointer.load();
                val &= ~MarkedSharedPointer::mask;
                uintptr_t other_val = other.pointer.load();
                other_val &= ~MarkedSharedPointer::mask;
                return val == other_val;
            }

            bool operator!=(const MarkedSharedPointer& other) const
            {
                uintptr_t val = this->pointer.load();
                val &= ~MarkedSharedPointer::mask;
                uintptr_t other_val = other.pointer.load();
                other_val &= ~MarkedSharedPointer::mask;
                return val != other_val;
            }

            void SetMark()
            {
                // Do using fetch or
                this->pointer.fetch_or(MarkedSharedPointer::mask);
            }

            bool TestAndSetMark(SharedPointer<T>& expected)
            {
                uintptr_t val = (uintptr_t)expected.pointer;
                val &= ~MarkedSharedPointer::mask;
                uintptr_t new_val = (uintptr_t)expected.pointer;
                new_val |= MarkedSharedPointer::mask;
                return this->pointer.compare_exchange_strong(val, new_val);
            }

            bool CompareExchange(SharedPointer<T>& old_value, SharedPointer<T>& new_value)
            {
                // Register old ref pointer and data pointer, so we can delete them on success
                std::atomic<uint64_t>* old_ref_count = old_value.reference_count;
                T* old_ptr = old_value.pointer;

                bool success = false;
                __asm__ __volatile__
                (
                    "lock cmpxchg16b %1\n\t"
                    "setz %0"
                    : "=q" (success),
                      "+m" (*this),
                      "+d" (old_value.pointer),
                      "+a" (old_value.reference_count)
                    : "c" (new_value.pointer),
                      "b" (new_value.reference_count)
                    : "cc", "memory"
                );

                if (success)
                {
                    // Decrement the old ref count
                    if (old_ref_count)
                    {
                        auto count = old_ref_count->fetch_sub(1);
                        if (count == 1)
                        {
                            delete old_ref_count;
                            delete old_ptr;
                        }
                    }
                }
                return success;
            }
    };
}

#endif // __CSLPQ_MARKEDPOINTER_HPP__