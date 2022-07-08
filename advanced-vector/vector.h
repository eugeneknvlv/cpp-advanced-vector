#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    RawMemory(RawMemory& other) = delete;
    RawMemory(const RawMemory& other) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(std::exchange(other.capacity_, 0))
    {}

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::exchange(rhs.buffer_, nullptr);
        capacity_ = std::exchange(rhs.capacity_, 0);
        return *this;
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + Size();
    }
    const_iterator begin() const noexcept {
        return const_cast<Vector&>(*this).begin();
    }
    const_iterator end() const noexcept {
        return const_cast<Vector&>(*this).end();
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    explicit Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0))
    {}

    Vector& operator=(const Vector& rhs) {
        if (this == &rhs) {
            return *this;
        }

        if (rhs.size_ > data_.Capacity()) {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        }
        else {
            if (rhs.size_ < size_) {
                std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
            }
            else {
                std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
            }
            size_ = rhs.size_;
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
        return *this;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        TransferDataSafely(data_.GetAddress(), size_, new_data.GetAddress());
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else {
            this->Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        if (size_ == this->Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(value);
            TransferDataSafely(data_.GetAddress(), size_, new_data.GetAddress());
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(value);
        }
        ++size_;
    }

    void PushBack(T&& value) {
        if (size_ == this->Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::move(value));
            TransferDataSafely(data_.GetAddress(), size_, new_data.GetAddress());
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            new (data_ + size_) T(std::move(value));
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* emplaced_value;
        if (size_ == this->Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            emplaced_value = new (new_data + size_) T(std::forward<Args>(args)...);
            TransferDataSafely(data_.GetAddress(), size_, new_data.GetAddress());
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            emplaced_value = new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *emplaced_value;
    }

    void PopBack() noexcept {
        std::destroy_at(data_.GetAddress() + (size_ - 1));
        --size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t offset = std::distance(cbegin(), pos);

        if (pos == end()) {
            EmplaceBack(std::forward<Args>(args)...);
            return begin() + offset;
        }

        if (size_ == this->Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + offset) T(std::forward<Args>(args)...);
            try {
                TransferDataSafely(begin(), offset, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_at(new_data.GetAddress() + offset);
                throw;
            }

            try {
                TransferDataSafely(begin() + offset, size_ - offset, new_data.GetAddress() + (offset + 1));
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress(), offset + 1);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);
        }
        else {
            new (end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(begin() + offset, end() - 1, end());
            data_[offset] = T(std::forward<Args>(args)...);
        }
        ++size_;
        return begin() + offset;
    }

    iterator Erase(const_iterator pos) {
        size_t offset = std::distance(cbegin(), pos);
        std::move(begin() + offset + 1, end(), begin() + offset);
        std::destroy_at(end() - 1);
        --size_;
        return const_cast<iterator>(pos);
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void TransferDataSafely(T* old_begin, size_t count, T* new_begin) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(old_begin, count, new_begin);
        }
        else {
            std::uninitialized_copy_n(old_begin, count, new_begin);
        }
    }
};

