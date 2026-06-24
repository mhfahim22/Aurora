#pragma once
// smart_ptr.hpp — AuroraSharedPtr<T>, AuroraWeakPtr<T>, CppSharedPtrBridge
// Phase 8: Memory Safety

#include "runtime/memory.hpp"
#include "runtime/interop/ref_count_bridge.hpp"
#include <cstddef>
#include <type_traits>
#include <utility>
#include <memory>

/* ════════════════════════════════════════════════════════════
   AuroraSharedPtr<T> — Thread-safe reference-counted smart pointer
   
   Uses the existing SharedBox infrastructure for atomic strong/weak
   ref-counting, weak lock, and custom destructor support.
   
   Features:
     - Atomic ref-count (thread-safe)
     - Custom deleter via destructor callback
     - Weak pointer support (AuroraWeakPtr<T>)
     - Nullable, copyable, movable
     - Interop with C API via SharedBox
   ════════════════════════════════════════════════════════════ */

template<typename T>
class AuroraSharedPtr {
public:
    /* ── Default constructor (null) ── */
    AuroraSharedPtr() noexcept : box_(nullptr) {}

    /* ── Construct from raw pointer (takes ownership) ── */
    template<typename U = T>
    explicit AuroraSharedPtr(U* ptr,
        void (*deleter)(void*) = [](void* p) { delete static_cast<T*>(p); })
        : box_(nullptr)
    {
        if (ptr) {
            box_ = static_cast<SharedBox*>(aurora_shared_new(ptr, deleter));
        }
    }

    /* ── Construct from existing SharedBox (retains) ── */
    explicit AuroraSharedPtr(SharedBox* box) : box_(box) {
        if (box_) aurora_refcount_inc(box_);
    }

    /* ── Copy constructor ── */
    AuroraSharedPtr(const AuroraSharedPtr& other) noexcept : box_(other.box_) {
        if (box_) aurora_refcount_inc(box_);
    }

    /* ── Move constructor ── */
    AuroraSharedPtr(AuroraSharedPtr&& other) noexcept : box_(other.box_) {
        other.box_ = nullptr;
    }

    /* ── Copy assignment ── */
    AuroraSharedPtr& operator=(const AuroraSharedPtr& other) noexcept {
        if (this != &other) {
            release();
            box_ = other.box_;
            if (box_) aurora_refcount_inc(box_);
        }
        return *this;
    }

    /* ── Move assignment ── */
    AuroraSharedPtr& operator=(AuroraSharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            box_ = other.box_;
            other.box_ = nullptr;
        }
        return *this;
    }

    /* ── Destructor ── */
    ~AuroraSharedPtr() { release(); }

    /* ── Reset to null ── */
    void reset() { release(); box_ = nullptr; }

    /* ── Reset to new pointer ── */
    template<typename U = T>
    void reset(U* ptr, void (*deleter)(void*) = [](void* p) { delete static_cast<T*>(p); }) {
        release();
        box_ = ptr ? static_cast<SharedBox*>(aurora_shared_new(ptr, deleter)) : nullptr;
    }

    /* ── Dereference ── */
    T& operator*() const { return *get(); }
    T* operator->() const { return get(); }

    /* ── Raw access ── */
    T* get() const {
        return box_ ? static_cast<T*>(box_->data) : nullptr;
    }

    SharedBox* box() const { return box_; }

    /* ── Null check ── */
    explicit operator bool() const noexcept { return box_ != nullptr; }

    /* ── Ref-count ── */
    int64_t use_count() const {
        return box_ ? aurora_refcount_get(box_) : 0;
    }

    bool unique() const { return use_count() == 1; }

    /* ── Equality ── */
    bool operator==(const AuroraSharedPtr& other) const { return box_ == other.box_; }
    bool operator!=(const AuroraSharedPtr& other) const { return box_ != other.box_; }
    bool operator==(std::nullptr_t) const { return box_ == nullptr; }
    bool operator!=(std::nullptr_t) const { return box_ != nullptr; }

    /* ── Swap ── */
    void swap(AuroraSharedPtr& other) noexcept {
        SharedBox* tmp = box_;
        box_ = other.box_;
        other.box_ = tmp;
    }

private:
    SharedBox* box_;

    void release() {
        if (box_) {
            aurora_refcount_dec(box_);
        }
    }
};

template<typename T>
void swap(AuroraSharedPtr<T>& a, AuroraSharedPtr<T>& b) noexcept {
    a.swap(b);
}

/* ════════════════════════════════════════════════════════════
   AuroraWeakPtr<T> — Non-owning weak reference
   
   Holds a weak reference to a SharedBox. Lock to get a
   AuroraSharedPtr<T> if the object is still alive.
   ════════════════════════════════════════════════════════════ */

template<typename T>
class AuroraWeakPtr {
public:
    AuroraWeakPtr() noexcept : box_(nullptr) {}

    AuroraWeakPtr(const AuroraSharedPtr<T>& shared) noexcept
        : box_(shared.box())
    {
        if (box_) aurora_weak_new(box_);
    }

    AuroraWeakPtr(const AuroraWeakPtr& other) noexcept : box_(other.box_) {
        if (box_) aurora_weak_new(box_);
    }

    AuroraWeakPtr(AuroraWeakPtr&& other) noexcept : box_(other.box_) {
        other.box_ = nullptr;
    }

    AuroraWeakPtr& operator=(const AuroraWeakPtr& other) noexcept {
        if (this != &other) {
            release();
            box_ = other.box_;
            if (box_) aurora_weak_new(box_);
        }
        return *this;
    }

    AuroraWeakPtr& operator=(AuroraWeakPtr&& other) noexcept {
        if (this != &other) {
            release();
            box_ = other.box_;
            other.box_ = nullptr;
        }
        return *this;
    }

    ~AuroraWeakPtr() { release(); }

    void reset() { release(); box_ = nullptr; }

    /* ── Lock: get a shared pointer (null if expired) ── */
    AuroraSharedPtr<T> lock() const {
        if (!box_) return AuroraSharedPtr<T>();
        void* locked = aurora_weak_lock(box_);
        if (locked) {
            return AuroraSharedPtr<T>(static_cast<SharedBox*>(locked));
        }
        return AuroraSharedPtr<T>();
    }

    /* ── Check if the object is still alive ── */
    bool expired() const {
        if (!box_) return true;
        AuroraSharedPtr<T> tmp = lock();
        return !tmp;
    }

    int64_t use_count() const {
        if (!box_) return 0;
        auto tmp = lock();
        return tmp ? tmp.use_count() : 0;
    }

    bool operator==(const AuroraWeakPtr& other) const { return box_ == other.box_; }
    bool operator!=(const AuroraWeakPtr& other) const { return box_ != other.box_; }

private:
    SharedBox* box_;

    void release() {
        if (box_) aurora_weak_release(box_);
    }
};

/* ════════════════════════════════════════════════════════════
   CppSharedPtrBridge — std::shared_ptr <-> Aurora interop
   
   Provides RefCountBridgeVTable for std::shared_ptr ABI, allowing
   std::shared_ptr<T> objects to be passed through the cross-language
   ref-count bridge with automatic retain/release.
   ════════════════════════════════════════════════════════════ */

/* Control block layout for std::shared_ptr ABI interop.
   This matches the Itanium C++ ABI used by GCC/Clang and MSVC's
   shared_ptr internal structure (approximate). */
struct SharedPtrControlBlock {
    std::atomic<int64_t> shared_count{1};
    std::atomic<int64_t> weak_count{0};
    void* object{nullptr};
    std::function<void(void*)> deleter;
};

/* Wrap a raw pointer in a SharedPtrControlBlock, returning a
   control-block pointer compatible with std::shared_ptr ABI. */
inline SharedPtrControlBlock* cpp_shared_ptr_create(void* ptr, std::function<void(void*)> dtor) {
    if (!ptr) return nullptr;
    auto* cb = new SharedPtrControlBlock();
    cb->object = ptr;
    cb->deleter = std::move(dtor);
    return cb;
}

/* Retain: increment shared count, return object pointer */
inline void* cpp_shared_ptr_retain(void* control_block) {
    if (!control_block) return nullptr;
    auto* cb = static_cast<SharedPtrControlBlock*>(control_block);
    cb->shared_count.fetch_add(1, std::memory_order_relaxed);
    return cb->object;
}

/* Release: decrement shared count; if zero, run deleter and
   decrement weak count; if weak count also zero, delete block */
inline void cpp_shared_ptr_release(void* control_block) {
    if (!control_block) return;
    auto* cb = static_cast<SharedPtrControlBlock*>(control_block);
    int64_t prev = cb->shared_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
        if (cb->deleter && cb->object) {
            cb->deleter(cb->object);
        }
        cb->object = nullptr;
        if (cb->weak_count.load(std::memory_order_acquire) == 0) {
            delete cb;
        }
    }
}

/* Check if control block is still valid */
inline bool cpp_shared_ptr_is_valid(void* control_block) {
    if (!control_block) return false;
    auto* cb = static_cast<SharedPtrControlBlock*>(control_block);
    return cb->shared_count.load(std::memory_order_acquire) > 0;
}

/* Copy: retain and return a new control block pointer? No — std::shared_ptr
   copy just increments refcount on the same control block. */
inline void* cpp_shared_ptr_copy(void* control_block) {
    cpp_shared_ptr_retain(control_block);
    return control_block;
}

/* Build the RefCountBridgeVTable for CppSharedPtr protocol */
inline RefCountBridgeVTable cpp_shared_ptr_vtable() {
    RefCountBridgeVTable vt;
    vt.retain   = cpp_shared_ptr_retain;
    vt.release  = cpp_shared_ptr_release;
    vt.is_valid = cpp_shared_ptr_is_valid;
    vt.copy     = cpp_shared_ptr_copy;
    return vt;
}

/* ── Helper: convert AuroraSharedPtr<T> to std::shared_ptr<T> and back ── */

template<typename T>
std::shared_ptr<T> to_std_shared(AuroraSharedPtr<T>& aurora_ptr) {
    return std::shared_ptr<T>(aurora_ptr.get(), [box = aurora_ptr.box()](T*) {
        // Do nothing — AuroraSharedPtr owns the ref. This shared_ptr is a
        // temporary alias and must not outlive the original AuroraSharedPtr.
    });
}

template<typename T>
AuroraSharedPtr<T> from_std_shared(std::shared_ptr<T>& std_ptr) {
    if (!std_ptr) return AuroraSharedPtr<T>();
    // Create a control block wrapper for interop
    auto* cb = cpp_shared_ptr_create(
        static_cast<void*>(std_ptr.get()),
        [](void* p) { /* std::shared_ptr manages its own lifetime */ }
    );
    // Wrap in AuroraSharedPtr using SharedBox
    struct SharedBox* box = static_cast<struct SharedBox*>(
        aurora_shared_new(cb, [](void* p) {
            cpp_shared_ptr_release(p);
        })
    );
    return AuroraSharedPtr<T>(box);
}

/* ── Helper: make_shared ── */

template<typename T, typename... Args>
AuroraSharedPtr<T> aurora_make_shared(Args&&... args) {
    return AuroraSharedPtr<T>(new T(std::forward<Args>(args)...));
}
