// SmartPointer.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// This general purpose header is published under GPL with
// the friendly permission of D'accord (www.daccord.net).
//
// 08.09.1998 - JR
//

#pragma once
#include <functional>

//
// SmartPointer<>. Custom template for WinApi cleanup.
// This template does that in its destructor.
//
template <typename T>
class SmartPointer final
{
public:

    SmartPointer(const SmartPointer<T>&) = delete; // operator not allowed for SmartPointer
    T operator=(const SmartPointer<T>& lp) = delete; // operator not allowed for SmartPointer

    SmartPointer(std::function<void(T)> cleanup) : m_cleanup(cleanup), m_data(nullptr) {}
    SmartPointer(std::function<void(T)> cleanup, T data) : m_cleanup(cleanup), m_data(data) {}

    ~SmartPointer()
    {
        if (m_data != nullptr)
        {
            m_cleanup(m_data);
        }
    }

    SmartPointer(SmartPointer<T>&& src)
    {
        m_cleanup = src.m_cleanup;
        m_data = src.m_data;
        src.m_data = nullptr;
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
        if (m_data != nullptr)
        {
            m_cleanup(m_data);
        }
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
