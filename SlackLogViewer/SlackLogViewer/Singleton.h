#ifndef SINGLETON_H
#define SINGLETON_H

#include <memory>

template <class T>
class Singleton
{
public:

	Singleton() = default;

	Singleton(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	const Singleton& operator=(const Singleton&) = delete;
	Singleton& operator=(Singleton&&) = delete;

	static void Construct() { if (!msInstance) msInstance = std::make_unique<T>(); }
	static void Destroy() { msInstance.reset(); }

	static T* Get()
	{
		Construct();
		return msInstance.get();
	}

private:
	static std::unique_ptr<T> msInstance;
};

template <class T>
std::unique_ptr<T> Singleton<T>::msInstance = nullptr;

#endif