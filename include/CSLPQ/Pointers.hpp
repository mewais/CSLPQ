// //-*-C++-*-
// Implementation of atomic_shared_ptr as per N4162
// (http://isocpp.org/files/papers/N4162.pdf)
//
// Copyright (c) 2014, Just Software Solutions Ltd
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or
// without modification, are permitted provided that the
// following conditions are met:
//
// 1. Redistributions of source code must retain the above
// copyright notice, this list of conditions and the following
// disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials
// provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of
// its contributors may be used to endorse or promote products
// derived from this software without specific prior written
// permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
// CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _JSS_ATOMIC_SHARED_PTR
#define _JSS_ATOMIC_SHARED_PTR
#include <atomic>
#include <memory>
#include <deque>
#include "Atomic128.hpp"

namespace jss{
    template<class T> class shared_ptr;

    struct shared_ptr_data_block_base{};

    template<class D>
    struct shared_ptr_deleter_base{
        D d;

        shared_ptr_deleter_base(D& d_):
                d(d_)
        {}

        template<typename P>
        void do_delete(P p)
        {
            d(p);
        }
    };

    template<>
    struct shared_ptr_deleter_base<void>{
        template<typename T>
        void do_delete(T* p)
        {
            thread_local std::deque<T*> to_delete;
            thread_local bool deleting;

            to_delete.emplace_back(p);
            if(!deleting){
                deleting=true;
                while(!to_delete.empty()){
                    T* dp=to_delete.back();
                    to_delete.pop_back();
                    delete dp;
                }
                deleting=false;
            }
        }
    };

    struct shared_ptr_header_block_base{
        struct counter{
            unsigned external_counters;
            int count;

            counter() noexcept:
                    external_counters(0),
                    count(1)
            {}
        };

        static unsigned const cast_pointer_count=3;
        struct ptr_extension_block{
            std::atomic<void*> cast_pointers[cast_pointer_count];
            std::atomic<ptr_extension_block*> cp_extension;

            ptr_extension_block():
                    cp_extension(0)
            {
                for(unsigned i=0;i<cast_pointer_count;++i){
                    cast_pointers[i]=0;
                }
            }

            unsigned get_ptr_index(void* p)
            {
                for(unsigned i=0;i<cast_pointer_count;++i){
                    void* entry=cast_pointers[i].load();
                    if(entry==p)
                        return i;
                    if(!entry){
                        if(cast_pointers[i].compare_exchange_strong(entry,p) ||
                           (entry==p)){
                            return i;
                        }
                    }
                }
                ptr_extension_block*extension=cp_extension.load();
                if(!extension){
                    ptr_extension_block* new_extension=new ptr_extension_block;
                    if(!cp_extension.compare_exchange_strong(extension,new_extension)){
                        delete new_extension;
                    }
                    else{
                        extension=new_extension;
                    }
                }
                return extension->get_ptr_index(p)+cast_pointer_count;
            }

            void* get_pointer(unsigned index)
            {
                return (index<cast_pointer_count)?
                       cast_pointers[index].load():
                       cp_extension.load()->get_pointer(index-cast_pointer_count);
            }

            ~ptr_extension_block()
            {
                delete cp_extension.load();
            }

        };

        std::atomic<counter> count;
        std::atomic<unsigned> weak_count;
        ptr_extension_block cp_extension;

        unsigned use_count()
        {
            counter c=count.load(std::memory_order_relaxed);
            return c.count+(c.external_counters?1:0);
        }

        unsigned get_ptr_index(void* p)
        {
            return cp_extension.get_ptr_index(p);
        }

        virtual ~shared_ptr_header_block_base()
        {}

        template<typename T>
        T* get_ptr(unsigned index)
        {
            return static_cast<T*>(cp_extension.get_pointer(index));
        }

        shared_ptr_header_block_base():
                count(counter()),weak_count(1)
        {}

        virtual void do_delete()=0;

        void delete_object()
        {
            do_delete();
            dec_weak_count();
        }

        void dec_weak_count()
        {
            if(weak_count.fetch_add(-1)==1){
                delete this;
            }
        }

        void inc_weak_count()
        {
            ++weak_count;
        }

        void dec_count()
        {
            counter old=count.load(std::memory_order_relaxed);
            for(;;){
                counter new_count=old;
                --new_count.count;
                if(count.compare_exchange_weak(old,new_count))
                    break;
            }
            if((old.count==1) && !old.external_counters){
                delete_object();
            }
        }

        bool shared_from_weak()
        {
            counter old=count.load(std::memory_order_relaxed);
            while(old.count||old.external_counters){
                counter new_count=old;
                ++new_count.count;
                if(count.compare_exchange_weak(old,new_count))
                    return true;
            }
            return false;
        }

        void inc_count()
        {
            counter old=count.load(std::memory_order_relaxed);
            for(;;){
                counter new_count=old;
                ++new_count.count;
                if(count.compare_exchange_weak(old,new_count))
                    break;
            }
        }

        void add_external_counters(unsigned external_count)
        {
            counter old=count.load(std::memory_order_relaxed);
            for(;;){
                counter new_count=old;
                new_count.external_counters+=external_count;
                if(count.compare_exchange_weak(old,new_count))
                    break;
            }
        }

        void remove_external_counter()
        {
            counter old=count.load(std::memory_order_relaxed);
            for(;;){
                counter new_count=old;
                --new_count.external_counters;
                if(count.compare_exchange_weak(old,new_count))
                    break;
            }
            if(!old.count && (old.external_counters==1)){
                delete_object();
            }
        }

    };

    template<class P>
    struct shared_ptr_header_block:
            shared_ptr_header_block_base{};

    template<class P,class D>
    struct shared_ptr_header_separate:
            public shared_ptr_header_block<P>,
            private shared_ptr_deleter_base<D>{
        P const ptr;

        void* get_base_ptr()
        {
            return ptr;
        }

        shared_ptr_header_separate(P p):
                ptr(p)
        {}

        template<typename D2>
        shared_ptr_header_separate(P p,D2& d):
                shared_ptr_deleter_base<D>(d),ptr(p)
        {}

        void do_delete()
        {
            shared_ptr_deleter_base<D>::do_delete(ptr);
        }
    };

    template<class T>
    struct shared_ptr_header_combined:
            public shared_ptr_header_block<T*>{
        typedef typename std::aligned_storage<sizeof(T),alignof(T)>::type storage_type;
        storage_type storage;

        T* value(){
            return static_cast<T*>(get_base_ptr());
        }

        void* get_base_ptr()
        {
            return &storage;
        }

        template<typename ... Args>
        shared_ptr_header_combined(Args&& ... args)
        {
            new(get_base_ptr()) T(static_cast<Args&&>(args)...);
        }

        void do_delete()
        {
            value()->~T();
        }
    };

    template<typename T,typename ... Args>
    shared_ptr<T> make_shared(Args&& ... args);

    template<class T> class shared_ptr {
        private:
            T* ptr;
            shared_ptr_header_block_base* header;

            template<typename U>
            friend class atomic_shared_ptr;
            template<typename U>
            friend class markable_atomic_shared_ptr;
            template<typename U>
            friend class shared_ptr;

            template<typename U,typename ... Args>
            friend shared_ptr<U> make_shared(Args&& ... args);

            shared_ptr(shared_ptr_header_block_base* header_,unsigned index):
                    ptr(header_?header_->get_ptr<T>(index):nullptr),header(header_)
            {
                if(header){
                    header->inc_count();
                }
            }

            shared_ptr(shared_ptr_header_block_base* header_,T* ptr_):
                    ptr(ptr_),header(header_)
            {
                if(header && !header->shared_from_weak()){
                    ptr=nullptr;
                    header=nullptr;
                }
            }

            shared_ptr(shared_ptr_header_combined<T>* header_):
                    ptr(header_->value()),header(header_)
            {}

            void clear()
            {
                header=nullptr;
                ptr=nullptr;
            }

        public:
            typedef T element_type;
            // 20.8.2.2.1, constructors:
            constexpr shared_ptr() noexcept:
                    ptr(nullptr),header(nullptr)
            {}

            template<class Y> explicit shared_ptr(Y* p)
            try:
                    ptr(p),
                    header(new shared_ptr_header_separate<Y*,void>(p))
            {}
            catch(...){
                delete p;
            }


            template<class Y, class D> shared_ptr(Y* p, D d)
            try:
                    ptr(p),header(new shared_ptr_header_separate<Y*,D>(p,d))
            {}
            catch(...){
                d(p);
            }

            template <class D> shared_ptr(std::nullptr_t p, D d)
            try:
                    ptr(p),
                    header(new shared_ptr_header_separate<std::nullptr_t,D>(p,d))
            {}
            catch(...){
                d(p);
            }

            template<class Y, class D, class A> shared_ptr(Y* p, D d, A a);
            template <class D, class A> shared_ptr(std::nullptr_t p, D d, A a);

            template<class Y> shared_ptr(const shared_ptr<Y>& r, T* p) noexcept:
                    ptr(p),header(r.header)
            {
                if(header)
                    header->inc_count();
            }

            shared_ptr(const shared_ptr& r) noexcept:
                    ptr(r.ptr),header(r.header)
            {
                if(header)
                    header->inc_count();
            }

            template<class Y> shared_ptr(const shared_ptr<Y>& r) noexcept:
                    ptr(r.ptr),header(r.header)
            {
                if(header)
                    header->inc_count();
            }

            shared_ptr(shared_ptr&& r) noexcept:
                    ptr(r.ptr),header(r.header)
            {
                r.clear();
            }

            template<class Y> shared_ptr(shared_ptr<Y>&& r) noexcept:
                    ptr(r.ptr),header(r.header)
            {
                r.clear();
            }

            constexpr shared_ptr(std::nullptr_t) : shared_ptr() { }
            // 20.8.2.2.2, destructor:
            ~shared_ptr()
            {
                if(header){
                    header->dec_count();
                }
            }

            // 20.8.2.2.3, assignment:
            shared_ptr& operator=(const shared_ptr& r) noexcept
            {
                if(&r!=this){
                    shared_ptr temp(r);
                    swap(temp);
                }
                return *this;
            }
            template<class Y> shared_ptr& operator=(const shared_ptr<Y>& r) noexcept
            {
                shared_ptr temp(r);
                swap(temp);
                return *this;
            }

            shared_ptr& operator=(shared_ptr&& r) noexcept
            {
                swap(r);
                r.reset();
                return *this;
            }

            template<class Y> shared_ptr& operator=(shared_ptr<Y>&& r) noexcept
            {
                shared_ptr temp(static_cast<shared_ptr<Y>&&>(r));
                swap(temp);
                return *this;
            }

            // 20.8.2.2.4, modifiers:
            void swap(shared_ptr& r) noexcept
            {
                std::swap(ptr,r.ptr);
                std::swap(header,r.header);
            }
            void reset() noexcept
            {
                if(header){
                    header->dec_count();
                }
                clear();
            }

            template<class Y> void reset(Y* p)
            {
                shared_ptr temp(p);
                swap(temp);
            }

            template<class Y, class D> void reset(Y* p, D d)
            {
                shared_ptr temp(p,d);
                swap(temp);
            }

            template<class Y, class D, class A> void reset(Y* p, D d, A a);
            // 20.8.2.2.5, observers:
            T* get() const noexcept
            {
                return ptr;
            }

            T& operator*() const noexcept
            {
                return *ptr;
            }

            T* operator->() const noexcept
            {
                return ptr;
            }

            long use_count() const noexcept
            {
                return header?header->use_count():0;
            }

            bool unique() const noexcept
            {
                return use_count()==1;
            }

            explicit operator bool() const noexcept
            {
                return ptr;
            }
            template<class U> bool owner_before(shared_ptr<U> const& b) const;

            friend inline bool operator==(shared_ptr const& lhs,shared_ptr const& rhs)
            {
                return lhs.ptr==rhs.ptr;
            }

            friend inline bool operator!=(shared_ptr const& lhs,shared_ptr const& rhs)
            {
                return !(lhs==rhs);
            }

    };

    template<typename T,typename ... Args>
    shared_ptr<T> make_shared(Args&& ... args){
        return shared_ptr<T>(
                new shared_ptr_header_combined<T>(
                        static_cast<Args&&>(args)...));
    }

#ifdef _MSC_VER
    #define JSS_ASP_ALIGN_TO(alignment) __declspec(align(alignment))
#ifdef _WIN64
#define JSS_ASP_BITFIELD_SIZE 32
#else
#define JSS_ASP_BITFIELD_SIZE 16
#endif
#else
#define JSS_ASP_ALIGN_TO(alignment) __attribute__((aligned(alignment)))
#ifdef __LP64__
#define JSS_ASP_BITFIELD_SIZE 32
#else
#define JSS_ASP_BITFIELD_SIZE 16
#endif
#endif

    template <class T>
    class atomic_shared_ptr
    {
            template<typename U>
            friend class atomic_shared_ptr;

            struct alignas(16) counted_ptr{
                unsigned access_count;
                unsigned index;
                shared_ptr_header_block_base* ptr;

                counted_ptr() noexcept:
                        access_count(0),index(0),ptr(nullptr)
                {}

                counted_ptr(shared_ptr_header_block_base* ptr_,unsigned index_):
                        access_count(0),index(index_),ptr(ptr_)
                {}
            };

            mutable A128::Atomic128<counted_ptr> p;

            struct local_access{
                A128::Atomic128<counted_ptr>& p;
                counted_ptr val;

                void acquire(std::memory_order order){
                    if(!val.ptr)
                        return;
                    for(;;){
                        counted_ptr newval=val;
                        ++newval.access_count;
                        if(p.CompareExchange(val,newval,order))
                            break;
                    }
                    ++val.access_count;
                }

                local_access(
                        A128::Atomic128<counted_ptr>& p_,
                        std::memory_order order=std::memory_order_relaxed):
                        p(p_),val(p.Load(order))
                {
                    acquire(order);
                }

                ~local_access()
                {
                    release();
                }

                void release(){
                    if(!val.ptr)
                        return;
                    counted_ptr target=val;
                    do{
                        counted_ptr newval=target;
                        --newval.access_count;
                        if(p.CompareExchange(target,newval))
                            break;
                    }while(target.ptr==val.ptr);
                    if(target.ptr!=val.ptr){
                        val.ptr->remove_external_counter();
                    }
                }

                void refresh(counted_ptr newval,std::memory_order order){
                    if(newval.ptr==val.ptr)
                        return;
                    release();
                    val=newval;
                    acquire(order);
                }

                shared_ptr_header_block_base* get_ptr()
                {
                    return val.ptr;
                }

                shared_ptr<T> get_shared_ptr()
                {
                    return shared_ptr<T>(val.ptr,val.index);
                }
            };

        public:

            bool is_lock_free() const noexcept
            {
                return p.is_lock_free();
            }

            void store(
                    shared_ptr<T> newptr,
                    std::memory_order order= std::memory_order_seq_cst) /*noexcept*/
            {
                unsigned index=0;
                if(newptr.header){
                    index=newptr.header->get_ptr_index(newptr.ptr);
                }
                counted_ptr old=p.Exchange(counted_ptr(newptr.header,index),order);
                if(old.ptr){
                    old.ptr->add_external_counters(old.access_count);
                    old.ptr->dec_count();
                }
                newptr.clear();
            }

            shared_ptr<T> load(
                    std::memory_order order= std::memory_order_seq_cst) const noexcept
            {
                local_access guard(p,order);
                return guard.get_shared_ptr();
            }

            operator shared_ptr<T>() const noexcept {
                return load();
            }

            shared_ptr<T> exchange(
                    shared_ptr<T> newptr,
                    std::memory_order order= std::memory_order_seq_cst) /*noexcept*/
            {
                counted_ptr newval(
                        newptr.header,
                        newptr.header?newptr.header->get_ptr_index(newptr.ptr):0);
                counted_ptr old=p.Exchange(newval,order);
                shared_ptr<T> res(old.ptr,old.index);
                if(old.ptr){
                    old.ptr->add_external_counters(old.access_count);
                    old.ptr->dec_count();
                }
                newptr.clear();
                return res;
            }

            bool compare_exchange_weak(
                    shared_ptr<T> & expected, shared_ptr<T> newptr,
                    std::memory_order success_order=std::memory_order_seq_cst,
                    std::memory_order failure_order=std::memory_order_seq_cst) /*noexcept*/
            {
                local_access guard(p);
                if(guard.get_ptr()!=expected.header){
                    expected=guard.get_shared_ptr();
                    return false;
                }

                counted_ptr expectedval(
                        expected.header,
                        expected.header?expected.header->get_ptr_index(expected.ptr):0);

                if(guard.val.index!=expectedval.index){
                    expected=guard.get_shared_ptr();
                    return false;
                }

                counted_ptr oldval(guard.val);
                counted_ptr newval(
                        newptr.header,
                        newptr.header?newptr.header->get_ptr_index(newptr.ptr):0);
                if((oldval.ptr==newval.ptr) && (oldval.index==newval.index)){
                    return true;
                }
                if(p.CompareExchange(oldval,newval,success_order,failure_order)){
                    if(oldval.ptr){
                        oldval.ptr->add_external_counters(oldval.access_count);
                        oldval.ptr->dec_count();
                    }
                    newptr.clear();
                    return true;
                }
                else{
                    guard.refresh(oldval,failure_order);
                    expected=guard.get_shared_ptr();
                    return false;
                }
            }

            bool compare_exchange_strong(
                    shared_ptr<T> &expected,shared_ptr<T> newptr,
                    std::memory_order success_order=std::memory_order_seq_cst,
                    std::memory_order failure_order=std::memory_order_seq_cst) noexcept
            {
                shared_ptr<T> local_expected=expected;
                do{
                    if(compare_exchange_weak(expected,newptr,success_order,failure_order))
                        return true;
                }
                while(expected==local_expected);
                return false;
            }

            atomic_shared_ptr() noexcept = default;
            atomic_shared_ptr( shared_ptr<T> val) /*noexcept*/:
                    p(counted_ptr(val.header,val.header?val.header->get_ptr_index(val.ptr):0))
            {
                val.header=nullptr;
                val.ptr=nullptr;
            }

            ~atomic_shared_ptr()
            {
                counted_ptr old=p.Load(std::memory_order_relaxed);
                if(old.ptr)
                    old.ptr->dec_count();
            }

            atomic_shared_ptr(const atomic_shared_ptr&) = delete;
            atomic_shared_ptr& operator=(const atomic_shared_ptr&) = delete;
            shared_ptr<T> operator=(shared_ptr<T> newval) noexcept
            {
                store(static_cast<shared_ptr<T>&&>(newval));
                return newval;
            }

    };

    template <class T>
    class markable_atomic_shared_ptr
    {
            template<typename U>
            friend class markable_atomic_shared_ptr;

            class alignas(16) counted_ptr{
                public:
                    uint32_t access_count : 32;
                    uint32_t index : 31;
                    bool mark: 1;
                    shared_ptr_header_block_base* ptr;

                    counted_ptr() noexcept:
                            access_count(0),index(0),mark(false),ptr(nullptr)
                    {}

                    counted_ptr(shared_ptr_header_block_base* ptr_,unsigned index_):
                            access_count(0),index(index_),mark(false),ptr(ptr_)
                    {}
            };

            mutable A128::Atomic128<counted_ptr> p;

            struct local_access{
                A128::Atomic128<counted_ptr>& p;
                counted_ptr val;

                void acquire(std::memory_order order){
                    if(!val.ptr)
                        return;
                    for(;;){
                        counted_ptr newval=val;
                        ++newval.access_count;
                        if(p.CompareExchange(val,newval))
                            break;
                    }
                    ++val.access_count;
                }

                local_access(
                        A128::Atomic128<counted_ptr>& p_,
                        std::memory_order order=std::memory_order_relaxed):
                        p(p_),val(p.Load())
                {
                    acquire(order);
                }

                ~local_access()
                {
                    release();
                }

                void release(){
                    if(!val.ptr)
                        return;
                    counted_ptr target=val;
                    do{
                        counted_ptr newval=target;
                        --newval.access_count;
                        if(p.CompareExchange(target,newval))
                            break;
                    }while(target.ptr==val.ptr);
                    if(target.ptr!=val.ptr){
                        val.ptr->remove_external_counter();
                    }
                }

                void refresh(counted_ptr newval,std::memory_order order){
                    if(newval.ptr==val.ptr)
                        return;
                    release();
                    val=newval;
                    acquire(order);
                }

                shared_ptr_header_block_base* get_ptr()
                {
                    return val.ptr;
                }

                shared_ptr<T> get_shared_ptr()
                {
                    return shared_ptr<T>(val.ptr,val.index);
                }

                bool is_marked() const
                {
                    return val.mark == true;
                }
            };

        public:

            bool is_lock_free() const noexcept
            {
                return p.is_lock_free();
            }

            void store(
                    shared_ptr<T> newptr,
                    std::memory_order order= std::memory_order_seq_cst) /*noexcept*/
            {
                unsigned index=0;
                if(newptr.header){
                    index=newptr.header->get_ptr_index(newptr.ptr);
                }
                counted_ptr old=p.Exchange(counted_ptr(newptr.header,index));
                if(old.ptr){
                    old.ptr->add_external_counters(old.access_count);
                    old.ptr->dec_count();
                }
                newptr.clear();
            }

            shared_ptr<T> load(
                    std::memory_order order= std::memory_order_seq_cst) const noexcept
            {
                local_access guard(p,order);
                return guard.get_shared_ptr();
            }

            operator shared_ptr<T>() const noexcept {
                return load();
            }

            bool is_marked(std::memory_order order= std::memory_order_seq_cst) const noexcept
            {
                local_access guard(p,order);
                return guard.is_marked();
            }

            std::pair<shared_ptr<T>, bool> load_marked(std::memory_order order= std::memory_order_seq_cst) const noexcept
            {
                local_access guard(p,order);
                return std::make_pair(guard.get_shared_ptr(), guard.is_marked());
            }

            shared_ptr<T> exchange(
                    shared_ptr<T> newptr,
                    std::memory_order order= std::memory_order_seq_cst) /*noexcept*/
            {
                counted_ptr newval(
                        newptr.header,
                        newptr.header?newptr.header->get_ptr_index(newptr.ptr):0);
                counted_ptr old=p.Exchange(newval,order);
                shared_ptr<T> res(old.ptr,old.index);
                if(old.ptr){
                    old.ptr->add_external_counters(old.access_count);
                    old.ptr->dec_count();
                }
                newptr.clear();
                return res;
            }

            void set_mark() noexcept
            {
                while (true)
                {
                    local_access la(p);
                    counted_ptr orig = la.val;
                    counted_ptr next = orig;
                    next.mark = true;
                    if (p.CompareExchange(orig, next))
                    {
                        break;
                    }
                }
            }

            bool test_and_set_mark(
                    shared_ptr<T> & expected,
                    std::memory_order success_order=std::memory_order_seq_cst,
                    std::memory_order failure_order=std::memory_order_seq_cst) /*noexcept*/
            {
                local_access guard(p);
                counted_ptr oldval(guard.val);
                counted_ptr newval = oldval;
                newval.mark = true;
                oldval.mark = false;
                if(p.CompareExchange(oldval,newval)){
                    return true;
                }
                else{
                    guard.refresh(oldval,failure_order);
                    expected=guard.get_shared_ptr();
                    return false;
                }
            }

            bool compare_exchange_weak(
                    shared_ptr<T> & expected, shared_ptr<T> newptr,
                    std::memory_order success_order=std::memory_order_seq_cst,
                    std::memory_order failure_order=std::memory_order_seq_cst) /*noexcept*/
            {
                local_access guard(p);
                if(guard.get_ptr()!=expected.header){
                    expected=guard.get_shared_ptr();
                    return false;
                }

                counted_ptr expectedval(
                        expected.header,
                        expected.header?expected.header->get_ptr_index(expected.ptr):0);

                if(guard.val.index!=expectedval.index){
                    expected=guard.get_shared_ptr();
                    return false;
                }

                counted_ptr oldval(guard.val);
                oldval.mark = false;
                counted_ptr newval(
                        newptr.header,
                        newptr.header?newptr.header->get_ptr_index(newptr.ptr):0);
                if((oldval.ptr==newval.ptr) && (oldval.index==newval.index) && (oldval.mark == newval.mark)){
                    return true;
                }
                if(p.CompareExchange(oldval,newval)){
                    if(oldval.ptr){
                        oldval.ptr->add_external_counters(oldval.access_count);
                        oldval.ptr->dec_count();
                    }
                    newptr.clear();
                    return true;
                }
                else{
                    guard.refresh(oldval,failure_order);
                    expected=guard.get_shared_ptr();
                    return false;
                }
            }

            bool compare_exchange_strong(
                    shared_ptr<T> &expected,shared_ptr<T> newptr,
                    std::memory_order success_order=std::memory_order_seq_cst,
                    std::memory_order failure_order=std::memory_order_seq_cst) noexcept
            {
                shared_ptr<T> local_expected=expected;
                do{
                    if(compare_exchange_weak(expected,newptr,success_order,failure_order))
                        return true;
                }
                while(expected==local_expected);
                return false;
            }

            markable_atomic_shared_ptr() noexcept = default;
            markable_atomic_shared_ptr( shared_ptr<T> val) /*noexcept*/:
                    p(counted_ptr(val.header,val.header?val.header->get_ptr_index(val.ptr):0))
            {
                val.header=nullptr;
                val.ptr=nullptr;
            }

            ~markable_atomic_shared_ptr()
            {
                counted_ptr old=p.Load();
                if(old.ptr)
                    old.ptr->dec_count();
            }

            markable_atomic_shared_ptr(const markable_atomic_shared_ptr&) = delete;
            markable_atomic_shared_ptr& operator=(const markable_atomic_shared_ptr&) = delete;
            shared_ptr<T> operator=(shared_ptr<T> newval) noexcept
            {
                store(static_cast<shared_ptr<T>&&>(newval));
                return newval;
            }

    };
}

#endif