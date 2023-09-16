#pragma once

#include <Windows.h>

// Inspired by OBS implementation but rewritten to be usable
class WinHandle {
public:
	inline void clear()
	{
		if (handle_ && handle_ != INVALID_HANDLE_VALUE)
			CloseHandle(handle_);
	}

	inline WinHandle() {}
	inline WinHandle(HANDLE handle) : handle_(handle) {}
	inline ~WinHandle() { clear(); }

	operator HANDLE() const { return handle_; }
	explicit operator bool() const
	{
		return handle_ && handle_ != INVALID_HANDLE_VALUE;
	}

	WinHandle &operator=(const WinHandle &) = delete;
	WinHandle(const WinHandle &) = delete;

	WinHandle &operator=(WinHandle &&o) noexcept
	{
		clear();
		handle_ = o.handle_;
		o.handle_ = INVALID_HANDLE_VALUE;
		return *this;
	}

	WinHandle(WinHandle &&o) noexcept : handle_(o.handle_)
	{
		o.handle_ = INVALID_HANDLE_VALUE;
	}

	inline HANDLE *operator&() { return &handle_; }

private:
	HANDLE handle_ = INVALID_HANDLE_VALUE;
};
