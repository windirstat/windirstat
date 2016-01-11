// myimagelist.cpp - Implementation of CMyImageList
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2016 WinDirStat team (windirstat.info)
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - oliver   -> http://windirstat.info/contact/oliver/
//

#include "stdafx.h"
#include "windirstat.h"
#include "myimagelist.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CMyImageList::CMyImageList()
    : m_filesFolderImage(-1)
    , m_freeSpaceImage(-1)
    , m_unknownImage(-1)
    , m_emptyImage(-1)
    , m_junctionImage(-1)
{
}

CMyImageList::~CMyImageList()
{
}

void CMyImageList::initialize()
{
    if(m_hImageList == NULL)
    {
        CString s;
        ::GetSystemDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
        s.ReleaseBuffer();
        VTRACE(_T("GetSystemDirectory() -> %s"), s.GetString());

        SHFILEINFO sfi;
        HIMAGELIST hil = (HIMAGELIST)::SHGetFileInfo(s, 0, &sfi, sizeof(sfi), WDS_SHGFI_DEFAULTS);

        this->Attach(ImageList_Duplicate(hil));

        VTRACE(_T("System image list has %i icons"), this->GetImageCount());
        for(int i = 0; i < this->GetImageCount(); i++)
        {
            m_indexMap.SetAt(i, i);
        }

        this->addCustomImages();
    }
}

COLORREF CMyImageList::greenify_(COLORREF c)
{
    if(c == RGB(255,255,255))
    {
        return c;
    }
    double b = CColorSpace::GetColorBrightness(c);
    b = b * b;
    return CColorSpace::MakeBrightColor(RGB(0, 255, 0), b);
}

COLORREF CMyImageList::blueify_(COLORREF c)
{
    if(c == RGB(255,255,255))
    {
        return c;
    }
    double b = CColorSpace::GetColorBrightness(c);
    return CColorSpace::MakeBrightColor(RGB(0, 0, 255), b);
}

COLORREF CMyImageList::yellowify_(COLORREF c)
{
    if(c == RGB(255,255,255))
    {
        return c;
    }
    double b = CColorSpace::GetColorBrightness(c);
    b = b * b;
    return CColorSpace::MakeBrightColor(RGB(255, 255, 0), b);
}

// Returns the index of the added icon
int CMyImageList::cacheIcon(LPCTSTR path, UINT flags, CString *psTypeName)
{
    ASSERT(m_hImageList != NULL); // should have been initialize()ed.

    flags |= WDS_SHGFI_DEFAULTS;
    if(psTypeName != NULL)
    {
        // Also retrieve the file type description
        flags |= SHGFI_TYPENAME;
    }

    SHFILEINFO sfi;
    HIMAGELIST hil = (HIMAGELIST)::SHGetFileInfo(path, 0, &sfi, sizeof(sfi), flags);
    if(hil == NULL)
    {
        VTRACE(_T("SHGetFileInfo() failed"));
        return getEmptyImage();
    }

    if(psTypeName != NULL)
    {
        *psTypeName = sfi.szTypeName;
    }

    int i;
    if(!m_indexMap.Lookup(sfi.iIcon, i)) // part of the system image list?
    {
        CImageList *sil = CImageList::FromHandle(hil); // does not have to be destroyed
        i = this->Add(sil->ExtractIcon(sfi.iIcon));
        m_indexMap.SetAt(sfi.iIcon, i);
    }

    return i;
}

int CMyImageList::getMyComputerImage()
{
    // FIXME: see whether we can wrap this up in some nice helper function instead ...
    LPITEMIDLIST pidl = NULL;
    HRESULT hr = ::SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &pidl);
    if(FAILED(hr))
    {
        VTRACE(_T("SHGetSpecialFolderLocation(CSIDL_DRIVES) failed!"));
        return 0;
    }

    int i = cacheIcon((LPCTSTR)pidl, SHGFI_PIDL);

    ::CoTaskMemFree(pidl);

    return i;
}

int CMyImageList::getMountPointImage()
{
    return cacheIcon(getADriveSpec(), 0); // The flag SHGFI_USEFILEATTRIBUTES doesn't work on W95.
}

int CMyImageList::getJunctionImage()
{
    // Intermediate solution until we find a nice icon for junction points
    return m_junctionImage;
}

int CMyImageList::getFolderImage()
{
    CString s;
    ::GetSystemDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
    s.ReleaseBuffer();

    return cacheIcon(s, 0);
}

int CMyImageList::getFileImage(LPCTSTR path)
{
    return cacheIcon(path, 0);
}

int CMyImageList::getExtImageAndDescription(LPCTSTR ext, CString& description)
{
    return cacheIcon(ext, SHGFI_USEFILEATTRIBUTES, &description);
}

int CMyImageList::getFilesFolderImage()
{
    ASSERT(m_hImageList != NULL); // should have been initialize()ed.
    return m_filesFolderImage;
}

int CMyImageList::getFreeSpaceImage()
{
    ASSERT(m_hImageList != NULL); // should have been initialize()ed.
    return m_freeSpaceImage;
}

int CMyImageList::getUnknownImage()
{
    ASSERT(m_hImageList != NULL); // should have been initialize()ed.
    return m_unknownImage;
}

int CMyImageList::getEmptyImage()
{
    ASSERT(m_hImageList != NULL);
    return m_emptyImage;
}


// Returns an arbitrary present drive
// TODO: doesn't work on Vista and up because the system drive has a different icon
CString CMyImageList::getADriveSpec()
{
    CString s;
    UINT u = ::GetWindowsDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
    s.ReleaseBuffer();
    if(u == 0 || s.GetLength() < 3 || s[1] != wds::chrColon || s[2] != wds::chrBackslash)
    {
        return _T("C:\\");
    }
    return s.Left(3);
}

void CMyImageList::addCustomImages()
{
    const int CUSTOM_IMAGE_COUNT = 5;
    const COLORREF bgcolor = RGB(255,255,255);

    const int folderImage = getFolderImage();
    const int driveImage = getMountPointImage();

    IMAGEINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    VERIFY(this->GetImageInfo(folderImage, &ii));
    const CRect rc(ii.rcImage);

    CClientDC dcClient(CWnd::GetDesktopWindow());

    CDC dcmem;
    dcmem.CreateCompatibleDC(&dcClient);
    CBitmap target;
    target.CreateCompatibleBitmap(&dcClient, rc.Width() * CUSTOM_IMAGE_COUNT, rc.Height());

    // Junction point
    CBitmap junc;
    junc.LoadBitmap(IDB_JUNCTIONPOINT);
    BITMAP bmjunc;
    junc.GetBitmap(&bmjunc);
    CDC dcjunc;
    dcjunc.CreateCompatibleDC(&dcClient);

    {
        CSelectObject sotarget(&dcmem, &target);
        CSelectObject sojunc(&dcjunc, &junc);

        dcmem.FillSolidRect(0, 0, rc.Width() * CUSTOM_IMAGE_COUNT, rc.Height(), bgcolor);
        CPoint pt(0, 0);
        COLORREF savedClr = this->SetBkColor(CLR_NONE);
        VERIFY(Draw(&dcmem, folderImage, pt, ILD_NORMAL));
        pt.x += rc.Width();
        VERIFY(Draw(&dcmem, driveImage, pt, ILD_NORMAL));
        pt.x += rc.Width();
        VERIFY(Draw(&dcmem, driveImage, pt, ILD_NORMAL));
        pt.x += rc.Width();
        VERIFY(Draw(&dcmem, folderImage, pt, ILD_NORMAL));
        this->SetBkColor(savedClr);

        // Now we re-color the images
        for(int i = 0; i < rc.Width(); i++)
        {
            for(int j = 0; j < rc.Height(); j++)
            {
                int idx = 0;

                // We "blueify" the folder image ("<Files>")
                COLORREF c = dcmem.GetPixel(idx * rc.Width() + i, j);
                dcmem.SetPixel(idx * rc.Width() + i, j, blueify_(c));
                idx++;

                // ... "greenify" the drive image ("<Free Space>")
                c = dcmem.GetPixel(idx * rc.Width() + i, j);
                dcmem.SetPixel(idx * rc.Width() + i, j, greenify_(c));
                idx++;

                // ...and "yellowify" the drive image ("<Unknown>")
                c = dcmem.GetPixel(idx * rc.Width() + i, j);
                dcmem.SetPixel(idx * rc.Width() + i, j, yellowify_(c));
                idx++;

                // ...and overlay the junction point image with the link symbol.
                int jjunc = j - (rc.Height() - bmjunc.bmHeight);

                c = dcmem.GetPixel(idx * rc.Width() + i, j);
                dcmem.SetPixel(idx * rc.Width() + i, j, c); // I don't know why this statement is required.
                if(i < bmjunc.bmWidth && jjunc >= 0)
                {
                    COLORREF cjunc = dcjunc.GetPixel(i, jjunc);
                    if(cjunc != RGB(255,0,255))
                    {
                        dcmem.SetPixel(idx * rc.Width() + i, j, cjunc);
                    }
                }
            }
        }
    }
    int k = this->Add(&target, bgcolor);
    VTRACE(_T("k == %i"), k);
    m_filesFolderImage = k++;
    m_freeSpaceImage = k++;
    m_unknownImage = k++;
    m_junctionImage = k++;
    m_emptyImage = k++;
}
