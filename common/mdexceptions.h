// mdexceptions.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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
// This general purpose header is derived from a file
// created by www.daccord.net und published
// here under GPL with friendly permission of D'accord.
//
// Last modified: $Date$

// Md is just a prefix.

#ifndef MDEXCEPTIONS_H_INCLUDED
#define MDEXCEPTIONS_H_INCLUDED

#ifndef _INC_STDARG
#include <stdarg.h>
#endif

#pragma warning(disable: 4290) // C++ Exception Specification ignored

class CMdStringException: public CException
{
public:
	CMdStringException(LPCTSTR pszText)
		: m_sText(pszText) // pszText may be a MAKEINTRESOURCE
	{}
	virtual BOOL GetErrorMessage(LPTSTR lpszError, UINT nMaxError, PUINT pnHelpContext = NULL)
	{
		if (pnHelpContext != NULL)
			*pnHelpContext= 0;
		if (nMaxError != 0 && lpszError != NULL)
			lstrcpyn(lpszError, m_sText, nMaxError);
		return true;
	}
protected:
	CString m_sText;
};

inline CString MdGetExceptionMessage(CException *pe)
{
	CString s;
	BOOL b= pe->GetErrorMessage(s.GetBuffer(1024), 1024);
	s.ReleaseBuffer();

	if (!b)
		s= _T("(no error message available)");

	return s;
}

inline CString MdGetWinerrorText(HRESULT hr)
{
	CString sRet;	
	LPVOID lpMsgBuf;
	DWORD dw= FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 
		NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPTSTR) &lpMsgBuf, 0, NULL
	);
	if (dw == NULL)
	{
		CString s(MAKEINTRESOURCE(AFX_IDP_NO_ERROR_AVAILABLE));
		sRet.Format(_T("%s (0x%08lx)"), s, hr);
	}
	else 
	{ 
		sRet= (LPCTSTR)lpMsgBuf; 
		LocalFree(lpMsgBuf); 
	}
	return sRet;
}

inline void MdThrowStringException(UINT resId) throw (CMdStringException *)
{
	throw new CMdStringException(MAKEINTRESOURCE(resId));
}

inline void MdThrowStringException(LPCTSTR pszText) throw (CMdStringException *)
{
	throw new CMdStringException(pszText);
}

inline void __MdFormatStringExceptionV(CString& rsText, LPCTSTR pszFormat, va_list vlist)
{
	CString sFormat(pszFormat); // may be a MAKEINTRESOURCE
	rsText.FormatMessageV(sFormat, &vlist);
}

inline void AFX_CDECL MdThrowStringExceptionF(LPCTSTR pszFormat, ...)
{
	CString sText;

	va_list vlist;
	va_start(vlist, pszFormat);
	__MdFormatStringExceptionV(sText, pszFormat, vlist);
	va_end(vlist);

	MdThrowStringException(sText);
}

inline void MdThrowStringExceptionV(LPCTSTR pszFormat, va_list vlist)
{
	CString sText;
	__MdFormatStringExceptionV(sText, pszFormat, vlist);
	MdThrowStringException(sText);
}

inline void AFX_CDECL MdThrowStringExceptionF(UINT nResIdFormat, ...)
{
	CString sText;

	va_list vlist;
	va_start(vlist, nResIdFormat);
	__MdFormatStringExceptionV(sText, MAKEINTRESOURCE(nResIdFormat), vlist);
	va_end(vlist);

	MdThrowStringException(sText);
}

inline void MdThrowStringExceptionF(UINT nResIdFormat, va_list vlist)
{
	CString sText;
	__MdFormatStringExceptionV(sText, MAKEINTRESOURCE(nResIdFormat), vlist);
	MdThrowStringException(sText);
}

inline void MdThrowWinerror(DWORD dw, LPCTSTR pszPrefix =NULL) throw (CMdStringException *)
{
	CString sMsg= pszPrefix;
	sMsg+= _T(": ") + MdGetWinerrorText(dw);
	MdThrowStringException(sMsg);
}

inline void MdThrowHresult(HRESULT hr, LPCTSTR pszPrefix =NULL) throw (CMdStringException *)
{
	CString sMsg= pszPrefix;
	sMsg+= _T(": ") + MdGetWinerrorText(hr);
	MdThrowStringException(sMsg);
}


inline void MdThrowLastWinerror(LPCTSTR pszPrefix =NULL) throw (CMdStringException *)
{
	MdThrowWinerror(GetLastError(), pszPrefix);
}

inline void MdThrowFailed(HRESULT hr, LPCTSTR pszPrefix =NULL) throw (CMdStringException *)
{
	if (FAILED(hr))
		MdThrowHresult(hr, pszPrefix);
}

#endif

// $Log$
// Revision 1.3  2004/11/05 16:53:05  assarbad
// Added Date and History tag where appropriate.
//
