#pragma once

#include <cstddef>
#include <utility>
#include <type_traits>

template <typename T>
class EnableSharedFromThis;

struct BaseBlock {
    size_t cnt = 1;
    virtual void DeleteObject() noexcept = 0;
    virtual ~BaseBlock() = default;
};

template <typename U>
struct ControlBlock : BaseBlock {
    U* ptr;
    explicit ControlBlock(U* p) noexcept : ptr(p) {
    }
    void DeleteObject() noexcept override {
        delete ptr;
    }
};

template <typename T>
class SharedPtr {
public:
    template <typename>
    friend class SharedPtr;

    template <typename U, typename... Args>
    friend SharedPtr<U> MakeShared(Args&&... args);

    SharedPtr() noexcept : ptr_(nullptr), block_(nullptr) {
    }
    SharedPtr(std::nullptr_t) noexcept : SharedPtr() {
    }

    template <typename U>
    explicit SharedPtr(U* p) : ptr_(static_cast<T*>(p)), block_(nullptr) {
        if (p) {
            block_ = new ControlBlock<U>(p);
            MakeEnableSharedFromThis(p);
        }
    }

    SharedPtr(const SharedPtr& other) noexcept : ptr_(other.ptr_), block_(other.block_) {
        if (block_) {
            ++block_->cnt;
        }
    }

    SharedPtr(SharedPtr&& other) noexcept : ptr_(other.ptr_), block_(other.block_) {
        other.ptr_ = nullptr;
        other.block_ = nullptr;
    }

    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other) noexcept : ptr_(other.ptr_), block_(other.block_) {
        if (block_) {
            ++block_->cnt;
        }
    }

    template <typename Y>
    SharedPtr(SharedPtr<Y>&& other) noexcept : ptr_(other.ptr_), block_(other.block_) {
        other.ptr_ = nullptr;
        other.block_ = nullptr;
    }

    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other, T* ptr) noexcept : ptr_(ptr), block_(other.block_) {
        if (block_) {
            ++block_->cnt;
        }
    }

    SharedPtr& operator=(const SharedPtr& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            block_ = other.block_;
            if (block_) {
                ++block_->cnt;
            }
        }
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            block_ = other.block_;
            other.ptr_ = nullptr;
            other.block_ = nullptr;
        }
        return *this;
    }

    template <typename Y>
    SharedPtr& operator=(const SharedPtr<Y>& other) noexcept {
        if (block_ != other.block_ || ptr_ != other.ptr_) {
            Reset();
            ptr_ = other.ptr_;
            block_ = other.block_;
            if (block_) {
                ++block_->cnt;
            }
        }
        return *this;
    }

    template <typename Y>
    SharedPtr& operator=(SharedPtr<Y>&& other) noexcept {
        if (block_ != other.block_ || ptr_ != other.ptr_) {
            Reset();
            ptr_ = other.ptr_;
            block_ = other.block_;
            other.ptr_ = nullptr;
            other.block_ = nullptr;
        }
        return *this;
    }

    ~SharedPtr() {
        Reset();
    }

    void Reset() noexcept {
        if (block_) {
            if (--block_->cnt == 0) {
                block_->DeleteObject();
                delete block_;
            }
        }
        ptr_ = nullptr;
        block_ = nullptr;
    }

    template <typename U>
    void Reset(U* p) {
        if (ptr_ != static_cast<T*>(p)) {
            Reset();
            block_ = new ControlBlock<U>(p);
            ptr_ = static_cast<T*>(p);
            MakeEnableSharedFromThis(p);
        }
    }

    void Swap(SharedPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(block_, other.block_);
    }

    T* Get() const noexcept {
        return ptr_;
    }
    T& operator*() const noexcept {
        return *ptr_;
    }
    T* operator->() const noexcept {
        return ptr_;
    }
    size_t UseCount() const noexcept {
        return block_ ? block_->cnt : 0;
    }
    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

private:
    template <typename Y>
    void MakeEnableSharedFromThis(Y* p) {
        if constexpr (std::is_base_of_v<EnableSharedFromThis<std::remove_cv_t<Y>>,
                                        std::remove_cv_t<Y>>) {
            if (p) {
                p->InternalSetWeakThis(*this);
            }
        }
    }

    T* ptr_;
    BaseBlock* block_;
};

template <typename T, typename U>
inline bool operator==(const SharedPtr<T>& left, const SharedPtr<U>& right) noexcept {
    return left.Get() == right.Get();
}

template <typename T, typename... Args>
SharedPtr<T> MakeShared(Args&&... args) {
    struct Control : BaseBlock {
        alignas(T) unsigned char storage[sizeof(T)];
        Control(Args&&... a) {
            new (storage) T(std::forward<Args>(a)...);
        }
        void DeleteObject() noexcept override {
            reinterpret_cast<T*>(storage)->~T();
        }
        T* GetObject() noexcept {
            return reinterpret_cast<T*>(storage);
        }
    };

    auto* ctrl = new Control(std::forward<Args>(args)...);
    SharedPtr<T> sp;
    sp.ptr_ = ctrl->GetObject();
    sp.block_ = ctrl;
    sp.MakeEnableSharedFromThis(sp.ptr_);
    return sp;
}

template <typename T>
class EnableSharedFromThis {
public:
    SharedPtr<T> SharedFromThis() {
        return weak_this_.Lock();
    }
    SharedPtr<const T> SharedFromThis() const {
        return weak_this_.Lock();
    }

private:
    friend class SharedPtr<T>;

    void InternalSetWeakThis(const SharedPtr<T>& sp) noexcept {
        weak_this_ = sp;
    }

    struct Weak {
        SharedPtr<T> ptr_;
        SharedPtr<T> Lock() const noexcept {
            return ptr_;
        }
        Weak& operator=(const SharedPtr<T>& sp) noexcept {
            ptr_ = sp;
            return *this;
        }
    };

    Weak weak_this_;
};
