// modalsendmail.cpp	- Implementation of CModalSendMail
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net
//
// Last modified: $Date$

#include "stdafx.h"
#include "windirstat.h"
#include "modalsendmail.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	CString GetMapiError(UINT r)
	{
		CString s;
		switch (r)
		{
		case MAPI_E_AMBIGUOUS_RECIPIENT: s= _T("A recipient matched more than one of the recipient descriptor structures and MAPI_DIALOG was not set. No message was sent."); break;
		case MAPI_E_ATTACHMENT_NOT_FOUND: s= _T("The specified attachment was not found. No message was sent."); break;
		case MAPI_E_ATTACHMENT_OPEN_FAILURE: s= _T("The specified attachment could not be opened. No message was sent."); break;
		case MAPI_E_BAD_RECIPTYPE: s= _T("The type of a recipient was not MAPI_TO, MAPI_CC, or MAPI_BCC. No message was sent."); break;
		case MAPI_E_FAILURE: s= _T("One or more unspecified errors occurred. No message was sent."); break;
		case MAPI_E_INSUFFICIENT_MEMORY: s= _T("There was insufficient memory to proceed. No message was sent."); break;
		case MAPI_E_INVALID_RECIPS: s= _T("One or more recipients were invalid or did not resolve to any address."); break;
		case MAPI_E_LOGIN_FAILURE: s= _T("There was no default logon, and the user failed to log on successfully when the logon dialog box was displayed. No message was sent."); break;
		case MAPI_E_TEXT_TOO_LARGE: s= _T("The text in the message was too large. No message was sent."); break;
		case MAPI_E_TOO_MANY_FILES: s= _T("There were too many file attachments. No message was sent."); break;
		case MAPI_E_TOO_MANY_RECIPIENTS: s= _T("There were too many recipients. No message was sent."); break;
		case MAPI_E_UNKNOWN_RECIPIENT: s= _T("A recipient did not appear in the address list. No message was sent."); break;
		case MAPI_E_USER_ABORT: s= _T("The user canceled one of the dialog boxes. No message was sent."); break;
		case SUCCESS_SUCCESS: s= _T("The call succeeded and the message was sent."); break;
		default:
			s.Format(_T("Unknown MAPI error code %u"), r);
			break;
		}
		return s;
	}
}

CModalSendMail::CModalSendMail()
{
}

// static 
bool CModalSendMail::IsSendMailAvailable()
{
	// This code is derived from the implementation of CDocument::OnUpdateFileSendMail()
	if (::GetProfileInt(_T("MAIL"), _T("MAPI"), 0) == 0)
		return false;
	
	if (!CMapi32Api::IsDllPresent())
		return false;

	return true;
}

bool CModalSendMail::SendMail(LPCTSTR recipient, LPCTSTR subject, LPCTSTR body)
{
	m_recipient= recipient;
	m_subject= subject;
	m_body= body;

	DoModal();

	return m_result == SUCCESS_SUCCESS;
}

void CModalSendMail::DoOperation()
{
	USES_CONVERSION;

	// This code is derived from the implementation of CDocument::OnFileSendMail()
	if (!IsSendMailAvailable())
		return;

	if (!m_api.IsSupported())
		return;

	CWaitCursor wc;

	// Call the MAPI-api
	MapiRecipDesc mrd;

	MapiMessage message;
	ZeroMemory(&message, sizeof(message));
	message.lpszSubject= T2A((LPTSTR)(LPCTSTR)m_subject);
    message.lpszNoteText= T2A((LPTSTR)(LPCTSTR)m_body);
	if (!m_recipient.IsEmpty())
	{
		message.nRecipCount= 1;
		message.lpRecips= &mrd;
		ZeroMemory(&mrd, sizeof(mrd));
		mrd.ulRecipClass= MAPI_TO;
		mrd.lpszName= T2A((LPTSTR)(LPCTSTR)m_recipient);
	}

	UINT r= m_api.MAPISendMail(0, (ULONG)AfxGetMainWnd()->m_hWnd, &message, MAPI_LOGON_UI|MAPI_DIALOG, 0);
	// This call ASSERTs in winocc.cpp, line 207, if, for example, an invalid recipient is entered,
	// and (in my case) Outlook Express displays a dialog.
	// The assertion can safely be ignored.
	// The original code (CDocument::OnFileSendMail()) asserts in the same way.

	if (r != SUCCESS_SUCCESS && r != MAPI_USER_ABORT && r != MAPI_E_LOGIN_FAILURE)
		AfxMessageBox(GetMapiError(r));

	m_result= r;
}


// $Log$
// Revision 1.4  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.3  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
