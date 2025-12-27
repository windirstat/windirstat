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
        Cleanup();
    }

    SmartPointer(SmartPointer&& src) noexcept
    {
        m_cleanup = src.m_cleanup;
        m_data = src.m_data;
        src.m_data = nullptr;
    }

    void Cleanup()
    {
        if (m_data != nullptr)
        {
            m_cleanup(m_data);
        }
    }

    SmartPointer& operator=(SmartPointer&& src) noexcept
    {
        if (std::addressof(*this) != std::addressof(src))
        {
            Cleanup();
            m_cleanup = src.m_cleanup;
            m_data = src.m_data;
            src.m_data = nullptr;
        }

        return *this;
    }

    void Release() noexcept
    {
        m_data = nullptr;
    }

    operator T()
    {
        return m_data;
    }

    T& operator*()
    {
        return m_data;
    }

    T* operator&()
    {
        return &m_data;
    }

    T operator->()
    {
        return m_data;
    }

    T operator=(T lp)
    {
        Cleanup();
        m_data = lp;
        return m_data;
    }

    bool operator!()
    {
        return m_data == nullptr;
    }

private:

    std::function<void(T)> m_cleanup;
    T m_data;
};
