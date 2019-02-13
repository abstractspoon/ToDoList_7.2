#if !defined(AFX_PREFERENCESBASE_H__19D0237F_28DC_4B45_9762_DF9B4F7D8492__INCLUDED_)
#define AFX_PREFERENCESBASE_H__19D0237F_28DC_4B45_9762_DF9B4F7D8492__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "scrollingpropertypagehost.h"
#include "dialoghelper.h"
#include "winhelpbutton.h"
#include "icon.h"
#include "Groupline.h"

#include "..\Interfaces\IPreferences.h"

#include <afxtempl.h>

/////////////////////////////////////////////////////////////////////////////

const UINT WM_PPB_CTRLCHANGE = ::RegisterWindowMessage(_T("WM_PPB_CTRLCHANGE"));

/////////////////////////////////////////////////////////////////////////////

class IPreferences;
class CDeferWndMove;

/////////////////////////////////////////////////////////////////////////////
// CPreferencesPageBase dialog

class CPreferencesPageBase : public CPropertyPage, protected CDialogHelper
{
	DECLARE_DYNAMIC(CPreferencesPageBase);

	// Construction
public:
	CPreferencesPageBase(UINT nDlgTemplateID);
	~CPreferencesPageBase();
	
	virtual void LoadPreferences(const IPreferences* /*pPrefs*/, LPCTSTR /*szKey*/) = 0;
	virtual void SavePreferences(IPreferences* /*pPrefs*/, LPCTSTR /*szKey*/) const = 0;

	void SetBackgroundColor(COLORREF color);
	CWnd* GetDlgItem(UINT nID) const;
	UINT GetHelpID() const { return m_nHelpID; }
	BOOL UITextContains(LPCTSTR szSearch) const;
	BOOL UITextContainsOneOf(const CStringArray& aSearch) const;
	BOOL HighlightUIText(const CStringArray& aSearch, COLORREF crHighlight);
	void ClearHighlights();
	CWnd* GetFirstHighlightedItem() const;

	static BOOL UITextContainsOneOf(const CWnd* pWnd, const CStringArray& aSearch);
	static BOOL UITextContainsOneOf(const CString& sUIText, const CStringArray& aSearch);

protected:
	HBRUSH m_brBack;
	COLORREF m_crback;
	BOOL m_bFirstShow;
	UINT m_nHelpID;

	CArray<HWND, HWND&> m_aHighlightedCtrls;
	HBRUSH m_brHighlight;
	COLORREF m_crHighlight;

	CGroupLineManager m_mgrGroupLines;

protected:
	virtual BOOL OnInitDialog();
	virtual void OnFirstShow();
	
protected:
	afx_msg void OnControlChange(UINT nID = -1);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	
	DECLARE_MESSAGE_MAP()

protected:
	BOOL AddGroupLine(UINT nIDStatic);
	int FindMatchingCtrls(const CWnd* pWnd, const CStringArray& aSearch, CArray<HWND, HWND&>& aMatching) const;
	
	BOOL GetHighlightRect(HWND hwnd, CRect& rHighlight) const;
};

/////////////////////////////////////////////////////////////////////////////
// CPreferencesDlgBase dialog

class CPreferencesDlgBase : public CDialog, protected CDialogHelper
{
	// Construction
public:
	CPreferencesDlgBase(UINT nDlgTemplateID, 
						UINT nPPHostFrameCtrlID,
						UINT nDialogIconID,
						UINT nHelpBtnIconID,
						CWnd* pParent);

	virtual ~CPreferencesDlgBase();
	
	int DoModal(IPreferences* prefs = NULL, LPCTSTR szKey = NULL, int nInitPage = -1);
	void SetPageBackgroundColor(COLORREF color);

	virtual void LoadPreferences(const IPreferences* prefs, LPCTSTR szKey);
	virtual void SavePreferences(IPreferences* prefs, LPCTSTR szKey) const;
	
protected:
	CScrollingPropertyPageHost m_ppHost;
	CWinHelpButton m_btnHelp;
	CIcon m_icon;

	UINT m_nDlgTemplateID;
	UINT m_nPPHostFrameCtrlID;
	UINT m_nHelpBtnIconID;
	UINT m_nDlgIconID;

	int m_nInitPage;
	CSize m_sizeOrgWindow, m_sizeCurWindow;
	CSize m_sizeCurClient;

	// Temporary pointer only
	IPreferences* m_pDoModalPrefs;
	CString m_sDoModalKey;
	
protected:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
	afx_msg void OnClickHelpButton();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	DECLARE_MESSAGE_MAP()
		
protected:
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	
	void OnApply(); // called from derived class
	
	BOOL EnsurePageCreated(int nPage) { return m_ppHost.EnsurePageCreated(nPage); }
	BOOL EnsurePageCreated(const CPreferencesPageBase* pPage) { return m_ppHost.EnsurePageCreated(pPage); }

	virtual BOOL SetActivePage(int nPage);
	virtual BOOL SetActivePage(CPreferencesPageBase* pPage);
	virtual BOOL AddPage(CPreferencesPageBase* pPage);
	virtual void ReposContents(CDeferWndMove& dwm, int nDX, int nDY);
	virtual void DoHelp();

	BOOL CreatePPHost(UINT nHostID);
	BOOL CreatePPHost(LPRECT pRect);
	void Resize(int cx = 0, int cy = 0);

	int GetActiveIndex() const { return m_ppHost.GetActiveIndex(); }
	CPreferencesPageBase* GetActivePage() { return (CPreferencesPageBase*)m_ppHost.GetActivePage(); }
	int FindPage(CPreferencesPageBase* pPage) { return m_ppHost.FindPage(pPage); }
	
	void ForwardMessage(UINT nMsg) { m_ppHost.ForwardMessage(nMsg); }
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PREFERENCESBASE_H__19D0237F_28DC_4B45_9762_DF9B4F7D8492__INCLUDED_)
