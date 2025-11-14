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
// This general purpose header is published under GPL with
// the friendly permission of D'accord (www.daccord.net).
//

#pragma once

#include <functional>
#include <memory>

//
// SmartPointer<>. Custom template for WinApi cleanup.
// This template does that in its destructor.
//
template <typename T>
class SmartPointer final
{
public:

    SmartPointer(const SmartPointer&) = delete; // operator not allowed for SmartPointer
    T operator=(const SmartPointer& lp) = delete; // operator not allowed for SmartPointer

    SmartPointer(std::function<void(T)> cleanup) : m_Cleanup(std::move(cleanup)), m_Data(nullptr) {}
    SmartPointer(std::function<void(T)> cleanup, T data) : m_Cleanup(std::move(cleanup)), m_Data(data) {}

    ~SmartPointer()
    {
        Cleanup();
    }

    SmartPointer(SmartPointer&& src) noexcept
    {
        m_Cleanup = src.m_Cleanup;
        m_Data = src.m_Data;
        src.m_Data = nullptr;
    }

    void Cleanup()
    {
        if (m_Data != nullptr)
        {
            m_Cleanup(m_Data);
        }
    }

    SmartPointer& operator=(SmartPointer&& src) noexcept
    {
        if (std::addressof(*this) != std::addressof(src))
        {
            Cleanup();
            m_Cleanup = src.m_Cleanup;
            m_Data = src.m_Data;
            src.m_Data = nullptr;
        }

        return *this;
    }

    void Release() noexcept
    {
        m_Data = nullptr;
    }

    operator T()
    {
        return m_Data;
    }

    T& operator*()
    {
        return m_Data;
    }

    T* operator&()
    {
        return &m_Data;
    }

    T operator->()
    {
        return m_Data;
    }

    T operator=(T lp)
    {
        Cleanup();
        m_Data = lp;
        return m_Data;
    }

    bool operator!()
    {
        return m_Data == nullptr;
    }

private:

    std::function<void(T)> m_Cleanup;
    T m_Data;
};
