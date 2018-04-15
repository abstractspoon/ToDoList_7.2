// EnImageList.h: interface for the CEnImageList class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENIMAGELIST_H__B4DFBCE6_8AB2_4542_8E32_4C0F0EDC1160__INCLUDED_)
#define AFX_ENIMAGELIST_H__B4DFBCE6_8AB2_4542_8E32_4C0F0EDC1160__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CEnImageList : public CImageList  
{
public:
	CEnImageList();
	virtual ~CEnImageList();

	int GetImageSize() const;
	BOOL GetImageSize(int& nCx, int& nCy) const;
	BOOL GetImageSize(CSize& size) const;

	int Add(CBitmap* pbmImage, CBitmap* pbmMask) { return CImageList::Add(pbmImage, pbmMask); }
	int Add(CBitmap* pbmImage, COLORREF crMask) { return CImageList::Add(pbmImage, crMask); }
	int Add(HICON hIcon, COLORREF crBkgnd = RGB(223, 223, 223));

	BOOL ScaleByDPIFactor(COLORREF crBkgnd = RGB(223, 223, 223));

	static int GetImageSize(HIMAGELIST hil);
	static BOOL GetImageSize(HIMAGELIST hil, int& nCx, int& nCy);
	static BOOL GetImageSize(HIMAGELIST hil, CSize& size);
	static BOOL ScaleByDPIFactor(CImageList& il, COLORREF crBkgnd = RGB(223, 223, 223));
};

#endif // !defined(AFX_ENIMAGELIST_H__B4DFBCE6_8AB2_4542_8E32_4C0F0EDC1160__INCLUDED_)
