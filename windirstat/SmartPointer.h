// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

//
// SmartPointer<T, Cleanup>. Custom template for WinAPI resource cleanup.
// Automatically invokes the provided cleanup callable in its destructor.
//
// Cleanup is a second template parameter so the callable is stored directly
// with no std::function overhead. CTAD deduces both T and Cleanup from the
// constructor arguments, so most call sites need no explicit template args:
//   SmartPointer handle(CloseHandle, CreateFile(...));
//   SmartPointer pidl(CoTaskMemFree, pidlValue);
//
template <typename T, typename Cleanup = void(*)(T) noexcept>
class SmartPointer final
{
public:

    SmartPointer(const SmartPointer&) = delete; // non-copyable
    T operator=(const SmartPointer& lp) = delete; // copy assignment forbidden

    SmartPointer(Cleanup cleanup, T data = T{}) : m_cleanup(cleanup), m_data(data) {}

    ~SmartPointer() noexcept
    {
        Release();
    }

    SmartPointer(SmartPointer&& src) noexcept
    {
        m_cleanup = src.m_cleanup;
        m_data = src.m_data;
        src.m_data = nullptr;
    }

    SmartPointer& operator=(SmartPointer&& src) noexcept
    {
        if (std::addressof(*this) != std::addressof(src))
        {
            Release();
            m_cleanup = std::move(src.m_cleanup);
            m_data = src.m_data;
            src.m_data = nullptr;
        }

        return *this;
    }

    T Get() const noexcept
    {
        return m_data;
    }

    bool IsValid() const noexcept
    {
        return m_data != nullptr && m_data != INVALID_HANDLE_VALUE;
    }

    void Release() noexcept
    {
        if (IsValid())
        {
            m_cleanup(m_data);
            m_data = nullptr;
        }
    }

    T Detach() noexcept
    {
        const T data = m_data;
        m_data = nullptr;
        return data;
    }

    T operator=(T lp) noexcept
    {
        Release();
        m_data = lp;
        return m_data;
    }

    operator T() const noexcept { return m_data; }
    T& operator*() const noexcept { return m_data; }
    const T& operator*() noexcept { return m_data; }
    T* operator&() noexcept { return &m_data; }
    T operator->() const noexcept { return m_data; }
    bool operator!() const noexcept { return m_data == nullptr; }

private:

    Cleanup m_cleanup;
    T m_data;
};

// CTAD: SmartPointer(cleanup, data) deduces T from data, Cleanup from cleanup.
template <typename Cleanup, typename T>
SmartPointer(Cleanup, T) -> SmartPointer<T, Cleanup>;
