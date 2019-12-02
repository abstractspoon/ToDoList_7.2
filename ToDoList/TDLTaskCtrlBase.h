// TDCTreeListCtrl.h: interface for the CTDCTaskCtrlBase class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TDCTASKCTRLBASE_H__155791A3_22AC_4083_B933_F39E9EBDADEF__INCLUDED_)
#define AFX_TDCTASKCTRLBASE_H__155791A3_22AC_4083_B933_F39E9EBDADEF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////

#include "tdcmsg.h"
#include "tdcstruct.h"
#include "todoctrlfind.h"
#include "todoctrldata.h"

/////////////////////////////////////////////////////////////////////////////

#include "..\Shared\EnHeaderCtrl.h"
#include "..\Shared\TreeListSyncer.h"
#include "..\Shared\Treeselectionhelper.h"
#include "..\Shared\runtimedlg.h"
#include "..\shared\sysimagelist.h"
#include "..\shared\iconcache.h"
#include "..\shared\fontcache.h"
#include "..\shared\graphicsmisc.h"
#include "..\shared\tooltipctrlex.h"
#include "..\Shared\enimagelist.h"

/////////////////////////////////////////////////////////////////////////////

#include <afxtempl.h>
typedef CArray<int, int> CIntArray;

/////////////////////////////////////////////////////////////////////////////

class CTreeCtrlHelper;
class CToDoCtrlData;
class CPreferences;
class CTDCImageList;

/////////////////////////////////////////////////////////////////////////////

class CTDLTaskCtrlBase : public CWnd, protected CTreeListSyncer   
{
	DECLARE_DYNAMIC(CTDLTaskCtrlBase);
	
protected: // base class only
	CTDLTaskCtrlBase(BOOL bSyncSelection,
					 const CTDCImageList& ilIcons,
					 const CToDoCtrlData& data, 
					 const CToDoCtrlFind& find, 
					 const CWordArray& aStyles,
					 const CTDCColumnIDMap& mapVisibleCols,
					 const CTDCCustomAttribDefinitionArray& aCustAttribDefs);

public:
	virtual ~CTDLTaskCtrlBase();
	operator HWND() const { return GetSafeHwnd(); }

	BOOL Create(CWnd* pParentWnd, const CRect& rect, UINT nID, BOOL bVisible = TRUE);
	void Show(BOOL bShow = TRUE) { CTreeListSyncer::Show(bShow); }

	void SetTasklistFolder(LPCTSTR szFolder) { m_sTasklistFolder = szFolder; }
	CString GetTasklistFolder() const { return m_sTasklistFolder; }

	void OnCustomAttributeChange();
	void OnColumnVisibilityChange(const CTDCColumnIDMap& mapChanges);
	void OnStyleUpdated(TDC_STYLE nStyle, BOOL bOn, BOOL bDoUpdate);
	void OnStylesUpdated();
	void OnUndoRedo(BOOL bUndo);
	void OnImageListChange();
	void OnReminderChange();
	void OnBeginRebuild();
	void OnEndRebuild();
 
	// setter responsible for deleting these shared resources
	HFONT GetFont() const;
	BOOL SetFont(HFONT hFont);
	const CImageList& GetCheckImageList() const { return m_ilCheckboxes; }

	void SaveState(CPreferences& prefs, const CString& sKey) const; 
	void LoadState(const CPreferences& prefs, const CString& sKey);

	void RedrawTasks(BOOL bErase = TRUE) const;
	void RedrawColumns(BOOL bErase = TRUE) const;
	void RedrawColumn(TDC_COLUMN nColID) const;
	void RecalcColumnWidth(TDC_COLUMN nColID);
	void RecalcColumnWidths();
	void RecalcAllColumnWidths();
	
 	inline const TODOITEM* GetTask(DWORD dwTaskID) const { return m_data.GetTrueTask(dwTaskID); }
 	inline UINT GetTaskCount() const { return m_data.GetTaskCount(); }
	inline BOOL HasSelection() const { return GetSelectedCount(); }
	inline BOOL VisibleItemCountsMatch() const { return (GetVisibleItemCount() == m_lcColumns.GetItemCount()); }

	virtual int GetItemCount() const = 0;
	virtual int GetVisibleItemCount() const = 0;
	virtual int GetSelectedCount() const = 0;
 	virtual int GetSelectedTaskIDs(CDWordArray& aTaskIDs, BOOL bTrue) const = 0;
	virtual DWORD GetSelectedTaskID() const = 0;
	virtual BOOL IsTaskSelected(DWORD dwTaskID, BOOL bSingly = FALSE) const = 0;
	virtual BOOL SelectTasks(const CDWordArray& aTaskIDs, BOOL bTrue) = 0;
	virtual BOOL SelectAll() = 0;
	virtual void DeleteAll() = 0;
	virtual BOOL InvalidateSelection(BOOL bUpdate) = 0;
	virtual BOOL InvalidateTask(DWORD dwTaskID, BOOL bUpdate = FALSE) = 0;
	virtual BOOL EnsureSelectionVisible() = 0;
	
	BOOL SaveToImage(CBitmap& bmImage);
	BOOL CanSaveToImage() const;

	COLORREF GetSelectedTaskColor() const; // -1 on no item selected
	CString GetSelectedTaskIcon() const;
	CString GetSelectedTaskComments() const;
	const CBinaryData& GetSelectedTaskCustomComments(CString& sCommentsTypeID) const;
	CString GetSelectedTaskTitle() const;
	double GetSelectedTaskTimeEstimate(TDC_UNITS& nUnits) const;
	double GetSelectedTaskTimeSpent(TDC_UNITS& nUnits) const;
	int GetSelectedTaskAllocTo(CStringArray& aAllocTo) const;
	int GetSelectedTaskAllocTo(CStringArray& aMatched, CStringArray& aMixed) const;
	CString GetSelectedTaskAllocBy() const;
	CString GetSelectedTaskStatus() const;
	int GetSelectedTaskCategories(CStringArray& aCats) const;
	int GetSelectedTaskCategories(CStringArray& aMatched, CStringArray& aMixed) const;
	int GetSelectedTaskDependencies(CStringArray& aDepends) const;
	int GetSelectedTaskTags(CStringArray& aTags) const;
	int GetSelectedTaskTags(CStringArray& aMatched, CStringArray& aMixed) const;
	CString GetSelectedTaskFileRef(int nFile) const;
	int GetSelectedTaskFileRefs(CStringArray& aFiles) const;
	int GetSelectedTaskFileRefCount() const;
	CString GetSelectedTaskExtID() const;
	int GetSelectedTaskPercent() const;
	int GetSelectedTaskPriority() const;
	int GetSelectedTaskRisk() const;
	double GetSelectedTaskCost() const;
	BOOL IsSelectedTaskFlagged() const;
	BOOL IsSelectedTaskLocked() const;
	BOOL GetSelectedTaskRecurrence(TDCRECURRENCE& tr) const;
	CString GetSelectedTaskVersion() const;
	BOOL SelectedTaskHasDate(TDC_DATE nDate) const;
	CString GetSelectedTaskPath(BOOL bIncludeTaskName, int nMaxLen = -1) const;
	COleDateTime GetSelectedTaskDate(TDC_DATE nDate) const;
	BOOL GetSelectedTaskCustomAttributeData(const CString& sAttribID, TDCCADATA& data, BOOL bFormatted = FALSE) const;
	BOOL IsSelectedTaskReference() const;
	DWORD GetSelectedTaskParentID() const;
	BOOL IsSelectedTaskDone() const;
	BOOL IsSelectedTaskDue() const;

	BOOL SelectionHasDependencies() const;
	BOOL SelectionHasIncompleteDependencies(CString& sIncomplete) const;
	BOOL SelectionHasIncompleteSubtasks(BOOL bExcludeRecurring) const;
	int SelectionHasCircularDependencies(CDWordArray& aTaskIDs) const;
	BOOL SelectionHasDates(TDC_DATE nDate, BOOL bAll = FALSE) const;
	BOOL SelectionHasReferences() const;
	BOOL SelectionHasNonReferences() const;
	BOOL SelectionHasDependents() const;
	BOOL SelectionHasSubtasks() const; // == SelectionHasParents
	BOOL SelectionHasIcons() const;
	BOOL SelectionHasUnlocked(BOOL bTreatRefsAsUnlocked = FALSE) const;
	BOOL SelectionHasLocked(BOOL bTreatRefsAsUnlocked = FALSE) const;
	BOOL SelectionAreAllDone() const;
	BOOL CanSplitSelectedTask() const;

	BOOL InvalidateColumnItem(int nItem, BOOL bUpdate = FALSE);
	BOOL InvalidateColumnSelection(BOOL bUpdate = FALSE);
	void InvalidateAll(BOOL bErase = FALSE, BOOL bUpdate = FALSE);
	void UpdateAll() { CTreeListSyncer::UpdateAll(); }
	void SetWindowPrompt(LPCTSTR szPrompt) { m_sTasksWndPrompt = szPrompt; }

	BOOL GetTaskTextColors(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, COLORREF& crText, 
							COLORREF& crBack, BOOL bRef = -1) const;
	BOOL GetTaskTextColors(DWORD dwTaskID, COLORREF& crText, COLORREF& crBack, BOOL bRef = -1) const;
	int GetTaskIconIndex(DWORD dwTaskID) const;
	int GetTaskIconIndex(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const;

	void UpdateSelectedTaskPath();
	void SetMaxInfotipCommentsLength(int nLength) { m_nMaxInfotipCommentsLength = max(-1, nLength); } // -1 to switch off

	BOOL ParseTaskLink(const CString& sLink, BOOL bURL, DWORD& dwTaskID, CString& sFile) const;
	static BOOL ParseTaskLink(const CString& sLink, BOOL bURL, const CString& sFolder, DWORD& dwTaskID, CString& sFile);

	TDC_HITTEST HitTest(const CPoint& ptScreen) const;
	TDC_COLUMN HitTestColumn(const CPoint& ptScreen) const;
	DWORD HitTestTask(const CPoint& ptScreen) const;
	int HitTestColumnsItem(const CPoint& pt, BOOL bClient, TDC_COLUMN& nColID, DWORD* pTaskID = NULL, LPRECT pRect = NULL) const;
	int HitTestFileLinkColumn(const CPoint& ptScreen) const;

	void GetWindowRect(CRect& rWindow) const { CWnd::GetWindowRect(rWindow); }
	virtual void GetBoundingRect(CRect& rect) const { CWnd::GetClientRect(rect); }
		
	void SetFocus() { CTreeListSyncer::SetFocus(); }
	BOOL HasFocus() const { return CTreeListSyncer::HasFocus(); }
	
	// sort functions
	void Sort(TDC_COLUMN nBy, BOOL bAllowToggle = TRUE); // calling twice with the same param will toggle ascending attrib
	BOOL CanSortBy(TDC_COLUMN nBy) const { return IsColumnShowing(nBy); }
	void MultiSort(const TDSORTCOLUMNS& sort);
	TDC_COLUMN GetSortBy() const { return m_sort.single.nBy; }
	void GetSortBy(TDSORTCOLUMNS& sort) const;
	void Resort(BOOL bAllowToggle = FALSE);
	BOOL IsSorting() const;
	BOOL IsMultiSorting() const;
	BOOL IsSortingBy(TDC_COLUMN nBy) const;
	const TDSORT& GetSort() const { return m_sort; }
	BOOL ModNeedsResort(TDC_ATTRIBUTE nModType) const;

	void SetModified(TDC_ATTRIBUTE nAttrib);
	void SetAlternateLineColor(COLORREF crAltLine);
	void SetGridlineColor(COLORREF crGridLine);
	void SetSplitBarColor(COLORREF crSplitBar) { CTreeListSyncer::SetSplitBarColor(crSplitBar); }

	BOOL SetPriorityColors(const CDWordArray& aColors); // must have 12 elements
	void GetPriorityColors(CDWordArray& aColors) const { aColors.Copy(m_aPriorityColors); }
	COLORREF GetPriorityColor(int nPriority) const;
	BOOL SetCompletedTaskColor(COLORREF color);
	COLORREF GetCompletedTaskColor() const { return m_crDone; }
	BOOL SetFlaggedTaskColor(COLORREF color);
	BOOL SetReferenceTaskColor(COLORREF color);
	BOOL SetAttributeColors(TDC_ATTRIBUTE nAttrib, const CTDCColorMap& colors);
	TDC_ATTRIBUTE GetColorByAttribute() const  { return m_nColorByAttrib; }
	BOOL SetStartedTaskColors(COLORREF crStarted, COLORREF crStartedToday);
	void GetStartedTaskColors(COLORREF& crStarted, COLORREF& crStartedToday) { crStarted = m_crStarted; crStartedToday = m_crStartedToday; }
	BOOL SetDueTaskColors(COLORREF crDue, COLORREF crDueToday);
	void GetDueTaskColors(COLORREF& crDue, COLORREF& crDueToday) const { crDue = m_crDue; crDueToday = m_crDueToday; }
	BOOL HasDueTodayColor() const { return (m_crDueToday != CLR_NONE); }
	BOOL ModCausesTaskTextColorChange(TDC_ATTRIBUTE nModType) const;

	void SetReadOnly(bool bReadOnly) { m_bReadOnly = bReadOnly; }
	void Resize(const CRect& rect);
	void SetTimeTrackTaskID(DWORD dwTaskID);
	void SetEditTitleTaskID(DWORD dwTaskID);
	void SetNextUniqueTaskID(DWORD dwTaskID);
	void SetCompletionStatus(const CString& sStatus);
	void SetDefaultTImeUnits(TDC_UNITS nTimeEstUnits, TDC_UNITS nTimeSpentUnits) { m_nDefTimeEstUnits = nTimeEstUnits; m_nDefTimeSpentUnits = nTimeSpentUnits; }

	BOOL PreTranslateMessage(MSG* pMsg);
	void ClientToScreen(LPRECT pRect) const { CWnd::ClientToScreen(pRect); }
	void ScreenToClient(LPRECT pRect) const { CWnd::ScreenToClient(pRect); }

	const CTDCTaskComparer& Comparer() const { return m_comparer; }

	static const TDCCOLUMN* GetColumn(TDC_COLUMN nColID);
	static BOOL IsReservedShortcut(DWORD dwShortcut);
	static void EnableExtendedSelection(BOOL bCtrl, BOOL bShift);

protected:
	CListCtrl m_lcColumns;
	CEnHeaderCtrl m_hdrColumns, m_hdrTasks;
	CToolTipCtrlEx m_tooltipColumns;

	const CToDoCtrlData& m_data;
	const CToDoCtrlFind& m_find;
	const CWordArray& m_aStyles;
	const CTDCImageList& m_ilTaskIcons;
	const CTDCColumnIDMap& m_mapVisibleCols;
	const CTDCCustomAttribDefinitionArray& m_aCustomAttribDefs;

	BOOL m_bReadOnly;
	BOOL m_bSortingColumns;

	CString m_sCompletionStatus;
	CString m_sTasksWndPrompt;
	DWORD m_dwEditTitleTaskID;
	DWORD m_dwNextUniqueTaskID;
	DWORD m_dwOptions;
	DWORD m_dwTimeTrackTaskID;
	TDC_COLUMN m_nSortColID;
	TDC_SORTDIR m_nSortDir;
	TDSORT m_sort;
	float m_fAveHeaderCharWidth;
	int m_nMaxInfotipCommentsLength;
	TDC_UNITS m_nDefTimeEstUnits, m_nDefTimeSpentUnits;
	CString m_sTasklistFolder;

	CTDCTaskComparer m_comparer;
	CTDCTaskCalculator m_calculator;
	CTDCTaskFormatter m_formatter;

	// font/color related
	COLORREF m_crAltLine, m_crGridLine, m_crDone;
	COLORREF m_crStarted, m_crStartedToday, m_crDue; 
	COLORREF m_crDueToday, m_crFlagged, m_crReference;
	CDWordArray m_aPriorityColors;
	CTDCColorMap m_mapAttribColors;
	TDC_ATTRIBUTE m_nColorByAttrib;
	CSysImageList m_ilFileRef;
	CBrush m_brDue, m_brDueToday;
	CFontCache m_fonts;
	CEnImageList m_ilCheckboxes, m_ilColSymbols;
	CIconCache m_imageIcons;

	static CMap<TDC_COLUMN, TDC_COLUMN, const TDCCOLUMN*, const TDCCOLUMN*&> s_mapColumns;
	static short s_nExtendedSelection;
	static double s_dRecentModPeriod;

private:
	BOOL m_bBoundSelecting;
	BOOL m_bAutoFitSplitter;

protected:
	// Virtual function overrides
	
protected:
	// Message map functions
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnSetRedraw(WPARAM wp, LPARAM lp);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

	DECLARE_MESSAGE_MAP()

	// pseudo-handlers
	void OnHeaderClick(TDC_COLUMN nColID);
	virtual int OnToolHitTest(CPoint point, TOOLINFO * pTI) const;

protected:
	// base-class overrides
	LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	LRESULT ScWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	
 	LRESULT OnListCustomDraw(NMLVCUSTOMDRAW* pLVCD);
 	LRESULT OnHeaderCustomDraw(NMCUSTOMDRAW* pNMCD);
	void OnNotifySplitterChange(int nSplitPos);

	void DrawSplitBar(CDC* pDC, const CRect& rSplitter, COLORREF crSplitBar);
	BOOL IsListItemSelected(HWND hwnd, int nItem) const;

protected:
	void Resize();
	void GetWindowRect(CRect& rWindow, BOOL bWithHeader) const;

	enum SELCHANGE_ACTION
	{
		SC_UNKNOWN,
		SC_BYMOUSE,
		SC_BYKEYBOARD
	};
	virtual void NotifyParentSelChange(SELCHANGE_ACTION nAction) = 0;
	virtual BOOL CreateTasksWnd(CWnd* pParentWnd, const CRect& rect, BOOL bVisible) = 0;
	virtual BOOL BuildColumns() = 0;
	virtual void Release() = 0;
	virtual DWORD GetColumnItemTaskID(int nItem) const = 0;
	virtual DWORD HitTestTasksTask(const CPoint& ptScreen) const = 0;
	virtual void SetTasksImageList(HIMAGELIST hil, BOOL bState, BOOL bOn = TRUE) = 0;
	virtual HWND Tasks() const = 0;
	virtual int RecalcColumnWidth(int nCol, CDC* pDC) const = 0;
	virtual GM_ITEMSTATE GetColumnItemState(int nItem) const = 0;
	virtual void DeselectAll() = 0;
	virtual DWORD GetHelpID() const = 0;

	virtual BOOL IsColumnShowing(TDC_COLUMN nColID) const;

	DWORD HitTestColumnsTask(const CPoint& ptScreen) const;
	BOOL IsColumnLineOdd(int nItem) const;
	void SetColor(COLORREF& color, COLORREF crNew);
	BOOL HasStyle(TDC_STYLE nStyle) const { return (m_aStyles[nStyle] ? TRUE : FALSE); }
	BOOL IsShowingColumnsOnRight() const;
	BOOL IsReadOnly() const { return HasStyle(TDCS_READONLY); }
	BOOL GetAttributeColor(const CString& sAttrib, COLORREF& color) const;
	BOOL FormatDate(const COleDateTime& date, TDC_DATE nDate, CString& sDate, CString& sTime, CString& sDow, BOOL bCustomWantsTime = FALSE) const;
	CString GetTaskColumnText(DWORD dwTaskID, const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, TDC_COLUMN nColID) const;
	int GetTaskColumnTooltip(const CPoint& ptScreen, CString& sTooltip) const;
	BOOL TaskHasReminder(DWORD dwTaskID) const;
	BOOL GetTaskReminder(DWORD dwTaskID, COleDateTime& dtRem) const;
	time_t GetTaskReminder(DWORD dwTaskID) const;
	TDC_COLUMN GetColumnID(int nCol) const; // zero is always 'tasks'
	int RecalcColumnWidth(int nCol, CDC* pDC, BOOL bVisibleOnly) const;
	CFont* GetTaskFont(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, BOOL bColumns = TRUE);
	BOOL HasThemedState(GM_ITEMSTATE nState) const;
	BOOL TaskHasIncompleteDependencies(DWORD dwTaskID, CString& sIncomplete) const;
	void UpdateHeaderSorting();
	CString FormatInfoTip(DWORD dwTaskID, int nMaxLen) const;
	void RecalcColumnWidths(BOOL bCustom);
	void RecalcColumnWidths(const CTDCColumnIDMap& aColIDs);
	const CEnHeaderCtrl& GetColumnHeaderCtrl(TDC_COLUMN nColID) const;
	BOOL IsVisible() const;
	CPoint CalcColumnIconTopLeft(const CRect& rSubItem, int nImageSize = 16, int nImage = 0, int nCount = 1) const;
	BOOL CalcFileIconRect(const CRect& rSubItem, CRect& rIcon, int nImage = 0, int nCount = 1) const;

	BOOL SetColumnOrder(const CDWordArray& aColumns);
	BOOL GetColumnOrder(CDWordArray& aColumns) const;
	void SetColumnWidths(const CDWordArray& aWidths);
	void GetColumnWidths(CDWordArray& aWidths) const;
	void SetTrackedColumns(const CDWordArray& aTracked);
	void GetTrackedColumns(CDWordArray& aTracked) const;

	POSITION GetFirstSelectedTaskPos() const;
	DWORD GetNextSelectedTaskID(POSITION& pos) const;
	int GetSelectedTaskArray(TDC_ATTRIBUTE nAttrib, CStringArray& aItems) const;
	int GetSelectedTaskArray(TDC_ATTRIBUTE nAttrib, CStringArray& aMatched, CStringArray& aMixed) const;

	void ClearSortColumn();
	void SetSortColumn(TDC_COLUMN nColID, TDC_SORTDIR nSortDir);
	TDC_COLUMN GetSortColumn(TDC_SORTDIR& nSortDir) const;
	void DoSort();
	BOOL ModNeedsResort(TDC_ATTRIBUTE nModType, TDC_COLUMN nSortBy) const;
	BOOL AttribMatchesSort(TDC_ATTRIBUTE nAttrib) const;

	// private structures to help with sorting --------------------------
	struct TDSORTFLAGS
	{
		TDSORTFLAGS() 
			: 
			bSortChildren(TRUE), 
			bSortDueTodayHigh(FALSE), 
			dwTimeTrackID(0), 
			bIncStartTime(FALSE), 
			bIncCreateTime(FALSE), 
			bIncDueTime(FALSE), 
			bIncDoneTime(FALSE)
		{
		}

		BOOL WantIncludeTime(TDC_COLUMN nColID) const
		{
			switch (nColID)
			{
			case TDCC_CREATIONDATE:
			case TDCC_CREATIONTIME:
				return bIncCreateTime;

			case TDCC_STARTDATE:
			case TDCC_STARTTIME:
				return bIncStartTime;

			case TDCC_DUEDATE:
			case TDCC_DUETIME:
				return bIncDueTime;

			case TDCC_DONEDATE:
			case TDCC_DONETIME:
				return bIncDoneTime;

			case TDCC_LASTMODDATE:
				return TRUE;
			}

			return FALSE;
		}
		
		BOOL bSortChildren;
		BOOL bSortDueTodayHigh;
		DWORD dwTimeTrackID;
		BOOL bIncCreateTime;
		BOOL bIncStartTime;
		BOOL bIncDueTime;
		BOOL bIncDoneTime;
	};
		
	struct TDSORTPARAMS
	{
		TDSORTPARAMS(const CTDLTaskCtrlBase& tcb) : base(tcb)
		{
		}
		
		const CTDLTaskCtrlBase& base;
		TDSORT sort;
		TDSORTFLAGS flags;
	};
	// ------------------------------------------------------------------

	PFNTLSCOMPARE PrepareSort(TDSORTPARAMS& ss) const;
	
	int CalcMaxDateColWidth(TDC_DATE nDate, CDC* pDC, BOOL bCustomWantsTime = FALSE) const;
	BOOL WantDrawColumnTime(TDC_DATE nDate, BOOL bCustomWantsTime = FALSE) const;
	int CalcSplitterPosToFitListColumns() const;
	void UpdateAttributePaneVisibility();
	void AdjustSplitterToFitAttributeColumns();

	BOOL NeedDrawColumnSelection() { return (HasFocus() && (GetFocus() != &m_lcColumns)); }
	void RepackageAndSendToParent(UINT msg, WPARAM wp, LPARAM lp);
	void NotifyParentOfColumnEditClick(TDC_COLUMN nColID, DWORD dwTaskID);
	
	void HandleFileLinkColumnClick(int nItem, DWORD dwTaskID, CPoint pt);
	void ShowFileLink(LPCTSTR szFilePath) const;
	BOOL HandleListLBtnDown(CListCtrl& lc, CPoint pt);
	BOOL ColumnItemIsLocked(int nItem) const;
	BOOL ItemColumnSupportsClickHandling(int nItem, TDC_COLUMN nColID, const CPoint* pCursor = NULL) const;
	BOOL AccumulateRecalcColumn(TDC_COLUMN nColID, CSet<TDC_COLUMN>& aColIDs) const;
	CString FormatTimeValue(double dTime, TDC_UNITS nUnits, BOOL bAllowNegative) const;

	void DrawColumnsRowText(CDC* pDC, int nItem, DWORD dwTaskID, const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, 
							COLORREF crText, BOOL bSelected);
	void DrawColumnText(CDC* pDC, const CString& sText, const CRect& rect, int nAlign, 
						COLORREF crText, BOOL bTaskTitle = FALSE, int nTextLen = -1);
	void DrawColumnFileLinks(CDC* pDC, const CStringArray& aFileLinks, const CRect& rect, COLORREF crText);
	void DrawColumnImage(CDC* pDC, TDC_COLUMN nColID, const CRect& rect, BOOL bAlternate = FALSE);
	void DrawGridlines(CDC* pDC, const CRect& rect, BOOL bSelected, BOOL bHorz, BOOL bVert);
	void DrawTasksRowBackground(CDC* pDC, const CRect& rRow, const CRect& rLabel, GM_ITEMSTATE nState, COLORREF crBack = CLR_NONE);
	void DrawCommentsText(CDC* pDC, const CRect& rRow, const CRect& rLabel, const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS);
	BOOL DrawItemCustomColumn(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, 
								TDC_COLUMN nColID, CDC* pDC, const CRect& rSubItem, COLORREF crText);

	enum TTCB_CHECK { TTCNC_UNCHECKED, TTCBC_CHECKED, TTCBC_MIXED };
	void DrawColumnCheckBox(CDC* pDC, const CRect& rSubItem, TTCB_CHECK nCheck);

	void DrawColumnDate(CDC* pDC, const COleDateTime& date, TDC_DATE nDate, const CRect& rect, COLORREF crText, 
						BOOL bCalculated = FALSE, BOOL bCustomWantsTime = FALSE, int nAlign = DT_RIGHT);

	inline BOOL IsBoundSelecting() const { return (m_bBoundSelecting && Misc::IsKeyPressed(VK_LBUTTON)); }
	inline BOOL IsEditingTask(DWORD dwTaskID) const { return (m_dwEditTitleTaskID == dwTaskID); }

	// internal version
	BOOL GetTaskTextColors(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, COLORREF& crText, 
							COLORREF& crBack, BOOL bRef, BOOL bSelected) const;
	COLORREF GetTaskCommentsTextColor(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const;

	static BOOL InvalidateSelection(CListCtrl& lc, BOOL bUpdate = FALSE);
	static BOOL InvalidateItem(CListCtrl& lc, int nItem, BOOL bUpdate = FALSE);
	static BOOL PtInClientRect(POINT point, HWND hWnd, BOOL bScreenCoords);
	static int GetUniqueToolTipID(DWORD dwTaskID, TDC_COLUMN nColID, int nIndex = 0);
	static int CalcRequiredIconColumnWidth(int nNumImage);
	static int SplitSelectedTaskArrayMatchCounts(const CMap<CString, LPCTSTR, int, int&>& mapCounts, int nNumTasks, 
												CStringArray& aMatched, CStringArray& aMixed);
	static BOOL CheckUpdateDueBrushColor(COLORREF crNew, COLORREF& crCur, CBrush& brCur);
	static inline BOOL HasColor(COLORREF color) { return (color != CLR_NONE) ? TRUE : FALSE; }

	static int CALLBACK SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort); 
	static int CALLBACK SortFuncMulti(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort); 
	static int CompareTasks(LPARAM lParam1, 
							LPARAM lParam2, 
							const CTDLTaskCtrlBase& base, 
							const TDSORTCOLUMN& sort, 
							const TDSORTFLAGS& flags);
	
#ifdef _DEBUG
	virtual LPCTSTR GetDebugName() const = 0;
#endif

};

#endif // !defined(AFX_TDCTASKCTRLBASE_H__155791A3_22AC_4083_B933_F39E9EBDADEF__INCLUDED_)
