#ifndef __CSLPQ_POINTERS_HPP__
#define __CSLPQ_POINTERS_HPP__

#include <cstdint>
#include <atomic>

#include "Atomic128.hpp"

namespace CSLPQ
{
    // This includes definitions for SharedPointer, AtomicSharedPointer, and MarkedAtomicSharedPointer.
    // The former two as described and implemented here: https://github.com/anthonywilliams/atomic_shared_ptr
    // The latter is a modification of the former to include a mark bit for lock-free operation.

    struct SharedPointerDataBlockBase{};

    template <typename D>
    struct SharedPointerDeleterBase
    {
        D d;

        SharedPointerDeleterBase(D& d) : d(d)
        {
        }

        template <typename P>
        void DoDelete(P p)
        {
            this->d(p);
        }
    };

    template<>
    struct SharedPointerDeleterBase<void>
    {
        template <typename T>
        void DoDelete(T* p)
        {
            delete p;
        }
    };

    struct SharedPointerHeaderBlockBase
    {
        struct Counter
        {
            uint32_t external_count;
            int32_t count;

            Counter() noexcept : external_count(0), count(1)
            {
            }
        };

        static unsigned const cast_pointer_count = 3;
        struct PointerExtensionBlock
        {
            std::atomic<void*> cast_pointers[cast_pointer_count];
            std::atomic<PointerExtensionBlock*> cast_pointers_extension;

            PointerExtensionBlock() : cast_pointers_extension(nullptr)
            {
                for (auto& cast_pointer : this->cast_pointers)
                {
                    cast_pointer = nullptr;
                }
            }

            ~PointerExtensionBlock()
            {
                delete this->cast_pointers_extension.load();
            }

            uint32_t GetPointerIndex(void* pointer)
            {
                for (uint32_t i = 0; i < cast_pointer_count; i++)
                {
                    void* entry = this->cast_pointers[i].load();
                    if (entry == pointer)
                    {
                        return i;
                    }
                    if (!entry)
                    {
                        if (this->cast_pointers[i].compare_exchange_strong(entry, pointer) || (entry == pointer))
                        {
                            return i;
                        }
                    }
                }
                PointerExtensionBlock* ext = this->cast_pointers_extension.load();
                if (!ext)
                {
                    auto new_extension = new PointerExtensionBlock();
                    if (!this->cast_pointers_extension.compare_exchange_strong(ext, new_extension))
                    {
                        delete new_extension;
                    }
                    else
                    {
                        ext = new_extension;
                    }
                }
                return ext->GetPointerIndex(pointer) + cast_pointer_count;
            }

            void* GetPointer(uint32_t index) const
            {
                if (index < cast_pointer_count)
                {
                    return this->cast_pointers[index].load();
                }
                else
                {
                    return  this->cast_pointers_extension.load()->GetPointer(index - cast_pointer_count);
                }
            }
        };

        std::atomic<Counter> count;
        std::atomic<uint32_t> weak_count;
        PointerExtensionBlock extension;

        SharedPointerHeaderBlockBase() : count(Counter()), weak_count(1)
        {
        }

        virtual ~SharedPointerHeaderBlockBase()
        {
        }

        uint32_t UseCount() const
        {
            Counter counter = this->count.load();
            return counter.count + (counter.external_count? 1 : 0);
        }

        uint32_t GetPointerIndex(void* pointer)
        {
            return this->extension.GetPointerIndex(pointer);
        }

        template <typename T>
        T* GetPointer(uint32_t index) const
        {
            return static_cast<T*>(this->extension.GetPointer(index));
        }

        void IncrementWeakCount()
        {
            ++this->weak_count;
        }

        void DecrementWeakCount()
        {
            if (this->weak_count.fetch_sub(1) == 1)
            {
                delete this;
            }
        }

        void IncrementCount()
        {
            Counter old = this->count.load();
            while (true)
            {
                Counter next = old;
                ++next.count;
                if (this->count.compare_exchange_weak(old, next))
                {
                    break;
                }
            }
        }

        void DecrementCount()
        {
            Counter old = this->count.load();
            while (true)
            {
                Counter next = old;
                --next.count;
                if (this->count.compare_exchange_weak(old, next))
                {
                    break;
                }
            }
            if ((old.count == 1) && !old.external_count)
            {
                this->DeleteObject();
            }
        }

        void AddExternalCounters(uint32_t ext_count)
        {
            Counter old = this->count.load();
            while (true)
            {
                Counter next = old;
                next.external_count += ext_count;
                if (this->count.compare_exchange_weak(old, next))
                {
                    break;
                }
            }
        }

        void RemoveExternalCounter()
        {
            Counter old = this->count.load();
            while (true)
            {
                Counter next = old;
                --next.external_count;
                if (this->count.compare_exchange_weak(old, next))
                {
                    break;
                }
            }
            if (!old.count && (old.external_count == 1))
            {
                this->DeleteObject();
            }
        }

        bool SharedFromWeak()
        {
            Counter old = this->count.load();
            while (old.count || old.external_count)
            {
                Counter next = old;
                ++next.count;
                if (this->count.compare_exchange_weak(old, next))
                {
                    return true;
                }
            }
            return false;
        }

        virtual void DoDelete() = 0;

        void DeleteObject()
        {
            this->DoDelete();
            this->DecrementWeakCount();
        }
    };

    template <typename P>
    struct SharedPointerHeaderBlock : SharedPointerHeaderBlockBase{};

    template <typename P, typename D>
    struct SharedPointerHeaderSeparate : public SharedPointerHeaderBlock<P>,
                                         private SharedPointerDeleterBase<D>
    {
        P const pointer;

        SharedPointerHeaderSeparate(P p) : pointer(p)
        {
        }

        template <typename D2>
        SharedPointerHeaderSeparate(P p, D2& d) : SharedPointerDeleterBase<D>(d), pointer(p)
        {
        }

        void* GetBasePointer() const
        {
            return this->pointer;
        }

        void DoDelete()
        {
            SharedPointerDeleterBase<D>::DoDelete(this->pointer);
        }
    };

    template <typename T>
    struct SharedPointerHeaderCombined : public SharedPointerHeaderBlock<T*>
    {
        typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type Storage;
        Storage storage;

        template <typename... Args>
        SharedPointerHeaderCombined(Args&&... args)
        {
            new (this->GetBasePointer()) T(static_cast<Args&&>(args)...);
        }

        T* value()
        {
            return static_cast<T*>(this->GetBasePointer());
        }

        void* GetBasePointer() const
        {
            return &this->storage;
        }
        
        void DoDelete()
        {
            this->value()->~T();
        }
    };

    template <typename T>
    class SharedPointer
    {
        private:
            T* pointer;
            SharedPointerHeaderBlockBase* header;

            template<typename U>
            friend class AtomicSharedPointer;

            template<typename U>
            friend class MarkableAtomicSharedPointer;

            template<typename U>
            friend class SharedPointer;

        public:
            typedef T element_type;

            constexpr SharedPointer() noexcept : pointer(nullptr), header(nullptr)
            {
            }

            template <typename Y>
            explicit SharedPointer(Y* p)
            try:
                pointer(p), header(new SharedPointerHeaderSeparate<Y*, void>(p))
                {
                }
            catch (...)
            {
                delete p;
            }

            template <typename Y, typename D>
            SharedPointer(Y* p, D d)
            try:
                pointer(p), header(new SharedPointerHeaderSeparate<Y*, D>(p, d))
                {
                }
            catch (...)
            {
                d(p);
            }

            template <typename D>
            SharedPointer(std::nullptr_t p, D d)
            try:
                pointer(p), header(new SharedPointerHeaderSeparate<std::nullptr_t, D>(p, d))
                {
                }
            catch (...)
            {
                d(p);
            }

            template <typename Y, typename D, typename A> SharedPointer(Y* p, D d, A a);
            template <typename D, typename A> SharedPointer(std::nullptr_t p, D d, A a);

            template <typename Y>
            SharedPointer(const SharedPointer<Y>& r, T* p) noexcept : pointer(p),  header(r.header)
            {
                if (this->header)
                {
                    this->header->IncrementCount();
                }
            }

            SharedPointer(const SharedPointer& r) noexcept : pointer(r.pointer), header(r.header)
            {
                if (this->header)
                {
                    this->header->IncrementCount();
                }
            }

            template <typename Y>
            explicit SharedPointer(const SharedPointer<Y>& r) noexcept : pointer(r.pointer), header(r.header)
            {
                if (this->header)
                {
                    this->header->IncrementCount();
                }
            }

            SharedPointer(SharedPointer&& r) noexcept : pointer(r.pointer), header(r.header)
            {
                r.Clear();
            }

            template <typename Y>
            explicit SharedPointer(SharedPointer<Y>&& r) noexcept : pointer(r.pointer), header(r.header)
            {
                r.Clear();
            }

            constexpr SharedPointer(std::nullptr_t) : SharedPointer()
            {
            }

        private:
            SharedPointer(SharedPointerHeaderBlockBase* header, uint32_t index) :
                          pointer(header? header->GetPointer<T>(index): nullptr), header(header)
            {
                if (header)
                {
                    header->IncrementCount();
                }
            }

            SharedPointer(SharedPointerHeaderBlockBase* header, T* pointer) : pointer(pointer), header(header)
            {
                if (header && !header->SharedFromWeak())
                {
                    this->pointer = nullptr;
                    this->header = nullptr;
                }
            }

            SharedPointer(SharedPointerHeaderCombined<T>* header) : pointer(header->value()), header(header)
            {
            }

            void Clear()
            {
                this->header = nullptr;
                this->pointer = nullptr;
            }

        public:
            ~SharedPointer()
            {
                if (this->header)
                {
                    this->header->DecrementCount();
                }
            }

            SharedPointer& operator=(const SharedPointer& r) noexcept
            {
                if (&r != this)
                {
                    SharedPointer temp(r);
                    this->Swap(temp);
                }
                return *this;
            }

            template <typename Y>
            SharedPointer& operator=(const SharedPointer<Y>& r) noexcept
            {
                SharedPointer temp(r);
                this->Swap(temp);
                return *this;
            }

            SharedPointer& operator=(SharedPointer&& r) noexcept
            {
                this->Swap(r);
                r.Reset();
                return *this;
            }

            template <typename Y>
            SharedPointer& operator=(SharedPointer<Y>&& r) noexcept
            {
                SharedPointer temp(static_cast<SharedPointer<Y>&&>(r));
                this->Swap(temp);
                return *this;
            }

            void Swap(SharedPointer& r) noexcept
            {
                std::swap(this->pointer, r.pointer);
                std::swap(this->header, r.header);
            }

            void Reset() noexcept
            {
                if (this->header)
                {
                    this->header->DecrementCount();
                }
                this->Clear();
            }

            template <typename Y>
            void Reset(Y* p)
            {
                SharedPointer temp(p);
                this->Swap(temp);
            }

            template <typename Y, typename D>
            void Reset(Y* p, D d)
            {
                SharedPointer temp(p, d);
                this->Swap(temp);
            }

            template <typename Y, typename D, typename A>
            void Reset(Y* p, D d, A a);

            T* Get() const noexcept
            {
                return this->pointer;
            }

            T& operator*() const noexcept
            {
                return *this->pointer;
            }

            T* operator->() const noexcept
            {
                return this->pointer;
            }

            explicit operator bool() const noexcept
            {
                return this->pointer;
            }

            uint32_t UseCount() const noexcept
            {
                if (this->header)
                {
                    return this->header->UseCount();
                }
                else
                {
                    return 0;
                }
            }

            bool Unique() const noexcept
            {
                return this->UseCount() == 1;
            }

            friend inline bool operator==(SharedPointer const& lhs, SharedPointer const& rhs)
            {
                return lhs.pointer == rhs.pointer;
            }

            friend inline bool operator!=(SharedPointer const& lhs, SharedPointer const& rhs)
            {
                return !(lhs == rhs);
            }
    };

    template <typename T>
    class AtomicSharedPointer
    {
        private:
            struct alignas(16) CountedPointer
            {
                uint32_t access_count;
                uint32_t index;
                SharedPointerHeaderBlockBase* pointer;

                CountedPointer() noexcept : access_count(0), index(0), pointer(nullptr)
                {
                }

                CountedPointer(uint32_t access_count, uint32_t index, SharedPointerHeaderBlockBase* pointer) noexcept :
                        access_count(access_count), index(index), pointer(pointer)
                {
                }
            };

            class LocalAccess
            {
                public:
                    A128::Atomic128<CountedPointer>& counted_pointer;
                    CountedPointer value;

                    LocalAccess() = delete;

                    LocalAccess(A128::Atomic128<CountedPointer>& counted_pointer) noexcept :
                            counted_pointer(counted_pointer), value(counted_pointer.Load())
                    {
                        this->Acquire();
                    }

                    ~LocalAccess()
                    {
                        this->Release();
                    }

                    void Acquire()
                    {
                        if (!this->value.pointer)
                        {
                            return;
                        }
                        while (true)
                        {
                            CountedPointer next = this->value;
                            ++next.access_count;
                            if (this->counted_pointer.CompareExchange(this->value, next))
                            {
                                break;
                            }
                        }
                        ++this->value.access_count;
                    }

                    void Release()
                    {
                        if (!this->value.pointer)
                        {
                            return;
                        }
                        CountedPointer target = this->value;
                        do
                        {
                            CountedPointer next = target;
                            --next.access_count;
                            if (this->counted_pointer.CompareExchange(target, next))
                            {
                                break;
                            }
                        } while (target.pointer == this->value.pointer);
                        if (target.pointer != this->value.pointer)
                        {
                            this->value.pointer->RemoveExternalCounter();
                        }
                    }

                    void Refresh(CountedPointer next)
                    {
                        if (next.pointer == this->value.pointer)
                        {
                            return;
                        }
                        this->Release();
                        this->value = next;
                        this->Acquire();
                    }

                    SharedPointerHeaderBlockBase* GetPointer() const
                    {
                        return this->value.pointer;
                    }

                    SharedPointer<T> GetSharedPointer() const
                    {
                        return SharedPointer<T>(this->value.pointer, this->value.index);
                    }
            };

            mutable A128::Atomic128<CountedPointer> counted_pointer;

        public:
            AtomicSharedPointer() noexcept = default;

            AtomicSharedPointer(SharedPointer<T> value) noexcept
            {
                if (value.header)
                {
                    this->counted_pointer = CountedPointer(0, value.index, value.header->GetPointerIndex(value.pointer));
                }
                else
                {
                    this->counted_pointer = CountedPointer(0, value.index, nullptr);
                }
                value.header = nullptr;
                value.pointer = nullptr;
            }

            AtomicSharedPointer(const AtomicSharedPointer&) = delete;

            AtomicSharedPointer& operator=(const AtomicSharedPointer&) = delete;

            SharedPointer<T> operator=(SharedPointer<T> next) noexcept
            {
                this->Store(static_cast<SharedPointer<T>&&>(next));
                return next;
            }

            SharedPointer<T> Load() const noexcept
            {
                LocalAccess local_access(this->counted_pointer);
                return local_access.GetSharedPointer();
            }

            operator SharedPointer<T>() const noexcept
            {
                return this->Load();
            }

            void Store(SharedPointer<T> next) noexcept
            {
                uint32_t index = 0;
                if (next.header)
                {
                    index = next.header->GetPointerIndex(next.pointer);
                }
                CountedPointer old = this->counted_pointer.Exchange(CountedPointer(0, index, next.header));
                if (old.pointer)
                {
                    old.pointer->AddExternalCounters(old.access_count);
                    old.pointer->DecrementCount();
                }
                next.Clear();
            }

            SharedPointer<T> Exchange(SharedPointer<T> next) noexcept
            {
                uint32_t index = 0;
                if (next.header)
                {
                    index = next.header->GetPointerIndex(next.pointer);
                }
                CountedPointer old = this->counted_pointer.Exchange(CountedPointer(0, index, next.header));
                SharedPointer<T> result(old.pointer, old.index);
                if (old.pointer)
                {
                    old.pointer->AddExternalCounters(old.access_count);
                    old.pointer->DecrementCount();
                }
                next.Clear();
                return result;
            }

            bool CompareExchangeWeak(SharedPointer<T>& expected, SharedPointer<T> next) noexcept
            {
                LocalAccess local_access(this->counted_pointer);
                if (local_access.GetPointer() != expected.header)
                {
                    expected = local_access.GetSharedPointer();
                    return false;
                }
                uint32_t index = 0;
                if (expected.header)
                {
                    index = expected.header->GetPointerIndex(expected.pointer);
                }
                CountedPointer expected_value = CountedPointer(0, index, expected.header);
                if (local_access.value.index != expected_value.index)
                {
                    expected = local_access.GetSharedPointer();
                    return false;
                }
                index = 0;
                if (next.header)
                {
                    index = next.header->GetPointerIndex(next.pointer);
                }
                CountedPointer old_value(local_access.value);
                CountedPointer new_value = CountedPointer(0, index, next.header);
                if ((old_value.pointer == new_value.pointer) && (old_value.index == new_value.index))
                {
                    return true;
                }
                if (this->counted_pointer.CompareExchange(old_value, new_value))
                {
                    if (old_value.pointer)
                    {
                        old_value.pointer->AddExternalCounters(old_value.access_count);
                        old_value.pointer->DecrementCount();
                    }
                    next.Clear();
                    return true;
                }
                else
                {
                    local_access.Refresh(old_value);
                    expected = local_access.GetSharedPointer();
                    return false;
                }
            }

            bool CompareExchangeStrong(SharedPointer<T>& expected, SharedPointer<T> next) noexcept
            {
                SharedPointer<T> local_expected = expected;
                do
                {
                    if (this->CompareExchangeWeak(expected, next))
                    {
                        return true;
                    }
                } while (local_expected == expected);
                return false;
            }
    };

    template <typename T>
    class MarkableAtomicSharedPointer
    {
        private:
            struct alignas(16) CountedPointer
            {
                uint32_t access_count;
                uint32_t index;
                SharedPointerHeaderBlockBase* pointer;

                CountedPointer() noexcept : access_count(0), index(0), pointer(nullptr)
                {
                }

                CountedPointer(uint32_t access_count, uint32_t index, SharedPointerHeaderBlockBase* pointer) noexcept :
                        access_count(access_count), index(index), pointer(pointer)
                {
                }
            };

            class LocalAccess
            {
                public:
                    A128::Atomic128<CountedPointer>& counted_pointer;
                    CountedPointer value;
                    static const uintptr_t mask = (uintptr_t)1 << ((sizeof(uintptr_t) * 8) - 1);

                    LocalAccess() = delete;

                    LocalAccess(A128::Atomic128<CountedPointer>& counted_pointer) noexcept :
                            counted_pointer(counted_pointer), value(counted_pointer.Load())
                    {
                        this->Acquire();
                    }

                    ~LocalAccess()
                    {
                        this->Release();
                    }

                    void Acquire()
                    {
                        if (!this->value.pointer)
                        {
                            return;
                        }
                        while (true)
                        {
                            CountedPointer next = this->value;
                            ++next.access_count;
                            if (this->counted_pointer.CompareExchange(this->value, next))
                            {
                                break;
                            }
                        }
                        ++this->value.access_count;
                    }

                    void Release()
                    {
                        if (!this->value.pointer)
                        {
                            return;
                        }
                        CountedPointer target = this->value;
                        do
                        {
                            CountedPointer next = target;
                            --next.access_count;
                            if (this->counted_pointer.CompareExchange(target, next))
                            {
                                break;
                            }
                        } while (target.pointer == this->value.pointer);
                        if (target.pointer != this->value.pointer)
                        {
                            this->value.pointer->RemoveExternalCounter();
                        }
                    }

                    void Refresh(CountedPointer next)
                    {
                        if (next.pointer == this->value.pointer)
                        {
                            return;
                        }
                        this->Release();
                        this->value = next;
                        this->Acquire();
                    }

                    SharedPointerHeaderBlockBase* GetPointer() const
                    {
                        uintptr_t val = (uintptr_t)this->value.pointer;
                        val &= ~LocalAccess::mask;
                        return (SharedPointerHeaderBlockBase*)val;
                    }

                    bool IsMarked() const
                    {
                        uintptr_t val = (uintptr_t)this->value.pointer;
                        return val & LocalAccess::mask;
                    }

                    SharedPointer<T> GetSharedPointer() const
                    {
                        uintptr_t val = (uintptr_t)this->value.pointer;
                        val &= ~LocalAccess::mask;
                        return SharedPointer<T>((SharedPointerHeaderBlockBase*)val, this->value.index);
                    }
            };

            mutable A128::Atomic128<CountedPointer> counted_pointer;

        public:
            MarkableAtomicSharedPointer() noexcept = default;

            MarkableAtomicSharedPointer(SharedPointer<T> value) noexcept
            {
                if (value.header)
                {
                    this->counted_pointer = CountedPointer(0, value.index, value.header->GetPointerIndex(value.pointer));
                }
                else
                {
                    this->counted_pointer = CountedPointer(0, value.index, nullptr);
                }
                value.header = nullptr;
                value.pointer = nullptr;
            }

            MarkableAtomicSharedPointer(const MarkableAtomicSharedPointer&) = delete;

            MarkableAtomicSharedPointer& operator=(const MarkableAtomicSharedPointer&) = delete;

            SharedPointer<T> operator=(SharedPointer<T> next) noexcept
            {
                this->Store(static_cast<SharedPointer<T>&&>(next));
                return next;
            }

            SharedPointer<T> Load() const noexcept
            {
                LocalAccess local_access(this->counted_pointer);
                return local_access.GetSharedPointer();
            }

            operator SharedPointer<T>() const noexcept
            {
                return this->Load();
            }

            bool IsMarked() const noexcept
            {
                LocalAccess local_access(this->counted_pointer);
                return local_access.IsMarked();
            }

            std::pair<SharedPointer<T>, bool> LoadMarked() const noexcept
            {
                LocalAccess local_access(this->counted_pointer);
                return std::make_pair(local_access.GetSharedPointer(), local_access.IsMarked());
            }

            void Store(SharedPointer<T> next) noexcept
            {
                uint32_t index = 0;
                if (next.header)
                {
                    index = next.header->GetPointerIndex(next.pointer);
                }
                CountedPointer old = this->counted_pointer.Exchange(CountedPointer(0, index, next.header));
                if (old.pointer)
                {
                    old.pointer->AddExternalCounters(old.access_count);
                    old.pointer->DecrementCount();
                }
                next.Clear();
            }

            void SetMark() noexcept
            {
                LocalAccess local_access(this->counted_pointer);
                while (true)
                {
                    CountedPointer next = local_access.value;
                    next.pointer = (SharedPointerHeaderBlockBase*)((uintptr_t)next.pointer | LocalAccess::mask);
                    if (this->counted_pointer.CompareExchange(local_access.value, next))
                    {
                        break;
                    }
                }
            }

            SharedPointer<T> Exchange(SharedPointer<T> next) noexcept
            {
                uint32_t index = 0;
                if (next.header)
                {
                    index = next.header->GetPointerIndex(next.pointer);
                }
                CountedPointer old = this->counted_pointer.Exchange(CountedPointer(0, index, next.header));
                SharedPointer<T> result(old.pointer, old.index);
                if (old.pointer)
                {
                    old.pointer->AddExternalCounters(old.access_count);
                    old.pointer->DecrementCount();
                }
                next.Clear();
                return result;
            }

            bool TestAndSetMark(SharedPointer<T>& expected)
            {
                LocalAccess local_access(this->counted_pointer);
                if (local_access.GetPointer() != expected.header)
                {
                    expected = local_access.GetSharedPointer();
                    return false;
                }
                uint32_t index = 0;
                if (expected.header)
                {
                    index = expected.header->GetPointerIndex(expected.pointer);
                }
                CountedPointer expected_value = CountedPointer(0, index, expected.header);
                if (local_access.value.index != expected_value.index)
                {
                    expected = local_access.GetSharedPointer();
                    return false;
                }
                CountedPointer old_value(local_access.value);
                CountedPointer new_value = CountedPointer(0, index, expected.header);
                new_value.pointer = (SharedPointerHeaderBlockBase*)((uintptr_t)new_value.pointer | LocalAccess::mask);
                if ((old_value.pointer == new_value.pointer) && (old_value.index == new_value.index))
                {
                    return true;
                }
                if (this->counted_pointer.CompareExchange(old_value, new_value))
                {
                    return true;
                }
                else
                {
                    local_access.Refresh(old_value);
                    expected = local_access.GetSharedPointer();
                    return false;
                }
            }

            bool CompareExchangeWeak(SharedPointer<T>& expected, SharedPointer<T> next) noexcept
            {
                LocalAccess local_access(this->counted_pointer);
                if (local_access.GetPointer() != expected.header)
                {
                    expected = local_access.GetSharedPointer();
                    return false;
                }
                uint32_t index = 0;
                if (expected.header)
                {
                    index = expected.header->GetPointerIndex(expected.pointer);
                }
                CountedPointer expected_value = CountedPointer(0, index, expected.header);
                if (local_access.value.index != expected_value.index)
                {
                    expected = local_access.GetSharedPointer();
                    return false;
                }
                index = 0;
                if (next.header)
                {
                    index = next.header->GetPointerIndex(next.pointer);
                }
                CountedPointer old_value(local_access.value);
                CountedPointer new_value = CountedPointer(0, index, next.header);
                if ((old_value.pointer == new_value.pointer) && (old_value.index == new_value.index))
                {
                    return true;
                }
                if (this->counted_pointer.CompareExchange(old_value, new_value))
                {
                    if (old_value.pointer)
                    {
                        old_value.pointer->AddExternalCounters(old_value.access_count);
                        old_value.pointer->DecrementCount();
                    }
                    next.Clear();
                    return true;
                }
                else
                {
                    local_access.Refresh(old_value);
                    expected = local_access.GetSharedPointer();
                    return false;
                }
            }

            bool CompareExchangeStrong(SharedPointer<T>& expected, SharedPointer<T> next) noexcept
            {
                SharedPointer<T> local_expected = expected;
                do
                {
                    if (this->CompareExchangeWeak(expected, next))
                    {
                        return true;
                    }
                } while (local_expected == expected);
                return false;
            }
    };

    template <typename T,typename ... Args>
    SharedPointer<T> MakeShared(Args&& ... args)
    {
        return SharedPointer<T>(new SharedPointerHeaderCombined<T>(static_cast<Args&&>(args)...));
    }
}

#endif // __CSLPQ_POINTERS_HPP__