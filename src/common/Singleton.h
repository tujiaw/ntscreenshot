#pragma once

template<typename T>
class Singleton {
public:
    static T* GetInstance() noexcept(std::is_nothrow_constructible<T>::value) {
        static T instance;
        return &instance;
    }
    virtual ~Singleton() noexcept  { }
    Singleton(const Singleton&) = delete;
    Singleton& operator =(const Singleton&) = delete;
protected:
    Singleton() {}

};
