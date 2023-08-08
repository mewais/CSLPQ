#ifndef __CSLPQ_MARKEDPOINTER_HPP__
#define __CSLPQ_MARKEDPOINTER_HPP__

#include <cstdint>
#include <atomic>

namespace CSLPQ
{
    // A pointer with a bit to indicate whether it is marked or not
    // Since the MSB is never used in today's 64-bit systems, we can use it to indicate whether the pointer is marked or not
    // This allows a single CAS operation to be used to set the pointer and mark atomically
    // From: https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=c9d0c04f24e8c47324cb18ff12a39ff89999afc8
    template<typename T>
    class MarkedPointer
    {
        private:
            std::atomic<uintptr_t> pointer;
            static const uintptr_t mask = (uintptr_t)1 << ((sizeof(uintptr_t) * 8) - 1);

        public:
            MarkedPointer() : pointer(0)
            {
            }

            MarkedPointer(T* pointer, bool marked = false)
            {
                if (marked)
                {
                    // Set the MSB
                    auto val = (uintptr_t)pointer;
                    val |= MarkedPointer::mask;
                    this->pointer.store((val));
                }
                else
                {
                    this->pointer.store((uintptr_t)pointer);
                }
            }

            T* GetPointer() const
            {
                // Return the pointer without the mark
                uintptr_t val = this->pointer.load();
                val &= ~MarkedPointer::mask;
                return (T*)(val);
            }

            bool IsMarked() const
            {
                // Check if the MSB is set
                uintptr_t val = this->pointer.load();
                return val & MarkedPointer::mask;
            }

            T* GetMarkedPointer() const
            {
                return (T*)this->pointer.load();
            }

            std::pair<T*, bool> GetPointerAndMark() const
            {
                uintptr_t val = this->pointer.load();
                T* ptr = (T*)(val & ~MarkedPointer::mask);
                bool mark = val & MarkedPointer::mask;
                return std::make_pair(ptr, mark);
            }

            T* operator->() const
            {
                return this->GetPointer();
            }

            void SetPointer(T* pointer)
            {
                // Set the pointer maintaining the mark
                uintptr_t old_val = this->pointer.load();
                uintptr_t new_val = old_val & MarkedPointer::mask;
                new_val |= (uintptr_t)pointer;
                // Insert using CAS
                while (!this->pointer.compare_exchange_weak(old_val, new_val))
                {
                    new_val = old_val & MarkedPointer::mask;
                    new_val |= (uintptr_t)pointer;
                }
            }

            void SetMark()
            {
                // Do using fetch or
                this->pointer.fetch_or(MarkedPointer::mask);
            }

            void ClearMark()
            {
                this->pointer.fetch_and(~MarkedPointer::mask);
            }

            void SetMarkedPointer(T* pointer, bool marked = false)
            {
                if (marked)
                {
                    auto val = (uintptr_t)pointer;
                    val |= MarkedPointer::mask;
                    this->pointer.store(val);
                }
                else
                {
                    this->pointer.store((uintptr_t)pointer);
                }
            }

            void operator=(T* pointer)
            {
                this->SetPointer(pointer);
            }

            void operator=(MarkedPointer<T>* pointer)
            {
                T* ptr = pointer->GetPointer();
                bool mark = pointer->IsMarked();
                this->SetMarkedPointer(ptr, mark);
            }

            bool CompareExchange(T* old_value, T* new_value)
            {
                return this->pointer.compare_exchange_strong(old_value, new_value);
            }

            bool CompareExchange(T* old_value, bool old_mark, T* new_value, bool new_mark)
            {
                auto old_val = (uintptr_t)old_value;
                auto new_val = (uintptr_t)new_value;
                if (old_mark)
                {
                    old_val |= MarkedPointer::mask;
                }
                if (new_mark)
                {
                    new_val |= MarkedPointer::mask;
                }
                return this->pointer.compare_exchange_strong(old_val, new_val);
            }
    };
}

#endif // __CSLPQ_MARKEDPOINTER_HPP__