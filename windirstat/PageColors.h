// pagecolors.h


#pragma once

#include "dirstatdoc.h" //SOptions

typedef CSet<CString, LPCSTR> CExtensionSet;
typedef  CIndexArray;

struct SColorGroup
{
	CString name;
	COLORREF color;
};

struct SOptions
{
	CExtensionColorMap extensionColorMap;
	CArray<SColorGroup, SColorGroup&> colorGroup;		// Group #0 = <unassigned>
	CMap<CString, LPCSTR, int, int> extension;		// extension -> colorGroup

	void ReadFromRegistry() {}
	void WriteToRegistry() {}
};


class CExtensionListControl: public CListCtrl
{
protected:
	enum
	{
		COL_EXTENSION,
		COL_DESCRIPTION
	};

	class CListItem
	{
	public:
		CListItem(LPCSTR extension);
		CString GetExtension();	
		CString GetText(int subitem);
		int GetImage();

	private:
		CString GetDescription();

		CString m_extension;
		CString m_description;
		int m_image;
	};

public:
	void Initialize();
	void AddExtensions(const CExtensionSet& extensions, bool select);
	void GetSelectedIndices(CArray<int, int>& selected);

protected:
	CListItem *GetListItem(int i);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
public:
	afx_msg void OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnDeleteitem(NMHDR *pNMHDR, LRESULT *pResult);
};

class CGroupListControl: public CListCtrl
{
public:
	void Initialize();
};


class CPageColors : public CPropertyPage
{
	DECLARE_DYNAMIC(CPageColors)

public:
	CPageColors();
	virtual ~CPageColors();

	void ReadOptions(SOptions *options);
	void WriteOptions(SOptions *options);

protected:
	// Dialog Data
	CArray<SColorGroup, SColorGroup&> m_colorGroup;		// Group #0 = <unassigned>
	CMap<CString, LPCSTR, int, int> m_extension;		// extension -> colorGroup

	enum { IDD = IDD_PAGE_COLORS };

	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
	CExtensionListControl m_extensionList;
	CExtensionListControl m_memberList;
	CGroupListControl m_groupList;
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnLvnBegindrag(NMHDR *pNMHDR, LRESULT *pResult);
};
