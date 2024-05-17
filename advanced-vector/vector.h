#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <iostream>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(std::move(other.GetAddress()))
        , capacity_(other.Capacity())
    {}

    RawMemory& operator=(RawMemory&& rhs) noexcept
    {
        buffer_ = std::move(rhs.GetAddress());
        capacity_ = rhs.Capacity();
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
class Vector
{

public:

    Vector() = default;

    Vector(Vector&& other) noexcept
    {
        *this = std::move(other);
    }

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept
    {
        if (size_ > 0)
            return &data_[0];
        else
            return nullptr;
    }

    iterator end() noexcept
    {
        return iterator{ begin() + size_ };
    }

    const_iterator begin() const noexcept
    {
        if (size_ > 0)
            return &data_[0];
        else
            return nullptr;
    }

    const_iterator end() const noexcept
    {
        return const_iterator{ begin() + size_ };
    }

    const_iterator cbegin() const noexcept
    {
        return begin();
    }

    const_iterator cend() const noexcept
    {
        return end();
    }


    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        size_t offset = pos - cbegin();

        if (Capacity() <= size_)
        {

            RawMemory<T> new_data(Capacity() == 0 ? 1 : (Capacity() * 2));

            new (new_data + offset) T(std::forward<Args>(args)...);


            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                try
                {
                    std::uninitialized_move_n(data_.GetAddress(), offset, new_data.GetAddress());
                    std::uninitialized_move_n(data_.GetAddress() + offset, std::distance(pos, cend()), new_data.GetAddress() + offset + 1);
                }
                catch (...)
                {
                    std::destroy_at(new_data.GetAddress() + offset);
                    new_data.~RawMemory();
                }
            }
            else
            {
                try
                {
                    std::uninitialized_copy_n(data_.GetAddress(), offset, new_data.GetAddress());
                    std::uninitialized_copy_n(data_.GetAddress() + offset, std::distance(pos, cend()), new_data.GetAddress() + offset);
                }
                catch (...)
                {
                    std::destroy_at(new_data.GetAddress() + offset);
                    new_data.~RawMemory();
                }
            }

            std::destroy_n(data_.GetAddress(), size_);

            data_.Swap(new_data);
        }
        else
        {
            T obj(std::forward<Args>(args)...);
            std::move_backward(begin() + offset, end(), begin() + size_ + 1);
            //std::destroy_at(data_.GetAddress() + offset);
            new (data_ + offset) T(std::forward<T>(obj));
        }

        ++size_;

        return &data_[offset];
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if constexpr (std::is_nothrow_move_assignable_v<T>)
        {
            for (iterator iter = begin() + std::distance(cbegin(), pos); iter != end() - 1; ++iter)
            {
                *iter = std::move(*std::next(iter, 1));
            }
            std::destroy_at(begin() + size_ - 1);
        }
        else
        {
            for (iterator iter = pos - begin(); iter != end() - 1; ++iter)
            {
                *iter = next(*iter, 1);
            }
            std::destroy_at(begin() + size_ - 1);
        }

        size_ -= 1;

        return &data_[pos - begin()];
    }

    iterator Insert(const_iterator pos, const T& value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value)
    {
        return Emplace(pos, std::move(value));
    }

    void Resize(size_t new_size)
    {
        if (new_size < size_)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);

        }
        else
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value)
    {
        EmplaceBack(std::move(value));
    }

    void PushBack(T&& value)
    {
        EmplaceBack(std::move(value));
    }

    void PopBack() /* noexcept */
    {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        size_ -= 1;
    }

    Vector& operator=(const Vector& rhs)
    {
        if (this != &rhs)
        {
            if (rhs.size_ > data_.Capacity())
            {
                Vector tmp(rhs);
                Swap(tmp);

            }
            else
            {
                size_t new_size = rhs.Size();

                if (new_size <= size_)
                {
                    std::copy_n(rhs.data_.GetAddress(), new_size, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
                }
                else
                {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, new_size - size_, data_.GetAddress() + size_);
                }
                size_ = new_size;
            }

        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept
    {
        if (this != &rhs)
        {
            data_.Swap(rhs.data_);
            std::swap(size_, rhs.size_);
        }

        return *this;
    }

    void Swap(Vector& other) noexcept
    {
        other.data_.Swap(data_);
        std::swap(other.size_, size_);
    }

    constexpr void MoveOrCopy(RawMemory<T>& new_data)
    {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity()) 
        {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        MoveOrCopy(new_data);
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept
    {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }


    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        if (Capacity() > size_)
        {
            new (data_ + size_) T(std::forward<Args>(args)...);

        }
        else
        {
            RawMemory<T> new_data(size_ == Capacity() ? (Capacity() == 0 ? 1 : (Capacity() * 2)) : Capacity()); // holy shit

            new (new_data + size_) T(std::forward<Args>(args)...);

            MoveOrCopy(new_data);
        }
        ++size_;

        return data_[size_ - 1];
    }

private:

    RawMemory<T> data_;
    size_t size_ = 0;
};
