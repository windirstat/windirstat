#pragma once
#include "afxwin.h"


// CStaticHyperlink

class CStaticHyperlink : public CStatic
{
	DECLARE_DYNAMIC(CStaticHyperlink)

public:
	CStaticHyperlink();
	virtual ~CStaticHyperlink();

protected:
	DECLARE_MESSAGE_MAP()
	virtual void PreSubclassWindow();
	bool m_bHovering;
	bool m_bShowTooltip;
	COLORREF m_clHovering;
	COLORREF m_clNormal;
	CFont m_fntHovering;
	CFont m_fntNormal;
	HCURSOR m_curHovering;
	CString m_url;
public:
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnStnClicked();
	void SetUrl(const CString &url);
	void SetText(const CString &text);
	CString GetUrl();
	CString GetText();
	void SetLinkColor(const COLORREF color);
	void SetHoverColor(const COLORREF color);
	COLORREF GetLinkColor();
	COLORREF GetHoverColor();
};


