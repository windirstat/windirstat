// layout.h	- Declaration of CLayout
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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

#pragma once

//
// CLayout. A poor men's dialog layout mechanism.
// Simple, flat, and sufficient for our purposes.
//
class CLayout
{
	struct SControlInfo
	{
		CWnd *control;
		double movex;
		double movey;
		double stretchx;
		double stretchy;

		CRect originalRectangle;
	};

	class CSizeGripper: public CWnd
	{
	public:
		static const int _width;

		CSizeGripper();
		void Create(CWnd *parent, CRect rc);

	private:
		void DrawShadowLine(CDC *pdc, CPoint start, CPoint end);

		DECLARE_MESSAGE_MAP()
		afx_msg void OnPaint();
		afx_msg UINT OnNcHitTest(CPoint point);
	};

public:
	CLayout(CWnd *dialog, LPCTSTR name);
	int AddControl(CWnd *control, double movex, double movey, double stretchx, double stretchy);
	void AddControl(UINT id, double movex, double movey, double stretchx, double stretchy);

	void OnInitDialog(bool centerWindow);
	void OnSize();
	void OnGetMinMaxInfo(MINMAXINFO *mmi);
	void OnDestroy();

protected:
	CWnd *m_dialog;
	CString m_name;
	CSize m_originalDialogSize;
	CArray<SControlInfo, SControlInfo&> m_control;
	CSizeGripper m_sizeGripper;
};
