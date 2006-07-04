// set.h - Declaration and implementatino of CSet<>
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> bseifert@users.sourceforge.net, bseifert@daccord.net
//            - assarbad -> http://assarbad.net/en/contact
//
// $Header$

#pragma once


//
// CSet. A set class. I simply use the CMap class, which is in fact
// a set of assocs (x->y) and ignore the ys.
//
template<class KEY, class ARG_KEY>
class CSet
{
public:
	CSet(int nBlockSize = 10): m_map(nBlockSize) { }
	~CSet()	{ }
	void Serialize(CArchive& ar) { m_map.Serialize(ar); }

	int GetCount() const { return m_map.GetCount(); }
	BOOL IsEmpty() const { return m_map.IsEmpty(); }
	BOOL Lookup(ARG_KEY key) const { int dummy; return m_map.Lookup(key, dummy); }
	void SetKey(ARG_KEY key) { m_map.SetAt(key, 0); }
	BOOL RemoveKey(ARG_KEY key) { return m_map.RemoveKey(key); }
	void RemoveAll() { m_map.RemoveAll(); }
	POSITION GetStartPosition() const { return m_map.GetStartPosition(); }
	void GetNextAssoc(POSITION& rNextPosition, KEY& rKey) const { int dummy; m_map.GetNextAssoc(rNextPosition, rKey, dummy); }
	UINT GetHashTableSize() const { return m_map.GetHashTableSize(); }
	void InitHashTable(UINT hashSize, BOOL bAllocNow = TRUE) { m_map.InitHashTable(hashSize, bAllocNow); }

protected:
	CMap<KEY, ARG_KEY, int, int> m_map;

public:
#ifdef _DEBUG
	void Dump(CDumpContext& dc) const { m_map.Dump(dc); }
	void AssertValid() const { m_map.AssertValid(); }
#endif
};

// $Log$
// Revision 1.6  2006/07/04 23:37:40  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
// Revision 1.5  2006/07/04 22:49:21  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.4  2006/07/04 20:45:23  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.3  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
