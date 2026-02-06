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
// SmartPointer<>. Custom template for WinAPI resource cleanup.
// Automatically invokes the provided cleanup callable in its destructor.
//
template <typename T>
class SmartPointer final
{
public:

    SmartPointer(const SmartPointer&) = delete; // non-copyable
    T operator=(const SmartPointer& lp) = delete; // copy assignment forbidden

    SmartPointer(std::function<void(T)> cleanup) : m_cleanup(std::move(cleanup)), m_data(nullptr) {}
    SmartPointer(std::function<void(T)> cleanup, T data) : m_cleanup(std::move(cleanup)), m_data(data) {}

    ~SmartPointer()
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

    T operator=(T lp)
    {
        Release();
        m_data = lp;
        return m_data;
    }

    operator T() { return m_data; }
    T& operator*() { return m_data; }
    T* operator&() { return &m_data; }
    T operator->() { return m_data; }
    bool operator!() { return m_data == nullptr; }

private:

    std::function<void(T)> m_cleanup;
    T m_data;
};
