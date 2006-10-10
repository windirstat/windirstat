// cotaskmem.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
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
// $Id$

#ifndef _COTASKMEM_H
#define _COTASKMEM_H

// return type for 'CCoTaskMem<char *>::operator ->' is not a UDT or reference to a UDT.  Will produce errors if applied using infix notation
#pragma warning(disable: 4284) 

//
// CCoTaskMem<>. Some Windows APIs return memory which must be freed with CoTaskMemFree().
// This template does that in its destructor.
//
template<class T>
class CCoTaskMem
{
// construction
public:
	CCoTaskMem( T lp = 0 )
	{ p = lp; }
	CCoTaskMem(const CCoTaskMem<T>&) // operator not allowed for CCoTaskMem 
	{ _ASSERTE( 0 ); p = 0; }
	~CCoTaskMem()
	{ if( p ) CoTaskMemFree( p ); }

	operator T() { return p; }
	T& operator*() { _ASSERTE( p != NULL ); return p; }
	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the p member explicitly.
	T* operator&() 
	{ _ASSERTE(NULL == p); return &p; }
	T operator->()
	{ _ASSERTE(NULL != p); return p; }
	T operator= ( T lp ) 
	{ if(NULL != p) CoTaskMemFree(p); p = lp; return p;}
	T operator= ( const CCoTaskMem<T>& lp ) // operator not allowed for CCoTaskMem 
	{ _ASSERTE( 0 ); return p;}

#if _MSC_VER>1020
	bool operator!() { return (NULL == p); }
#else
	BOOL operator!() { return (NULL == p) ? TRUE : FALSE; }
#endif
	
	T p;
};

#endif

// $Log$
// Revision 1.6  2006/10/10 01:41:49  assarbad
// - Added credits for Gerben Wieringa (Dutch translation)
// - Replaced Header tag by Id for the CVS tags in the source files ...
// - Started re-ordering of the files inside the project(s)/solution(s)
//
// Revision 1.5  2006/07/04 22:49:18  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.4  2006/07/04 20:45:16  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.3  2004/11/05 16:53:05  assarbad
// Added Date and History tag where appropriate.
//
