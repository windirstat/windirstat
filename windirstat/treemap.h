// treemap.h	- Deklaration of CTreemap and CTreemapPreview
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

#ifndef TREEMAP_H_INCLUDED
#define TREEMAP_H_INCLUDED


//
// CTreemap. Can create a treemap. Knows 3 squarification methods:
// KDirStat-like, SequoiaView-like and Simple.
//
// This class is fairly reusable.
//
class CTreemap
{
public:
	//
	// Item. Interface which must be supported by the tree items.
	// If you prefer to use the getHead()/getNext() pattern rather
	// than using an array for the children, you will have to
	// rewrite CTreemap.
	// 
	class Item
	{
	public:
		virtual         bool TmiIsLeaf()                const = 0;
		virtual const CRect& TmiGetRectangle()          const = 0;
		virtual         void TmiSetRectangle(const CRect& rc) = 0;
		virtual     COLORREF TmiGetGraphColor()         const = 0;
		virtual          int TmiGetChildrenCount()      const = 0;
		virtual        Item *TmiGetChild(int c)         const = 0;
		virtual     LONGLONG TmiGetSize()               const = 0;
	};

	//
	// Callback. Interface with 1 "callback" method. Can be given
	// to the CTreemap-constructor. The CTreemap will call the
	// method very rapidly during building the treemap.
	// It's because, if the tree has been paged out by the system,
	// building the treemap can last long (> 30 seconds).
	// TreemapDrawingCallback() gives the chance to provide at
	// least a little visual feedback (Update of RAM usage
	// indicator, for instance).
	//
	class Callback
	{
	public:
		virtual void TreemapDrawingCallback() = 0;
	};

	//
	// Treemap squarification style.
	//
	enum STYLE
	{
		SimpleStyle,		// This style is not used in WinDirStat (it's rather uninteresting).
		KDirStatStyle,		// Children are layed out in rows. Similar to the style used by KDirStat.
		SequoiaViewStyle	// The 'classical' squarification as described in at http://www.win.tue.nl/~vanwijk/.
	};

	//
	// Collection of all treemap options.
	//
	struct Options
	{
		STYLE style;			// Squarification method
		bool grid;				// Whether or not to draw grid lines
		COLORREF gridColor;		// Color of grid lines
		double brightness;		// 0..1.0	(default = 0.84)
		double height;			// 0..oo	(default = 0.40)	Factor "H"
		double scaleFactor;		// 0..1.0	(default = 0.90)	Factor "F"
		double ambientLight;	// 0..1.0	(default = 0.15)	Factor "Ia"
		double lightSourceX;	// -2.0..+2.0 (default = -1.0), negative = left
		double lightSourceY;	// -2.0..+2.0 (default = -1.0), negative = top

		int GetBrightnessPercent()	{ return RoundDouble(brightness * 100); }
		int GetHeightPercent()		{ return RoundDouble(height * 100); }
		int GetScaleFactorPercent()	{ return RoundDouble(scaleFactor * 100); }
		int GetAmbientLightPercent(){ return RoundDouble(ambientLight * 100); }
		int GetLightSourceXPercent(){ return RoundDouble(lightSourceX * 100); }
		int GetLightSourceYPercent(){ return RoundDouble(lightSourceY * 100); }
		CPoint GetLightSourcePoint(){ return CPoint(GetLightSourceXPercent(), GetLightSourceYPercent()); }

		void SetBrightnessPercent(int n)	{ brightness = n / 100.0; }
		void SetHeightPercent(int n)		{ height = n / 100.0; }
		void SetScaleFactorPercent(int n)	{ scaleFactor = n / 100.0; }
		void SetAmbientLightPercent(int n)	{ ambientLight = n / 100.0; }
		void SetLightSourceXPercent(int n)	{ lightSourceX = n / 100.0; }
		void SetLightSourceYPercent(int n)	{ lightSourceY = n / 100.0; }
		void SetLightSourcePoint(CPoint pt) { SetLightSourceXPercent(pt.x); SetLightSourceYPercent(pt.y); }

		int RoundDouble(double d) { return signum(d) * (int)(abs(d) + 0.5); }
	};

public:
	// Get a good palette of 13 colors (7 if system has 256 colors)
	static void GetDefaultPalette(CArray<COLORREF, COLORREF&>& palette);

	// Create a equally-bright palette from a set of arbitrary colors
	static void EqualizeColors(const COLORREF *colors, int count, CArray<COLORREF, COLORREF&>& out);

	// Good values
	static Options GetDefaultOptions();

	// WinDirStat <= 1.0.1 default options
	static Options GetOldDefaultOptions();

	// Returns true, if the system has <= 256 colors
	static bool Is256Colors();

public:
	// Construct the treemap generator and register the callback interface.
	CTreemap(Callback *callback = NULL);

	// Alter the options
	void SetOptions(const Options *options);
	Options GetOptions();

	// Create and draw a treemap
	void DrawTreemap(CDC *pdc, CRect rc, Item *root, const Options *options =NULL);

	// Same as above but double buffered
	void DrawTreemapDoubleBuffered(CDC *pdc, const CRect& rc, Item *root, const Options *options =NULL);

	// In the resulting treemap, find the item below a given coordinate.
	// Return value is not NULL, but may be a non-leaf, if grid lines are used.
	Item *FindItemByPoint(Item *root, CPoint point);

	// Draws a sample rectangle in the given style (for color legend)
	void DrawColorPreview(CDC *pdc, const CRect& rc, COLORREF color, const Options *options =NULL);

protected:
	// The recursive drawing function
	void RecurseDrawGraph(
		CDC *pdc,
		Item *item, 
		const CRect& rc,
		bool asroot,
		const double *psurface,
		double h,
		DWORD flags
	);

	// This function switches to KDirStat-, SequoiaView- or Simple_DrawChildren
	void DrawChildren(
		CDC *pdc, 
		Item *parent, 
		const double *surface,
		double h,
		DWORD flags
	);

	// KDirStat-like squarification
	void KDirStat_DrawChildren(CDC *pdc, Item *parent, const double *surface, double h, DWORD flags);
	bool KDirStat_ArrangeChildren(Item *parent,	CArray<double, double>& childWidth,	CArray<double, double>& rows, CArray<int, int>& childrenPerRow);
	double KDirStat_CalcutateNextRow(Item *parent, const int nextChild, double width, int& childrenUsed, CArray<double, double>& childWidth);

	// Classical SequoiaView-like squarification
	void SequoiaView_DrawChildren(CDC *pdc, Item *parent, const double *surface, double h, DWORD flags);

	// No squarification (simple style, not used in WinDirStat)
	void Simple_DrawChildren(CDC *pdc, Item *parent, const double *surface, double h, DWORD flags);

	// Sets brightness to a good value, if system has only 256 colors
	void SetBrightnessFor256();

	// Returns true, if height and scaleFactor are > 0 and ambientLight is < 1.0
	bool IsCushionShading();

	// Leaves space for grid and then calls RenderRectangle()
	void RenderLeaf(CDC *pdc, Item *item, const double *surface);

	// Either calls RenderCushion() or FillSolidRect()
	void RenderRectangle(CDC *pdc, const CRect& rc, const double *surface, COLORREF col);

	// Draws the surface using SetPixel()
	void RenderCushion(CDC *pdc, const CRect& rc, const double *surface, COLORREF col);

	// Adds a new ridge to surface
	static void AddRidge(const CRect& rc, double *surface, double h);

	// Swaps values above 255 to the other two values
	static void NormalizeColor(int& red, int& green, int& blue);

	// Helper function for NormalizeColor()
	static void DistributeFirst(int& first, int& second, int& third);

	static const Options  _defaultOptions;				// Good values. Default for WinDirStat 1.0.2
	static const Options  _defaultOptionsOld;			// WinDirStat 1.0.1 default options
	static const COLORREF _defaultCushionColors[];		// Standard palette for WinDirStat
	static const COLORREF _defaultCushionColors256[];	// Palette for 256-colors mode

	Options m_options;		// Current options
	double m_Lx;			// Derived parameters
	double m_Ly;
	double m_Lz;

	Callback *m_callback;	// Current callback
};


//
// CTreemapPreview. A child window, which demonstrates the options
// with an own little demo tree.
//
class CTreemapPreview: public CStatic
{
	//
	// CItem. Element of the demo tree.
	//
	class CItem: public CTreemap::Item
	{
	public:
		CItem(int size, COLORREF color)
		{
			m_size= size;
			m_color= color;
		}
		CItem(const CArray<CItem *, CItem *>& children)
		{
			m_size= 0;
			for (int i=0; i < children.GetSize(); i++)
			{
				m_children.Add(children[i]);
				m_size+= (int)children[i]->TmiGetSize();
			}
			qsort(m_children.GetData(), m_children.GetSize(), sizeof(CItem *), &_compareItems);
		}
		~CItem()
		{
			for (int i=0; i < m_children.GetSize(); i++)
				delete m_children[i];
		}
		static int _compareItems(const void *p1, const void *p2)
		{
			CItem *item1 = *(CItem **)p1;
			CItem *item2 = *(CItem **)p2;
			return signum(item2->m_size - item1->m_size);
		}

		virtual         bool TmiIsLeaf()                const	{ return m_children.GetSize() == 0; }
		virtual const CRect& TmiGetRectangle()          const	{ return m_rect; }
		virtual         void TmiSetRectangle(const CRect& rc)	{ m_rect = rc; }
		virtual     COLORREF TmiGetGraphColor()         const	{ return m_color; }
		virtual          int TmiGetChildrenCount()      const	{ return (int)m_children.GetSize(); }
		virtual        Item *TmiGetChild(int c)         const	{ return m_children[c]; }
		virtual     LONGLONG TmiGetSize()               const	{ return m_size; }

	private:
		CArray<CItem *, CItem *> m_children;	// Our children
		int m_size;								// Our size (in fantasy units)
		COLORREF m_color;						// Our color
		CRect m_rect;							// Our Rectangle in the treemap
	};

public:
	CTreemapPreview();
	~CTreemapPreview();
	void SetOptions(const CTreemap::Options *options);

protected:
	void BuildDemoData();
	COLORREF GetNextColor(int& i);

	CArray<COLORREF, COLORREF&> m_colors;	// Our color palette
	CItem *m_root;							// Demo tree
	CTreemap m_treemap;						// Our treemap creator

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
};

#endif
