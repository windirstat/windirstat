// item.cpp	- Implementation of CItem
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

#include "stdafx.h"
#include "windirstat.h"
#include "dirstatdoc.h"	// GetItemColor()
#include "mainframe.h"
#include "item.h"

namespace
{
	CString GetFreeSpaceItemName() 	{ return LoadString(IDS_FREESPACE_ITEM); }
	CString GetUnknownItemName() { return LoadString(IDS_UNKNOWN_ITEM); }

	const SIZE sizeDeflatePacman = { 0, 2 };
}


DWORD CItem::_m_lastRamUsageUpdate;

CItem::CItem(ITEMTYPE type, LPCTSTR name, bool dontFollow)
	: m_type(type)
	, m_name(name)
	, m_size(0)
	, m_files(0)
	, m_subdirs(0)
	, m_done(false)
	, m_ticksWorked(0)
	, m_readJobs(0)
{
	if (GetType() == IT_FILE || dontFollow || GetType() == IT_FREESPACE || GetType() == IT_UNKNOWN || GetType() == IT_MYCOMPUTER)
	{
		SetReadJobDone();
		m_readJobs= 0;
	}
	else if (GetType() == IT_DIRECTORY || GetType() == IT_DRIVE || GetType() == IT_FILESFOLDER)
	{
		SetReadJobDone(false);
	}

	if (GetType() == IT_DRIVE)
	{
		m_name= FormatVolumeNameOfRootPath(m_name);
	}

	ZeroMemory(&m_lastChange, sizeof(m_lastChange));
}

CItem::~CItem()
{
	for (int i=0; i < m_children.GetSize(); i++) 
		delete m_children[i];
}

bool CItem::DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width) const
{
	if (subitem == COL_NAME)
	{
		return CTreeListItem::DrawSubitem(subitem, pdc, rc, state, width);
	}
	if (subitem != COL_SUBTREEPERCENTAGE)
		return false;

	bool showReadJobs= MustShowReadJobs();

	if (showReadJobs && !GetOptions()->IsPacmanAnimation())
		return false;

	if (showReadJobs && IsDone())
		return false;

	if (width != NULL)
	{
		*width= GetSubtreePercentageWidth();
		return true;
	}

	if (showReadJobs)
	{
		rc.DeflateRect(sizeDeflatePacman);
		DrawPacman(pdc, rc);
	}
	else
	{
		rc.DeflateRect(2, 3);
		for (int i=0; i < GetIndent(); i++)
			rc.left+= rc.Width() / 10;

		DrawPercentage(pdc, rc, GetFraction(), GetPercentageColor());
	}
	return true;
}

CString CItem::GetText(int subitem) const
{
	CString s;
	switch (subitem)
	{
	case COL_NAME:
		s= m_name;
		break;

	case COL_SUBTREEPERCENTAGE:
		if (IsDone())
		{
			ASSERT(m_readJobs == 0);
			//s= "ok";
		}
		else
		{
			if (m_readJobs == 1)
				s.LoadString(IDS_ONEREADJOB);
			else
				s.FormatMessage(IDS_sREADJOBS, FormatCount(m_readJobs));
		}
		break;

	case COL_PERCENTAGE:
		if (GetOptions()->IsShowTimeSpent() && MustShowReadJobs() || IsRootItem())
		{
			s.Format(_T("[%s s]"), FormatMilliseconds(GetTicksWorked()));
		}
		else
		{
			s.Format(_T("%s%%"), FormatDouble(GetFraction() * 100));
		}
		break;

	case COL_SUBTREETOTAL:
		s= FormatBytes(GetSize());
		break;

	case COL_ITEMS:
		if (GetType() != IT_FILE && GetType() != IT_FREESPACE && GetType() != IT_UNKNOWN)
			s= FormatCount(GetItemsCount());
		break;

	case COL_FILES:
		if (GetType() != IT_FILE && GetType() != IT_FREESPACE && GetType() != IT_UNKNOWN)
			s= FormatCount(GetFilesCount());
		break;

	case COL_SUBDIRS:
		if (GetType() != IT_FILE && GetType() != IT_FREESPACE && GetType() != IT_UNKNOWN)
			s= FormatCount(GetSubdirsCount());
		break;

	case COL_LASTCHANGE:
		if (GetType() != IT_FREESPACE && GetType() != IT_UNKNOWN)
		{
			s= FormatFileTime(m_lastChange);
		}
		break;

	default:
		ASSERT(0);
		break;
	}
	return s;
}

int CItem::CompareSibling(const CTreeListItem *tlib, int subitem) const
{ 
	CItem *other= (CItem *)tlib;

	int r=0;
	switch (subitem)
	{
	case COL_NAME:
		if (GetType() == IT_DRIVE)
		{
			ASSERT(other->GetType() == IT_DRIVE);
			r= GetPath().CompareNoCase(other->GetPath());
		}
		else
		{
			r= m_name.CompareNoCase(other->m_name);
		}
		break;

	case COL_SUBTREEPERCENTAGE:
		if (MustShowReadJobs())
			r= signum(m_readJobs - other->m_readJobs);
		else
			r= signum(GetFraction() - other->GetFraction());
		break;

	case COL_PERCENTAGE:
		r= signum(GetFraction() - other->GetFraction());
		break;

	case COL_SUBTREETOTAL:
		r= signum(GetSize() - other->GetSize());
		break;

	case COL_ITEMS:
		r= signum(GetItemsCount() - other->GetItemsCount());
		break;

	case COL_FILES:
		r= signum(GetFilesCount() - other->GetFilesCount());
		break;

	case COL_SUBDIRS:
		r= signum(GetSubdirsCount() - other->GetSubdirsCount());
		break;

	case COL_LASTCHANGE:
		{
			if (m_lastChange < other->m_lastChange)
				return -1;
			else if (m_lastChange == other->m_lastChange)
				return 0;
			else 
				return 1;
		}
		break;

	default:
		ASSERT(false);
		break;
	}
	return r;
}

int CItem::GetChildrenCount() const
{
	return m_children.GetSize();
}

CTreeListItem *CItem::GetTreeListChild(int i) const
{
	return m_children[i];
}

int CItem::GetImageToCache() const
{ 
	// (Caching is done in CTreeListItem::m_vi.)

	int image;

	if (GetType() == IT_MYCOMPUTER)
	{
		image= GetMyImageList()->GetMyComputerImage();
	}
	else if (GetType() == IT_FILESFOLDER)
	{
		image= GetMyImageList()->GetFilesFolderImage();
	}
	else if (GetType() == IT_FREESPACE)
	{
		image= GetMyImageList()->GetFreeSpaceImage();
	}
	else if (GetType() == IT_UNKNOWN)
	{
		image= GetMyImageList()->GetUnknownImage();
	}
	else
	{
		CString path= GetPath();

		if (GetType() == IT_DIRECTORY && GetApp()->IsMountPoint(path))
		{
			image= GetMyImageList()->GetMountPointImage();
		}
		else
		{
			image= GetMyImageList()->GetFileImage(path);
		}
	}
	return image; 
}

void CItem::DrawAdditionalState(CDC *pdc, const CRect& rcLabel) const
{
	if (!IsRootItem() && this == GetDocument()->GetZoomItem())
	{
		CRect rc= rcLabel;
		rc.InflateRect(1, 0);
		rc.bottom++;

		CSelectStockObject sobrush(pdc, NULL_BRUSH);
		CPen pen(PS_SOLID, 2, GetDocument()->GetZoomColor());
		CSelectObject sopen(pdc, &pen);

		pdc->Rectangle(rc);
	}
}

int CItem::GetSubtreePercentageWidth()
{
	return 105;
}

CItem *CItem::FindCommonAncestor(const CItem *item1, const CItem *item2)
{
	const CItem *parent= item1;
	while (!parent->IsAncestorOf(item2))
		parent= parent->GetParent();
	ASSERT(parent != NULL);
	return const_cast<CItem *>(parent);
}

bool CItem::IsAncestorOf(const CItem *item) const
{
	const CItem *p= item;
	while (p != NULL)
	{
		if (p == this)
			break;
		p= p->GetParent();
	}
	return (p != NULL);
}

LONGLONG CItem::GetProgressRange() const
{
	switch (GetType())
	{
	case IT_MYCOMPUTER:
		return GetProgressRangeMyComputer();

	case IT_DRIVE:
		return GetProgressRangeDrive();

	case IT_DIRECTORY:
	case IT_FILESFOLDER:
	case IT_FILE:
		return 0;

	case IT_FREESPACE:
	case IT_UNKNOWN:
	default:
		ASSERT(0);
		return 0;
	}
}

LONGLONG CItem::GetProgressPos() const
{
	switch (GetType())
	{
	case IT_MYCOMPUTER:
		return GetProgressPosMyComputer();

	case IT_DRIVE:
		return GetProgressPosDrive();

	case IT_DIRECTORY:
		return m_files + m_subdirs;

	case IT_FILE:
	case IT_FILESFOLDER:
	case IT_FREESPACE:
	case IT_UNKNOWN:
	default:
		ASSERT(0);
		return 0;
	}
}

const CItem *CItem::UpwardGetRoot() const
{
	if (GetParent() == NULL)
		return this;
	else
		return GetParent()->UpwardGetRoot();
}

void CItem::UpdateLastChange()
{
	ZeroMemory(&m_lastChange, sizeof(m_lastChange));
	if (GetType() == IT_DIRECTORY || GetType() == IT_FILE)
	{
		CString path= GetPath();

		int i= path.ReverseFind(_T('\\'));
		CString basename= path.Mid(i + 1);
		CString pattern;
		pattern.Format(_T("%s\\..\\%s"), path, basename);
		CFileFind finder;
		BOOL b= finder.FindFile(pattern);
		if (!b)
			return; // no chance
		finder.FindNextFile();
		finder.GetLastWriteTime(&m_lastChange);
	}
}

CItem *CItem::GetChild(int i) const
{
	return m_children[i];
}

CItem *CItem::GetParent() const
{ 
	return (CItem *)CTreeListItem::GetParent(); 
}

int CItem::FindChildIndex(const CItem *child) const
{
	for (int i=0; i < GetChildrenCount(); i++)
		if (child == m_children[i])
			return i;
	ASSERT(0);
	return 0;
}

void CItem::AddChild(CItem *child)
{
	ASSERT(!IsDone()); // SetDone() computed m_childrenBySize

	// This sequence is essential: First add numbers, then CTreeListControl::OnChildAdded(),
	// because the treelist will display it immediately.
	// If we did it the other way round, CItem::GetFraction() could ASSERT.
	UpwardAddSize(child->GetSize());
	UpwardAddReadJobs(child->GetReadJobs());
	UpwardUpdateLastChange(child->GetLastChange());

	m_children.Add(child); 
	child->SetParent(this); 

	GetTreeListControl()->OnChildAdded(this, child);
}

void CItem::RemoveChild(int i) 
{ 
	CItem *child= GetChild(i);
	m_children.RemoveAt(i); 
	GetTreeListControl()->OnChildRemoved(this, child);
	delete child; 
}

void CItem::RemoveAllChildren()
{
	GetTreeListControl()->OnRemovingAllChildren(this);

	for (int i=0; i < GetChildrenCount(); i++)
	{
		delete m_children[i];
	}
	m_children.SetSize(0);
}

void CItem::UpwardAddSubdirs(LONGLONG dirCount)
{
	m_subdirs+= dirCount;
	if (GetParent() != NULL)
		GetParent()->UpwardAddSubdirs(dirCount);
}

void CItem::UpwardAddFiles(LONGLONG fileCount)
{
	m_files+= fileCount;
	if (GetParent() != NULL)
		GetParent()->UpwardAddFiles(fileCount);
}

void CItem::UpwardAddSize(LONGLONG bytes)
{
	m_size+= bytes;
	if (GetParent() != NULL)
		GetParent()->UpwardAddSize(bytes);
}

void CItem::UpwardAddReadJobs(/* signed */LONGLONG count)
{
	m_readJobs+= count;
	if (GetParent() != NULL)
		GetParent()->UpwardAddReadJobs(count);
}

// This method increases the last change
void CItem::UpwardUpdateLastChange(const FILETIME& t)
{
	if (m_lastChange < t)
	{
		m_lastChange= t;
		if (GetParent() != NULL)
			GetParent()->UpwardUpdateLastChange(t);
	}
}

// This method may also decrease the last change
void CItem::UpwardRecalcLastChange()
{
	UpdateLastChange();

	for (int i=0; i < GetChildrenCount(); i++)
	{
		if (m_lastChange < GetChild(i)->GetLastChange())
			m_lastChange= GetChild(i)->GetLastChange();
	}
	if (GetParent() != NULL)
		GetParent()->UpwardRecalcLastChange();
}

LONGLONG CItem::GetSize() const
{
	return m_size;
}

void CItem::SetSize(LONGLONG ownSize)
{
	ASSERT(IsLeaf(GetType()));
	ASSERT(ownSize >= 0);
	m_size= ownSize;
}

LONGLONG CItem::GetReadJobs() const
{
	return m_readJobs;
}

FILETIME CItem::GetLastChange() const
{
	return m_lastChange;
}

void CItem::SetLastChange(const FILETIME& t)
{
	m_lastChange= t;
}

double CItem::GetFraction() const
{
	if (GetParent() == NULL)
		return 1.0;
	if (GetParent()->GetSize() == 0)
	{
		return 1.0;
	}
	return (double) GetSize() / GetParent()->GetSize();
}

ITEMTYPE CItem::GetType() const
{ 
	return (ITEMTYPE)(m_type & ~ITF_FLAGS); 
}

bool CItem::IsRootItem() const
{
	return ((m_type & ITF_ROOTITEM) != 0);
}

CString CItem::GetPath()  const
{ 
	CString path= UpwardGetPathWithoutBackslash();
	if (GetType() == IT_DRIVE || GetType() == IT_FILESFOLDER && GetParent()->GetType() == IT_DRIVE)
		path+= _T("\\");
	return path;
}

bool CItem::HasUncPath() const
{
	CString path= GetPath();
	return (path.GetLength() >= 2 && path.Left(2) == _T("\\\\"));
}

CString CItem::GetFindPattern() const
{
	CString pattern= GetPath();
	if (pattern.Right(1) != _T('\\'))
		pattern+= _T("\\");
	pattern+= _T("*.*");
	return pattern;
}

// Returns the path for "Explorer here" or "Command Prompt here"
CString CItem::GetFolderPath() const
{
	CString path;

	if (GetType() == IT_MYCOMPUTER)
	{
		path= GetParseNameOfMyComputer();
	}
	else
	{
		path= GetPath();
		if (GetType() == IT_FILE)
		{
			int i= path.ReverseFind(_T('\\'));
			ASSERT(i != -1);
			path= path.Left(i + 1);
		}
	}
	return path;
}

// returns the path for the mail-report
CString CItem::GetReportPath() const
{
	CString path= UpwardGetPathWithoutBackslash();
	if (GetType() == IT_DRIVE || GetType() == IT_FILESFOLDER)
		path+= _T("\\");
	if (GetType() == IT_FILESFOLDER
	|| GetType() == IT_FREESPACE
	|| GetType() == IT_UNKNOWN)
		path+= GetName();

	return path;
}

CString CItem::GetName() const
{
	return m_name;
}

CString CItem::GetExtension() const
{
	CString ext;

	switch (GetType())
	{
	case IT_FILE:
		{
			int i= GetName().ReverseFind(_T('.'));
			if (i == -1)
				ext= _T(".");
			else
				ext= GetName().Mid(i);
			ext.MakeLower();
			break;
		}
	case IT_FREESPACE:
	case IT_UNKNOWN:
		ext= GetName();
		break;

	default:
		ASSERT(0);
	}

	return ext;
}

LONGLONG CItem::GetFilesCount() const
{
	return m_files;
}

LONGLONG CItem::GetSubdirsCount() const
{
	return m_subdirs;
}

LONGLONG CItem::GetItemsCount() const
{
	return m_files + m_subdirs;
}

bool CItem::IsReadJobDone() const
{ 
	return m_readJobDone;
}

void CItem::SetReadJobDone(bool done) 
{ 
	if (!IsReadJobDone() && done)
	{
		UpwardAddReadJobs(-1);
	}
	else
	{
		UpwardAddReadJobs(1 - m_readJobs);
	}
	m_readJobDone= done;

}

bool CItem::IsDone() const
{ 
	return m_done; 
}

void CItem::SetDone() 
{ 
	if (m_done)
		return;

	if (GetType() == IT_DRIVE)
	{
		UpdateFreeSpaceItem();

		if (GetDocument()->OptionShowUnknown())
		{
			CItem *unknown= FindUnknownItem();

			LONGLONG total;
			LONGLONG free;
			MyGetDiskFreeSpace(GetPath(), total, free);
			
			LONGLONG unknownspace= total - GetSize();
			if (!GetDocument()->OptionShowFreeSpace())
				unknownspace-= free;

			// For CDs, the GetDiskFreeSpaceEx()-function is not correct.
			if (unknownspace < 0)
			{
				TRACE(_T("GetDiskFreeSpace(%s) incorrect.\n"), GetPath());
				unknownspace= 0;
			}
			unknown->SetSize(unknownspace);

			UpwardAddSize(unknownspace);
		}
	}

	for (int i=0; i < GetChildrenCount(); i++)
		ASSERT(GetChild(i)->IsDone());

	//m_children.FreeExtra(); // Doesn't help much.
	qsort(m_children.GetData(), m_children.GetSize(), sizeof(CItem *), &_compareBySize);

	m_rect.SetRectEmpty();

	m_done= true;
}

DWORD CItem::GetTicksWorked() const
{ 
	return m_ticksWorked; 
}

void CItem::AddTicksWorked(DWORD more) 
{ 
	m_ticksWorked+= more; 
}

void CItem::DoSomeWork(DWORD ticks)
{
	if (IsDone())
		return;

	StartPacman(true);

	DriveVisualUpdateDuringWork();

	DWORD start= GetTickCount();

	if (GetType() == IT_DRIVE || GetType() == IT_DIRECTORY)
	{
		if (!IsReadJobDone())
		{
			LONGLONG dirCount= 0;
			LONGLONG fileCount= 0;

			CList<FILEINFO, FILEINFO> files;


			CFileFind finder;
			BOOL b= finder.FindFile(GetFindPattern());
			while (b)
			{
				DriveVisualUpdateDuringWork();

				b= finder.FindNextFile();
				if (finder.IsDots())
					continue;
				if (finder.IsDirectory())
				{
					dirCount++;
					AddDirectory(finder);
				}
				else
				{
					fileCount++;

					FILEINFO fi;
					fi.name= finder.GetFileName();
					fi.length= finder.GetLength();
					finder.GetLastWriteTime(&fi.lastWriteTime);
					// (We don't use GetLastWriteTime(CTime&) here, because, if the file has
					// an invalid timestamp, that function would ASSERT and throw an Exception.)

					files.AddTail(fi);
				}
			}

			CItem *filesFolder= 0;
			if (dirCount > 0 && fileCount > 1)
			{
				filesFolder= new CItem(IT_FILESFOLDER, LoadString(IDS_FILES_ITEM));
				filesFolder->SetReadJobDone();
				AddChild(filesFolder);
			}
			else if (fileCount > 0)
			{
				filesFolder= this;
			}

			for (POSITION pos=files.GetHeadPosition(); pos != NULL; files.GetNext(pos))
			{
				const FILEINFO& fi= files.GetAt(pos);
				filesFolder->AddFile(fi);
			}

			if (filesFolder != NULL)
			{
				filesFolder->UpwardAddFiles(fileCount);
				if (dirCount > 0 && fileCount > 1)
					filesFolder->SetDone();
			}

			UpwardAddSubdirs(dirCount);
			SetReadJobDone();
			AddTicksWorked(GetTickCount() - start);
		}
		if (GetType() == IT_DRIVE)
			UpdateFreeSpaceItem();

		if (GetTickCount() - start > ticks)
		{
			StartPacman(false);
			return;
		}
	}
	if (GetType() == IT_DRIVE || GetType() == IT_DIRECTORY || GetType() == IT_MYCOMPUTER)
	{
		ASSERT(IsReadJobDone());
		if (IsDone())
		{
			StartPacman(false);
			return;
		}
		if (GetChildrenCount() == 0)
		{
			SetDone();
			StartPacman(false);
			return;
		}

		DWORD startChildren= GetTickCount();
		while (GetTickCount() - start < ticks)
		{
			DWORD minticks= UINT_MAX;
			CItem *minchild= NULL;
			for (int i=0; i < GetChildrenCount(); i++)
			{
				CItem *child= GetChild(i);
				if (child->IsDone())
					continue;
				if (child->GetTicksWorked() < minticks)
				{
					minticks= child->GetTicksWorked();
					minchild= child;
				}
			}
			if (minchild == NULL)
			{
				SetDone();
				break;
			}
			DWORD tickssofar= GetTickCount() - start;
			if (ticks > tickssofar)
				minchild->DoSomeWork(ticks - tickssofar);
		}
		AddTicksWorked(GetTickCount() - startChildren);
	}
	else
	{
		SetDone();
	}
	StartPacman(false);
}

// Return: false if deleted
bool CItem::StartRefresh() 
{
	ASSERT(GetType() != IT_FREESPACE);
	ASSERT(GetType() != IT_UNKNOWN);

	m_ticksWorked= 0;

	// Special case IT_MYCOMPUTER
	if (GetType() == IT_MYCOMPUTER)
	{
		ZeroMemory(&m_lastChange, sizeof(m_lastChange));

		for (int i=0; i < GetChildrenCount(); i++)
			GetChild(i)->StartRefresh();

		return true;
	}
	ASSERT(GetType() == IT_FILE || GetType() == IT_DRIVE || GetType() == IT_DIRECTORY || GetType() == IT_FILESFOLDER);

	bool wasExpanded= IsVisible() && IsExpanded();
	int oldScrollPosition =0;
	if (IsVisible())
		oldScrollPosition= GetScrollPosition();

	UncacheImage();

	// Upward clear data
	UpdateLastChange();

	UpwardSetUndone();

	UpwardAddReadJobs(-GetReadJobs());
	ASSERT(GetReadJobs() == 0);

	if (GetType() == IT_FILE)
		GetParent()->UpwardAddFiles(-1);
	else
		UpwardAddFiles(-GetFilesCount());
	ASSERT(GetFilesCount() == 0);

	if (GetType() == IT_DIRECTORY || GetType() == IT_DRIVE)
		UpwardAddSubdirs(-GetSubdirsCount());
	ASSERT(GetSubdirsCount() == 0);

	UpwardAddSize(-GetSize());
	ASSERT(GetSize() == 0);

	RemoveAllChildren();
	UpwardRecalcLastChange();

	// Special case IT_FILESFOLDER
	if (GetType() == IT_FILESFOLDER)
	{
		CFileFind finder;
		BOOL b= finder.FindFile(GetFindPattern());
		while (b)
		{
			b= finder.FindNextFile();
			if (finder.IsDirectory())
				continue;

			FILEINFO fi;
			fi.name= finder.GetFileName();
			fi.length= finder.GetLength();
			finder.GetLastWriteTime(&fi.lastWriteTime);

			AddFile(fi);
			UpwardAddFiles(1);
		}
		SetDone();

		if (wasExpanded)
			GetTreeListControl()->ExpandItem(this);
		return true;
	}
	ASSERT(GetType() == IT_FILE || GetType() == IT_DRIVE || GetType() == IT_DIRECTORY);

	// The item may have been deleted.
	bool deleted= false;
	if (GetType() == IT_DRIVE)
		deleted= !DriveExists(GetPath());
	else if (GetType() == IT_FILE)
		deleted= !FileExists(GetPath());
	else if (GetType() == IT_DIRECTORY)
		deleted= !FolderExists(GetPath());

	if (deleted)
	{
		if (GetParent() == NULL)
		{
			GetDocument()->UnlinkRoot();
		}
		else
		{
			GetParent()->UpwardRecalcLastChange();
			GetParent()->RemoveChild(GetParent()->FindChildIndex(this)); // --> delete this
		}
		return false;
	}

	// Case IT_FILE
	if (GetType() == IT_FILE)
	{
		CFileFind finder;
		BOOL b= finder.FindFile(GetPath());
		if (b)
		{
			finder.FindNextFile();
			if (!finder.IsDirectory())
			{
				FILEINFO fi;
				fi.name= finder.GetFileName();
				fi.length= finder.GetLength();
				finder.GetLastWriteTime(&fi.lastWriteTime);

				SetLastChange(fi.lastWriteTime);

				UpwardAddSize(fi.length);
				UpwardUpdateLastChange(GetLastChange());
				GetParent()->UpwardAddFiles(1);
			}
		}
		SetDone();
		return true;
	}

	ASSERT(GetType() == IT_DRIVE || GetType() == IT_DIRECTORY);

	if (GetType() == IT_DIRECTORY && !IsRootItem() && GetApp()->IsMountPoint(GetPath()) && !GetOptions()->IsFollowMountPoints())
		return true;

	// Initiate re-read
	SetReadJobDone(false);

	// Re-create <free space> and <unknown>
	if (GetType() == IT_DRIVE)
	{
		if (GetDocument()->OptionShowFreeSpace())
			CreateFreeSpaceItem();
		if (GetDocument()->OptionShowUnknown())
			CreateUnknownItem();
	}

	DoSomeWork(0);

	if (wasExpanded)
		GetTreeListControl()->ExpandItem(this);

	if (IsVisible())
		SetScrollPosition(oldScrollPosition);

	return true;
}

void CItem::UpwardSetUndone()
{
	if (GetType() == IT_DRIVE && IsDone() && GetDocument()->OptionShowUnknown())
	{
		for (int i=0; i < GetChildrenCount(); i++)
			if (GetChild(i)->GetType() == IT_UNKNOWN)
				break;
		CItem *unknown= GetChild(i);

		UpwardAddSize(- unknown->GetSize());

		unknown->SetSize(0);
	}

	m_done= false; 

	if (GetParent() != NULL)
		GetParent()->UpwardSetUndone();
}

void CItem::RefreshRecycler()
{
	ASSERT(GetType() == IT_DRIVE);
	DWORD dummy;
	CString system;
	BOOL b= GetVolumeInformation(GetPath(), NULL, 0, NULL, &dummy, &dummy, system.GetBuffer(128), 128);
	system.ReleaseBuffer();
	if (!b)
	{
		TRACE(_T("GetVolumeInformation(%s) failed.\n"), GetPath());
		return; // nix zu machen
	}

	CString recycler;
	if (system.CompareNoCase(_T("NTFS")) == 0)
	{
		recycler= _T("recycler");
	}
	else if (system.CompareNoCase(_T("FAT32")) == 0)
	{
		recycler= _T("recycled");
	}
	else
	{
		TRACE(_T("%s: unknown file system type %s\n"), GetPath(), system);
		return; // nix zu machen.
	}

	for (int i=0; i < GetChildrenCount(); i++)
	{
		if (GetChild(i)->GetName().CompareNoCase(recycler) == 0)
			break;
	}
	if (i >= GetChildrenCount())
	{
		TRACE(_T("%s: Recycler(%s) not found.\n"), GetPath(), recycler);
		return; // nicht gefunden
	}

	GetChild(i)->StartRefresh();
}

void CItem::CreateFreeSpaceItem()
{
	ASSERT(GetType() == IT_DRIVE);

	UpwardSetUndone();

	LONGLONG total;
	LONGLONG free;
	MyGetDiskFreeSpace(GetPath(), total, free);

	CItem *freespace= new CItem(IT_FREESPACE, GetFreeSpaceItemName());
	freespace->SetSize(free);
	freespace->SetDone();

	AddChild(freespace);
}

CItem *CItem::FindFreeSpaceItem() const
{
	int i= FindFreeSpaceItemIndex();
	if (i < GetChildrenCount())
		return GetChild(i);
	else
		return NULL;
}


void CItem::UpdateFreeSpaceItem()
{
	ASSERT(GetType() == IT_DRIVE);

	if (!GetDocument()->OptionShowFreeSpace())
		return;

	CItem *freeSpaceItem= FindFreeSpaceItem();
	ASSERT(freeSpaceItem != NULL);

	LONGLONG total;
	LONGLONG free;
	MyGetDiskFreeSpace(GetPath(), total, free);

	LONGLONG before= freeSpaceItem->GetSize();
	LONGLONG diff= free - before;

	freeSpaceItem->UpwardAddSize(diff);
	
	ASSERT(freeSpaceItem->GetSize() == free);
}

void CItem::RemoveFreeSpaceItem()
{
	ASSERT(GetType() == IT_DRIVE);

	UpwardSetUndone();

	int i= FindFreeSpaceItemIndex();
	ASSERT(i < GetChildrenCount());

	CItem *freespace= GetChild(i);

	UpwardAddSize(-freespace->GetSize());

	RemoveChild(i);
}

void CItem::CreateUnknownItem()
{
	ASSERT(GetType() == IT_DRIVE);

	UpwardSetUndone();

	CItem *unknown= new CItem(IT_UNKNOWN, GetUnknownItemName());
	unknown->SetDone();

	AddChild(unknown);
}

CItem *CItem::FindUnknownItem() const
{
	int i= FindUnknownItemIndex();
	if (i < GetChildrenCount())
		return GetChild(i);
	else
		return NULL;
}

void CItem::RemoveUnknownItem()
{
	ASSERT(GetType() == IT_DRIVE);

	UpwardSetUndone();

	int i= FindUnknownItemIndex();
	ASSERT(i < GetChildrenCount());

	CItem *unknown= GetChild(i);

	UpwardAddSize(-unknown->GetSize());

	RemoveChild(i);
}

CItem *CItem::FindDirectoryByPath(const CString& path)
{
	CString myPath= GetPath();
	myPath.MakeLower();

	int i=0;
	while (i < myPath.GetLength() && i < path.GetLength() && myPath[i] == path[i])
		i++;

	if (i < myPath.GetLength())
		return NULL;

	if (i >= path.GetLength())
	{
		ASSERT(myPath == path);
		return this;
	}

	for (i=0; i < GetChildrenCount(); i++)
	{
		CItem *item= GetChild(i)->FindDirectoryByPath(path);
		if (item != NULL)
			return item;
	}

	return NULL;
}

void CItem::RecurseCollectExtensionData(CExtensionData *ed)
{
	UpdateRamUsage();

	if (IsLeaf(GetType()))
	{
		if (GetType() == IT_FILE)
		{
			CString ext= GetExtension();
			SExtensionRecord r;
			if (ed->Lookup(ext, r))
			{
				r.bytes+= GetSize();
				r.files++;
			}
			else
			{
				r.bytes= GetSize();
				r.files= 1;
			}
			ed->SetAt(ext, r);
		}
	}
	else
	{
		for (int i=0; i < GetChildrenCount(); i++)
		{
			GetChild(i)->RecurseCollectExtensionData(ed);
		}
	}
}

void CItem::DrawGraph(CDC *pdc, CRect rc) const
{
	//pdc->FillSolidRect(rc, RGB(255,0,0));	// This is a test. There shouldn't be any leaks

/*	DWORD start= GetTickCount(); */

	// Pen for grid. This pen remains selected during the recursive drawing.
	CPen pen(PS_SOLID, 1, GetOptions()->GetTreemapGridColor());
	CSelectObject sopen(pdc, &pen);

	if (GetOptions()->IsTreemapGrid())
	{
		pdc->FillSolidRect(rc, GetOptions()->GetTreemapGridColor());
	}

	double surface[4];
	for (int i=0; i < 4; i++)
		surface[i]= 0;

	RecurseDrawGraph(pdc, rc, true, surface, GetOptions()->GetHeightFactor()/100.0, GetOptions()->GetScaleFactor() / 100.0);

/*	DWORD time= GetTickCount() - start;
	CString s;
	s.Format("%d ms", time);
	AfxMessageBox(s);
*/

#ifdef _STRONG_DEBUG
	int bottom= rc.bottom;
	int right= rc.right;
	if (GetOptions()->IsTreemapGrid())
	{
		bottom--;
		right--;
	}
	for (int y=rc.top; y < bottom; y++)
	for (int x=rc.left; x < right; x++)
		ASSERT(NULL != FindChildByPoint(CPoint(x, y)));
#endif
}

CRect CItem::GetRect() const
{
	return m_rect;
}

const CItem *CItem::FindChildByPoint(CPoint point) const
{
	ASSERT(IsDone());

	const CItem *item= NULL;

	if (m_rect.PtInRect(point))
	{
		for (int i=0; i < GetChildrenCount(); i++)
		{
			if (GetChild(i)->GetRect().PtInRect(point))
			{
				item= GetChild(i)->FindChildByPoint(point);
				break;
			}
		}
		if (item == NULL)
		{
			item= this;
		}
	}
	return item;
}

int __cdecl CItem::_compareBySize(const void *p1, const void *p2)
{
	CItem *item1= *(CItem **)p1;
	CItem *item2= *(CItem **)p2;

	LONGLONG size1= item1->GetSize();
	LONGLONG size2= item2->GetSize();

	// TODO: Use 2nd sort column (as set in our TreeListView?)

	return signum(size2 - size1); // biggest first
}

LONGLONG CItem::GetProgressRangeMyComputer() const
{
	ASSERT(GetType() == IT_MYCOMPUTER);

	LONGLONG range= 0;
	for (int i=0; i < GetChildrenCount(); i++)
	{
		range+= GetChild(i)->GetProgressRangeDrive();
	}
	return range;
}

LONGLONG CItem::GetProgressPosMyComputer() const
{
	ASSERT(GetType() == IT_MYCOMPUTER);

	LONGLONG pos= 0;
	for (int i=0; i < GetChildrenCount(); i++)
	{
		pos+= GetChild(i)->GetProgressPosDrive();
	}
	return pos;
}

LONGLONG CItem::GetProgressRangeDrive() const
{
	LONGLONG total;
	LONGLONG free;
	MyGetDiskFreeSpace(GetPath(), total, free);

	LONGLONG range= total - free;

	ASSERT(range >= 0);
	return range;
}

LONGLONG CItem::GetProgressPosDrive() const
{
	LONGLONG pos= GetSize();

	CItem *fs= FindFreeSpaceItem();
	if (fs != NULL)
	{
		pos-= fs->GetSize();
	}

	return pos;
}

void CItem::RecurseDrawGraph(
	CDC *pdc,			// Device Context
	const CRect& rc,	// Rectangle, where to draw item
	bool asroot,		// If we are the root (zoom) item
	const double *psurface, // Coefficients so far
	double h,				// Height factor
	double f				// Scale factor
) const
{
	ASSERT(IsDone());
	ASSERT(rc.Width() >= 0);
	ASSERT(rc.Height() >= 0);

	// The application may have been paged out by the system.
	// When the user tops it again an manipulates, for example, the splitter,
	// then the treemap is repainted and so everything must be paged in again.
	// That can last a long time (30 sec).
	// In order to give some visual feedback, we periodically update the RAM usage indicator.
	UpdateRamUsage();

	m_rect= rc;

	int gridWidth= GetOptions()->IsTreemapGrid() ? 1 : 0;

	if (m_rect.Width() <= gridWidth || m_rect.Height() <= gridWidth)
	{
		return;
	}

	double surface[4];

	if (GetOptions()->IsCushionShading())
	{
		for (int i=0; i < 4; i++)
			surface[i]= psurface[i];

		if (!asroot)
			AddRidge(m_rect, surface, h);
	}

	if (GetChildrenCount() > 0)
	{
		CArray<double, double> rows;	// Our rectangle is divided into rows, each of which gets this height (fraction of total height).
		CArray<int, int> childrenPerRow;// childrenPerRow[i] = # of children in rows[i]

		CArray<double, double> childWidth; // Widths of the children (fraction of row width).
		childWidth.SetSize(GetChildrenCount());

		bool horizontalRows= ArrangeChildren(childWidth, rows, childrenPerRow);

		const int width= horizontalRows ? rc.Width() : rc.Height();
		const int height= horizontalRows ? rc.Height() : rc.Width();
		ASSERT(width >= 0);
		ASSERT(height >= 0);

		int c = 0;
		double top= horizontalRows ? rc.top : rc.left;
		for (int row=0; row < rows.GetSize(); row++)
		{
			double fBottom= top + rows[row] * height;
			int bottom= (int)fBottom;
			if (row == rows.GetSize() - 1)
				bottom= horizontalRows ? rc.bottom : rc.right;
			double left= horizontalRows ? rc.left : rc.top;
			for (int i=0; i < childrenPerRow[row]; i++, c++)
			{
				CItem *child= GetChild(c);
				ASSERT(child->IsDone());
				ASSERT(childWidth[c] >= 0);
				double fRight= left + childWidth[c] * width;
				int right= (int)fRight;
				if (i == childrenPerRow[row] - 1)
					right= horizontalRows ? rc.right : rc.bottom;

				CRect rcChild;
				if (horizontalRows)
				{
					rcChild.left= (int)left;
					rcChild.right= right;
					rcChild.top= (int)top;
					rcChild.bottom= bottom;
				}
				else
				{
					rcChild.left= (int)top;
					rcChild.right= bottom;
					rcChild.top= (int)left;
					rcChild.bottom= right;
				}

				#ifdef _DEBUG
				if (rcChild.Width() > 0 && rcChild.Height() > 0)
				{
					CRect test;
					test.IntersectRect(m_rect, rcChild);
					ASSERT(test == rcChild);
				}
				#endif			
				
				child->RecurseDrawGraph(pdc, rcChild, false, surface, h * f, f);
				left= fRight;
			}
			// This asserts due to rounding error: ASSERT(left == (horizontalRows ? rc.right : rc.bottom));
			top= fBottom;
		}
		// This asserts due to rounding error: ASSERT(top == (horizontalRows ? rc.bottom : rc.right));
	}
	else
	{
		RenderLeaf(pdc, surface);
	}
}

// return: whether the rows are horizontal.
bool CItem::ArrangeChildren(
	CArray<double, double>& childWidth,
	CArray<double, double>& rows,
    CArray<int, int>& childrenPerRow
) const
{
	ASSERT(IsDone());
	ASSERT(GetChildrenCount() > 0);

	if (GetSize() == 0)
	{
		rows.Add(1.0);
		childrenPerRow.Add(GetChildrenCount());
		for (int i=0; i < GetChildrenCount(); i++)
			childWidth[i]= 1.0 / GetChildrenCount();
		return true;
	}

	bool horizontalRows= (m_rect.Width() >= m_rect.Height());

	double width= 1.0;
	if (horizontalRows)
	{
		if (m_rect.Height() > 0)
			width= (double)m_rect.Width() / m_rect.Height();
	}
	else
	{
		if (m_rect.Width() > 0)
			width= (double)m_rect.Height() / m_rect.Width();
	}

	int nextChild= 0;
	while (nextChild < GetChildrenCount())
	{
		int childrenUsed;
		rows.Add(CalcutateNextRow(nextChild, width, childrenUsed, childWidth));
		childrenPerRow.Add(childrenUsed);
		nextChild+= childrenUsed;
	}

	return horizontalRows;
}

double CItem::CalcutateNextRow(const int nextChild, double width, int& childrenUsed, CArray<double, double>& childWidth) const
{
	ASSERT(IsDone());

	static const double _minProportion = 0.4;
	ASSERT(_minProportion < 1);

	ASSERT(nextChild < GetChildrenCount());
	ASSERT(width >= 1.0);

	const double mySize= (double)GetSize();
	ASSERT(mySize > 0);
	LONGLONG sizeUsed= 0;
	double rowHeight= 0;

	for (int i=nextChild; i < GetChildrenCount(); i++)
	{
		LONGLONG childSize= GetChild(i)->GetSize();
		if (childSize == 0)
		{
			ASSERT(i > nextChild);	// first child has size > 0
			break;
		}

		sizeUsed+= childSize;
		double virtualRowHeight= sizeUsed / mySize;
		ASSERT(virtualRowHeight > 0);
		ASSERT(virtualRowHeight <= 1);
		
		// Rectangle(mySize)    = width * 1.0
		// Rectangle(childSize) = childWidth * virtualRowHeight
		// Rectangle(childSize) = childSize / mySize * width

		double childWidth= childSize / mySize * width / virtualRowHeight;

		if (childWidth / virtualRowHeight < _minProportion)
		{
			ASSERT(i > nextChild); // because width >= 1 and _minProportion < 1.
			// For the first child we have:
			// childWidth / rowHeight
			// = childSize / mySize * width / rowHeight / rowHeight
			// = childSize * width / sizeUsed / sizeUsed * mySize
			// > childSize * mySize / sizeUsed / sizeUsed
			// > childSize * childSize / childSize / childSize 
			// = 1 > _minProportion.
			break;
		}
		rowHeight= virtualRowHeight;
	}
	ASSERT(i > nextChild);

	// Now i-1 is the last child used
	// and rowHeight is the height of the row.

	// We add the rest of the children, if their size is 0.
	while (i < GetChildrenCount() && GetChild(i)->GetSize() == 0)
		i++;

	childrenUsed= i - nextChild;

	// Now as we know the rowHeight, we compute the widths of our children.
	for (i=0; i < childrenUsed; i++)
	{
		// Rectangle(1.0 * 1.0) = mySize
		double rowSize= mySize * rowHeight;
        double childSize= (double)GetChild(nextChild + i)->GetSize();
		double cw= childSize / rowSize;
		ASSERT(cw >= 0);
		childWidth[nextChild + i]= cw;
	}

	return rowHeight;
}

COLORREF CItem::GetGraphColor() const
{
	COLORREF color;

	switch (GetType())
	{
	case IT_UNKNOWN:
		color= RGB(255,255,0);
		break;

	case IT_FREESPACE:
		color= RGB(100,100,100);
		break;

	case IT_FILE:
		color= GetDocument()->GetCushionColor(GetExtension());
		break;

	default:
		color= RGB(0,0,0);
		break;
	}

	return color;
}

bool CItem::MustShowReadJobs() const
{
	if (GetParent() != NULL)
	{
		return !GetParent()->IsDone();
	}
	else
	{
		return !IsDone();
	}
}

COLORREF CItem::GetPercentageColor() const
{
	int i= GetIndent() % GetOptions()->GetTreelistColorCount();
	return GetOptions()->GetTreelistColor(i);
}

int CItem::FindFreeSpaceItemIndex() const
{
	for (int i=0; i < GetChildrenCount(); i++)
	{
		if (GetChild(i)->GetType() == IT_FREESPACE)
			break;
	}
	return i; // maybe == GetChildrenCount() (=> not found)
}

int CItem::FindUnknownItemIndex() const
{
	for (int i=0; i < GetChildrenCount(); i++)
	{
		if (GetChild(i)->GetType() == IT_UNKNOWN)
			break;
	}
	return i; // maybe == GetChildrenCount() (=> not found)
}

void CItem::RenderLeaf(CDC *pdc, const double *surface) const
{
	CRect rc= m_rect;
	if (GetOptions()->IsTreemapGrid())
	{
		rc.top++;
		rc.left++;
		if (rc.Width() <= 0 || rc.Height() <= 0)
			return;
	}
	
	::RenderRectangle(pdc, rc, surface, GetGraphColor());
}

CString CItem::UpwardGetPathWithoutBackslash() const
{
	CString path;
	if (GetParent() != NULL)
		path= GetParent()->UpwardGetPathWithoutBackslash();

	switch (GetType())
	{
	case IT_MYCOMPUTER:
		// empty
		break;

	case IT_DRIVE:
		// (we don't use our parent's path here.)
		path= PathFromVolumeName(m_name);
		break;

	case IT_DIRECTORY:
		if (!path.IsEmpty())
			path+= _T("\\");
		path+= m_name;
		break;

	case IT_FILE:
		path+= _T("\\") + m_name;
		break;

	case IT_FILESFOLDER:
		break;

	case IT_FREESPACE:
	case IT_UNKNOWN:
		break;

	default:
		ASSERT(0);
	}

	return path; 
}

void CItem::AddDirectory(CFileFind& finder)
{
	bool dontFollow= GetApp()->IsMountPoint(finder.GetFilePath()) && !GetOptions()->IsFollowMountPoints();

	CItem *child= new CItem(IT_DIRECTORY, finder.GetFileName(), dontFollow);
	FILETIME t;
	finder.GetLastWriteTime(&t);
	child->SetLastChange(t);
	AddChild(child);
}

void CItem::AddFile(const FILEINFO& fi)
{
	CItem *child= new CItem(IT_FILE, fi.name);
	child->SetSize(fi.length);
	child->SetLastChange(fi.lastWriteTime);
	child->SetDone();

	AddChild(child);
}

void CItem::DriveVisualUpdateDuringWork()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
		DispatchMessage(&msg);

	GetMainFrame()->DrivePacman();
	UpwardDrivePacman();
}

void CItem::UpwardDrivePacman()
{
	if (!GetOptions()->IsPacmanAnimation())
		return;

	DrivePacman();
	if (GetParent() != NULL)
		GetParent()->UpwardDrivePacman();
}

void CItem::DrivePacman()
{
	if (!IsVisible())
		return;

	if (!CTreeListItem::DrivePacman(GetReadJobs()))
		return;

	CClientDC dc(GetTreeListControl());
	CRect rc= GetTreeListControl()->GetWholeSubitemRect(GetTreeListControl()->FindTreeItem(this), COL_SUBTREEPERCENTAGE);
	rc.DeflateRect(sizeDeflatePacman);
	DrawPacman(&dc, rc);
}

void CItem::UpdateRamUsage()
{
	if (GetTickCount() - _m_lastRamUsageUpdate > 1200)
	{
		GetApp()->UpdateRamUsage();
		_m_lastRamUsageUpdate= GetTickCount();
	}
}


