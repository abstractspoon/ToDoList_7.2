// TDCTreeListCtrl.cpp: implementation of the CTDCTaskCtrlBase class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TDlTaskCtrlBase.h"
#include "todoctrldata.h"
#include "tdcstatic.h"
#include "tdcmsg.h"
#include "tdccustomattributehelper.h"
#include "tdcimagelist.h"
#include "resource.h"

#include "..\shared\graphicsmisc.h"
#include "..\shared\autoflag.h"
#include "..\shared\holdredraw.h"
#include "..\shared\timehelper.h"
#include "..\shared\misc.h"
#include "..\shared\filemisc.h"
#include "..\shared\themed.h"
#include "..\shared\wndprompt.h"
#include "..\shared\osversion.h"
#include "..\shared\webmisc.h"
#include "..\shared\enbitmap.h"
#include "..\shared\msoutlookhelper.h"

#include "..\3rdparty\colordef.h"

#include "..\Interfaces\Preferences.h"

#include <math.h>

/////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

const int LV_COLPADDING			= GraphicsMisc::ScaleByDPIFactor(3);
const int HD_COLPADDING			= GraphicsMisc::ScaleByDPIFactor(6);
const int ICON_SIZE				= GraphicsMisc::ScaleByDPIFactor(16); 
const int MIN_RESIZE_WIDTH		= (ICON_SIZE + 3); 
const int COL_ICON_SIZE			= ICON_SIZE; 
const int COL_ICON_SPACING		= GraphicsMisc::ScaleByDPIFactor(2); 
const int MIN_COL_WIDTH			= GraphicsMisc::ScaleByDPIFactor(6);
const int MIN_TASKS_WIDTH		= GraphicsMisc::ScaleByDPIFactor(200);

const COLORREF COMMENTSCOLOR	= RGB(98, 98, 98);
const COLORREF ALTCOMMENTSCOLOR = RGB(164, 164, 164);

const UINT TIMER_BOUNDINGSEL	= 100;

const LPCTSTR APP_ICON			= _T("TDL_APP_ICON");

//////////////////////////////////////////////////////////////////////

enum
{
	IDC_TASKTREE = 100,		
	IDC_TASKTREECOLUMNS,		
	IDC_TASKTREEHEADER,		
};

//////////////////////////////////////////////////////////////////////

CMap<TDC_COLUMN, TDC_COLUMN, const TDCCOLUMN*, const TDCCOLUMN*&>
			CTDLTaskCtrlBase::s_mapColumns;

short		CTDLTaskCtrlBase::s_nExtendedSelection = HOTKEYF_CONTROL | HOTKEYF_SHIFT;
double		CTDLTaskCtrlBase::s_dRecentModPeriod = 0.0;												

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CTDLTaskCtrlBase, CWnd)

//////////////////////////////////////////////////////////////////////

CTDLTaskCtrlBase::CTDLTaskCtrlBase(BOOL bSyncSelection,
								   const CTDCImageList& ilIcons,
								   const CToDoCtrlData& data, 
								   const CToDoCtrlFind& find,
								   const CWordArray& aStyles,
								   const CTDCColumnIDMap& mapVisibleCols,
								   const CTDCCustomAttribDefinitionArray& aCustAttribDefs) 
	: 
	CTreeListSyncer(TLSF_SYNCFOCUS | TLSF_BORDER | TLSF_SYNCDATA | TLSF_SPLITTER | (bSyncSelection ? TLSF_SYNCSELECTION : 0)),
	m_data(data),
	m_find(find),
	m_aStyles(aStyles),
	m_ilTaskIcons(ilIcons),
	m_mapVisibleCols(mapVisibleCols),
	m_aCustomAttribDefs(aCustAttribDefs),
	m_crDone(CLR_NONE),
	m_crDue(CLR_NONE), 
	m_crDueToday(CLR_NONE),
	m_crFlagged(CLR_NONE),
	m_crStarted(CLR_NONE), 
	m_crStartedToday(CLR_NONE),
	m_crReference(CLR_NONE),
	m_crAltLine(CLR_NONE),
	m_crGridLine(CLR_NONE),
	m_nSortColID(TDCC_NONE),
	m_nSortDir(TDC_SORTNONE),
	m_dwTimeTrackTaskID(0), 
	m_dwEditTitleTaskID(0),
	m_dwNextUniqueTaskID(100),
	m_nMaxInfotipCommentsLength(-1),
	m_bSortingColumns(FALSE),
	m_nColorByAttrib(TDCA_NONE),
	m_bBoundSelecting(FALSE),
	m_nDefTimeEstUnits(TDCU_HOURS), 
	m_nDefTimeSpentUnits(TDCU_HOURS),
	m_comparer(data),
	m_calculator(data),
	m_formatter(data),
	m_bAutoFitSplitter(TRUE),
	m_imageIcons(16, 16)
{
	// build one time column map
	if (s_mapColumns.IsEmpty())
	{
		// add all columns
		int nCol = NUM_COLUMNS;

		while (nCol--)
		{
			const TDCCOLUMN& tdcc = COLUMNS[nCol];
			s_mapColumns[tdcc.nColID] = &tdcc;
		}
	}
}

CTDLTaskCtrlBase::~CTDLTaskCtrlBase()
{
	Release();
}

///////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CTDLTaskCtrlBase, CWnd)
//{{AFX_MSG_MAP(CTDCTaskCtrlBase)
//}}AFX_MSG_MAP
ON_WM_DESTROY()
ON_WM_SIZE()
ON_WM_CREATE()
ON_MESSAGE(WM_SETREDRAW, OnSetRedraw)
ON_WM_SETCURSOR()
ON_WM_TIMER()
ON_WM_HELPINFO()

END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////

BOOL CTDLTaskCtrlBase::Create(CWnd* pParentWnd, const CRect& rect, UINT nID, BOOL bVisible)
{
	DWORD dwStyle = (WS_CHILD | (bVisible ? WS_VISIBLE : 0) | WS_TABSTOP);
	
	// create ourselves
	return CWnd::CreateEx(WS_EX_CONTROLPARENT, NULL, NULL, dwStyle, rect, pParentWnd, nID);
}

int CTDLTaskCtrlBase::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rect(0, 0, lpCreateStruct->cx, lpCreateStruct->cy);
	
	if (!CreateTasksWnd(this, rect, TRUE))
		return -1;

	DWORD dwStyle = (WS_CHILD | WS_VISIBLE);

	// Tasks Header ---------------------------------------------------------------------
	if (!m_hdrTasks.Create((dwStyle | HDS_BUTTONS), rect, this, IDC_TASKTREEHEADER))
	{
		return FALSE;
	}

	// Column List ---------------------------------------------------------------------
	rect.OffsetRect(rect.Width(), 0);

	if (!m_lcColumns.Create((dwStyle | WS_TABSTOP),	rect, this, IDC_TASKTREECOLUMNS))
	{
		return FALSE;
	}
	
	// extended styles
	ListView_SetExtendedListViewStyleEx(m_lcColumns, LVS_EX_HEADERDRAGDROP, LVS_EX_HEADERDRAGDROP);
	
	// subclass the tree and list
	if (HasStyle(TDCS_RIGHTSIDECOLUMNS))
	{
		if (!Sync(Tasks(), m_lcColumns, TLSL_RIGHTDATA_IS_LEFTITEM, m_hdrTasks))
		{
			return FALSE;
		}
	}
	else // left side
	{
		if (!Sync(m_lcColumns, Tasks(), TLSL_LEFTDATA_IS_RIGHTITEM, m_hdrTasks))
		{
			return FALSE;
		}
	}
		
	// Column Header ---------------------------------------------------------------------
	if (!m_hdrColumns.SubclassWindow(ListView_GetHeader(m_lcColumns)))
	{
		return FALSE;
	}
	m_hdrColumns.EnableToolTips();
	
	// set header font and calc char width
	CFont* pFont = m_hdrColumns.GetFont();
	m_hdrTasks.SetFont(pFont);

	CClientDC dc(&m_hdrTasks);
	m_fAveHeaderCharWidth = GraphicsMisc::GetAverageCharWidth(&dc, pFont);

	VERIFY(GraphicsMisc::InitCheckboxImageList(*this, m_ilCheckboxes, IDB_CHECKBOXES, 255));

	BuildColumns();
	RecalcColumnWidths();
	OnColumnVisibilityChange(CTDCColumnIDMap());
	PostResize();

	// Tooltips for columns
	if (m_tooltipColumns.Create(this))
	{
		m_tooltipColumns.ModifyStyleEx(0, WS_EX_TRANSPARENT);
		m_tooltipColumns.SetDelayTime(TTDT_INITIAL, 50);
		m_tooltipColumns.SetDelayTime(TTDT_AUTOPOP, 10000);
		m_tooltipColumns.SetMaxTipWidth((UINT)(WORD)-1);

		// Disable columns own tooltips
		HWND hwndTooltips = (HWND)m_lcColumns.SendMessage(LVM_GETTOOLTIPS);

		if (hwndTooltips)
			::SendMessage(hwndTooltips, TTM_ACTIVATE, FALSE, 0);
	}

	return 0;
}

int CTDLTaskCtrlBase::OnToolHitTest(CPoint point, TOOLINFO * pTI) const
{
	CWnd::ClientToScreen(&point);

	CString sTooltip;
	int nHitTest = GetTaskColumnTooltip(point, sTooltip);
	
	if (nHitTest == -1)
		return -1;

	// Fill in the TOOLINFO structure
	pTI->hwnd = m_lcColumns;
	pTI->uId = (UINT)nHitTest;
	pTI->lpszText = _tcsdup(sTooltip);
	pTI->uFlags = 0;

	CWnd::GetClientRect(&pTI->rect);

	return nHitTest;
}

int CTDLTaskCtrlBase::GetUniqueToolTipID(DWORD dwTaskID, TDC_COLUMN nColID, int nIndex)
{
	ASSERT(nIndex < 100);
	ASSERT(nColID < TDCC_COUNT);

	return (int)((((dwTaskID * TDCC_COUNT) + nColID) * 100) + nIndex);
}

int CTDLTaskCtrlBase::GetTaskColumnTooltip(const CPoint& ptScreen, CString& sTooltip) const
{
	TDC_COLUMN nColID = TDCC_NONE;
	DWORD dwTaskID = 0;
	
	if (HitTestColumnsItem(ptScreen, FALSE, nColID, &dwTaskID) == -1)
		return -1;

	ASSERT(nColID != TDCC_NONE);
	ASSERT(dwTaskID);

	const TODOITEM* pTDI = m_data.GetTrueTask(dwTaskID);

	if (!pTDI)
	{
		ASSERT(0);
		return -1;
	}

	switch (nColID)
	{
	case TDCC_RECURRENCE:
		break;

	case TDCC_RECENTEDIT:
		{
			COleDateTime dtLastMod = m_calculator.GetTaskLastModifiedDate(dwTaskID);

			if (TODOITEM::IsRecentlyModified(dtLastMod))
				sTooltip = CDateHelper::FormatDate(dtLastMod, (DHFD_DOW | DHFD_TIME | DHFD_NOSEC));

			return GetUniqueToolTipID(dwTaskID, nColID);
		}
		break;

	case TDCC_DEPENDENCY:
		if (pTDI->aDependencies.GetSize())
		{
			// Build list of Names and IDs
			for (int nDepend = 0; nDepend < pTDI->aDependencies.GetSize(); nDepend++)
			{
				if (nDepend > 0)
					sTooltip += '\n';

				const CString& sDepends = Misc::GetItem(pTDI->aDependencies, nDepend);
				DWORD dwDependID = (DWORD)_ttol(sDepends);
				
				sTooltip += sDepends; // always

				// If local, append task name
				if ((dwDependID != 0) && m_data.HasTask(dwDependID))
				{
					sTooltip += ' ';
					sTooltip += '(';
					sTooltip += m_data.GetTaskTitle(dwDependID);
					sTooltip += ')';
				}
			}			
			return GetUniqueToolTipID(dwTaskID, nColID);
		}
		break;

	case TDCC_DONE:
		if (pTDI->IsDone())
		{
			sTooltip = CDateHelper::FormatDate(pTDI->dateDone, (DHFD_DOW | DHFD_TIME | DHFD_NOSEC));
			return GetUniqueToolTipID(dwTaskID, nColID);
		}
		break;

	case TDCC_TRACKTIME:
		break;

	case TDCC_REMINDER:
		if (!HasStyle(TDCS_SHOWREMINDERSASDATEANDTIME))
		{
			time_t tRem = GetTaskReminder(dwTaskID);
			
			if (tRem == -1)
			{
				sTooltip = CEnString(IDS_REMINDER_DATENOTSET);
			}
			else if (tRem > 0)
			{
				sTooltip = CDateHelper::FormatDate(tRem, (DHFD_DOW | DHFD_TIME | DHFD_NOSEC));
			}

			if (!sTooltip.IsEmpty())
				return GetUniqueToolTipID(dwTaskID, nColID);
		}
		break;

	case TDCC_FILEREF:
		{
			int nIndex = HitTestFileLinkColumn(ptScreen);

			if (nIndex != -1)
			{
				sTooltip = FileMisc::GetFullPath(pTDI->aFileLinks[nIndex], m_sTasklistFolder);
				return GetUniqueToolTipID(dwTaskID, nColID, nIndex);
			}
		}
		break;

	case TDCC_ALL:
	case TDCC_NONE:
	case TDCC_CLIENT:
		ASSERT(0);
		return -1;

	default:
		break;
	}
	
	return -1;
}

void CTDLTaskCtrlBase::UpdateSelectedTaskPath()
{
	CEnString sHeader(IDS_TDC_COLUMN_TASK);
	
	// add the item path to the header
	if (HasStyle(TDCS_SHOWPATHINHEADER) && m_hdrTasks.GetItemCount())
	{
		if (GetSelectedCount() == 1)
		{
			CRect rHeader;
			::GetClientRect(m_hdrTasks, rHeader);
			
			int nColWidthInChars = (int)(rHeader.Width() / m_fAveHeaderCharWidth);
			CString sPath = m_formatter.GetTaskPath(GetSelectedTaskID(), nColWidthInChars);
			
			if (!sPath.IsEmpty())
				sHeader.Format(_T("%s [%s]"), CEnString(IDS_TDC_COLUMN_TASK), sPath);
		}
	}
	
	m_hdrTasks.SetItemText(0, sHeader);
	m_hdrTasks.Invalidate(FALSE);
}

void CTDLTaskCtrlBase::SetTimeTrackTaskID(DWORD dwTaskID)
{
	if (m_dwTimeTrackTaskID != dwTaskID)
	{
		m_dwTimeTrackTaskID = dwTaskID;
		
		// resort if appropriate
		if (m_sort.IsSortingBy(TDCC_TRACKTIME, FALSE))
		{
			Sort(TDCC_TRACKTIME, FALSE);
		}
		else
		{
			RedrawColumn(TDCC_TRACKTIME);
			RedrawColumn(TDCC_TIMESPENT);
		}
	}
}

void CTDLTaskCtrlBase::SetEditTitleTaskID(DWORD dwTaskID)
{
	if (m_dwEditTitleTaskID != dwTaskID)
	{
		m_dwEditTitleTaskID = dwTaskID;
	}
}

void CTDLTaskCtrlBase::SetNextUniqueTaskID(DWORD dwTaskID)
{
	if (m_dwNextUniqueTaskID != dwTaskID)
	{
		m_dwNextUniqueTaskID = dwTaskID;

		if (GetSafeHwnd() && IsColumnShowing(TDCC_ID))
			RecalcColumnWidth(TDCC_ID);
	}
}

void CTDLTaskCtrlBase::OnDestroy() 
{
	Release();
	
	CWnd::OnDestroy();
}

void CTDLTaskCtrlBase::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	
	if (cx && cy)
	{
		CRect rect(0, 0, cx, cy);
		CTreeListSyncer::Resize(rect);

		if (m_bAutoFitSplitter)
			AdjustSplitterToFitAttributeColumns();
	}
}

LRESULT CTDLTaskCtrlBase::OnSetRedraw(WPARAM wp, LPARAM /*lp*/)
{
	::SendMessage(Tasks(), WM_SETREDRAW, wp, 0);
	m_lcColumns.SetRedraw(wp);

	return 0L;
}

BOOL CTDLTaskCtrlBase::IsListItemSelected(HWND hwnd, int nItem) const
{
	return (ListView_GetItemState(hwnd, nItem, LVIS_SELECTED) & LVIS_SELECTED);
}

void CTDLTaskCtrlBase::OnStylesUpdated()
{
	SetTasksImageList(m_ilCheckboxes, TRUE, (!IsColumnShowing(TDCC_DONE) && HasStyle(TDCS_ALLOWTREEITEMCHECKBOX)));

	RecalcColumnWidths();
	UpdateHeaderSorting();
	PostResize();

	if (IsVisible())
		InvalidateAll();
}

void CTDLTaskCtrlBase::OnStyleUpdated(TDC_STYLE nStyle, BOOL bOn, BOOL bDoUpdate)
{
	switch (nStyle)
	{
	case TDCS_NODUEDATEISDUETODAYORSTART:
	case TDCS_SHOWDATESINISO:
	case TDCS_USEEARLIESTDUEDATE:
	case TDCS_USELATESTDUEDATE:
	case TDCS_USEEARLIESTSTARTDATE:
	case TDCS_USELATESTSTARTDATE:
	case TDCS_SHOWCOMMENTSINLIST:
	case TDCS_SHOWFIRSTCOMMENTLINEINLIST:
	case TDCS_STRIKETHOUGHDONETASKS:
	case TDCS_TASKCOLORISBACKGROUND:
	case TDCS_CALCREMAININGTIMEBYDUEDATE:
	case TDCS_CALCREMAININGTIMEBYSPENT:
	case TDCS_CALCREMAININGTIMEBYPERCENT:
	case TDCS_COLORTEXTBYATTRIBUTE:
		if (bDoUpdate)
			InvalidateAll();
		break;

	case TDCS_SORTDONETASKSATBOTTOM:
 		Resort();
		break;
		
	case TDCS_DUEHAVEHIGHESTPRIORITY:
	case TDCS_DONEHAVELOWESTPRIORITY:
		if (IsSortingBy(TDCC_PRIORITY))
			Resort();
		break;
		
	case TDCS_DONEHAVELOWESTRISK:
		if (IsSortingBy(TDCC_RISK))
			Resort();
		break;
		
	case TDCS_RIGHTSIDECOLUMNS:
		if (bOn != IsShowingColumnsOnRight())
			SwapSides();
		break;
		
	case TDCS_DISPLAYHMSTIMEFORMAT:
	case TDCS_TREATSUBCOMPLETEDASDONE:
		if (bDoUpdate)
			RecalcColumnWidths();
		break;
		
	case TDCS_USEHIGHESTPRIORITY:
	case TDCS_INCLUDEDONEINPRIORITYCALC:
	case TDCS_HIDEPRIORITYNUMBER:
		if (bDoUpdate && IsColumnShowing(TDCC_PRIORITY))
			InvalidateAll();
		break;
		
	case TDCS_USEHIGHESTRISK:
	case TDCS_INCLUDEDONEINRISKCALC:
		if (bDoUpdate && IsColumnShowing(TDCC_RISK))
			InvalidateAll();
		break;
		
	case TDCS_SHOWNONFILEREFSASTEXT:
		if (bDoUpdate && IsColumnShowing(TDCC_FILEREF))
			RecalcColumnWidths();
		break;
		
	case TDCS_USEPERCENTDONEINTIMEEST:
		if (bDoUpdate && IsColumnShowing(TDCC_TIMEEST))
			RecalcColumnWidths();
		break;
		
	case TDCS_SHOWREMINDERSASDATEANDTIME:
		if (IsColumnShowing(TDCC_REMINDER))
		{
			// Reset 'tracked' flag to ensure correct resize
			int nCol = m_hdrColumns.FindItem(TDCC_REMINDER);
			ASSERT(nCol != -1);

			m_hdrColumns.SetItemTracked(nCol, FALSE);

			if (bDoUpdate)
				RecalcColumnWidths();
		}
		break;
		
	case TDCS_HIDEZEROTIMECOST:
		if (bDoUpdate && 
			(IsColumnShowing(TDCC_TIMEEST) || 
			IsColumnShowing(TDCC_TIMESPENT) || 
			IsColumnShowing(TDCC_COST)))
		{
			RecalcColumnWidths();
		}
		break;
		
	case TDCS_ROUNDTIMEFRACTIONS:
		if (bDoUpdate && 
			(IsColumnShowing(TDCC_TIMEEST) || 
			IsColumnShowing(TDCC_TIMESPENT)))
		{
			RecalcColumnWidths();
		}
		break;
		
	case TDCS_HIDEPERCENTFORDONETASKS:
	case TDCS_INCLUDEDONEINAVERAGECALC:
	case TDCS_WEIGHTPERCENTCALCBYNUMSUB:
	case TDCS_SHOWPERCENTASPROGRESSBAR:
	case TDCS_HIDEZEROPERCENTDONE:
		if (bDoUpdate && IsColumnShowing(TDCC_PERCENT))
			InvalidateAll();
		break;
		
	case TDCS_AVERAGEPERCENTSUBCOMPLETION:
	case TDCS_AUTOCALCPERCENTDONE:
		if (bDoUpdate)
		{
			if (IsColumnShowing(TDCC_PERCENT))
				RecalcColumnWidths();
			else
				InvalidateAll();
		}
		break;
		
	case TDCS_HIDESTARTDUEFORDONETASKS:
		if (bDoUpdate && 
			(IsColumnShowing(TDCC_STARTDATE) || 
			IsColumnShowing(TDCC_DUEDATE)))
		{
			RecalcColumnWidths();
		}
		break;
		
	case TDCS_SHOWWEEKDAYINDATES:
		if (bDoUpdate && 
			(IsColumnShowing(TDCC_STARTDATE) || 
			IsColumnShowing(TDCC_LASTMODDATE) ||
			IsColumnShowing(TDCC_DUEDATE) || 
			IsColumnShowing(TDCC_DONEDATE)))
		{
			RecalcColumnWidths();
		}
		break;
		
	case TDCS_SHOWPATHINHEADER:
		UpdateSelectedTaskPath();
		break;
		
	case TDCS_HIDEPANESPLITBAR:
		CTreeListSyncer::SetSplitBarWidth(bOn ? 0 : 10);
		break;

	// all else not handled
	}
}

BOOL CTDLTaskCtrlBase::InvalidateColumnItem(int nItem, BOOL bUpdate)
{
	return InvalidateItem(m_lcColumns, nItem, bUpdate);
}

BOOL CTDLTaskCtrlBase::InvalidateColumnSelection(BOOL bUpdate)
{
	return InvalidateSelection(m_lcColumns, bUpdate);
}

BOOL CTDLTaskCtrlBase::InvalidateSelection(CListCtrl& lc, BOOL bUpdate)
{
	BOOL bInvalidated = FALSE;
	POSITION pos = lc.GetFirstSelectedItemPosition();

	while (pos)
	{
		int nItem = lc.GetNextSelectedItem(pos);
		bInvalidated |= InvalidateItem(lc, nItem, FALSE); // don't update until the end
	}
	
	if (bUpdate && bInvalidated)
		lc.UpdateWindow();

	return bInvalidated;
}

BOOL CTDLTaskCtrlBase::InvalidateItem(CListCtrl& lc, int nItem, BOOL bUpdate)
{
	ASSERT(nItem != -1);
	
	if (nItem == -1)
		return FALSE;

	BOOL bInvalidated = FALSE;
	CRect rItem;

	if (lc.GetItemRect(nItem, rItem, LVIR_BOUNDS))
	{
		lc.InvalidateRect(rItem);
		bInvalidated = TRUE;
	}
	
	if (bUpdate && bInvalidated)
		lc.UpdateWindow();

	return bInvalidated;
}

void CTDLTaskCtrlBase::InvalidateAll(BOOL bErase, BOOL bUpdate) 
{ 
	CTreeListSyncer::InvalidateAll(bErase, bUpdate); 
}

BOOL CTDLTaskCtrlBase::IsShowingColumnsOnRight() const
{
	return (Right() == m_lcColumns);
}

void CTDLTaskCtrlBase::OnUndoRedo(BOOL /*bUndo*/)
{
	// resync scroll pos
	PostResync(m_lcColumns, FALSE);
}

void CTDLTaskCtrlBase::OnColumnVisibilityChange(const CTDCColumnIDMap& mapChanges)
{
	CHoldRedraw hr(m_lcColumns);
	
	int nNumCols = m_hdrColumns.GetItemCount();
	
	for (int nItem = 1; nItem < nNumCols; nItem++)
	{		
		TDC_COLUMN nColID = (TDC_COLUMN)m_hdrColumns.GetItemData(nItem);
		m_hdrColumns.ShowItem(nItem, IsColumnShowing(nColID));
	}

	UpdateAttributePaneVisibility();

	if (mapChanges.Has(TDCC_ICON) || mapChanges.Has(TDCC_DONE))
		OnImageListChange();

	RecalcColumnWidths(mapChanges);

	if (m_bAutoFitSplitter)
		AdjustSplitterToFitAttributeColumns();
}

void CTDLTaskCtrlBase::UpdateAttributePaneVisibility()
{
	// we only need to find one visible column
	BOOL bShow = FALSE;
	int nItem = m_hdrColumns.GetItemCount();
	
	while (nItem-- && !bShow)
	{		
		TDC_COLUMN nColID = (TDC_COLUMN)m_hdrColumns.GetItemData(nItem);
		bShow = IsColumnShowing(nColID);
	}
	
	if (bShow)
		SetHidden(TLSH_NONE);
	else
		SetHidden(IsLeft(m_lcColumns) ? TLSH_LEFT : TLSH_RIGHT);
}

void CTDLTaskCtrlBase::OnImageListChange()
{
	SetTasksImageList(m_ilTaskIcons, FALSE, !IsColumnShowing(TDCC_ICON));
	SetTasksImageList(m_ilCheckboxes, TRUE, (!IsColumnShowing(TDCC_DONE) && HasStyle(TDCS_ALLOWTREEITEMCHECKBOX)));

	if (IsVisible())
		::InvalidateRect(Tasks(), NULL, FALSE);
}

BOOL CTDLTaskCtrlBase::IsVisible() const
{
	HWND hwnd = GetSafeHwnd();

	return (hwnd && ::IsWindowVisible(::GetParent(hwnd)) && ::IsWindowVisible(hwnd));
}

void CTDLTaskCtrlBase::OnCustomAttributeChange()
{
	for (int nAttrib = 0; nAttrib < m_aCustomAttribDefs.GetSize(); nAttrib++)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& attribDef = m_aCustomAttribDefs.GetData()[nAttrib];
		
		int nItem = m_hdrColumns.FindItem(attribDef.GetColumnID());
		ASSERT(nItem != -1);

		m_hdrColumns.EnableItemTracking(nItem, attribDef.bEnabled);
		m_hdrColumns.ShowItem(nItem, attribDef.bEnabled);

		if (attribDef.bEnabled)
		{
			m_hdrColumns.SetItemText(nItem, attribDef.GetColumnTitle());
			m_hdrColumns.SetItemToolTip(nItem, attribDef.GetToolTip());

			LVCOLUMN lvc = { 0 };
			lvc.mask = LVCF_FMT;
			lvc.fmt = attribDef.GetColumnHeaderAlignment();

			m_lcColumns.SetColumn(nItem, &lvc);
		}
		else
		{
			m_hdrColumns.SetItemText(nItem, _T(""));
			m_hdrColumns.SetItemToolTip(nItem, _T(""));
		}
	}

	UpdateAttributePaneVisibility();
	RecalcColumnWidths(TRUE); // TRUE -> Custom columns
}

BOOL CTDLTaskCtrlBase::IsColumnShowing(TDC_COLUMN nColID) const
{
	// Some columns are always visible
	if (nColID == TDCC_CLIENT)
	{
		return TRUE;
	}
	else if (CTDCCustomAttributeHelper::IsCustomColumn(nColID))
	{
		return CTDCCustomAttributeHelper::IsCustomColumnEnabled(nColID, m_aCustomAttribDefs);
	}

	return m_mapVisibleCols.Has(nColID);
}

BOOL CTDLTaskCtrlBase::SetColumnOrder(const CDWordArray& aColumns)
{
	CIntArray aOrder;
	aOrder.SetSize(aColumns.GetSize() + 1);

	// hidden column is always first
	aOrder[0] = 0;
	
	// convert columns IDs to indices
	int nNumCols = aColumns.GetSize();
	
	for (int nCol = 0; nCol < nNumCols; nCol++)
	{		
		int nItem = m_hdrColumns.FindItem((DWORD)aColumns[nCol]);
		ASSERT(nItem != -1);

		aOrder[nCol + 1] = nItem;
	}
	
	return m_lcColumns.SetColumnOrderArray(aOrder.GetSize(), aOrder.GetData());
}

BOOL CTDLTaskCtrlBase::GetColumnOrder(CDWordArray& aColumnIDs) const
{
	CIntArray aOrder;
	int nNumCols = m_hdrColumns.GetItemOrder(aOrder);

	if (nNumCols)
	{
		// ignore first column because that's our dummy column
		aColumnIDs.SetSize(nNumCols - 1); 

		for (int nItem = 1; nItem < nNumCols; nItem++)
		{		
			TDC_COLUMN nColID = (TDC_COLUMN)m_hdrColumns.GetItemData(aOrder[nItem]);
			ASSERT(nColID != TDCC_NONE);
			
			aColumnIDs[nItem - 1] = nColID;
		}
	
		return TRUE;
	}
	
	return FALSE;
}

void CTDLTaskCtrlBase::SetColumnWidths(const CDWordArray& aWidths)
{
	int nNumCols = aWidths.GetSize();
	
	// omit first column because that's our dummy column
	for (int nCol = 0; nCol < nNumCols; nCol++)
		m_hdrColumns.SetItemWidth(nCol + 1, aWidths[nCol]);
}

void CTDLTaskCtrlBase::GetColumnWidths(CDWordArray& aWidths) const
{
	CIntArray aIntWidths;
	int nNumCols = m_hdrColumns.GetItemWidths(aIntWidths);

	// omit first column because that's our dummy column
	aWidths.SetSize(nNumCols - 1);
	
	for (int nCol = 1; nCol < nNumCols; nCol++)
		aWidths[nCol - 1] = aIntWidths[nCol];
}

void CTDLTaskCtrlBase::SetTrackedColumns(const CDWordArray& aTracked)
{
	int nNumCols = aTracked.GetSize();
	
	// omit first column because that's our dummy column
	for (int nCol = 0; nCol < nNumCols; nCol++)
		m_hdrColumns.SetItemTracked(nCol + 1, (int)aTracked[nCol]);
}

void CTDLTaskCtrlBase::GetTrackedColumns(CDWordArray& aTracked) const
{
	CIntArray aIntTracked;
	int nNumCols = m_hdrColumns.GetTrackedItems(aIntTracked);

	// omit first column because that's our dummy column
	aTracked.SetSize(nNumCols - 1);
	
	for (int nCol = 1; nCol < nNumCols; nCol++)
		aTracked[nCol - 1] = aIntTracked[nCol];
}

BOOL CTDLTaskCtrlBase::BuildColumns()
{
	if (m_hdrColumns.GetItemCount())
		return FALSE;

	// Create imagelist for columns using symbols
	ASSERT(m_ilColSymbols.GetSafeHandle() == NULL);

	if (!m_ilColSymbols.Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1))
		return FALSE;

	CBitmap bmp;

	if (!bmp.LoadBitmap(IDB_COLUMN_SYMBOLS) || (m_ilColSymbols.Add(&bmp, RGB(255, 0, 255)) == -1))
		return FALSE;
	
	m_ilColSymbols.ScaleByDPIFactor(RGB(253, 253, 253));

	// primary header
	const TDCCOLUMN* pClient = GetColumn(TDCC_CLIENT);
	ASSERT(pClient);
		
	// add empty column as placeholder so we can easily replace the 
	// other columns without losing all our items too
	m_lcColumns.InsertColumn(0, _T(""));
	m_hdrColumns.ShowItem(0, FALSE);
	m_hdrColumns.SetItemWidth(0, 0);
	
	// add all columns in two stages because m_lcColumns 
	// doesn't immediately update the header control
	int nCol = 0;
	
	for (nCol = 0; nCol < NUM_COLUMNS; nCol++)
	{
		const TDCCOLUMN& tdcc = COLUMNS[nCol];
		
		if (tdcc.nColID != TDCC_CLIENT)
		{
			m_lcColumns.InsertColumn((nCol + 1), CEnString(tdcc.nIDName), tdcc.GetColumnHeaderAlignment(), 100);
		}
		else
		{
			ASSERT(!m_hdrTasks.GetItemCount());

			m_hdrTasks.AppendItem(150, CEnString(pClient->nIDName), HDF_LEFT);
			m_hdrTasks.SetItemData(0, TDCC_CLIENT);
			m_hdrTasks.EnableItemTracking(0, FALSE); // always
		}
	}
	
	// Add column IDs to header
	for (nCol = 0; nCol < NUM_COLUMNS; nCol++)
	{
		const TDCCOLUMN& tdcc = COLUMNS[nCol];
		
		if (tdcc.nColID != TDCC_CLIENT)
		{
			int nItem = (nCol + 1); // zero'th column ignored

			m_hdrColumns.SetItemData(nItem, tdcc.nColID);
			m_hdrColumns.SetItemToolTip(nItem, CEnString(tdcc.nIDLongName));
		}
	}
	
	// add custom columns
	int nNumCols = (TDCC_CUSTOMCOLUMN_LAST - TDCC_CUSTOMCOLUMN_FIRST + 1);

	for (nCol = 0; nCol < nNumCols; nCol++)
	{
		m_lcColumns.InsertColumn((NUM_COLUMNS + nCol), _T(""), LVCFMT_LEFT, 0);
	}

	// and their IDs
	for (nCol = 0; nCol < nNumCols; nCol++)
	{
		int nItem = (NUM_COLUMNS + nCol);

		m_hdrColumns.SetItemData(nItem, (TDCC_CUSTOMCOLUMN_FIRST + nCol));
		m_hdrColumns.EnableItemTracking(nItem, FALSE);
		m_hdrColumns.ShowItem(nItem, TRUE);
	}

	RecalcColumnWidths();

	return TRUE;
}

void CTDLTaskCtrlBase::RecalcAllColumnWidths()
{
	m_hdrColumns.ClearAllTracked();
	RecalcColumnWidths();
}

void CTDLTaskCtrlBase::RecalcColumnWidths()
{
	VERIFY(m_ilFileRef.Initialize());
	
	RecalcColumnWidths(FALSE); // standard
	RecalcColumnWidths(TRUE); // custom
}

void CTDLTaskCtrlBase::RecalcColumnWidths(const CTDCColumnIDMap& aColIDs)
{
	if (aColIDs.Has(TDCC_ALL))
	{
		RecalcColumnWidths();
	}
	else
	{
		POSITION pos = aColIDs.GetStartPosition();

		while (pos)
			RecalcColumnWidth(aColIDs.GetNext(pos));
	}
}

void CTDLTaskCtrlBase::RecalcColumnWidths(BOOL bCustom)
{
	CHoldRedraw hr(m_lcColumns);

	// recalc the widths of 'non-tracked' items
	CClientDC dc(&m_lcColumns);
	CFont* pOldFont = GraphicsMisc::PrepareDCFont(&dc, m_lcColumns);

	m_hdrColumns.SetItemWidth(0, 0);
	int nNumCols = m_hdrColumns.GetItemCount();
	
	for (int nItem = 1; nItem < nNumCols; nItem++)
	{		
		TDC_COLUMN nColID = (TDC_COLUMN)m_hdrColumns.GetItemData(nItem);
		BOOL bCustomCol = CTDCCustomAttributeHelper::IsCustomColumn(nColID);

		if ((bCustom && bCustomCol) || (!bCustom && !bCustomCol))
		{
			if (m_hdrColumns.IsItemVisible(nItem))
			{
				if (!m_hdrColumns.IsItemTracked(nItem))
				{
					int nColWidth = RecalcColumnWidth(nItem, &dc);
					m_hdrColumns.SetItemWidth(nItem, nColWidth);
				}
			}
			else
			{
				m_hdrColumns.SetItemWidth(nItem, 0);
			}
		}
	}

	// cleanup
	dc.SelectObject(pOldFont);
}

void CTDLTaskCtrlBase::RecalcColumnWidth(TDC_COLUMN nColID)
{
	switch (nColID)
	{
	case TDCC_NONE:
		break;

	case TDCC_ALL:
		RecalcColumnWidths();
		break;

	case TDCC_CREATIONTIME:
		RecalcColumnWidth(TDCC_CREATIONDATE);
		break;

	case TDCC_STARTTIME:
		RecalcColumnWidth(TDCC_STARTDATE);
		break;

	case TDCC_DUETIME:
		RecalcColumnWidth(TDCC_DUEDATE);
		break;

	case TDCC_DONETIME:
		RecalcColumnWidth(TDCC_DONEDATE);
		break;

	default:
		{
			int nItem = m_hdrColumns.FindItem(nColID);
			ASSERT(nItem != -1);
			
			if (m_hdrColumns.IsItemVisible(nItem) && !m_hdrColumns.IsItemTracked(nItem))
			{
				CClientDC dc(&m_lcColumns);
				CFont* pOldFont = GraphicsMisc::PrepareDCFont(&dc, m_lcColumns);
				
				int nColWidth = RecalcColumnWidth(nItem, &dc);
				m_hdrColumns.SetItemWidth(nItem, nColWidth);
				
				dc.SelectObject(pOldFont);
			}
		}
		break;
	}
}

void CTDLTaskCtrlBase::SaveState(CPreferences& prefs, const CString& sKey) const
{
	ASSERT (GetSafeHwnd());
	ASSERT (!sKey.IsEmpty());
	
	CDWordArray aOrder, aWidths, aTracked;

	GetColumnOrder(aOrder);
	GetColumnWidths(aWidths);
	GetTrackedColumns(aTracked);

	prefs.WriteProfileArray((sKey + _T("\\ColumnOrder")), aOrder);
	prefs.WriteProfileArray((sKey + _T("\\ColumnWidth")), aWidths);
	prefs.WriteProfileArray((sKey + _T("\\ColumnTracked")), aTracked);

	if (!m_bAutoFitSplitter)
		prefs.WriteProfileInt(sKey, _T("SplitPos"), GetSplitPos());
	else
		prefs.DeleteProfileEntry(sKey, _T("SplitPos"));
	
	m_sort.SaveState(prefs, sKey);
}

void CTDLTaskCtrlBase::LoadState(const CPreferences& prefs, const CString& sKey)
{
	ASSERT (GetSafeHwnd());
	ASSERT (!sKey.IsEmpty());

	// make sure columns are configured right
	OnColumnVisibilityChange(CTDCColumnIDMap());
	
	// load column customisations
	CDWordArray aOrder, aWidths, aTracked;
	
	if (prefs.GetProfileArray((sKey + _T("\\ColumnOrder")), aOrder))
		SetColumnOrder(aOrder);

	if (prefs.GetProfileArray((sKey + _T("\\ColumnWidth")), aWidths))
		SetColumnWidths(aWidths);
	
	if (prefs.GetProfileArray((sKey + _T("\\ColumnTracked")), aTracked))
		SetTrackedColumns(aTracked);

	int nSplitPos = prefs.GetProfileInt(sKey, _T("SplitPos"), -1);

	if (nSplitPos > 0)
	{
		m_bAutoFitSplitter = FALSE;
		SetSplitPos(nSplitPos);
	}

	RefreshSize();

	m_sort.LoadState(prefs, sKey);
}

int CALLBACK CTDLTaskCtrlBase::SortFuncMulti(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TDSORTPARAMS* pSS = (TDSORTPARAMS*)lParamSort;
	ASSERT (pSS && pSS->sort.bMulti && pSS->sort.multi.IsSorting());

	int nCompare = 0;
	const TDSORTCOLUMN* pCols = pSS->sort.multi.Cols();
	
	for (int nCol = 0; ((nCol < 3) && (nCompare == 0)); nCol++)
	{
		if (!pCols[nCol].IsSorting())
			break;

		nCompare = CompareTasks(lParam1, lParam2, 
								pSS->base, 
								pCols[nCol], 
								pSS->flags);
	}

	// finally, if the items are equal we sort by raw
	// position so that the sort is stable
	if (nCompare == 0)
	{
		static TDSORTCOLUMN nullCol(TDCC_NONE, FALSE);
		static TDSORTFLAGS nullFlags;

		nCompare = CompareTasks(lParam1, lParam2, pSS->base, nullCol, nullFlags);
	}
	
	return nCompare;
}

int CALLBACK CTDLTaskCtrlBase::SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TDSORTPARAMS* pSS = (TDSORTPARAMS*)lParamSort;
	
	ASSERT (!pSS->sort.bMulti);
	
	int nCompare = CompareTasks(lParam1, lParam2, 
								pSS->base, 
								pSS->sort.single, 
								pSS->flags);
	
	// finally, if the items are equal we sort by raw
	// position so that the sort is stable
	if (nCompare == 0)
	{
		static TDSORTCOLUMN nullCol(TDCC_NONE, FALSE);
		static TDSORTFLAGS nullFlags;

		nCompare = CompareTasks(lParam1, lParam2, pSS->base, nullCol, nullFlags);
	}
	
	return nCompare;
}

int CTDLTaskCtrlBase::CompareTasks(LPARAM lParam1, 
									LPARAM lParam2, 
									const CTDLTaskCtrlBase& base, 
									const TDSORTCOLUMN& sort, 
									const TDSORTFLAGS& flags)
{
	ASSERT(sort.bAscending != -1);

	DWORD dwTaskID1 = lParam1, dwTaskID2 = lParam2;
	
	// special cases first
	if (sort.IsSortingBy(TDCC_TRACKTIME))
	{
		BOOL bTracked1 = ((dwTaskID1 == flags.dwTimeTrackID) ? 1 : 0);
		BOOL bTracked2 = ((dwTaskID2 == flags.dwTimeTrackID) ? 1 : 0);
		ASSERT(!(bTracked1 && bTracked2));
		
		return (sort.bAscending ? (bTracked1 - bTracked2) : (bTracked2 - bTracked1));
	}
	else if (sort.IsSortingBy(TDCC_REMINDER))
	{
		COleDateTime dtRem1, dtRem2;

		BOOL bHasReminder1 = base.GetTaskReminder(dwTaskID1, dtRem1);
		BOOL bHasReminder2 = base.GetTaskReminder(dwTaskID2, dtRem2);

		int nCompare = 0;

		if (bHasReminder1 && bHasReminder2)
		{
			nCompare = ((dtRem1 < dtRem2) ? -1 : (dtRem1 > dtRem2) ? 1 : 0);
		}
		else if (bHasReminder1)
		{
			nCompare = 1;
		}
		else if (bHasReminder2)
		{
			nCompare = -1;
		}
		
		return (sort.bAscending ? nCompare : -nCompare);
	}
	else if (sort.IsSortingByCustom())
	{
		TDCCUSTOMATTRIBUTEDEFINITION attribDef;
		
		// this can still fail
		if (!CTDCCustomAttributeHelper::GetAttributeDef(sort.nBy, base.m_aCustomAttribDefs, attribDef))
			return 0;
		
		return base.Comparer().CompareTasks(dwTaskID1, dwTaskID2, attribDef, sort.bAscending);
	}
	
	// else default attribute
	return base.Comparer().CompareTasks(dwTaskID1, 
										dwTaskID2, 
										sort.nBy, 
										sort.bAscending, 
										flags.bSortDueTodayHigh,
										flags.WantIncludeTime(sort.nBy));
}

DWORD CTDLTaskCtrlBase::HitTestTask(const CPoint& ptScreen) const
{
	DWORD dwTaskID = HitTestColumnsTask(ptScreen);

	if (dwTaskID == 0)
		dwTaskID = HitTestTasksTask(ptScreen);

	return dwTaskID;
}

int CTDLTaskCtrlBase::HitTestColumnsItem(const CPoint& pt, BOOL bClient, TDC_COLUMN& nColID, DWORD* pTaskID, LPRECT pRect) const
{
	LVHITTESTINFO lvHit = { 0 };
	lvHit.pt = pt;

	if (!bClient)
		m_lcColumns.ScreenToClient(&lvHit.pt);
	
	ListView_SubItemHitTest(m_lcColumns, &lvHit);

	if ((lvHit.iItem < 0) || (lvHit.iSubItem < 0))
		return -1;

	nColID = GetColumnID(lvHit.iSubItem);
	ASSERT(nColID != TDCC_NONE);

	if (pTaskID)
	{
		*pTaskID = GetColumnItemTaskID(lvHit.iItem);
		ASSERT(*pTaskID);
	}

	if (pRect)
	{
		ListView_GetSubItemRect(m_lcColumns, lvHit.iItem, lvHit.iSubItem, LVIR_BOUNDS, pRect);
	}
		
	return lvHit.iItem;
}

int CTDLTaskCtrlBase::HitTestFileLinkColumn(const CPoint& ptScreen) const
{
	TDC_COLUMN nColID = TDCC_NONE;
	DWORD dwTaskID = 0;
	CRect rSubItem;
	
	if (HitTestColumnsItem(ptScreen, FALSE, nColID, &dwTaskID, &rSubItem) == -1)
	{
		ASSERT(0);
		return -1;
	}
	ASSERT(nColID == TDCC_FILEREF);

	const TODOITEM* pTDI = m_data.GetTrueTask(dwTaskID);
	
	if (!pTDI)
	{
		ASSERT(0);
		return -1;
	}
	
	int nNumFiles = pTDI->aFileLinks.GetSize();
	
	if (nNumFiles == 1)
	{
		return 0;
	}
	else
	{
		CPoint ptList(ptScreen);
		m_lcColumns.ScreenToClient(&ptList);
		
		for (int nFile = 0; nFile < nNumFiles; nFile++)
		{
			CRect rIcon;
			
			if (!CalcFileIconRect(rSubItem, rIcon, nFile, nNumFiles))
				break;

			if (rIcon.PtInRect(ptList))
				return nFile;
		}
	}

	return -1;
}

TDC_HITTEST CTDLTaskCtrlBase::HitTest(const CPoint& ptScreen) const
{
	if (PtInClientRect(ptScreen, m_hdrTasks, TRUE)) // task header
	{
		return TDCHT_COLUMNHEADER;
	}
	else if (PtInClientRect(ptScreen, m_hdrColumns, TRUE))	// column header
	{
		return TDCHT_COLUMNHEADER;
	}
	else if (PtInClientRect(ptScreen, Tasks(), TRUE)) // tasks
	{
		// see if we hit a task
		if (HitTestTasksTask(ptScreen))
		{
			return TDCHT_TASK;
		}
		else
		{
			CPoint ptClient(ptScreen);
			::ScreenToClient(Tasks(), &ptClient);

			if (PtInClientRect(ptClient, Tasks(), FALSE))
			{
				return TDCHT_TASKLIST;
			}
		}
	}
	else if (PtInClientRect(ptScreen, m_lcColumns, TRUE)) // columns
	{
		// see if we hit a task
		if (HitTestColumnsTask(ptScreen))
		{
			return TDCHT_TASK;
		}
		else
		{
			CPoint ptClient(ptScreen);
			m_lcColumns.ScreenToClient(&ptClient);

			if (PtInClientRect(ptClient, m_lcColumns, FALSE))
			{
				return TDCHT_TASKLIST;
			}
		}
	}
	
	// all else
	return TDCHT_NOWHERE;
}

DWORD CTDLTaskCtrlBase::HitTestColumnsTask(const CPoint& ptScreen) const
{
	// see if we hit a task in the list
	CPoint ptClient(ptScreen);
	m_lcColumns.ScreenToClient(&ptClient);
	
	int nItem = m_lcColumns.HitTest(ptClient);

	if (nItem != -1)
		return GetColumnItemTaskID(nItem);

	// all else
	return 0;
}

TDC_COLUMN CTDLTaskCtrlBase::HitTestColumn(const CPoint& ptScreen) const
{
	if (PtInClientRect(ptScreen, m_hdrColumns, TRUE) || // tree header
		PtInClientRect(ptScreen, Tasks(), TRUE)) // tree
	{
		return TDCC_CLIENT;
	}
	else if (PtInClientRect(ptScreen, m_hdrColumns, TRUE))	// column header
	{
		CPoint ptHeader(ptScreen);
		m_hdrColumns.ScreenToClient(&ptHeader);
		
		int nCol = m_hdrColumns.HitTest(ptHeader);
		
		if (nCol >= 0)
			return (TDC_COLUMN)m_hdrColumns.GetItemData(nCol);
	}
	else if (PtInClientRect(ptScreen, m_lcColumns, TRUE)) // columns
	{
		TDC_COLUMN nColID = TDCC_NONE;
		
		if (HitTestColumnsItem(ptScreen, FALSE, nColID) != -1)
			return nColID;
	}

	// else
	return TDCC_NONE;
}

BOOL CTDLTaskCtrlBase::PtInClientRect(POINT point, HWND hWnd, BOOL bScreenCoords)
{
	CRect rect;
	
	if (bScreenCoords)
		::GetWindowRect(hWnd, rect);
	else
		::GetClientRect(hWnd, rect);
	
	return rect.PtInRect(point);
}

void CTDLTaskCtrlBase::Release() 
{ 
	m_imageIcons.Clear();
	m_fonts.Release();
	m_ilCheckboxes.DeleteImageList();
	
	GraphicsMisc::VerifyDeleteObject(m_brDue);
	GraphicsMisc::VerifyDeleteObject(m_brDueToday);

	if (::IsWindow(m_hdrColumns))
		m_hdrColumns.UnsubclassWindow();
	
	Unsync(); 
}

BOOL CTDLTaskCtrlBase::SetFont(HFONT hFont)
{	
	ASSERT(Tasks() != NULL);
	ASSERT(hFont);

	HFONT hTaskFont = GetFont();
	BOOL bChange = !GraphicsMisc::SameFontNameSize(hFont, hTaskFont);
	
	if (bChange)
	{
		m_fonts.Clear();

		CHoldRedraw hr(*this);
		::SendMessage(Tasks(), WM_SETFONT, (WPARAM)hFont, TRUE);

		RecalcColumnWidths();
	}
	
	return bChange;
}

HFONT CTDLTaskCtrlBase::GetFont() const
{
	return (HFONT)::SendMessage(m_lcColumns, WM_GETFONT, 0, 0);
}

BOOL CTDLTaskCtrlBase::IsColumnLineOdd(int nItem) const
{
	return ((nItem % 2) == 1);
}

BOOL CTDLTaskCtrlBase::IsSorting() const
{
	return m_sort.IsSorting();
}

BOOL CTDLTaskCtrlBase::IsSortingBy(TDC_COLUMN nSortBy) const
{
	return (m_sort.IsSortingBy(nSortBy, TRUE));
}

void CTDLTaskCtrlBase::UpdateHeaderSorting()
{
	BOOL bEnable = HasStyle(TDCS_COLUMNHEADERSORTING);
	DWORD dwAdd(bEnable ? HDS_BUTTONS : 0), dwRemove(bEnable ? 0 : HDS_BUTTONS);

	if (m_hdrTasks.GetSafeHwnd())
		m_hdrTasks.ModifyStyle(dwRemove, dwAdd);

	if (m_hdrColumns)
		m_hdrColumns.ModifyStyle(dwRemove, dwAdd);
}

void CTDLTaskCtrlBase::Resort(BOOL bAllowToggle) 
{ 
	if (IsMultiSorting())
	{
		TDSORTCOLUMNS sort;

		GetSortBy(sort);
		MultiSort(sort);
	}
	else
	{
		Sort(GetSortBy(), bAllowToggle); 
	}
}

BOOL CTDLTaskCtrlBase::IsMultiSorting() const
{
	return (m_sort.bMulti && m_sort.multi.IsSorting());
}

void CTDLTaskCtrlBase::Sort(TDC_COLUMN nBy, BOOL bAllowToggle)
{
	// special case
	if (nBy == TDCC_NONE && !m_sort.IsSorting())
		return; // nothing to do

	BOOL bAscending = m_sort.single.bAscending;

	if (nBy != TDCC_NONE)
	{
		// first time?
		if ((bAscending == -1) || !m_sort.single.IsSortingBy(nBy))
		{
			const TDCCOLUMN* pTDCC = GetColumn(nBy);

			if (pTDCC)
			{
				bAscending = pTDCC->bSortAscending;
			}
			else if (CTDCCustomAttributeHelper::IsCustomColumn(nBy))
			{
				// TODO
				bAscending = FALSE;//(m_ctrlTreeList.Tree().GetGutterColumnSort(nBy) != NCGSORT_DOWN);
			}
		}
		// if there's been a mod since last sorting then its reasonable to assume
		// that the user is not toggling direction but wants to simply resort
		// in the same direction, otherwise we toggle the direction.
		else if (!m_sort.bModSinceLastSort && bAllowToggle)
		{
			bAscending = !bAscending; // toggle 
		}
		
		// update the column header
		SetSortColumn(nBy, (bAscending ? TDC_SORTUP : TDC_SORTDOWN));
	}
	else
	{
		ClearSortColumn();
		bAscending = TRUE;
	}

	m_sort.SetSortBy(nBy, bAscending);
	m_sort.bModSinceLastSort = FALSE;
	
	if (m_data.GetTaskCount() > 1)
		DoSort();
}

PFNTLSCOMPARE CTDLTaskCtrlBase::PrepareSort(TDSORTPARAMS& ss) const
{
	ss.sort = m_sort;
	ss.flags.bSortChildren = TRUE;
	ss.flags.bSortDueTodayHigh = HasColor(m_crDueToday);
	ss.flags.dwTimeTrackID = m_dwTimeTrackTaskID;
	ss.flags.bIncCreateTime = IsColumnShowing(TDCC_CREATIONTIME);
	ss.flags.bIncStartTime = IsColumnShowing(TDCC_STARTTIME);
	ss.flags.bIncDueTime = IsColumnShowing(TDCC_DUETIME);
	ss.flags.bIncDoneTime = IsColumnShowing(TDCC_DONETIME);
	
	return (ss.sort.bMulti ? SortFuncMulti : SortFunc);
}

void CTDLTaskCtrlBase::DoSort()
{
	// Scope the hold to have finished before resyncing
	{
		CHoldListVScroll hold(m_lcColumns);

		TDSORTPARAMS ss(*this);
		CTreeListSyncer::Sort(PrepareSort(ss), (LPARAM)&ss);

		ResyncSelection(m_lcColumns, Tasks(), FALSE);
	}

	ResyncScrollPos(Tasks(), m_lcColumns);
	EnsureSelectionVisible();
}

void CTDLTaskCtrlBase::GetSortBy(TDSORTCOLUMNS& sort) const
{
	sort = m_sort.multi;

	// initialise multisort if first time
	if (!sort.IsSorting())
		sort.SetSortBy(0, m_sort.single.nBy, (m_sort.single.bAscending ? TRUE : FALSE));
}

void CTDLTaskCtrlBase::MultiSort(const TDSORTCOLUMNS& sort)
{
	if (!sort.IsSorting())
	{
		ASSERT(0);
		return;
	}

	m_sort.SetSortBy(sort);
	m_sort.bModSinceLastSort = FALSE;

	if (m_data.GetTaskCount() > 1)
		DoSort();

	// clear sort direction on header
	ClearSortColumn();
}

void CTDLTaskCtrlBase::Resize(const CRect& rect)
{
	if (GetSafeHwnd())
		MoveWindow(rect);
	else
		CTreeListSyncer::Resize(rect);
}

void CTDLTaskCtrlBase::Resize()
{
	CRect rect;
	GetBoundingRect(rect);
	
	Resize(rect);
}

POSITION CTDLTaskCtrlBase::GetFirstSelectedTaskPos() const
{
	return m_lcColumns.GetFirstSelectedItemPosition();
}

DWORD CTDLTaskCtrlBase::GetNextSelectedTaskID(POSITION& pos) const
{
	if (pos == NULL)
		return 0;

	int nSel = m_lcColumns.GetNextSelectedItem(pos);

	if (nSel == -1)
		return 0;

	return GetColumnItemTaskID(nSel);
}

void CTDLTaskCtrlBase::OnNotifySplitterChange(int /*nSplitPos*/)
{
	if (IsSplitting())
		m_bAutoFitSplitter = FALSE;

	InvalidateAll(TRUE);
}

void CTDLTaskCtrlBase::DrawSplitBar(CDC* pDC, const CRect& rSplitter, COLORREF crSplitBar)
{
	GraphicsMisc::DrawSplitBar(pDC, rSplitter, crSplitBar);
}

BOOL CTDLTaskCtrlBase::GetTaskTextColors(DWORD dwTaskID, COLORREF& crText, COLORREF& crBack, BOOL bRef) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	
	if (!m_data.GetTrueTask(dwTaskID, pTDI, pTDS))
		return FALSE;
	
	// else
	return GetTaskTextColors(pTDI, pTDS, crText, crBack, bRef, FALSE);
}

BOOL CTDLTaskCtrlBase::GetTaskTextColors(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS,
										COLORREF& crText, COLORREF& crBack, BOOL bRef) const
{
	return GetTaskTextColors(pTDI, pTDS, crText, crBack, bRef, FALSE);
}

BOOL CTDLTaskCtrlBase::GetTaskTextColors(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, COLORREF& crText, 
										COLORREF& crBack, BOOL bRef, BOOL bSelected) const
{
	ASSERT(pTDI && pTDS);

	if (!(pTDI && pTDS))
		return FALSE;

	if (bRef == -1)
		bRef = pTDI->IsReference();

	// all else
	crBack = CLR_NONE;
	crText = GetSysColor(COLOR_WINDOWTEXT);

	BOOL bDone = m_calculator.IsTaskDone(pTDI, pTDS);

	if (bDone)
	{
		if (HasColor(m_crDone))
		{
			crText = m_crDone; // parent and/or item is done
		}
		else if (bRef && HasColor(m_crReference))
		{
			crText = m_crReference;
		}
		else
		{
			crText = pTDI->color; 
		}
	}
	else // all incomplete tasks
	{
		while (true)
		{
			// if it's a ref task just return the ref colour
			if (bRef && HasColor(m_crReference))
			{
				crText = m_crReference;
				break;
			}

			// else
			BOOL bDueToday = m_calculator.IsTaskDueToday(pTDI, pTDS);
			BOOL bOverDue = m_calculator.IsTaskOverDue(pTDI, pTDS);

			// overdue takes priority
			if (HasColor(m_crDue) && bOverDue)
			{
				crText = m_crDue;
				break;
			}
			else if (HasColor(m_crDueToday) && bDueToday)
			{
				crText = m_crDueToday;
				break;
			}

			// started 'by now' takes priority
			if (HasColor(m_crStarted) && m_calculator.IsTaskStarted(pTDI, pTDS))
			{
				crText = m_crStarted;
				break;
			}
			else if (HasColor(m_crStartedToday) && m_calculator.IsTaskStarted(pTDI, pTDS, TRUE))
			{
				crText = m_crStartedToday;
				break;
			}

			// else
			if (HasColor(m_crFlagged) && pTDI->bFlagged)
			{
				crText = m_crFlagged;
				break;
			}

			if (HasStyle(TDCS_COLORTEXTBYPRIORITY))
			{
				int nPriority = FM_NOPRIORITY;

				if (bDueToday)
				{
					nPriority = m_calculator.GetTaskHighestPriority(pTDI, pTDS, FALSE); // ignore due tasks
				}
				else if (bOverDue && HasStyle(TDCS_DUEHAVEHIGHESTPRIORITY))
				{
					nPriority = 10;
				}
				else
				{
					nPriority = m_calculator.GetTaskHighestPriority(pTDI, pTDS);
				}

				if (nPriority != FM_NOPRIORITY)
				{
					crText = GetPriorityColor(nPriority); 
					break;
				}
			}
			else if (HasStyle(TDCS_COLORTEXTBYATTRIBUTE))
			{
				switch (m_nColorByAttrib)
				{
				case TDCA_CATEGORY:
					GetAttributeColor(pTDI->GetCategory(0), crText);
					break;

				case TDCA_ALLOCBY:
					GetAttributeColor(pTDI->sAllocBy, crText);
					break;

				case TDCA_ALLOCTO:
					GetAttributeColor(pTDI->GetAllocTo(0), crText);
					break;

				case TDCA_STATUS:
					GetAttributeColor(pTDI->sStatus, crText);
					break;

				case TDCA_VERSION:
					GetAttributeColor(pTDI->sVersion, crText);
					break;

				case TDCA_EXTERNALID:
					GetAttributeColor(pTDI->sExternalID, crText);
					break;

				case TDCA_TAGS:
					GetAttributeColor(pTDI->GetTag(0), crText);
					break;

				default:
					ASSERT(0);
					break;
				}
			}
			else if (!HasStyle(TDCS_COLORTEXTBYNONE) && pTDI->color)
			{
				crText = pTDI->color; 
			}

			break; // always
		}
	}
	ASSERT(HasColor(crText));
	
	if (bSelected && !m_bSavingToImage)
	{
		crText = GraphicsMisc::GetExplorerItemTextColor(crText, GMIS_SELECTED, GMIB_THEMECLASSIC);
	}
	else
	{
		if (HasStyle(TDCS_TASKCOLORISBACKGROUND) && 
			(crText != GetSysColor(COLOR_WINDOWTEXT)) &&
			!m_calculator.IsTaskDone(pTDI, pTDS))
		{
			crBack = crText;
			crText = GraphicsMisc::GetBestTextColor(crBack);
		}
	}

	return TRUE;
}

COLORREF CTDLTaskCtrlBase::GetTaskCommentsTextColor(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	COLORREF crText = COMMENTSCOLOR;

	if (HasColor(m_crDone) && m_calculator.IsTaskDone(pTDI, pTDS, TDCCHECKALL))
		crText = m_crDone;

	return crText;
}

BOOL CTDLTaskCtrlBase::SetPriorityColors(const CDWordArray& aColors)
{
	ASSERT (aColors.GetSize() == 11);
	
	if ((aColors.GetSize() == 11) && !Misc::MatchAllT(aColors, m_aPriorityColors, FALSE))
	{
		m_aPriorityColors.Copy(aColors);
			
		if (GetSafeHwnd())
			InvalidateAll();
			
		return TRUE;
	}
	
	// else
	return FALSE; // invalid combination or no change
}

BOOL CTDLTaskCtrlBase::SetStartedTaskColors(COLORREF crStarted, COLORREF crStartedToday)
{
	if ((m_crStarted != crStarted) || (m_crStartedToday != crStartedToday))
	{
		m_crStarted = crStarted;
		m_crStartedToday = crStartedToday;
		
		if (GetSafeHwnd())
			InvalidateAll();

		return TRUE;
	}

	// else no change
	return FALSE;
}

BOOL CTDLTaskCtrlBase::CheckUpdateDueBrushColor(COLORREF crNew, COLORREF& crCur, CBrush& brCur)
{
	if (crCur != crNew)
	{
		GraphicsMisc::VerifyDeleteObject(brCur);

		if (HasColor(crNew))
			brCur.CreateSolidBrush(crNew);

		crCur = crNew;
		return TRUE;
	}

	return FALSE;
}

BOOL CTDLTaskCtrlBase::SetDueTaskColors(COLORREF crDue, COLORREF crDueToday)
{
	BOOL bResort = (IsSortingBy(TDCC_PRIORITY) && HasStyle(TDCS_DUEHAVEHIGHESTPRIORITY) && (HasColor(crDueToday) != HasColor(m_crDueToday)));

	BOOL bChange = CheckUpdateDueBrushColor(crDue, m_crDue, m_brDue);
	bChange |= CheckUpdateDueBrushColor(crDueToday, m_crDueToday, m_brDueToday);

	if (bChange)
	{
		if (GetSafeHwnd())
		{
			if (bResort)
				Resort(FALSE);
			else
				InvalidateAll();
		}

		return TRUE;
	}

	// else no change
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SetCompletedTaskColor(COLORREF color)
{
	if (m_crDone != color)
	{
		m_crDone = color;
		
		if (GetSafeHwnd())
			InvalidateAll();

		return TRUE;
	}

	// else no change
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SetFlaggedTaskColor(COLORREF color)
{
	if (m_crFlagged != color)
	{
		m_crFlagged = color;
		
		if (GetSafeHwnd())
			InvalidateAll();

		return TRUE;
	}

	// else no change
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SetReferenceTaskColor(COLORREF color)
{
	if (m_crReference != color)
	{
		m_crReference = color;
			
		if (GetSafeHwnd())
			InvalidateAll();

		return TRUE;
	}

	// else no change
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SetAttributeColors(TDC_ATTRIBUTE nAttrib, const CTDCColorMap& colors)
{
	// see if there is any change
	if ((m_nColorByAttrib == nAttrib) && m_mapAttribColors.MatchAll(colors))
	{
		return FALSE; // no change
	}

	m_nColorByAttrib = nAttrib;
	m_mapAttribColors.Copy(colors);
	
	if (GetSafeHwnd() && HasStyle(TDCS_COLORTEXTBYATTRIBUTE))
		InvalidateAll();

	return TRUE;
}

BOOL CTDLTaskCtrlBase::GetAttributeColor(const CString& sAttrib, COLORREF& color) const
{
	return m_mapAttribColors.GetColor(sAttrib, color);
}

COLORREF CTDLTaskCtrlBase::GetPriorityColor(int nPriority) const
{
	if (nPriority < 0 || nPriority >= m_aPriorityColors.GetSize())
		return 0;
	
	return (COLORREF)m_aPriorityColors[nPriority];
}

void CTDLTaskCtrlBase::DrawCommentsText(CDC* pDC, const CRect& rRow, const CRect& rLabel, const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS)
{
	// Avoid drawing wherever possible
	if (m_bSavingToImage || IsEditingTask(pTDS->GetTaskID()) || pTDI->sComments.IsEmpty())
	{
		return;
	}

	CRect rClip;
	pDC->GetClipBox(rClip);
	
 	CRect rComments((rLabel.right + 10), (rRow.top + 1), min(rRow.right, m_hdrTasks.GetItemWidth(0)), rRow.bottom);

	if ((rClip.right <= rComments.left) || (rComments.Width() <= 0))
	{
		return;
	}

	// Draw the minimum necessary
	COLORREF crText = GetTaskCommentsTextColor(pTDI, pTDS);

	if (HasStyle(TDCS_SHOWCOMMENTSINLIST))
	{
		int nFind = pTDI->sComments.FindOneOf(_T("\n\r")); 

		if (HasStyle(TDCS_SHOWFIRSTCOMMENTLINEINLIST))
		{
			if (nFind == 0) 
				return; // comments start with a newline -> show nothing

			// else
			DrawColumnText(pDC, pTDI->sComments, rComments, DT_LEFT, crText, TRUE, nFind);
		}
		else
		{
			// Calculate the max length of comments we are likely to show
			int nShow = ((int)(rComments.Width() / GraphicsMisc::GetAverageCharWidth(pDC)) * 2);
			nShow = min(nShow, pTDI->sComments.GetLength());

			CString sShow;
			LPTSTR szBuffer = sShow.GetBuffer(nShow);
			
			for (int nChar = 0; nChar < nShow; nChar++)
			{
				TCHAR cChar = pTDI->sComments[nChar];

				switch (cChar)
				{
				case '\r':
				case '\n':
				case '\t':
					cChar = ' ';
					// fall thru
				}

				szBuffer[nChar] = cChar;
			}
			sShow.ReleaseBuffer(nShow);
			sShow.TrimRight();

			DrawColumnText(pDC, sShow, rComments, DT_LEFT, crText, TRUE, nShow);
		}
	}
	else
	{
		DrawColumnText(pDC, _T("[...]"), rComments, DT_LEFT, crText, FALSE, 5);
	}
}

CFont* CTDLTaskCtrlBase::GetTaskFont(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, BOOL bColumns)
{
	if (!m_fonts.GetHwnd() && !m_fonts.Initialise(Tasks()))
		return NULL;

	BOOL bStrikeThru = (HasStyle(TDCS_STRIKETHOUGHDONETASKS) && pTDI->IsDone());
	BOOL bBold = (pTDS->ParentIsRoot() && !bColumns);

	if (bBold || bStrikeThru)
		return m_fonts.GetFont(bBold, FALSE, FALSE, bStrikeThru);

	// else
	return NULL;
}

BOOL CTDLTaskCtrlBase::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if ((pWnd == &m_lcColumns) && Misc::ModKeysArePressed(0))
	{
		CPoint ptScreen(::GetMessagePos());
		
		if (PtInClientRect(ptScreen, m_lcColumns, TRUE))
		{
			TDC_COLUMN nColID = TDCC_NONE;
			CPoint ptCursor(::GetMessagePos());

			int nHit = HitTestColumnsItem(ptCursor, FALSE, nColID);
			
			if (ItemColumnSupportsClickHandling(nHit, nColID, &ptCursor))
			{
				GraphicsMisc::SetHandCursor();
				return TRUE;
			}
		}
	}
	
	// else
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

BOOL CTDLTaskCtrlBase::TaskHasReminder(DWORD dwTaskID) const
{
	return (GetTaskReminder(dwTaskID) != 0);
}

BOOL CTDLTaskCtrlBase::GetTaskReminder(DWORD dwTaskID, COleDateTime& dtRem) const
{
	time_t tRem = GetTaskReminder(dwTaskID);

	if (tRem > 0)
	{
		dtRem = tRem;
		return CDateHelper::IsDateSet(dtRem);
	}
	
	// else
	CDateHelper::ClearDate(dtRem);
	return FALSE;
}

time_t CTDLTaskCtrlBase::GetTaskReminder(DWORD dwTaskID) const
{
	return CWnd::GetParent()->SendMessage(WM_TDCM_GETTASKREMINDER, dwTaskID, (LPARAM)CWnd::GetSafeHwnd());
}

void CTDLTaskCtrlBase::DrawGridlines(CDC* pDC, const CRect& rect, BOOL bSelected, BOOL bHorz, BOOL bVert)
{
	if (HasColor(m_crGridLine))
	{
		if (bHorz)
			GraphicsMisc::DrawHorzLine(pDC, rect.left, rect.right, (rect.bottom - 1), m_crGridLine);

		if (bVert)
		{
			CRect rGridline(rect);

			// don't overdraw selection
			if (bSelected)
				rGridline.DeflateRect(0, 1);

			GraphicsMisc::DrawVertLine(pDC, rGridline.top, rGridline.bottom, rect.right - 1, m_crGridLine);
		}
	}
}

LRESULT CTDLTaskCtrlBase::OnListCustomDraw(NMLVCUSTOMDRAW* pLVCD)
{
	ASSERT(pLVCD->nmcd.hdr.hwndFrom == m_lcColumns);

	if (pLVCD->nmcd.hdr.hwndFrom == m_lcColumns)
	{
		switch (pLVCD->nmcd.dwDrawStage)
		{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
			{
				// task data
				int nItem = (int)pLVCD->nmcd.dwItemSpec;
				DWORD dwTaskID = GetColumnItemTaskID(nItem), dwTrueID(dwTaskID);
				
				const TODOITEM* pTDI = NULL;
				const TODOSTRUCTURE* pTDS = NULL;
				
				if (m_data.GetTrueTask(dwTrueID, pTDI, pTDS))
				{
					CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);

					// draw gridlines and row colour full width of list
					BOOL bAlternate = (HasColor(m_crAltLine) && !IsColumnLineOdd(nItem));
					COLORREF crRowBack = (bAlternate ? m_crAltLine : GetSysColor(COLOR_WINDOW));
					
					CRect rItem;
					m_lcColumns.GetItemRect(nItem, rItem, LVIR_BOUNDS);

					// this call will update rFullWidth to full client width
					CRect rFullWidth(rItem);
					GraphicsMisc::FillItemRect(pDC, rFullWidth, crRowBack, m_lcColumns);

					// if the columns are to the right then
					// we don't want to draw the rounded end 
					// on the left so it looks continuous with the tasks
					DWORD dwFlags = GMIB_THEMECLASSIC;

					if (HasStyle(TDCS_RIGHTSIDECOLUMNS))
						dwFlags |= GMIB_CLIPLEFT;

					// selection state
					GM_ITEMSTATE nState = GetColumnItemState(nItem);
					
					if ((nState == GMIS_SELECTEDNOTFOCUSED) && m_dwEditTitleTaskID)
						nState = GMIS_SELECTED;
					
					BOOL bSelected = (nState != GMIS_NONE);
					BOOL bRef = (dwTaskID != dwTrueID);
					
					// colors
					COLORREF crText, crBack;
					GetTaskTextColors(pTDI, pTDS, crText, crBack, bRef, bSelected);

					// draw task background
					// Note: using the non-full width item rect
					if (!bSelected && HasColor(crBack))
						pDC->FillSolidRect(rItem, crBack);

					// draw horz gridline before selection
					DrawGridlines(pDC, rFullWidth, FALSE, TRUE, FALSE);

					// draw task background
					// Note: using the non-full width item rect
					if (bSelected)
					{
						crText = GraphicsMisc::GetExplorerItemTextColor(crText, nState, GMIB_THEMECLASSIC);
						GraphicsMisc::DrawExplorerItemBkgnd(pDC, m_lcColumns, nState, rItem, dwFlags);
					}

					// draw row text
					CFont* pOldFont = pDC->SelectObject(GetTaskFont(pTDI, pTDS));
					
					DrawColumnsRowText(pDC, nItem, dwTaskID, pTDI, pTDS, crText, bSelected);

					pDC->SelectObject(pOldFont);
				}
			}
			return CDRF_SKIPDEFAULT;
		}
	}
	
	return CDRF_DODEFAULT;
}

BOOL CTDLTaskCtrlBase::HasThemedState(GM_ITEMSTATE nState) const
{ 
	return ((nState != GMIS_NONE) && CThemed::AreControlsThemed()); 
}

void CTDLTaskCtrlBase::DrawTasksRowBackground(CDC* pDC, const CRect& rRow, const CRect& rLabel, GM_ITEMSTATE nState, COLORREF crBack)
{
	ASSERT(!m_bSavingToImage || (nState == GMIS_NONE));

	BOOL bSelected = ((nState != GMIS_NONE) && !m_bSavingToImage);

	if (!bSelected)
	{
		ASSERT(HasColor(crBack));

		CRect rBack(rLabel);
		rBack.right = rRow.right; // else overwriting with comments produces artifacts

		// if we have gridlines we don't fill the bottom line so 
		// as to avoid overwriting gridlines previously drawn
		if (HasColor(m_crGridLine))
			rBack.bottom--;
		
		pDC->FillSolidRect(rBack, crBack);
	}
	
	// draw horz gridline before selection
	DrawGridlines(pDC, rRow, FALSE, TRUE, FALSE);
	
	if (bSelected) // selection of some sort
	{
		DWORD dwFlags = (GMIB_THEMECLASSIC | GMIB_EXTENDRIGHT);
		
		// if the columns are on the right then
		// we don't want to draw the rounded end 
		// on the right so it looks continuous with the columns
		if (HasStyle(TDCS_RIGHTSIDECOLUMNS))
			dwFlags |= GMIB_CLIPRIGHT;
		
		GraphicsMisc::DrawExplorerItemBkgnd(pDC, Tasks(), nState, rLabel, dwFlags);
	}
}

void CTDLTaskCtrlBase::DrawColumnsRowText(CDC* pDC, int nItem, DWORD dwTaskID, const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, 
										  COLORREF crText, BOOL bSelected)
{
	DWORD dwTrueID = pTDS->GetTaskID();

	// draw each column separately
	int nNumCol = m_hdrColumns.GetItemCount();
	int nLastCol = m_hdrColumns.GetLastVisibleItem();

	CRect rSubItem, rClient, rClip;
	m_lcColumns.GetClientRect(rClient);

	pDC->GetClipBox(rClip);
	rClient.IntersectRect(rClient, rClip);

	if (rClient.IsRectEmpty())
		return;

	for (int nCol = 1; nCol < nNumCol; nCol++)
	{
		if (!m_lcColumns.GetSubItemRect(nItem, nCol, LVIR_BOUNDS, rSubItem))
			continue;

		// don't draw columns outside of client rect
		if (rSubItem.IsRectEmpty() || (rSubItem.right <= rClient.left))
			continue;

		if (rSubItem.left >= rClient.right)
			continue;

		// draw vertical gridlines for all but the
		// last item if the row is selected
		if (!(bSelected && (nCol == nLastCol)))
			DrawGridlines(pDC, rSubItem, bSelected, FALSE, TRUE);

		// don't draw min sized columns
		if (rSubItem.Width() <= MIN_COL_WIDTH)
			continue;
		
		TDC_COLUMN nColID = (TDC_COLUMN)m_hdrColumns.GetItemData(nCol);

		// Note: we pass dwTaskID NOT dwTrueID here so that references 
		// can be handled correctly
		CString sTaskColText = GetTaskColumnText(dwTaskID, pTDI, pTDS, nColID);
		
		const TDCCOLUMN* pCol = GetColumn(nColID);

		switch (nColID)
		{
		case TDCC_POSITION:
		case TDCC_RISK:
		case TDCC_RECURRENCE:
		case TDCC_ID:
		case TDCC_PARENTID:
		case TDCC_RECENTEDIT:
		case TDCC_COST:
		case TDCC_EXTERNALID:
		case TDCC_VERSION:
		case TDCC_ALLOCTO:
		case TDCC_ALLOCBY:
		case TDCC_STATUS:
		case TDCC_CATEGORY:
		case TDCC_TAGS:
		case TDCC_CREATEDBY:
		case TDCC_PATH:
		case TDCC_REMAINING:
		case TDCC_SUBTASKDONE:
		case TDCC_TIMEEST:
		case TDCC_LASTMODBY:
		case TDCC_COMMENTSSIZE:
			DrawColumnText(pDC, sTaskColText, rSubItem, pCol->nTextAlignment, crText);
			break;
			
		case TDCC_TIMESPENT:
			{
				// show text in red if we're currently tracking
				COLORREF crTemp = ((m_dwTimeTrackTaskID == dwTrueID) ? 255 : crText);

				DrawColumnText(pDC, sTaskColText, rSubItem, pCol->nTextAlignment, crTemp);
			}
			break;
			
		case TDCC_PRIORITY:
			// priority color
			if (!HasStyle(TDCS_DONEHAVELOWESTPRIORITY) || !m_calculator.IsTaskDone(pTDI, pTDS))
			{
				rSubItem.DeflateRect(2, 1, 3, 2);
				
				// first draw the priority colour
				int nPriority = m_calculator.GetTaskHighestPriority(pTDI, pTDS, FALSE);
				BOOL bHasPriority = (nPriority != FM_NOPRIORITY);
				
				if (bHasPriority)
				{
					COLORREF crFill = GetPriorityColor(nPriority);
					COLORREF crBorder = GraphicsMisc::Darker(crFill, 0.5);
					
					GraphicsMisc::DrawRect(pDC, rSubItem, crFill, crBorder);//, nCornerRadius);
				}
				
				// then, if the task is also due, draw a small tag
				// unless it's due today and the user doesn't want today's tasks marked as due
				// or there's no due color 
				HBRUSH brTag = NULL;
				
				if (HasColor(m_crDue) && m_calculator.IsTaskOverDue(pTDI, pTDS))
				{
					brTag = m_brDue;
				}
				else if (HasColor(m_crDueToday) && m_calculator.IsTaskDueToday(pTDI, pTDS))
				{
					brTag = m_brDueToday;
				}

				if (brTag != NULL)
				{
					POINT pt[3] = 
					{ 
						{ rSubItem.left, rSubItem.top + 7 }, 
						{ rSubItem.left, rSubItem.top }, 
						{ rSubItem.left + 7, rSubItem.top } 
					};
					
					HGDIOBJ hOldBr = pDC->SelectObject(brTag);
					pDC->SelectStockObject(NULL_PEN);
					
					pDC->Polygon(pt, 3);
					pDC->SelectObject(hOldBr);
					
					// a black line between the two
					pDC->SelectStockObject(BLACK_PEN);
					pDC->MoveTo(rSubItem.left, rSubItem.top + 6);
					pDC->LineTo(rSubItem.left + 7, rSubItem.top - 1);
				}
				
				// draw priority number over the top
				if (bHasPriority && !HasStyle(TDCS_HIDEPRIORITYNUMBER))
				{
					COLORREF color = GetPriorityColor(nPriority);
					
					// pick best colour for text
					BYTE nLum = RGBX(color).Luminance();
					COLORREF crTemp = ((nLum < 128) ? RGB(255, 255, 255) : 0);
					
					DrawColumnText(pDC, sTaskColText, rSubItem, pCol->nTextAlignment, crTemp);
				}
			}
			break;
			
		case TDCC_PERCENT:
			if (!sTaskColText.IsEmpty())
			{
				rSubItem.DeflateRect(2, 1, 3, 2);

				// draw default text first
				DrawColumnText(pDC, sTaskColText, rSubItem, pCol->nTextAlignment, crText);

				if (HasStyle(TDCS_SHOWPERCENTASPROGRESSBAR))
				{
					// if the task is done then draw in 'done' colour else priority colour
					BOOL bDone = m_calculator.IsTaskDone(pTDI, pTDS);
					
					COLORREF crBar(m_crDone);
					
					if (!bDone || !HasStyle(TDCS_DONEHAVELOWESTPRIORITY)) // determine appropriate priority
					{
						int nPriority = m_calculator.GetTaskHighestPriority(pTDI, pTDS, FALSE);
						crBar = GetPriorityColor(nPriority);
						
						// check for due
						if (m_calculator.IsTaskOverDue(pTDI, pTDS))
						{
							if (HasColor(m_crDueToday) && m_calculator.IsTaskDueToday(pTDI, pTDS))
							{
								crBar = m_crDueToday;
							}
							else if (HasColor(m_crDue))
							{
								crBar = m_crDue;
							}
						}
					}
					
					if (HasColor(crBar))
					{
						CRect rProgress(rSubItem);
						
						// draw border
						COLORREF crBorder = GraphicsMisc::Darker(crBar, 0.5);
						GraphicsMisc::DrawRect(pDC, rProgress, CLR_NONE, crBorder);
						
						// draw fill
						int nPercent = m_calculator.GetTaskPercentDone(pTDI, pTDS);

						rProgress.DeflateRect(1, 1);
						rProgress.right = rProgress.left + MulDiv(rProgress.Width(), nPercent, 100);
						
						if (rProgress.Width() > 0)
						{
							pDC->FillSolidRect(rProgress, crBar); 
							
							// Exclude the 'unfilled' part so that we do not
							// overwrite the text
							CRect rUnfilled(rSubItem);
							rUnfilled.left = rProgress.right;

							pDC->ExcludeClipRect(rUnfilled);
							
							// draw text in colour to suit progress bar
							COLORREF crTemp = GraphicsMisc::GetBestTextColor(crBar);
							DrawColumnText(pDC, sTaskColText, rSubItem, pCol->nTextAlignment, crTemp);
						}
					}
				}
				
			}
			break;
			
		case TDCC_TRACKTIME:
			if (m_dwTimeTrackTaskID == dwTrueID)
				DrawColumnImage(pDC, nColID, rSubItem);
			break;
			
		case TDCC_FLAG:
			if (pTDI->bFlagged)
			{
				DrawColumnImage(pDC, nColID, rSubItem);
			}
			else if (m_calculator.IsTaskFlagged(pTDI, pTDS))
			{
				DrawColumnImage(pDC, nColID, rSubItem, TRUE);
			}
			break;
			
		case TDCC_LOCK:
			if (pTDI->bLocked)
			{
				DrawColumnImage(pDC, nColID, rSubItem);
			}
			else if (m_calculator.IsTaskLocked(pTDI, pTDS))
			{
				DrawColumnImage(pDC, nColID, rSubItem, TRUE);
			}
			break;

		case TDCC_REMINDER:
			{
				time_t tRem = GetTaskReminder(dwTrueID);

				// Reminder must be set and start/due date must be set
				if (tRem != 0)
				{
					if (tRem != -1)
					{
						if (HasStyle(TDCS_SHOWREMINDERSASDATEANDTIME))
						{
							COleDateTime dtRem(tRem);

							if (CDateHelper::IsDateSet(dtRem))
							{
								DrawColumnDate(pDC, dtRem, TDCD_CUSTOM, rSubItem, crText);
							}
						}
						else
						{
							DrawColumnImage(pDC, nColID, rSubItem);
						}
					}
					else
					{
						DrawColumnImage(pDC, nColID, rSubItem, TRUE);
					}
				}
			}
			break;
			
		case TDCC_STARTDATE:
			{
				COleDateTime date;
				BOOL bDone = m_calculator.IsTaskDone(pTDI, pTDS);
				BOOL bCalculated = FALSE;
				
				if (bDone && !HasStyle(TDCS_HIDESTARTDUEFORDONETASKS))
				{
					date = pTDI->dateStart;
				}
				else if (!bDone)
				{
					date = m_calculator.GetTaskStartDate(pTDI, pTDS);
					bCalculated = (date != pTDI->dateStart);
				}
				
				DrawColumnDate(pDC, date, TDCD_START, rSubItem, crText, bCalculated);
			}
			break;
			
		case TDCC_DUEDATE:
			{
				COleDateTime date;
				BOOL bDone = m_calculator.IsTaskDone(pTDI, pTDS);
				BOOL bCalculated = FALSE;
				
				if (bDone && !HasStyle(TDCS_HIDESTARTDUEFORDONETASKS))
				{
					date = pTDI->dateDue;
				}
				else if (!bDone)
				{
					date = m_calculator.GetTaskDueDate(pTDI, pTDS);
					bCalculated = (date != pTDI->dateDue);
				}
				
				DrawColumnDate(pDC, date, TDCD_DUE, rSubItem, crText, bCalculated);
			}
			break;
			
			
		case TDCC_DONEDATE:
			DrawColumnDate(pDC, pTDI->dateDone, TDCD_DONE, rSubItem, crText);
			break;
			
		case TDCC_CREATIONDATE:
			DrawColumnDate(pDC, pTDI->dateCreated, TDCD_CREATE, rSubItem, crText);
			break;
			
		case TDCC_LASTMODDATE:
			DrawColumnDate(pDC, m_calculator.GetTaskLastModifiedDate(pTDI, pTDS), TDCD_LASTMOD, rSubItem, crText);
			break;
			
		case TDCC_ICON:
			{
				int nIcon = GetTaskIconIndex(pTDI, pTDS);
												
				if (nIcon >= 0)
				{
					int nImageSize = m_ilTaskIcons.GetImageSize();
					CPoint pt(CalcColumnIconTopLeft(rSubItem, nImageSize));

					m_ilTaskIcons.Draw(pDC, nIcon, pt, ILD_TRANSPARENT);
				}
			}
			break;
			
		case TDCC_DEPENDENCY:
			if (pTDI->aDependencies.GetSize())
				DrawColumnImage(pDC, nColID, rSubItem);
			break;
			
		case TDCC_FILEREF:
			DrawColumnFileLinks(pDC, pTDI->aFileLinks, rSubItem, crText);
			break;
			
		case TDCC_DONE:
			{
				TTCB_CHECK nCheck = (pTDI->IsDone() ? TTCBC_CHECKED : TTCNC_UNCHECKED);

				if ((nCheck == TTCNC_UNCHECKED) && m_data.TaskHasCompletedSubtasks(pTDS))
					nCheck = TTCBC_MIXED;

				DrawColumnCheckBox(pDC, rSubItem, nCheck);
			}
			break;
			
		default:
			// custom attribute columns
			VERIFY (DrawItemCustomColumn(pTDI, pTDS, nColID, pDC, rSubItem, crText));
			break;
		}
	}
}

int CTDLTaskCtrlBase::GetTaskIconIndex(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;

	if (!m_data.GetTrueTask(dwTaskID, pTDI, pTDS))
		return -1;

	return GetTaskIconIndex(pTDI, pTDS);
}

int CTDLTaskCtrlBase::GetTaskIconIndex(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	int nIcon = m_ilTaskIcons.GetImageIndex(pTDI->sIcon);

	if ((nIcon == -1) && pTDS->HasSubTasks() && HasStyle(TDCS_SHOWPARENTSASFOLDERS))
		nIcon = 0;

	return nIcon;
}

void CTDLTaskCtrlBase::DrawColumnFileLinks(CDC* pDC, const CStringArray& aFileLinks, const CRect& rect, COLORREF crText)
{
	int nNumFiles = aFileLinks.GetSize();

	switch (nNumFiles)
	{
	case 0:
		break;

	case 1:
		// TDCS_SHOWNONFILEREFSASTEXT only works for one file
		if (HasStyle(TDCS_SHOWNONFILEREFSASTEXT))
		{
			CString sFileRef = aFileLinks[0];
			int nImage = m_ilFileRef.GetFileImageIndex(sFileRef, TRUE);
			
			if (nImage == -1)
			{
				DrawColumnText(pDC, sFileRef, rect, DT_LEFT, crText);
				break;
			}
		}
		// else fall thru
		
	default:
		{
			// Everything else
			for (int nFile = 0; nFile < nNumFiles; nFile++)
			{
				CRect rIcon;
				
				if (!CalcFileIconRect(rect, rIcon, nFile, nNumFiles))
					break; // out of bounds
				
				// first check for a tdl://
				CString sFileRef = aFileLinks[nFile];
				
				if (sFileRef.Find(TDL_PROTOCOL) != -1)
				{
					// draw our app icon 
					if (m_imageIcons.HasIcon(APP_ICON) || 
						m_imageIcons.Add(APP_ICON, GraphicsMisc::GetAppWindowIcon(FALSE)))
					{
						m_imageIcons.Draw(pDC, APP_ICON, rIcon.TopLeft());
					}
				}
				else
				{
					// get the associated image, failing if necessary
					sFileRef.Remove('\"'); // remove quotes
					FileMisc::MakeFullPath(sFileRef, m_sTasklistFolder);
					
					if (m_imageIcons.HasIcon(sFileRef) || 
						(CEnBitmap::IsSupportedImageFile(sFileRef) && 
						FileMisc::PathExists(sFileRef) &&
						m_imageIcons.Add(sFileRef, sFileRef)))
					{
						m_imageIcons.Draw(pDC, sFileRef, rIcon.TopLeft());
					}
					else
					{
						m_ilFileRef.Draw(pDC, sFileRef, rIcon.TopLeft());
					}
				}
			}
		}
		break;
	}
}

void CTDLTaskCtrlBase::DrawColumnImage(CDC* pDC, TDC_COLUMN nColID, const CRect& rect, BOOL bAlternate)
{
	const TDCCOLUMN* pCol = GetColumn(nColID);
	ASSERT(pCol);

	if (pCol)
	{
		TDCC_IMAGE iImage = (bAlternate ? pCol->iAlternateImage : pCol->iImage);
		ASSERT(iImage != TDCC_NONE);
	
		if (iImage != TDCC_NONE)
		{
			int nImageSize = m_ilColSymbols.GetImageSize();
			CPoint ptDraw(CalcColumnIconTopLeft(rect, nImageSize, iImage));

			m_ilColSymbols.Draw(pDC, iImage, ptDraw, ILD_TRANSPARENT);
		}
	}
}

void CTDLTaskCtrlBase::DrawColumnCheckBox(CDC* pDC, const CRect& rSubItem, TTCB_CHECK nCheck)
{
	int nImageSize = m_ilCheckboxes.GetImageSize();
	CPoint pt(CalcColumnIconTopLeft(rSubItem, nImageSize));
				
	// if the line height is odd, move one pixel down
	// to avoid collision with selection rect
	pt.y += (rSubItem.Height() % 2);

	int nImage = (nCheck + 1); // first image is blank
	m_ilCheckboxes.Draw(pDC, nImage, pt, ILD_TRANSPARENT);
}

CPoint CTDLTaskCtrlBase::CalcColumnIconTopLeft(const CRect& rSubItem, int nImageSize, int nImage, int nCount) const
{
	CRect rImage(rSubItem.TopLeft(), CSize(nImageSize, nImageSize));
	GraphicsMisc::CentreRect(rImage, rSubItem, (nCount == 1), TRUE);
	
	if (nCount > 1)
		rImage.OffsetRect((nImage * (nImageSize + 1)), 0);

	return rImage.TopLeft();
}

BOOL CTDLTaskCtrlBase::CalcFileIconRect(const CRect& rSubItem, CRect& rIcon, int nImage, int nCount) const
{
	int nImageSize = m_ilFileRef.GetImageSize();

	rIcon = CRect(CalcColumnIconTopLeft(rSubItem, nImageSize, nImage, nCount), CSize(nImageSize, nImageSize));

	// we always draw the first icon
	if ((nImage == 0) || (rIcon.right <= rSubItem.right))
		return TRUE;

	// else
	rIcon.SetRectEmpty();
	return FALSE;
}

BOOL CTDLTaskCtrlBase::DrawItemCustomColumn(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, TDC_COLUMN nColID, 
											CDC* pDC, const CRect& rSubItem, COLORREF crText)
{
	if (!CTDCCustomAttributeHelper::IsCustomColumn(nColID))
		return FALSE;

	TDCCUSTOMATTRIBUTEDEFINITION attribDef;
	
	if (!CTDCCustomAttributeHelper::GetAttributeDef(nColID, m_aCustomAttribDefs, attribDef))
		return TRUE;

	TDCCADATA data;
	pTDI->GetCustomAttributeValue(attribDef.sUniqueID, data);

	CRect rCol(rSubItem);
	
	switch (attribDef.GetDataType())
	{
	case TDCCA_DATE:
		{
			double dDate = 0.0;
			m_calculator.GetTaskCustomAttributeData(pTDI, pTDS, attribDef, dDate);

			DrawColumnDate(pDC, dDate, TDCD_CUSTOM, rCol, crText, FALSE, 
							attribDef.HasFeature(TDCCAF_SHOWTIME), attribDef.nTextAlignment);
		}
		break;
		
	case TDCCA_DOUBLE:
	case TDCCA_INTEGER:
		{
			double dValue = 0.0;
			m_calculator.GetTaskCustomAttributeData(pTDI, pTDS, attribDef, dValue);
			
			if ((dValue != 0.0) || !attribDef.HasFeature(TDCCAF_HIDEZERO))
			{
				CString sText(Misc::Format(dValue, (attribDef.IsDataType(TDCCA_DOUBLE) ? 2 : 0)));
				DrawColumnText(pDC, sText, rCol, attribDef.nTextAlignment, crText);
			}			
		}
		break;

	case TDCCA_TIMEPERIOD:
		{
			double dValue = 0.0;
			TDC_UNITS nUnits = data.GetTimeUnits();

			m_calculator.GetTaskCustomAttributeData(pTDI, pTDS, attribDef, dValue, nUnits);

			CString sText(FormatTimeValue(dValue, nUnits, TRUE));
			DrawColumnText(pDC, sText, rCol, attribDef.nTextAlignment, crText);
		}
		break;

	case TDCCA_ICON:
		if (!data.IsEmpty() && (rCol.Width() > CalcRequiredIconColumnWidth(1)))
		{
			CStringArray aImages;
			int nNumImage = data.AsArray(aImages);

			int nReqWidth = CalcRequiredIconColumnWidth(nNumImage);
			int nAvailWidth = rCol.Width();

			if (nAvailWidth < nReqWidth)
			{
				nNumImage = min(nNumImage, ((nAvailWidth + COL_ICON_SPACING - (LV_COLPADDING * 2)) / (COL_ICON_SIZE + COL_ICON_SPACING)));
				nReqWidth = CalcRequiredIconColumnWidth(nNumImage);
			}

			CString sName;
			
			if (nNumImage == 1)
				sName = attribDef.GetImageName(data.AsString());

			rCol.bottom = (rCol.top + COL_ICON_SIZE);
			GraphicsMisc::CentreRect(rCol, rSubItem, FALSE, TRUE); // centre vertically

			int nTextAlign = attribDef.nTextAlignment;
			
			switch (nTextAlign)
			{
			case DT_RIGHT:
				// We still draw from the left just like text
				rCol.left = (rCol.right - nReqWidth);
				break;
				
			case DT_CENTER:
				// if there is associated text then we align left
				if (sName.IsEmpty())
				{
					rCol.right = (rCol.left + nReqWidth);
					GraphicsMisc::CentreRect(rCol, rSubItem, TRUE, FALSE);
				}
				else 
				{
					nTextAlign = DT_LEFT;
				}
				break;
				
			case DT_LEFT:
			default:
				break;
			}

			BOOL bOverrun = FALSE;
			rCol.left += LV_COLPADDING;

			for (int nImg = 0; ((nImg < nNumImage) && !bOverrun); nImg++)
			{
				CString sImage, sDummy;

				if (TDCCUSTOMATTRIBUTEDEFINITION::DecodeImageTag(aImages[nImg], sImage, sDummy))
				{
					m_ilTaskIcons.Draw(pDC, sImage, rCol.TopLeft(), ILD_TRANSPARENT);
					rCol.left += (COL_ICON_SIZE + COL_ICON_SPACING);

					bOverrun = ((rCol.left + COL_ICON_SIZE) > rCol.right);
				}
			}
			
			// optional text for single list images
			if (!bOverrun && (nNumImage == 1) && attribDef.IsList() && !sName.IsEmpty())
			{
				DrawColumnText(pDC, sName, rCol, nTextAlign, crText);
			}
		}
		break;
		
	case TDCCA_BOOL:
		DrawColumnCheckBox(pDC, rSubItem, (data.AsBool() ? TTCBC_CHECKED : TTCNC_UNCHECKED));
		break;

	case TDCCA_FILELINK:
		{
			CStringArray aItems;
			
			if (data.AsArray(aItems))
				DrawColumnFileLinks(pDC, aItems, rSubItem, crText);
		}
		break;

	default:
		if (!data.IsEmpty())
		{
			if (attribDef.IsMultiList())
				DrawColumnText(pDC, data.FormatAsArray(), rCol, attribDef.nTextAlignment, crText);
			else
				DrawColumnText(pDC, data.AsString(), rCol, attribDef.nTextAlignment, crText);
		}
		break;
	}

	return TRUE; // we handled it
}

int CTDLTaskCtrlBase::CalcRequiredIconColumnWidth(int nNumImage)
{
	return ((nNumImage * (COL_ICON_SIZE + COL_ICON_SPACING)) - COL_ICON_SPACING + (LV_COLPADDING * 2));
}

BOOL CTDLTaskCtrlBase::FormatDate(const COleDateTime& date, TDC_DATE nDate, CString& sDate, CString& sTime, CString& sDow, BOOL bCustomWantsTime) const
{	
	sDate.Empty();
	sTime.Empty();
	sDow.Empty();
	
	if (CDateHelper::IsDateSet(date))
	{
		DWORD dwFmt = 0;
		
		if (HasStyle(TDCS_SHOWWEEKDAYINDATES))
			dwFmt |= DHFD_DOW;
		
		if (HasStyle(TDCS_SHOWDATESINISO))
			dwFmt |= DHFD_ISO;
		
		BOOL bWantDrawTime = WantDrawColumnTime(nDate, bCustomWantsTime);

		if (bWantDrawTime && CDateHelper::DateHasTime(date))
			dwFmt |= DHFD_TIME | DHFD_NOSEC;
		
		if (CDateHelper::FormatDate(date, dwFmt, sDate, sTime, sDow))
		{
			// Substitute 'calculated' time if none supplied
			if (bWantDrawTime && sTime.IsEmpty())
			{
				int nHour = ((nDate == TDCD_DUE) ? 23 : 0);
				int nMin = ((nDate == TDCD_DUE) ? 59 : 0);

				sTime = CTimeHelper::FormatClockTime(nHour, nMin, 0, FALSE, HasStyle(TDCS_SHOWDATESINISO));
			}

			return TRUE;
		}
	}
	
	// else
	return FALSE;
}

void CTDLTaskCtrlBase::DrawColumnDate(CDC* pDC, const COleDateTime& date, TDC_DATE nDate, const CRect& rect, 
										COLORREF crText, BOOL bCalculated, BOOL bCustomWantsTime, int nAlign)
{
	CString sDate, sTime, sDow;

	if (!FormatDate(date, nDate, sDate, sTime, sDow, bCustomWantsTime))
		return; // nothing to do
	
	// Work out how much space we need
	COleDateTime dateMax(2000, 12, 31, 23, 59, 0);
	
	CString sDateMax, sTimeMax, sDummy;
	FormatDate(dateMax, nDate, sDateMax, sTimeMax, sDummy, bCustomWantsTime);
	
	// Always want date
	int nMaxDate = pDC->GetTextExtent(sDateMax).cx, nReqWidth = nMaxDate;

	// If there's too little space even for the date then 
	// switch to left alignment so that the month and day are visible
	BOOL bWantDrawTime = FALSE, bWantDrawDOW = FALSE;
	int nMaxTime = 0;
	int nDateAlign = DT_RIGHT;

	if (nReqWidth > rect.Width())
	{
		nDateAlign = DT_LEFT;
	}
	else
	{
		int nSpace = pDC->GetTextExtent(_T(" ")).cx;
		nMaxDate += nSpace;

		// Check for time
		if (!sTime.IsEmpty())
		{
			nMaxTime = pDC->GetTextExtent(sTimeMax).cx;

			if ((nReqWidth + nSpace + nMaxTime) < rect.Width())
			{	
				nMaxTime += nSpace;
				nReqWidth += nMaxTime;
				bWantDrawTime = TRUE;
			}
		}
	
		// Check for day of week
		if (!sDow.IsEmpty())
		{
			int nMaxDOW = CDateHelper::CalcLongestDayOfWeekName(pDC, TRUE);

			if ((nReqWidth + nSpace + nMaxDOW) < rect.Width())
			{
				nMaxDOW += nSpace;
				nReqWidth += nMaxDOW;
				bWantDrawDOW = TRUE;
			}
		}
	}
	
	// Draw calculated dates in a lighter colour
	if (bCalculated && !Misc::IsHighContrastActive())
	{
		crText = (HasColor(crText) ? crText : pDC->GetTextColor());
		crText = GraphicsMisc::Lighter(crText, 0.5);
	}

	// We always draw FROM THE RIGHT and with each component 
	// aligned to the right
	CRect rDraw(rect);

	switch (nAlign)
	{
	case DT_LEFT:
		rDraw.right = min(rDraw.right, (rDraw.left + nReqWidth));
		break;

	case DT_RIGHT:
		break;

	case DT_CENTER:
		rDraw.right = min(rDraw.right, (rDraw.CenterPoint().x + (nReqWidth / 2)));
		break;
	}

	// draw time first
	if (bWantDrawTime)
	{
		// if NO time component, render 'default' start and due time 
		// in a lighter colour to indicate it wasn't user-set
		COLORREF crTime(crText);
		
		if (!CDateHelper::DateHasTime(date))
		{
			// Note: If we've already calculated it above we need not again
			if (!bCalculated && !Misc::IsHighContrastActive())
			{
				if (!HasColor(crTime))
					crTime = pDC->GetTextColor();

				crTime = GraphicsMisc::Lighter(crTime, 0.5);
			}
		}
		
		// draw and adjust rect
		if (!sTime.IsEmpty())
		{
			DrawColumnText(pDC, sTime, rDraw, TA_RIGHT, crTime);
			rDraw.right -= nMaxTime;
		}
	}
	
	DrawColumnText(pDC, sDate, rDraw, nDateAlign, crText);
	rDraw.right -= nMaxDate;
	
	// then dow
	if (bWantDrawDOW)
		DrawColumnText(pDC, sDow, rDraw, TA_RIGHT, crText);
}

void CTDLTaskCtrlBase::DrawColumnText(CDC* pDC, const CString& sText, const CRect& rect, int nAlign, 
										COLORREF crText, BOOL bTaskTitle, int nTextLen)
{
	ASSERT(crText != CLR_NONE);

	if (sText.IsEmpty())
		return;

	if (nTextLen == -1)
		nTextLen = sText.GetLength();
	
	CRect rText(rect);
	CPoint ptText(0, rText.top);
	
	if (!bTaskTitle)
	{
		switch (nAlign)
		{
		case DT_LEFT:
			rText.left += LV_COLPADDING;
			break;
			
		case DT_RIGHT:
			rText.right -= LV_COLPADDING;
			break;
			
		case DT_CENTER:
			break;
		}
	}
	
	UINT nFlags = (nAlign | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | GraphicsMisc::GetRTLDrawTextFlags(Tasks()));

	if (!m_bSavingToImage && bTaskTitle)
		nFlags |= DT_END_ELLIPSIS;

	COLORREF crOld = pDC->SetTextColor(crText);
	
	pDC->SetBkMode(TRANSPARENT);
	pDC->DrawText(sText, nTextLen, rText, nFlags);
	
	// cleanup
	if (HasColor(crOld))
		pDC->SetTextColor(crOld);
}

LRESULT CTDLTaskCtrlBase::OnHeaderCustomDraw(NMCUSTOMDRAW* pNMCD)
{
	switch (pNMCD->dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;
		
	case CDDS_ITEMPREPAINT:
		{
			// don't draw columns having min width
			CRect rItem(pNMCD->rc);

			if (rItem.Width() > MIN_COL_WIDTH)
				return CDRF_NOTIFYPOSTPAINT;
		}
		break;

	case CDDS_ITEMPOSTPAINT:
		{
			// don't draw columns having min width
			CRect rItem(pNMCD->rc);

			if (rItem.Width() > MIN_COL_WIDTH)
			{
				CDC* pDC = CDC::FromHandle(pNMCD->hdc);
				CFont* pFont = NULL;

				// draw sort direction
				int nCol = (int)pNMCD->dwItemSpec;
				TDC_COLUMN nColID = (TDC_COLUMN)pNMCD->lItemlParam;

				if (nColID == m_nSortColID)
				{
					BOOL bUp = (m_nSortDir == TDC_SORTUP);
					GetColumnHeaderCtrl(nColID).DrawItemSortArrow(pDC, nCol, bUp);
				}

				const TDCCOLUMN* pTDCC = GetColumn(nColID);
				int nAlignment = DT_LEFT;
				
				if (pTDCC)
				{
					nAlignment = pTDCC->GetColumnHeaderAlignment();

					// handle symbol images
					if (pTDCC->iImage != -1)
					{
						CRect rImage(0, 0, COL_ICON_SIZE, COL_ICON_SIZE);
						GraphicsMisc::CentreRect(rImage, rItem, TRUE, TRUE);

 						m_ilColSymbols.Draw(pDC, pTDCC->iImage, rImage.TopLeft(), ILD_TRANSPARENT);
						return CDRF_SKIPDEFAULT;
					}
				}

				// Handle RTL text column headers
				if (GraphicsMisc::GetRTLDrawTextFlags(pNMCD->hdr.hwndFrom) == DT_RTLREADING)
				{
					CString sColumn(GetColumnHeaderCtrl(nColID).GetItemText(nCol));
					DrawColumnText(pDC, sColumn, pNMCD->rc, nAlignment, GetSysColor(COLOR_WINDOWTEXT));
					
					return CDRF_SKIPDEFAULT;
				}
			}
		}
		break;
	}
	
	return CDRF_DODEFAULT;
}

const CEnHeaderCtrl& CTDLTaskCtrlBase::GetColumnHeaderCtrl(TDC_COLUMN nColID) const
{
	return ((nColID == TDCC_CLIENT) ? m_hdrTasks : m_hdrColumns);
}

const TDCCOLUMN* CTDLTaskCtrlBase::GetColumn(TDC_COLUMN nColID)
{
	if (CTDCCustomAttributeHelper::IsCustomColumn(nColID))
		return NULL;

	ASSERT(!s_mapColumns.IsEmpty());

	const TDCCOLUMN* pCol = NULL;
	VERIFY(s_mapColumns.Lookup(nColID, pCol));

	ASSERT(pCol);
	return pCol;
}

TDC_COLUMN CTDLTaskCtrlBase::GetColumnID(int nCol) const
{
	switch (nCol)
	{
	case 0:
		// zero is always 'tasks'
		return TDCC_CLIENT;

	default:
		if (nCol > 0)
		{
			return (TDC_COLUMN)m_hdrColumns.GetItemData(nCol);
		}
		break;
	}

	ASSERT(0);
	return TDCC_NONE;
}


void CTDLTaskCtrlBase::SetCompletionStatus(const CString& sStatus) 
{ 
	if (sStatus != m_sCompletionStatus)
	{
		m_sCompletionStatus = sStatus; 

		if (IsColumnShowing(TDCC_STATUS))
			RedrawColumn(TDCC_STATUS);
	}
}

CString CTDLTaskCtrlBase::GetTaskColumnText(DWORD dwTaskID, 
	const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, TDC_COLUMN nColID) const
{
	ASSERT(pTDS && pTDI && dwTaskID && (nColID != TDCC_NONE));

	if (!(pTDS && pTDI && dwTaskID && (nColID != TDCC_NONE)))
		return _T("");

	CString sTaskColText;
		
	switch (nColID)
	{
	case TDCC_CLIENT:
		sTaskColText = pTDI->sTitle;
		break;

	case TDCC_POSITION:
		sTaskColText = m_formatter.GetTaskPosition(pTDS);
		break;

	case TDCC_PRIORITY:
		// priority color
		if (!HasStyle(TDCS_DONEHAVELOWESTPRIORITY) || !m_calculator.IsTaskDone(pTDI, pTDS))
		{
			int nPriority = m_calculator.GetTaskHighestPriority(pTDI, pTDS, FALSE);
			BOOL bHasPriority = (nPriority != FM_NOPRIORITY);

			// draw priority number over the top
			if (bHasPriority && !HasStyle(TDCS_HIDEPRIORITYNUMBER))
				sTaskColText = Misc::Format(nPriority);
		}
		break;

	case TDCC_RISK:
		if (HasStyle(TDCS_INCLUDEDONEINRISKCALC) || !m_calculator.IsTaskDone(pTDI, pTDS))
		{
			int nRisk = m_calculator.GetTaskHighestRisk(pTDI, pTDS);

			if (nRisk != FM_NORISK)
				sTaskColText = Misc::Format(nRisk);
		}
		break;

	case TDCC_RECURRENCE:
		sTaskColText = pTDI->trRecurrence.GetRegularityText(FALSE);
		break;

	case TDCC_ID:
		// Figure out is this is really a reference
		if (pTDS->GetTaskID() != dwTaskID)
			sTaskColText.Format(_T("(%lu) %lu"), pTDS->GetTaskID(), dwTaskID);
		else
			sTaskColText.Format(_T("%lu"), dwTaskID);
		break;

	case TDCC_PARENTID:
		sTaskColText = Misc::Format(pTDS->GetParentTaskID());
		break;

	case TDCC_RECENTEDIT:
		if (m_calculator.IsTaskRecentlyModified(pTDI, pTDS))
			sTaskColText = _T("*");
		break;

	case TDCC_COST:
		{
			double dCost = m_calculator.GetTaskCost(pTDI, pTDS);

			if (dCost != 0.0 || !HasStyle(TDCS_HIDEZEROTIMECOST))
				sTaskColText = Misc::Format(dCost, 2);
		}
		break;

	case TDCC_EXTERNALID:
		sTaskColText = pTDI->sExternalID;
		break;

	case TDCC_VERSION:
		sTaskColText = pTDI->sVersion;
		break;

	case TDCC_LASTMODBY:
		sTaskColText = pTDI->sLastModifiedBy;
		break;

	case TDCC_ALLOCTO:
		sTaskColText = m_formatter.GetTaskAllocTo(pTDI);
		break;

	case TDCC_ALLOCBY:
		sTaskColText = pTDI->sAllocBy;
		break;

	case TDCC_STATUS:
		{
			sTaskColText = pTDI->sStatus;

			// if a task is completed and has no status and the completion status
			// has been specified then draw the completion status
			if (sTaskColText.IsEmpty() && !m_sCompletionStatus.IsEmpty() && m_calculator.IsTaskDone(pTDI, pTDS))
				sTaskColText = m_sCompletionStatus;
		}
		break;

	case TDCC_CATEGORY:
		sTaskColText = m_formatter.GetTaskCategories(pTDI);
		break;

	case TDCC_TAGS:
		sTaskColText = m_formatter.GetTaskTags(pTDI);
		break;

	case TDCC_CREATEDBY:
		sTaskColText = pTDI->sCreatedBy;
		break;

	case TDCC_PERCENT:
		{
			if (HasStyle(TDCS_HIDEPERCENTFORDONETASKS) && 
				m_calculator.IsTaskDone(pTDI, pTDS))
			{
				break; // nothing to do
			}

			int nPercent = m_calculator.GetTaskPercentDone(pTDI, pTDS);

			if (!nPercent && HasStyle(TDCS_HIDEZEROPERCENTDONE))
				break; // nothing to do

			sTaskColText = Misc::Format(nPercent, _T("%"));
		}
		break;

	case TDCC_REMAINING:
		{
			TDC_UNITS nUnits = TDCU_NULL;

			double dRemaining = m_calculator.GetTaskRemainingTime(pTDI, pTDS, nUnits);

			if (nUnits == TDCU_NULL)
			{
				if (HasStyle(TDCS_HIDEZEROTIMECOST))
					break;

				// else
				ASSERT(dRemaining == 0.0);
				nUnits = pTDI->nTimeEstUnits;
			}

			// format appropriately
			TH_UNITS nTHUnits = TDC::MapUnitsToTHUnits(nUnits);

			if (HasStyle(TDCS_CALCREMAININGTIMEBYPERCENT) || HasStyle(TDCS_CALCREMAININGTIMEBYSPENT))
			{
				sTaskColText = CTimeHelper().FormatTime(dRemaining, nTHUnits, 1);
			}
			else // TDCS_CALCREMAININGTIMEBYDUEDATE
			{
				COleDateTime date = m_calculator.GetTaskDueDate(pTDI, pTDS);

				if (CDateHelper::IsDateSet(date)) 
				{
					if (HasStyle(TDCS_DISPLAYHMSTIMEFORMAT))
					{
						sTaskColText = CTimeHelper().FormatTimeHMS(dRemaining, THU_DAYS, TRUE);
					}
					else
					{
						// find best units for display
						if (fabs(dRemaining) >= 1.0)
						{
							sTaskColText = CTimeHelper().FormatTime(dRemaining, THU_DAYS, 1);
						}
						else
						{
							dRemaining *= 24; // to hours

							if (fabs(dRemaining) >= 1.0)
							{
								sTaskColText = CTimeHelper().FormatTime(dRemaining, THU_HOURS, 1);
							}
							else
							{
								dRemaining *= 60; // to mins
								sTaskColText = CTimeHelper().FormatTime(dRemaining, THU_MINS, 0);
							}
						}
					}
				}
			}
		}
		break;

	case TDCC_TIMEEST:
	case TDCC_TIMESPENT:
		{
			BOOL bTimeEst = (nColID == TDCC_TIMEEST);
			TDC_UNITS nUnits = (bTimeEst ? m_nDefTimeEstUnits : m_nDefTimeSpentUnits); // good default value

			// get actual task time units
			if (!pTDS->HasSubTasks() || HasStyle(TDCS_ALLOWPARENTTIMETRACKING))
				nUnits = pTDI->GetTimeUnits(bTimeEst);

			// draw time
			double dTime = (bTimeEst ? m_calculator.GetTaskTimeEstimate(pTDI, pTDS, nUnits) :
										m_calculator.GetTaskTimeSpent(pTDI, pTDS, nUnits));

			sTaskColText = FormatTimeValue(dTime, nUnits, !bTimeEst);
		}
		break;

	case TDCC_PATH:
		sTaskColText = m_formatter.GetTaskPath(pTDI, pTDS);
		break;

		// items having no text or rendered differently
	case TDCC_STARTDATE:
	case TDCC_DUEDATE:
	case TDCC_DONEDATE:
	case TDCC_CREATIONDATE:
	case TDCC_LASTMODDATE:
	case TDCC_ICON:
	case TDCC_DEPENDENCY:
	case TDCC_DONE:
	case TDCC_TRACKTIME:
	case TDCC_FLAG:
	case TDCC_LOCK:
	case TDCC_REMINDER:
	case TDCC_FILEREF:
		break;

	case TDCC_SUBTASKDONE:
		sTaskColText = m_formatter.GetTaskSubtaskCompletion(pTDI, pTDS);
		break;

	case TDCC_COMMENTSSIZE:
		{
			float fSize = pTDI->GetCommentsSizeInKB();

			if (fSize >= 1)
			{
				sTaskColText = Misc::Format(max(1, (int)fSize));
			}
			else if (fSize > 0)
			{
				sTaskColText = _T(">0");
			}
		}
		break;

	default:
		// handled during drawing
		ASSERT(CTDCCustomAttributeHelper::IsCustomColumn(nColID));
		break;
	}
	
	return sTaskColText;
}

CString CTDLTaskCtrlBase::FormatTimeValue(double dTime, TDC_UNITS nUnits, BOOL bAllowNegative) const
{
	CString sTime;

	// first handle zero times
	if ((dTime == 0.0) && HasStyle(TDCS_HIDEZEROTIMECOST))
	{
		// do nothing
	}
	// then check for negative times
	else if (!bAllowNegative && (dTime < 0.0))
	{
		// do nothing
	}
	else
	{
		int nDecPlaces = HasStyle(TDCS_ROUNDTIMEFRACTIONS) ? 0 : 2;
		TH_UNITS nTHUnits = TDC::MapUnitsToTHUnits(nUnits);

		if (HasStyle(TDCS_DISPLAYHMSTIMEFORMAT))
			sTime = CTimeHelper().FormatTimeHMS(dTime, nTHUnits, (BOOL)nDecPlaces);
		else
			sTime = CTimeHelper().FormatTime(dTime, nTHUnits, nDecPlaces);
	}

	return sTime;
}

// message and notifications for 'us'
LRESULT CTDLTaskCtrlBase::WindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (!IsResyncEnabled())
		return CTreeListSyncer::WindowProc(hRealWnd, msg, wp, lp);
	
	ASSERT(hRealWnd == GetHwnd()); // 'us'

	switch (msg)
	{
	case WM_CONTEXTMENU:
		if ((HWND)wp == m_scLeft.GetHwnd() || 
			(HWND)wp == m_scRight.GetHwnd() ||
			(HWND)wp == m_hdrTasks.GetSafeHwnd() ||
			(HWND)wp == m_hdrColumns.GetSafeHwnd())
		{
			// pass on to parent
			::SendMessage(GetHwnd(), WM_CONTEXTMENU, (WPARAM)hRealWnd, lp);

			return 0L; // eat
		}
		break;
		
	case WM_LBUTTONDBLCLK:
		{
			CPoint ptCursor(lp);
			CRect rSplitter;
			
			if (GetSplitterRect(rSplitter) && rSplitter.PtInRect(ptCursor))
			{
				AdjustSplitterToFitAttributeColumns();
				m_bAutoFitSplitter = TRUE;

				return 0L; // eat
			}
		}
		break;
		
	case WM_NOTIFY:
		// only interested in notifications from the tree/list pair to their parent
		if (wp == m_scLeft.GetDlgCtrlID() || 
			wp == m_scRight.GetDlgCtrlID() ||
			wp == (UINT)m_hdrTasks.GetDlgCtrlID() ||
			wp == (UINT)m_hdrColumns.GetDlgCtrlID())
		{
			// let base class have its turn first
			LRESULT lr = CTreeListSyncer::WindowProc(hRealWnd, msg, wp, lp);
			
			// our extra handling
			LPNMHDR pNMHDR = (LPNMHDR)lp;
			HWND hwnd = pNMHDR->hwndFrom;
			
			switch (pNMHDR->code)
			{
			case HDN_ITEMCLICK:
				// column header clicks handled in ScWindowProc
				if (hwnd == m_hdrTasks)
				{
					NMHEADER* pHDN = (NMHEADER*)pNMHDR;
					
					if (pHDN->iButton == 0)
						OnHeaderClick(TDCC_CLIENT);

					return 0L; // eat
				}
				break;
				
			case NM_RCLICK:
				// headers don't generate their own WM_CONTEXTMENU
				if (hwnd == m_hdrTasks)
				{
					// pass on to parent
					::SendMessage(GetHwnd(), WM_CONTEXTMENU, (WPARAM)hwnd, (LPARAM)::GetMessagePos());
				}
				break;

			case NM_CLICK:
				if (hwnd == m_lcColumns)
				{
					const NMITEMACTIVATE* pNMIA = (NMITEMACTIVATE*)lp;

					if (pNMIA->iItem != -1)// valid items only
					{
						DWORD dwTaskID = GetColumnItemTaskID(pNMIA->iItem); // task ID
						TDC_COLUMN nColID = GetColumnID(pNMIA->iSubItem);
						
						if (ItemColumnSupportsClickHandling(pNMIA->iItem, nColID))
						{
							if (nColID == TDCC_FILEREF)
								HandleFileLinkColumnClick(pNMIA->iItem, dwTaskID, pNMIA->ptAction);
							else
								NotifyParentOfColumnEditClick(nColID, dwTaskID);
						}
					}
				}
				break;
			}
				
			return lr;
		}
		break;
	}
	
	return CTreeListSyncer::WindowProc(hRealWnd, msg, wp, lp);
}

int CTDLTaskCtrlBase::CalcSplitterPosToFitListColumns() const
{
	int nFirst = m_hdrColumns.GetFirstVisibleItem();
	int nLast = m_hdrColumns.GetLastVisibleItem();

	if ((nFirst == -1) && (nLast == -1))
		return -1;

	CRect rFirst, rLast;
	VERIFY(m_hdrColumns.GetItemRect(nFirst, rFirst) && m_hdrColumns.GetItemRect(nLast, rLast));
	
	CRect rClient;
	CWnd::GetClientRect(rClient);

	int nNewSplitPos = 0;
	int nColsWidth = ((rLast.right - rFirst.left) + 10/*GetSplitterWidth()*/);

	if (IsRight(m_lcColumns))
	{
		if (HasVScrollBar(m_lcColumns))
			nColsWidth += GetSystemMetrics(SM_CXVSCROLL);

		nNewSplitPos = max(MIN_TASKS_WIDTH, (rClient.right - nColsWidth));
	}
	else // cols on left
	{
		nNewSplitPos = min(nColsWidth, (rClient.right - MIN_TASKS_WIDTH));
	}

	return nNewSplitPos;
}

void CTDLTaskCtrlBase::AdjustSplitterToFitAttributeColumns()
{
	int nNewSplitPos = CalcSplitterPosToFitListColumns();

	if ((nNewSplitPos != -1) && (nNewSplitPos != GetSplitPos()))
	{
		CRect rClient;
		CWnd::GetClientRect(rClient);
		
		CTreeListSyncer::Resize(rClient, nNewSplitPos);
	}
}

void CTDLTaskCtrlBase::RepackageAndSendToParent(UINT msg, WPARAM /*wp*/, LPARAM lp)
{
	// sanity check
	ASSERT(msg == WM_NOTIFY);

	if (msg == WM_NOTIFY)
	{
		NMHDR* pNMHDR = (NMHDR*)lp;
		NMHDR hdrOrg = *pNMHDR; // so we can restore after

		pNMHDR->hwndFrom = GetSafeHwnd();
		pNMHDR->idFrom = CWnd::GetDlgCtrlID();
		
		CWnd::GetParent()->SendMessage(msg, pNMHDR->idFrom, lp);

		// restore
		pNMHDR->hwndFrom = hdrOrg.hwndFrom;
		pNMHDR->idFrom = hdrOrg.idFrom;
	}
}

// messages and notifications sent to m_lcColumns 
LRESULT CTDLTaskCtrlBase::ScWindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (!IsResyncEnabled())
		return CTreeListSyncer::ScWindowProc(hRealWnd, msg, wp, lp);
	
	if (hRealWnd == Tasks())
	{
		switch (msg)
		{
		case WM_PRINT:
			if (!m_lcColumns.GetItemCount() && !m_sTasksWndPrompt.IsEmpty())
			{
				LRESULT lr = ScDefault(hRealWnd);
				CWndPrompt::DrawPrompt(hRealWnd, m_sTasksWndPrompt, (HDC)wp, TRUE);

				return lr;
			}
			break;
			
		case WM_PAINT:
			if (!m_lcColumns.GetItemCount() && !m_sTasksWndPrompt.IsEmpty())
			{
				LRESULT lr = ScDefault(hRealWnd);
				CWndPrompt::DrawPrompt(hRealWnd, m_sTasksWndPrompt, (HDC)wp, TRUE);

				return lr;
			}
			break;
			
		case WM_HSCROLL:
			// Windows only invalidates the item labels but
			// we need the whole row because we render the 
			// comments after the task text
			::InvalidateRect(hRealWnd, NULL, FALSE);
			break;

		case WM_MOUSEWHEEL:
			// Windows only invalidates the item labels but
			// we need the whole row because we render the 
			// comments after the task text
			if (HasHScrollBar(hRealWnd) && !HasVScrollBar(hRealWnd))
				::InvalidateRect(hRealWnd, NULL, FALSE);
			break;
		}
	}
	else if (hRealWnd == m_lcColumns)
	{
		switch (msg)
		{
		case WM_TIMER:
			{
				switch (wp)
				{
				case 0x2A:
				case 0x2B:
					// These are timers internal to the list view associated
					// with editing labels and which cause unwanted selection
					// changes. Given that we have disabled label editing for 
					// the attribute columns we can safely kill these timers
					::KillTimer(hRealWnd, wp);
					return TRUE;
				}
			}
			break;
		
		case WM_NOTIFY:
			{
				LPNMHDR pNMHDR = (LPNMHDR)lp;
				HWND hwnd = pNMHDR->hwndFrom;
				
				switch (pNMHDR->code)
				{
				case NM_RCLICK:
					// headers don't generate their own WM_CONTEXTMENU
					if (hwnd == m_hdrColumns)
					{
						// pass on to parent
						::SendMessage(GetHwnd(), WM_CONTEXTMENU, (WPARAM)hwnd, ::GetMessagePos());
					}
					break;

				case HDN_DIVIDERDBLCLICK:
					if (hwnd == m_hdrColumns)
					{
						// resize just that column
						int nItem = ((NMHEADER*)pNMHDR)->iItem;
						
						if (m_hdrColumns.IsItemVisible(nItem))
						{
							CClientDC dc(&m_lcColumns);
							CFont* pOldFont = GraphicsMisc::PrepareDCFont(&dc, m_lcColumns);
							int nColWidth = RecalcColumnWidth(nItem, &dc);
							
							m_hdrColumns.SetItemWidth(nItem, nColWidth);
							m_hdrColumns.SetItemTracked(nItem, FALSE); // width now auto-calc'ed
							
							dc.SelectObject(pOldFont);
						}
						return 0L;
					}

				case HDN_ITEMCLICK:
					if (hwnd == m_hdrColumns)
					{
						NMHEADER* pHDN = (NMHEADER*)pNMHDR;

						// forward on to our parent
						if ((pHDN->iButton == 0) && m_hdrColumns.IsItemVisible(pHDN->iItem))
						{
							OnHeaderClick((TDC_COLUMN)m_hdrColumns.GetItemData(pHDN->iItem));
						}
						return 0L;
					}
					break;

				case HDN_ITEMCHANGING:
					if (hwnd == m_hdrColumns)
					{
						NMHEADER* pHDN = (NMHEADER*)pNMHDR;
						
						// don't let user drag column too narrow
						// Exclude first column which is always zero width
						if (pHDN->iItem == 0)
						{
							pHDN->pitem->cxy = 0;
						}
						else if ((pHDN->iButton == 0) && (pHDN->pitem->mask & HDI_WIDTH))
						{
							TDC_COLUMN nColID = GetColumnID(pHDN->iItem);

							if (IsColumnShowing(nColID))
							{
								pHDN->pitem->cxy = max(MIN_COL_WIDTH, pHDN->pitem->cxy);
							}
						}
					}
					break;
				}
			}
			break;

 		case WM_ERASEBKGND:
			if (COSVersion() == OSV_LINUX)
			{
				CRect rClient;
				m_lcColumns.GetClientRect(rClient);

				CDC::FromHandle((HDC)wp)->FillSolidRect(rClient, GetSysColor(COLOR_WINDOW));
			}
			return TRUE;

		case WM_KEYUP:
			switch (wp)
			{
			case VK_NEXT:  
			case VK_DOWN:
			case VK_UP:
			case VK_PRIOR: 
			case VK_HOME:
			case VK_END: 
			case VK_SHIFT: 
			case VK_CONTROL: 
				// force a parent notification
				NotifyParentSelChange(SC_BYKEYBOARD);
				break;
			}
			break;
			
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
			// Don't let the selection to be set to -1 when clicking below the last item
			// BUT NOT ON Linux because it interferes with context menu handling
			if (COSVersion() != OSV_LINUX)
			{
				// let parent handle any focus changes first
				m_lcColumns.SetFocus();

				// don't let the selection to be set to -1
				// when clicking below the last item
				if (m_lcColumns.HitTest(lp) == -1)
				{
					CPoint pt(lp);
					::ClientToScreen(hRealWnd, &pt);

					// we don't want to disable drag selecting
					if (!::DragDetect(m_lcColumns, pt))
					{
						TRACE(_T("Ate Listview ButtonDown\n"));
						return 0; // eat it
					}
				}
			}
			break;
				
		case WM_LBUTTONDOWN:
			if (HandleListLBtnDown(m_lcColumns, lp))
				return 0L; // eat it
			break;
		}
	}
	
	return CTreeListSyncer::ScWindowProc(hRealWnd, msg, wp, lp);
}

void CTDLTaskCtrlBase::HandleFileLinkColumnClick(int nItem, DWORD dwTaskID, CPoint pt)
{
	const TODOITEM* pTDI = GetTask(dwTaskID);
	ASSERT(pTDI);
	
	if (pTDI == NULL)
		return;
	
	int nNumFiles = pTDI->aFileLinks.GetSize();
	
	switch (nNumFiles)
	{
	case 0:
		break;
		
	case 1:
		ShowFileLink(pTDI->aFileLinks[0]);
		break;
		
	default: // > 1 file links
		{
			int nCol = m_hdrColumns.FindItem(TDCC_FILEREF);
			ASSERT(nCol != -1);
			ASSERT(m_hdrColumns.IsItemVisible(nCol));
			
			CRect rSubItem;
			VERIFY(m_lcColumns.GetSubItemRect(nItem, nCol, LVIR_BOUNDS, rSubItem));
			
			ASSERT(rSubItem.PtInRect(pt));
			
			for (int nFile = 0; nFile < nNumFiles; nFile++)
			{
				CRect rIcon;
				
				if (!CalcFileIconRect(rSubItem, rIcon, nFile, nNumFiles))
					break; // outside the subitem
				
				if (rIcon.PtInRect(pt))
				{
					ShowFileLink(pTDI->aFileLinks[nFile]);
					break;
				}
			}
		}
		break; 
	}
}

void CTDLTaskCtrlBase::ShowFileLink(LPCTSTR szFilePath) const
{
	// Handle Outlook manually because under Windows 10 ShellExecute 
	// will succeed even if Outlook is not installed
	if (CMSOutlookHelper::IsOutlookUrl(szFilePath))
	{
		if (CMSOutlookHelper::HandleUrl(*this, szFilePath))
			return;
	}
	else if (FileMisc::Run(GetSafeHwnd(), szFilePath, NULL, SW_SHOWNORMAL, m_sTasklistFolder) >= SE_ERR_SUCCESS)
	{
		return;
	}

	// else forward to our parent
	::SendMessage(::GetParent(GetSafeHwnd()), WM_TDCM_FAILEDLINK, (WPARAM)GetSafeHwnd(), (LPARAM)szFilePath);
}

BOOL CTDLTaskCtrlBase::HandleListLBtnDown(CListCtrl& lc, CPoint pt)
{
	m_bBoundSelecting = FALSE;

	// let parent handle any focus changes first
	lc.SetFocus();

	int nHit = -1;
	TDC_COLUMN nColID = TDCC_NONE;

	if (lc == m_lcColumns)
	{
		DWORD dwTaskID = 0;
		nHit = HitTestColumnsItem(pt, TRUE, nColID, &dwTaskID);

		if (nColID != TDCC_NONE)
		{
			if (Misc::ModKeysArePressed(MKS_ALT))
			{
				NMITEMACTIVATE nmia = { 0 };

				nmia.hdr.hwndFrom = GetSafeHwnd();
				nmia.hdr.code = NM_CLICK;
				nmia.hdr.idFrom = CWnd::GetDlgCtrlID();

				nmia.iItem = nHit;
				nmia.iSubItem = nColID;

				CWnd::GetParent()->SendMessage(WM_NOTIFY, nmia.hdr.idFrom, (LPARAM)&nmia);
				return TRUE; // eat it
			}
			else
			{
				// if the user clicked on a column that allows direct input
				// AND multi items are selected and the item clicked is 
				// already selected then we generate a NM_CLICK and eat the 
				// message to prevent a selection change
				BOOL bMultiSelection = (m_lcColumns.GetSelectedCount() > 1);
				BOOL bTaskSelected = IsListItemSelected(m_lcColumns, nHit);

				if (bMultiSelection && bTaskSelected && 
					ItemColumnSupportsClickHandling(nHit, nColID))
				{
					// special case
					if (nColID == TDCC_FILEREF)
						HandleFileLinkColumnClick(nHit, dwTaskID, pt);
					else
						NotifyParentOfColumnEditClick(nColID, dwTaskID);

					TRACE(_T("Ate Listview LButtonDown\n"));
					return TRUE; // eat it
				}
			}
		}
	}
	else
	{
		nHit = lc.HitTest(pt);
	}

	// De-selecting a lot of items can be slow because 
	// OnListSelectionChange is called once for each.
	// So we try to detect big changes here and handle 
	// them ourselves.
	lc.ClientToScreen(&pt);

	if (nHit != -1)
	{
		BOOL bHitSelected = IsListItemSelected(m_lcColumns, nHit);

		if (Misc::ModKeysArePressed(0) && 
			(!bHitSelected || !ItemColumnSupportsClickHandling(nHit, nColID)))
		{
			DeselectAll();
		}
	}
	else if (::DragDetect(lc, pt))
	{
		m_bBoundSelecting = -1;

		if (!Misc::IsKeyPressed(VK_CONTROL))
		{
			DeselectAll();
		}

		// there's no reliable to way to detect the end of a
		// bounding-box selection especially if the mouse 
		// cursor ends up outside the window so we use a timer
		SetTimer(TIMER_BOUNDINGSEL, 50, NULL);
	}
	else // prevent deselection
	{
		TRACE(_T("Ate Listview ButtonDown\n"));
		return TRUE; // eat it
	}

	return FALSE;
}

void CTDLTaskCtrlBase::OnTimer(UINT nIDEvent)
{
	switch (nIDEvent)
	{
	case TIMER_BOUNDINGSEL:
		if (m_bBoundSelecting && !Misc::IsKeyPressed(VK_LBUTTON))
		{
			m_bBoundSelecting = FALSE;

			KillTimer(TIMER_BOUNDINGSEL);
			NotifyParentSelChange(SC_BYMOUSE);
		}
		break;
	}

	CWnd::OnTimer(nIDEvent);
}

void CTDLTaskCtrlBase::NotifyParentOfColumnEditClick(TDC_COLUMN nColID, DWORD dwTaskID)
{
	ASSERT(nColID != TDCC_FILEREF); // handled by HandleFileLinkColumnClick

	CWnd::GetParent()->SendMessage(WM_TDCN_COLUMNEDITCLICK, nColID, dwTaskID);
}

void CTDLTaskCtrlBase::OnHeaderClick(TDC_COLUMN nColID)
{
	TDC_COLUMN nPrev = m_sort.single.nBy;
	TDC_COLUMN nSortBy = TDCC_NONE;
	
	// check for default attribute
	const TDCCOLUMN* pTDCC = GetColumn(nColID);
	
	// could also be a custom column
	if (pTDCC)
	{
		nSortBy = nColID;
	}	
	else if (CTDCCustomAttributeHelper::IsColumnSortable(nColID, m_aCustomAttribDefs))
	{
		nSortBy = nColID;
	}
	
	// do the sort
	if (nSortBy != TDCC_NONE)
	{
		Sort(nSortBy);
		
		// notify parent
		CWnd::GetParent()->SendMessage(WM_TDCN_SORT, CWnd::GetDlgCtrlID(), MAKELPARAM((WORD)nPrev, (WORD)nSortBy));
	}
}

BOOL CTDLTaskCtrlBase::ItemColumnSupportsClickHandling(int nItem, TDC_COLUMN nColID, const CPoint* pCursor) const
{
	if ((nItem == -1) || !Misc::ModKeysArePressed(0))
		return FALSE;

	DWORD dwTaskID = GetColumnItemTaskID(nItem);
	ASSERT(dwTaskID);

	BOOL bNoModifiers = Misc::ModKeysArePressed(0);
	BOOL bSingleSelection = (GetSelectedCount() == 1);
	BOOL bTaskSelected = IsListItemSelected(m_lcColumns, nItem);
	BOOL bReadOnly = IsReadOnly();
	BOOL bLocked = m_calculator.IsTaskLocked(dwTaskID);

	// Edit operations
	if (!bReadOnly)
	{
		switch (nColID)
		{
		case TDCC_DONE:
		case TDCC_FLAG:
		case TDCC_RECURRENCE:
		case TDCC_ICON:
			return !bLocked;

		case TDCC_LOCK:
			// Prevent editing of subtasks inheriting parent lock state
			if (HasStyle(TDCS_SUBTASKSINHERITLOCK))
				return !m_calculator.IsTaskLocked(m_data.GetTaskParentID(dwTaskID));

			// else
			return TRUE;
			
		case TDCC_TRACKTIME:
			// check tasklist is editable, task is trackable and 
			// neither the ctrl not shift keys are pressed (ie => multiple selection)
			// and either the task is not selected or it's only singly selected
			return (bNoModifiers && !bLocked &&
					m_data.IsTaskTimeTrackable(dwTaskID) &&
					(!bTaskSelected || bSingleSelection));

		default: // try custom columns
			if (!bLocked && CTDCCustomAttributeHelper::IsCustomColumn(nColID))
			{
				TDCCUSTOMATTRIBUTEDEFINITION attribDef;
			
				if (CTDCCustomAttributeHelper::GetAttributeDef(nColID, m_aCustomAttribDefs, attribDef))
				{
					switch (attribDef.GetDataType())
					{
					case TDCCA_BOOL:
						return TRUE;
					
					case TDCCA_ICON:
						switch (attribDef.GetListType())
						{
						case TDCCA_FIXEDLIST:
						case TDCCA_NOTALIST:
							return TRUE;
						}
						break;
					
					default: // Allow item cycling for fixed lists
						return (attribDef.GetListType() == TDCCA_FIXEDLIST);
					}
				}
			}
			break;
		}
	}

	// Non-edit operations
	switch (nColID)
	{
	case TDCC_REMINDER:
		return !m_calculator.IsTaskDone(dwTaskID);

	case TDCC_FILEREF:
		if (pCursor)
			return (HitTestFileLinkColumn(*pCursor) != -1);
		
		// else
		return m_data.TaskHasFileRef(dwTaskID);
			
	case TDCC_DEPENDENCY:
		return m_data.IsTaskDependent(dwTaskID);
			
	default: // try custom columns
		if (CTDCCustomAttributeHelper::IsCustomColumn(nColID))
		{
			TDCCUSTOMATTRIBUTEDEFINITION attribDef;
			
			if (CTDCCustomAttributeHelper::GetAttributeDef(nColID, m_aCustomAttribDefs, attribDef))
			{
				switch (attribDef.GetDataType())
				{
				case TDCCA_FILELINK:
					// TODO
					return TRUE;
				}
			}
		}
		break;
	}

	return FALSE;
}

void CTDLTaskCtrlBase::SetModified(TDC_ATTRIBUTE nAttrib)
{
	if (AttribMatchesSort(nAttrib))
		m_sort.bModSinceLastSort = TRUE;

	// Recalc or redraw columns as required
	BOOL bRedrawCols = FALSE, bRedrawTasks = ModCausesTaskTextColorChange(nAttrib);
	
	TDC_COLUMN nColID = TDC::MapAttributeToColumn(nAttrib);
	CTDCColumnIDMap aColIDs;
	
	switch (nAttrib)
	{
	case TDCA_CREATIONDATE:
		// this can only be modified for new tasks via the commandline
		// so nothing needs to be done
		break;
		
	case TDCA_DONEDATE:
		{
			bRedrawTasks |= (HasStyle(TDCS_STRIKETHOUGHDONETASKS) ||
							(HasStyle(TDCS_ALLOWTREEITEMCHECKBOX) && !IsColumnShowing(TDCC_DONE)));
			
			AccumulateRecalcColumn(TDCC_DONEDATE, aColIDs);
			AccumulateRecalcColumn(TDCC_DUEDATE, aColIDs);
			AccumulateRecalcColumn(TDCC_DONE, aColIDs);

			if (HasStyle(TDCS_USEPERCENTDONEINTIMEEST))
				AccumulateRecalcColumn(TDCC_TIMEEST, aColIDs);

			if (!m_sCompletionStatus.IsEmpty())
				AccumulateRecalcColumn(TDCC_STATUS, aColIDs);
		}
		break;
		
	case TDCA_DUEDATE:
		if (!AccumulateRecalcColumn(TDCC_DUEDATE, aColIDs))
			bRedrawCols = IsColumnShowing(TDCC_PRIORITY);
		break;
		
	case TDCA_PRIORITY:
		if (!bRedrawTasks)
		{
			bRedrawCols = IsColumnShowing(TDCC_PRIORITY);
			
			if (!bRedrawCols && HasStyle(TDCS_SHOWPERCENTASPROGRESSBAR))
				bRedrawCols = IsColumnShowing(TDCC_PERCENT);
		}
		break;
		
	case TDCA_ALLOCTO:
	case TDCA_ALLOCBY:
	case TDCA_STATUS:
	case TDCA_VERSION:
	case TDCA_CATEGORY:
	case TDCA_TAGS:
	case TDCA_COST:
	case TDCA_STARTDATE:
	case TDCA_EXTERNALID:
	case TDCA_RECURRENCE:
	case TDCA_FILEREF:
		AccumulateRecalcColumn(nColID, aColIDs);
		break;
		
	case TDCA_TIMEEST:
		{
			bRedrawCols |= !AccumulateRecalcColumn(TDCC_TIMEEST, aColIDs);

			if (HasStyle(TDCS_CALCREMAININGTIMEBYSPENT))
				bRedrawCols |= !AccumulateRecalcColumn(TDCC_REMAINING, aColIDs);

			if (bRedrawCols)
				bRedrawCols = HasStyle(TDCS_AUTOCALCPERCENTDONE);
		}
		break;
		
	case TDCA_TIMESPENT:
		{
			bRedrawCols |= !AccumulateRecalcColumn(TDCC_TIMESPENT, aColIDs);

			if (HasStyle(TDCS_CALCREMAININGTIMEBYSPENT))
				bRedrawCols |= !AccumulateRecalcColumn(TDCC_REMAINING, aColIDs);

			if (bRedrawCols)
				bRedrawCols = HasStyle(TDCS_AUTOCALCPERCENTDONE);
		}
		break;
		
	case TDCA_DEPENDENCY:
	case TDCA_RISK:
	case TDCA_FLAG:
	case TDCA_LOCK:
	case TDCA_PERCENT:
		if (!bRedrawTasks)
			bRedrawCols = IsColumnShowing(nColID);
		break;
		
	case TDCA_ICON:
		{
			if (IsColumnShowing(TDCC_ICON))
				bRedrawCols = TRUE;
			else
				bRedrawTasks = TRUE;
		}
		break;
		
	case TDCA_TASKNAME:
		bRedrawCols = IsColumnShowing(TDCC_PATH);
		break;
		
	case TDCA_PROJECTNAME:
	case TDCA_COMMENTS:
	case TDCA_ENCRYPT:
		break;
		
	case TDCA_COLOR:
		bRedrawTasks = TRUE;
		break;
		
	case TDCA_NONE:
	case TDCA_PASTE:
	case TDCA_MERGE:
	case TDCA_POSITION: // == move
	case TDCA_DELETE:
	case TDCA_ARCHIVE:
	case TDCA_UNDO:
	case TDCA_NEWTASK:
	case TDCA_CUSTOMATTRIB:
	case TDCA_CUSTOMATTRIBDEFS:
		aColIDs.Add(TDCC_ALL);
		break;
		
	case TDCA_TASKNAMEORCOMMENTS:
	case TDCA_ANYTEXTATTRIBUTE:
		ASSERT(0);
		break;

	default:
		if (CTDCCustomAttributeHelper::IsCustomColumn(nColID))
			aColIDs.Add(nColID);
		else
			ASSERT(0);
		break;
	}
		
	RecalcColumnWidths(aColIDs);
	
	if (bRedrawTasks)
	{
		InvalidateAll();
	}
	else if (bRedrawCols || !aColIDs.IsEmpty())
	{
		m_lcColumns.Invalidate();
	}
}

BOOL CTDLTaskCtrlBase::ModCausesTaskTextColorChange(TDC_ATTRIBUTE nModType) const
{
	switch (nModType)
	{
	case TDCA_COLOR:
		return !HasStyle(TDCS_COLORTEXTBYPRIORITY) &&
				!HasStyle(TDCS_COLORTEXTBYATTRIBUTE) &&
				!HasStyle(TDCS_COLORTEXTBYNONE);

	case TDCA_CATEGORY:
	case TDCA_ALLOCBY:
	case TDCA_ALLOCTO:
	case TDCA_STATUS:
	case TDCA_VERSION:
	case TDCA_EXTERNALID:
	case TDCA_TAGS:
		return (HasStyle(TDCS_COLORTEXTBYATTRIBUTE) && (GetColorByAttribute() == nModType));

	case TDCA_DONEDATE:
		return (GetCompletedTaskColor() != CLR_NONE);

	case TDCA_DUEDATE:
		{
			COLORREF crDue, crDueToday;
			GetDueTaskColors(crDue, crDueToday);

			return ((crDue != CLR_NONE) || (crDueToday != CLR_NONE));
		}

	case TDCA_PRIORITY:
		return HasStyle(TDCS_COLORTEXTBYPRIORITY);
	}

	// all else
	return FALSE;
}

BOOL CTDLTaskCtrlBase::AccumulateRecalcColumn(TDC_COLUMN nColID, CSet<TDC_COLUMN>& aColIDs) const
{
	if (aColIDs.Has(nColID))
	{
		return TRUE;
	}
	else if (IsColumnShowing(nColID))
	{
		aColIDs.Add(nColID);
		return TRUE;
	}
	
	// else
	return FALSE;
}

BOOL CTDLTaskCtrlBase::ModNeedsResort(TDC_ATTRIBUTE nModType) const
{
	if (!HasStyle(TDCS_RESORTONMODIFY))
		return FALSE;

	return AttribMatchesSort(nModType);
}

BOOL CTDLTaskCtrlBase::AttribMatchesSort(TDC_ATTRIBUTE nAttrib) const
{
	if (!m_sort.IsSorting())
		return FALSE;

	if (nAttrib == TDCA_ALL)
		return TRUE;
	
	BOOL bNeedSort = FALSE;
	
	if (m_sort.bMulti)
	{
		for (int nCol = 0; ((nCol < 3) && !bNeedSort); nCol++)
		{
			if (!m_sort.multi.IsSorting(nCol))
				break;

			bNeedSort = ModNeedsResort(nAttrib, m_sort.multi.GetSortBy(nCol));
		}
	}
	else
	{
		bNeedSort = ModNeedsResort(nAttrib, m_sort.single.nBy);
	}
	
	return bNeedSort;
}

BOOL CTDLTaskCtrlBase::ModNeedsResort(TDC_ATTRIBUTE nModType, TDC_COLUMN nSortBy) const
{
	if (nSortBy == TDCC_NONE)
		return FALSE;

	TDC_COLUMN nModCol = TDC::MapAttributeToColumn(nModType);

	switch (nModType)
	{
	case TDCA_TASKNAME:		
	case TDCA_STARTDATE:	
	case TDCA_PRIORITY:		
	case TDCA_ALLOCTO:		
	case TDCA_ALLOCBY:		
	case TDCA_STATUS:		
	case TDCA_CATEGORY:		
	case TDCA_TAGS:			
	case TDCA_TIMEEST:		
	case TDCA_RISK:			
	case TDCA_EXTERNALID:	
	case TDCA_VERSION:		
	case TDCA_RECURRENCE:	
	case TDCA_ICON:			
	case TDCA_COLOR:		
	case TDCA_FLAG:			
	case TDCA_DUEDATE:
	case TDCA_PERCENT:
	case TDCA_TIMESPENT:
	case TDCA_DEPENDENCY:
	case TDCA_COST:
	case TDCA_FILEREF:
	case TDCA_POSITION:
	case TDCA_LOCK:			
		{
			ASSERT(nModCol != TDCC_NONE);

			if (nModCol == nSortBy)
				return TRUE;
		}
		break;

	case TDCA_DONEDATE:
		{
			ASSERT(nModCol != TDCC_NONE);

			if (nModCol == nSortBy)
				return TRUE;

			if (HasStyle(TDCS_SORTDONETASKSATBOTTOM))
			{
				// some sort columns are unaffected by completed tasks
				switch (nSortBy)
				{
				case TDCC_ID:
					return FALSE;
				}

				// all else
				return TRUE;
			}
		}
		break;

	case TDCA_UNDO:
	case TDCA_PASTE:
	case TDCA_MERGE:
		ASSERT(nModCol == TDCC_NONE);
		return TRUE;

	case TDCA_NEWTASK:
	case TDCA_DELETE:
	case TDCA_ARCHIVE:
	case TDCA_ENCRYPT:
	case TDCA_NONE:
	case TDCA_PROJECTNAME:
	case TDCA_COMMENTS:
		ASSERT(nModCol == TDCC_NONE);
		return FALSE;

	case TDCA_CUSTOMATTRIBDEFS:	// Resort all custom columns
		ASSERT(nModCol == TDCC_NONE);
		return CTDCCustomAttributeHelper::IsCustomColumn(nSortBy);

	default:
		if (CTDCCustomAttributeHelper::IsCustomAttribute(nModType))
		{
			ASSERT(CTDCCustomAttributeHelper::IsColumnSortable(nModCol, m_aCustomAttribDefs));
			return (nModCol == nSortBy);
		}
		// else unhandled attribute
		ASSERT(0);
		return FALSE;
	}

	ASSERT(nModCol != TDCC_NONE);

	// Attribute interdependencies
	switch (nSortBy)
	{
	case TDCC_DONE:
	case TDCC_STARTDATE:
	case TDCC_DUEDATE:
	case TDCC_PERCENT:
		return (nModType == TDCA_DONEDATE);
		
	case TDCC_RISK:
		return ((nModType == TDCA_DONEDATE) && HasStyle(TDCS_DONEHAVELOWESTRISK));
		
	case TDCC_PRIORITY:
		return (((nModType == TDCA_DONEDATE) && HasStyle(TDCS_DONEHAVELOWESTPRIORITY)) ||
				((nModType == TDCA_DUEDATE) && HasStyle(TDCS_DUEHAVEHIGHESTPRIORITY)));
		
	case TDCC_REMAINING: 
		return (((nModType == TDCA_DUEDATE) && HasStyle(TDCS_CALCREMAININGTIMEBYDUEDATE)) ||
				((nModType == TDCA_TIMESPENT) && HasStyle(TDCS_CALCREMAININGTIMEBYSPENT)) ||
				((nModType == TDCA_PERCENT) && HasStyle(TDCS_CALCREMAININGTIMEBYPERCENT)));
	}

	// all else
	return FALSE;
}

void CTDLTaskCtrlBase::OnReminderChange()
{
	if (IsColumnShowing(TDCC_REMINDER))
	{
		if (IsSortingBy(TDCC_REMINDER))
			Resort(FALSE);
		else
			RedrawColumn(TDCC_REMINDER);
	}
}

void CTDLTaskCtrlBase::SetAlternateLineColor(COLORREF crAltLine)
{
	SetColor(m_crAltLine, crAltLine);
}

void CTDLTaskCtrlBase::SetGridlineColor(COLORREF crGridLine)
{
	SetColor(m_crGridLine, crGridLine);
}

void CTDLTaskCtrlBase::SetColor(COLORREF& color, COLORREF crNew)
{
	if (IsHooked() && (crNew != color))
		InvalidateAll(FALSE);
	
	color = crNew;
}

void CTDLTaskCtrlBase::RedrawColumns(BOOL bErase) const
{
	::InvalidateRect(m_lcColumns, NULL, bErase);
	::UpdateWindow(m_lcColumns);
}

void CTDLTaskCtrlBase::RedrawColumn(TDC_COLUMN nColID) const
{
	int nCol = m_hdrColumns.FindItem(nColID);

	if (m_hdrColumns.IsItemVisible(nCol))
	{
		CRect rCol, rClient;
		
		m_lcColumns.GetClientRect(rClient);
		m_hdrColumns.GetItemRect(nCol, rCol);

		// Adjust header rect for list scrollpos
		m_hdrColumns.ClientToScreen(rCol);
		m_lcColumns.ScreenToClient(rCol);
		
		rCol.top = rClient.top;
		rCol.bottom = rClient.bottom;
		
		::InvalidateRect(m_lcColumns, rCol, TRUE);
		::UpdateWindow(m_lcColumns);
	}
}

void CTDLTaskCtrlBase::RedrawTasks(BOOL bErase) const
{
	::InvalidateRect(Tasks(), NULL, bErase);
	::UpdateWindow(Tasks());
}

void CTDLTaskCtrlBase::OnBeginRebuild()
{
	EnableResync(FALSE);
	OnSetRedraw(FALSE, 0);
}

void CTDLTaskCtrlBase::OnEndRebuild()
{
	OnSetRedraw(TRUE, 0);
	EnableResync(TRUE, Tasks());
}

int CTDLTaskCtrlBase::CalcMaxDateColWidth(TDC_DATE nDate, CDC* pDC, BOOL bCustomWantsTime) const
{
	COleDateTime dateMax(2000, 12, 31, 23, 59, 0);
	CString sDateMax, sTimeMax, sDow;

	FormatDate(dateMax, nDate, sDateMax, sTimeMax, sDow, bCustomWantsTime);

	// Always want date
	int nSpace = pDC->GetTextExtent(_T(" ")).cx;
	int nWidth = pDC->GetTextExtent(sDateMax).cx;

	if (!sTimeMax.IsEmpty())
	{
		nWidth += nSpace;
		nWidth += pDC->GetTextExtent(sTimeMax).cx;
	}

	if (!sDow.IsEmpty())
	{
		nWidth += nSpace;
		nWidth += CDateHelper::CalcLongestDayOfWeekName(pDC, TRUE);
	}

	return nWidth;
}

BOOL CTDLTaskCtrlBase::WantDrawColumnTime(TDC_DATE nDate, BOOL bCustomWantsTime) const
{
	switch (nDate)
	{
	case TDCD_CREATE:
		return IsColumnShowing(TDCC_CREATIONTIME);

	case TDCD_START:		
	case TDCD_STARTDATE:	
	case TDCD_STARTTIME:
		return IsColumnShowing(TDCC_STARTTIME);
		
	case TDCD_DUE:		
	case TDCD_DUEDATE:	
	case TDCD_DUETIME:	
		return IsColumnShowing(TDCC_DUETIME);
		
	case TDCD_DONE:		
	case TDCD_DONEDATE:	
	case TDCD_DONETIME:	
		return IsColumnShowing(TDCC_DONETIME);
		
	case TDCD_CUSTOM:
		return (bCustomWantsTime || HasStyle(TDCS_SHOWREMINDERSASDATEANDTIME));
		
	case TDCD_LASTMOD:
		return TRUE; // always
	}
	
	// all else
	ASSERT(0);
	return FALSE;
}

void CTDLTaskCtrlBase::ClearSortColumn()
{
	if ((m_nSortColID != TDCC_NONE) && GetSafeHwnd())
	{
		::InvalidateRect(m_hdrTasks, NULL, FALSE);
		m_hdrColumns.Invalidate(FALSE);
	}

	m_nSortColID = TDCC_NONE;
	m_nSortDir = TDC_SORTNONE;
}

void CTDLTaskCtrlBase::SetSortColumn(TDC_COLUMN nColID, TDC_SORTDIR nSortDir)
{
	m_nSortDir = nSortDir;

	if (m_nSortColID != nColID)
	{
		if ((m_nSortColID == TDCC_CLIENT) || (nColID == TDCC_CLIENT))
			m_hdrTasks.Invalidate(FALSE);

		m_nSortColID = nColID;
	}
}

TDC_COLUMN CTDLTaskCtrlBase::GetSortColumn(TDC_SORTDIR& nSortDir) const
{
	nSortDir = m_nSortDir;
	return m_nSortColID;
}

int CTDLTaskCtrlBase::RecalcColumnWidth(int nCol, CDC* pDC, BOOL bVisibleOnly) const
{
	TDC_COLUMN nColID = (TDC_COLUMN)m_hdrColumns.GetItemData(nCol);

	// handle hidden columns
	if (!IsColumnShowing(nColID))
 		return 0;
	
	int nColWidth = 0;
	
	switch (nColID)
	{
	case TDCC_TRACKTIME:
	case TDCC_FLAG:
	case TDCC_LOCK:
	case TDCC_RECENTEDIT:
	case TDCC_DEPENDENCY:
	case TDCC_ICON:
	case TDCC_DONE:
		break; 
		
	case TDCC_REMINDER:
		if (HasStyle(TDCS_SHOWREMINDERSASDATEANDTIME))
			nColWidth = CalcMaxDateColWidth(TDCD_CUSTOM, pDC); // no time component
		// else use MINCOLWIDTH
		break; 
		
	case TDCC_ID:
		{
			DWORD dwRefID = m_find.GetLargestReferenceID();

			if (dwRefID)
				nColWidth = GraphicsMisc::GetFormattedTextWidth(pDC, _T("%u (%u)"), m_dwNextUniqueTaskID - 1, dwRefID);
			else
				nColWidth = GraphicsMisc::GetFormattedTextWidth(pDC, _T("%u"), m_dwNextUniqueTaskID - 1);
		}
		break; 

	case TDCC_PARENTID:
		nColWidth = GraphicsMisc::GetFormattedTextWidth(pDC, _T("%u"), m_dwNextUniqueTaskID - 1);
		break; 

	case TDCC_POSITION:
	case TDCC_RECURRENCE:
	case TDCC_EXTERNALID:
	case TDCC_VERSION:
	case TDCC_ALLOCBY:
	case TDCC_CREATEDBY:
	case TDCC_COST:
	case TDCC_PATH:
	case TDCC_SUBTASKDONE:
	case TDCC_ALLOCTO:
	case TDCC_CATEGORY:
	case TDCC_TAGS:
	case TDCC_LASTMODBY:
		{
			TDC_ATTRIBUTE nAttrib = TDC::MapColumnToAttribute(nColID);
			ASSERT(nAttrib != TDCA_NONE);
			
			// determine the longest visible string
			CString sLongest = m_find.GetLongestValue(nAttrib, bVisibleOnly);
			nColWidth = GraphicsMisc::GetAverageMaxStringWidth(sLongest, pDC);
		}
		break;
		
	case TDCC_STATUS:
		{
			// determine the longest visible string
			CString sLongest = m_find.GetLongestValue(TDCA_STATUS, m_sCompletionStatus, bVisibleOnly);
			nColWidth = GraphicsMisc::GetAverageMaxStringWidth(sLongest, pDC);
		}
		break;
		
	case TDCC_PRIORITY:
		if (!HasStyle(TDCS_HIDEPRIORITYNUMBER))
			nColWidth = pDC->GetTextExtent("10").cx;
		break; 
		
	case TDCC_RISK:
		nColWidth = pDC->GetTextExtent("10").cx;
		break; 
		
	case TDCC_FILEREF:
		if (HasStyle(TDCS_SHOWNONFILEREFSASTEXT))
		{
			nColWidth = 60; 
		}
		else
		{
			int nMaxCount = m_find.GetLargestFileLinkCount(bVisibleOnly);

			if (nMaxCount >= 1)
			{
				nColWidth = ((nMaxCount * (m_ilFileRef.GetImageSize() + COL_ICON_SPACING)) - COL_ICON_SPACING);

				// compensate for extra padding we don't want 
				nColWidth -= (2 * LV_COLPADDING);
			}
			// else use MINCOLWIDTH
		}
		break; 
		
	case TDCC_TIMEEST:
	case TDCC_TIMESPENT:
	case TDCC_REMAINING:
		if (HasStyle(TDCS_DISPLAYHMSTIMEFORMAT))
		{
			nColWidth = pDC->GetTextExtent("-12m4w").cx;
		}
		else
		{
			CString sLongest;

			switch (nColID)
			{
			case TDCC_TIMEEST:   sLongest = m_find.GetLongestTimeEstimate(m_nDefTimeEstUnits);	break;
			case TDCC_TIMESPENT: sLongest = m_find.GetLongestTimeSpent(m_nDefTimeEstUnits);		break;
			case TDCC_REMAINING: sLongest = m_find.GetLongestTimeRemaining(m_nDefTimeEstUnits); break;
			}

			nColWidth = (pDC->GetTextExtent(sLongest).cx + 4); // add a bit to handle different time unit widths
		}
		break;
		
	case TDCC_PERCENT:
		nColWidth = pDC->GetTextExtent("100%").cx;
		break;
		
	case TDCC_LASTMODDATE:
	case TDCC_DUEDATE:
	case TDCC_CREATIONDATE:
	case TDCC_STARTDATE:
	case TDCC_DONEDATE:
		nColWidth = CalcMaxDateColWidth(TDC::MapColumnToDate(nColID), pDC);
		break;

	case TDCC_COMMENTSSIZE:
		{
			float fSize = m_find.GetLargestCommentsSizeInKB();

			if (fSize > 0)
				nColWidth = (1 + (int)log10(fSize));
		}
		break;

	default:
		// custom columns
		if (CTDCCustomAttributeHelper::IsCustomColumn(nColID))
		{
			// determine the longest visible string depending on type
			TDCCUSTOMATTRIBUTEDEFINITION attribDef;

			if (CTDCCustomAttributeHelper::GetAttributeDef(nColID, m_aCustomAttribDefs, attribDef))
			{
				if (!attribDef.bEnabled)
				{
					return 0; // hidden
				}
				else 
				{
					switch (attribDef.GetDataType())
					{
					case TDCCA_DATE:
						nColWidth = CalcMaxDateColWidth(TDCD_CUSTOM, pDC, attribDef.HasFeature(TDCCAF_SHOWTIME));
						break;

					case TDCCA_ICON:
						if (attribDef.IsList())
						{
							switch (attribDef.GetListType())
							{
							case TDCCA_FIXEDLIST:
								nColWidth = attribDef.CalcLongestListItem(pDC);
								break;

							case TDCCA_FIXEDMULTILIST:
								nColWidth = ((attribDef.aDefaultListData.GetSize() * (COL_ICON_SIZE + COL_ICON_SPACING)) - COL_ICON_SPACING);
								break;
							}
						}
						// else single icon, no text: use MINCOLWIDTH
						break;

					case TDCCA_BOOL:
						if (attribDef.sColumnTitle.GetLength() == 1)
						{
							nColWidth = GraphicsMisc::GetTextWidth(pDC, attribDef.sColumnTitle);
						}
						else 
						{
							nColWidth = pDC->GetTextExtent(_T("+")).cx;
						}
						break;

					case TDCCA_DOUBLE:
					case TDCCA_INTEGER:
						{
							// numerals are always the same width so we don't need average width
							CString sLongest = m_find.GetLongestCustomAttribute(attribDef, bVisibleOnly);
							nColWidth = pDC->GetTextExtent(sLongest).cx;
						}
						break;

					case TDCCA_FILELINK:
						nColWidth = (attribDef.aDefaultListData.GetSize() * 18);
						break;

					default:
						{
							CString sLongest = m_find.GetLongestCustomAttribute(attribDef, bVisibleOnly);
							nColWidth = GraphicsMisc::GetAverageMaxStringWidth(sLongest, pDC);
						}
						break;
					}
				}
			}
			else
			{
				return 0; // hidden
			}
		}
		else
		{
			ASSERT (0);
		}
		break;
	}

	if (nColWidth == 0)
		nColWidth = MIN_RESIZE_WIDTH;
	else
		nColWidth = max((nColWidth + (2 * LV_COLPADDING)), MIN_RESIZE_WIDTH);
	
	// take max of this and column title
	int nTitleWidth = (m_hdrColumns.GetItemTextWidth(nCol, pDC) + (2 * HD_COLPADDING));

	return max(nTitleWidth, nColWidth);
}

BOOL CTDLTaskCtrlBase::SelectionHasIncompleteDependencies(CString& sIncomplete) const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		if (TaskHasIncompleteDependencies(dwTaskID, sIncomplete))
			return TRUE;
	}

	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasDependencies() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	CString sUnused;
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		if (m_data.TaskHasDependencies(dwTaskID))
			return TRUE;
	}
	
	return FALSE;
}

BOOL CTDLTaskCtrlBase::TaskHasIncompleteDependencies(DWORD dwTaskID, CString& sIncomplete) const
{
	CStringArray aDepends;
	int nNumDepends = m_data.GetTaskDependencies(dwTaskID, aDepends);
	
	for (int nDepends = 0; nDepends < nNumDepends; nDepends++)
	{
		CString sFile;
		DWORD dwDependID;
		
		VERIFY(ParseTaskLink(aDepends[nDepends], FALSE, dwDependID, sFile));
		
		// see if dependent is one of 'our' tasks
		if (dwDependID && sFile.IsEmpty())
		{
			if (m_data.HasTask(dwDependID) && !m_data.IsTaskDone(dwDependID))
			{
				sIncomplete = aDepends[nDepends];
				return TRUE;
			}
		}
		else if (!sFile.IsEmpty()) // pass to parent if we can't handle
		{
			BOOL bDependentIsDone = CWnd::GetParent()->SendMessage(WM_TDCM_ISTASKDONE, dwDependID, (LPARAM)(LPCTSTR)sFile);
			
			if (!bDependentIsDone)
			{
				sIncomplete = aDepends[nDepends];
				return TRUE;
			}
		}
	}
	
	// check this tasks subtasks
	const TODOSTRUCTURE* pTDS = m_data.LocateTask(dwTaskID);
	ASSERT(pTDS);
	
	if (pTDS && pTDS->HasSubTasks())
	{
		int nPos = pTDS->GetSubTaskCount();
		
		while (nPos--)
		{
			if (TaskHasIncompleteDependencies(pTDS->GetSubTaskID(nPos), sIncomplete))
				return TRUE;
		}
	}
	
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasDates(TDC_DATE nDate, BOOL bAll) const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	// traverse the selected items looking for the first one
	// who has a due date or the first that doesn't
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		double dDate = m_data.GetTaskDate(dwTaskID, nDate);

		if (!bAll && dDate > 0)
		{
			return TRUE;
		}
		else if (bAll && dDate == 0)
		{
			return FALSE;
		}
	}

	return bAll;
}

BOOL CTDLTaskCtrlBase::SelectionHasIcons() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		if (!m_data.GetTaskIcon(dwTaskID).IsEmpty())
			return TRUE;
	}

	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasUnlocked() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		if (!m_calculator.IsTaskLocked(dwTaskID))
			return TRUE;
	}

	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasLocked() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		if (m_calculator.IsTaskLocked(dwTaskID))
			return TRUE;
	}

	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionAreAllDone() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		if (!m_data.IsTaskDone(dwTaskID))
			return FALSE;
	}

	return TRUE;
}

int CTDLTaskCtrlBase::SelectionHasCircularDependencies(CDWordArray& aTaskIDs) const
{
	aTaskIDs.RemoveAll();

	POSITION pos = GetFirstSelectedTaskPos();

	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		if (m_data.TaskHasLocalCircularDependencies(dwTaskID))
		{
			aTaskIDs.Add(dwTaskID);
			return TRUE;
		}
	}

	return aTaskIDs.GetSize();
}

BOOL CTDLTaskCtrlBase::SelectionHasDependents() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		if (m_data.TaskHasDependents(dwTaskID))
			return TRUE;
	}
	
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasReferences() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		if (m_data.IsTaskReference(dwTaskID))
			return TRUE;
	}
	
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasNonReferences() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		if (!m_data.IsTaskReference(dwTaskID))
			return TRUE;
	}
	
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasIncompleteSubtasks(BOOL bExcludeRecurring) const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		if (m_data.TaskHasIncompleteSubtasks(dwTaskID, bExcludeRecurring))
			return TRUE;
	}
	
	return FALSE;
}

BOOL CTDLTaskCtrlBase::SelectionHasSubtasks() const
{
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		const TODOSTRUCTURE* pTDS = m_data.LocateTask(dwTaskID);
		ASSERT(pTDS);

		if (pTDS && pTDS->HasSubTasks())
			return TRUE;
	}
	
	return FALSE;
}

BOOL CTDLTaskCtrlBase::ParseTaskLink(const CString& sLink, BOOL bURL, DWORD& dwTaskID, CString& sFile) const
{
	return ParseTaskLink(sLink, bURL, m_sTasklistFolder, dwTaskID, sFile);
}

// Static
BOOL CTDLTaskCtrlBase::ParseTaskLink(const CString& sLink, BOOL bURL, const CString& sFolder, DWORD& dwTaskID, CString& sFile)
{
	CString sCleaned(sLink);
	
	// strip off protocol (if not done)
	int nProtocol = sCleaned.Find(TDL_PROTOCOL);
	
	if (nProtocol != -1)
	{
		sCleaned = sCleaned.Mid(nProtocol + lstrlen(TDL_PROTOCOL));
	}
	else if (bURL)
	{
		return FALSE;
	}
	
	// cleanup
	sCleaned.Replace(_T("%20"), _T(" "));
	sCleaned.Replace(_T("/"), _T("\\"));

	// Make full path only if it looks like a path
	if (!sFolder.IsEmpty() && ((sCleaned.Find('?') != -1) || !Misc::IsNumber(sCleaned)))
		FileMisc::MakeFullPath(sCleaned, sFolder);
	
	// parse the url
	return TODOITEM::ParseTaskLink(sCleaned, dwTaskID, sFile);
}

void CTDLTaskCtrlBase::EnableExtendedSelection(BOOL bCtrl, BOOL bShift)
{
	if (bCtrl)
		s_nExtendedSelection |= HOTKEYF_CONTROL;
	else
		s_nExtendedSelection &= ~HOTKEYF_CONTROL;
	
	if (bShift)
		s_nExtendedSelection |= HOTKEYF_SHIFT;
	else
		s_nExtendedSelection &= ~HOTKEYF_SHIFT;
}

BOOL CTDLTaskCtrlBase::IsReservedShortcut(DWORD dwShortcut)
{
	// check this is not a reserved shortcut used by the tree or a.n.other ctrl
	switch (dwShortcut)
	{
	case MAKELONG(VK_UP, HOTKEYF_EXT):
	case MAKELONG(VK_PRIOR, HOTKEYF_EXT):
	case MAKELONG(VK_DOWN, HOTKEYF_EXT):
	case MAKELONG(VK_NEXT, HOTKEYF_EXT):
		
	case MAKELONG(VK_SPACE, HOTKEYF_CONTROL):
	case MAKELONG(VK_DELETE, HOTKEYF_CONTROL | HOTKEYF_EXT):
		return TRUE;
		
	case MAKELONG(VK_UP, HOTKEYF_CONTROL | HOTKEYF_EXT):
	case MAKELONG(VK_PRIOR, HOTKEYF_CONTROL | HOTKEYF_EXT):
	case MAKELONG(VK_DOWN, HOTKEYF_CONTROL | HOTKEYF_EXT):
	case MAKELONG(VK_NEXT, HOTKEYF_CONTROL | HOTKEYF_EXT):
		return (s_nExtendedSelection & HOTKEYF_CONTROL);
		
	case MAKELONG(VK_UP, HOTKEYF_SHIFT | HOTKEYF_EXT):
	case MAKELONG(VK_PRIOR, HOTKEYF_SHIFT | HOTKEYF_EXT):
	case MAKELONG(VK_DOWN, HOTKEYF_SHIFT | HOTKEYF_EXT):
	case MAKELONG(VK_NEXT, HOTKEYF_SHIFT | HOTKEYF_EXT):
		return (s_nExtendedSelection & HOTKEYF_SHIFT);
		
	case MAKELONG(VK_UP, HOTKEYF_SHIFT | HOTKEYF_CONTROL | HOTKEYF_EXT):
	case MAKELONG(VK_PRIOR, HOTKEYF_SHIFT | HOTKEYF_CONTROL | HOTKEYF_EXT):
	case MAKELONG(VK_DOWN, HOTKEYF_SHIFT | HOTKEYF_CONTROL | HOTKEYF_EXT):
	case MAKELONG(VK_NEXT, HOTKEYF_SHIFT | HOTKEYF_CONTROL | HOTKEYF_EXT):
		return (s_nExtendedSelection & (HOTKEYF_CONTROL | HOTKEYF_SHIFT)); // both
	}
	
	// all else
	return FALSE;
}

CString CTDLTaskCtrlBase::GetSelectedTaskComments() const
{
	if (GetSelectedCount() == 1)
		return m_data.GetTaskComments(GetSelectedTaskID());
	
	// else
	return _T("");
}

const CBinaryData& CTDLTaskCtrlBase::GetSelectedTaskCustomComments(CString& sCommentsTypeID) const
{
	if (GetSelectedCount() == 1)
		return m_data.GetTaskCustomComments(GetSelectedTaskID(), sCommentsTypeID);
	
	// else
	sCommentsTypeID.Empty();
    POSITION pos = GetFirstSelectedTaskPos();
	
    while (pos)
    {
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		const TODOITEM* pTDI = m_data.GetTrueTask(dwTaskID);
		ASSERT (pTDI);
		
		if (pTDI)
		{
			if (sCommentsTypeID.IsEmpty())
			{
				sCommentsTypeID = pTDI->sCommentsTypeID;
			}
			else if (sCommentsTypeID != pTDI->sCommentsTypeID)
			{
				sCommentsTypeID.Empty();
				break;
			}
		}
	}
	
	static CBinaryData content;
	return content;
}

CString CTDLTaskCtrlBase::GetSelectedTaskTitle() const
{
	if (GetSelectedCount() == 1)
		return m_data.GetTaskTitle(GetSelectedTaskID());
	
	// else
	return _T("");
}

int CTDLTaskCtrlBase::GetSelectedTaskPriority() const
{
   int nPriority = -1;
   POSITION pos = GetFirstSelectedTaskPos();

   while (pos)
   {
      DWORD dwTaskID = GetNextSelectedTaskID(pos);
      int nTaskPriority = m_data.GetTaskPriority(dwTaskID);

      if (nPriority == -1)
         nPriority = nTaskPriority;

      else if (nPriority != nTaskPriority)
         return -1;
   }
	
	return nPriority;
}

DWORD CTDLTaskCtrlBase::GetSelectedTaskParentID() const
{
	// If multiple tasks are selected they must all
	// have the same parent else we return 0
	DWORD dwParentID = (DWORD)-1;
	POSITION pos = GetFirstSelectedTaskPos();
	
	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		DWORD dwTaskParentID = m_data.GetTaskParentID(dwTaskID);
		
		if (dwParentID == (DWORD)-1)
		{
			dwParentID = dwTaskParentID;
		}
		else if (dwParentID != dwTaskParentID)
		{
			return 0;
		}
	}
	
	return dwParentID;
}

int CTDLTaskCtrlBase::GetSelectedTaskRisk() const
{
   int nRisk = -1;
   POSITION pos = GetFirstSelectedTaskPos();

   while (pos)
   {
      DWORD dwTaskID = GetNextSelectedTaskID(pos);
      int nTaskRisk = m_data.GetTaskRisk(dwTaskID);

      if (nRisk == -1)
         nRisk = nTaskRisk;

      else if (nRisk != nTaskRisk)
         return -1; // == various
   }
	
	return nRisk;
}

CString CTDLTaskCtrlBase::GetSelectedTaskIcon() const
{
	CString sIcon;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		sIcon = m_data.GetTaskIcon(dwTaskID);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			CString sTaskIcon = m_data.GetTaskIcon(dwTaskID);
			
			if (sIcon != sTaskIcon)
				return _T("");
		}
	}
	
	return sIcon;
}

BOOL CTDLTaskCtrlBase::SelectedTaskHasDate(TDC_DATE nDate) const
{
	return CDateHelper::IsDateSet(GetSelectedTaskDate(nDate));
}

COleDateTime CTDLTaskCtrlBase::GetSelectedTaskDate(TDC_DATE nDate) const
{
	COleDateTime date; // == 0
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		date = m_data.GetTaskDate(dwTaskID, nDate);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			COleDateTime dateTask = m_data.GetTaskDate(dwTaskID, nDate);
			
			// first check if both dates are not set
			if (!CDateHelper::IsDateSet(date) && !CDateHelper::IsDateSet(dateTask))
				continue;

			if (!CDateHelper::IsDateSet(date)) // means dateTask.m_dt != 0
				return dateTask;

			// else 
			return date;
		}
		// if we get here then no dates were set
	}
	
	return date;
}

int CTDLTaskCtrlBase::IsSelectedTaskFlagged() const
{
	return m_data.IsTaskFlagged(GetSelectedTaskID());
}

int CTDLTaskCtrlBase::IsSelectedTaskLocked() const
{
	return m_data.IsTaskLocked(GetSelectedTaskID());
}

BOOL CTDLTaskCtrlBase::IsSelectedTaskReference() const
{
	return m_data.IsTaskReference(GetSelectedTaskID());
}

double CTDLTaskCtrlBase::GetSelectedTaskTimeEstimate(TDC_UNITS& nUnits) const
{
	double dTime = 0.0;
	nUnits = m_nDefTimeEstUnits;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		dTime = m_data.GetTaskTimeEstimate(dwTaskID, nUnits);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			
			TDC_UNITS nTaskUnits;
			double dTaskTime = m_data.GetTaskTimeEstimate(dwTaskID, nTaskUnits);
			
			if ((dTime != dTaskTime) || (nUnits != nTaskUnits))
			{
				nUnits = TDCU_NULL;
				return 0.0;
			}
		}
	}
	
	return dTime;
}

double CTDLTaskCtrlBase::GetSelectedTaskTimeSpent(TDC_UNITS& nUnits) const
{
	double dTime = 0.0;
	nUnits = m_nDefTimeSpentUnits;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		dTime = m_data.GetTaskTimeSpent(dwTaskID, nUnits);
		
		while (pos)
		{
			DWORD dwTaskID = GetNextSelectedTaskID(pos);
			
			TDC_UNITS nTaskUnits;
			double dTaskTime = m_data.GetTaskTimeSpent(dwTaskID, nTaskUnits);
			
			if ((dTime != dTaskTime) || (nUnits != nTaskUnits))
			{
				nUnits = TDCU_NULL;
				return 0.0;
			}
		}
	}
	
	return dTime;
}

COLORREF CTDLTaskCtrlBase::GetSelectedTaskColor() const
{
	return m_data.GetTaskColor(GetSelectedTaskID());
}

BOOL CTDLTaskCtrlBase::GetSelectedTaskRecurrence(TDCRECURRENCE& tr) const
{
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		m_data.GetTaskRecurrence(dwTaskID, tr);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			
			TDCRECURRENCE trTask;
			m_data.GetTaskRecurrence(dwTaskID, trTask);
			
			if (tr != trTask)
				tr = TDCRECURRENCE();
		}
	}
	
	return tr.IsRecurring();
}

int CTDLTaskCtrlBase::GetSelectedTaskPercent() const
{
	int nPercent = 0;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		nPercent = m_data.GetTaskPercent(dwTaskID, FALSE);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			int nTaskPercent = m_data.GetTaskPercent(dwTaskID, FALSE);
			
			if (nPercent != nTaskPercent)
				return -1;
		}
	}
	
	return nPercent;
}

CString CTDLTaskCtrlBase::GetSelectedTaskPath(BOOL bIncludeTaskName, int nMaxLen) const
{
	CString sPath;

	if (GetSelectedCount() == 1)
	{
		DWORD dwTaskID = GetSelectedTaskID();
		CString sTaskTitle = bIncludeTaskName ? m_data.GetTaskTitle(dwTaskID) : _T("");

		if (bIncludeTaskName && nMaxLen != -1)
			nMaxLen -= sTaskTitle.GetLength();

		sPath = m_formatter.GetTaskPath(dwTaskID, nMaxLen);
	
		if (bIncludeTaskName)
			sPath += sTaskTitle;
	}

	return sPath;
}

double CTDLTaskCtrlBase::GetSelectedTaskCost() const
{
	double dCost = 0.0;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		dCost = m_data.GetTaskCost(dwTaskID);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			double dTaskCost = m_data.GetTaskCost(dwTaskID);

			if (dCost != dTaskCost)
				return 0.0;
		}
	}
	
	return dCost;
}

BOOL CTDLTaskCtrlBase::GetSelectedTaskCustomAttributeData(const CString& sAttribID, TDCCADATA& data, BOOL bFormatted) const
{
	data.Clear();

	int nSelCount = GetSelectedCount();

	if (nSelCount)
	{
		TDCCUSTOMATTRIBUTEDEFINITION attribDef;
		VERIFY(CTDCCustomAttributeHelper::GetAttributeDef(sAttribID, m_aCustomAttribDefs, attribDef));

		// Multi-selection check lists need special handling
		if (attribDef.IsMultiList())
		{
			CMap<CString, LPCTSTR, int, int&> mapCounts;
			POSITION pos = GetFirstSelectedTaskPos();

			while (pos)
			{
				DWORD dwTaskID = GetNextSelectedTaskID(pos);
				
				if (m_data.GetTaskCustomAttributeData(dwTaskID, sAttribID, data))
				{
					CStringArray aTaskItems;
					int nItem = data.AsArray(aTaskItems);

					while (nItem--)
					{
						int nCount = 0;
						mapCounts.Lookup(aTaskItems[nItem], nCount);

						mapCounts[aTaskItems[nItem]] = ++nCount;
					}
				}
			}

			CStringArray aMatched, aMixed;
			SplitSelectedTaskArrayMatchCounts(mapCounts, nSelCount, aMatched, aMixed);

			data.Set(aMatched, aMixed);
		}
		else
		{
			// get first item's value as initial
			POSITION pos = GetFirstSelectedTaskPos();
			DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
			m_data.GetTaskCustomAttributeData(dwTaskID, sAttribID, data);
		
			while (pos)
			{
				dwTaskID = GetNextSelectedTaskID(pos);
			
				TDCCADATA dataNext;
				m_data.GetTaskCustomAttributeData(dwTaskID, sAttribID, dataNext);

				if (data != dataNext)
				{
					data.Clear();
					return FALSE;
				}
			}
		}

		if (bFormatted && !data.IsEmpty())
			data.Set(CTDCCustomAttributeHelper::FormatData(data, sAttribID, m_aCustomAttribDefs));
	}
	
	return !data.IsEmpty();
}

int CTDLTaskCtrlBase::GetSelectedTaskAllocTo(CStringArray& aAllocTo) const
{
	return GetSelectedTaskArray(TDCA_ALLOCTO, aAllocTo);
}

int CTDLTaskCtrlBase::GetSelectedTaskAllocTo(CStringArray& aMatched, CStringArray& aMixed) const
{
	return GetSelectedTaskArray(TDCA_ALLOCTO, aMatched, aMixed);
}

CString CTDLTaskCtrlBase::GetSelectedTaskAllocBy() const
{
	CString sAllocBy;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		sAllocBy = m_data.GetTaskAllocBy(dwTaskID);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			CString sTaskAllocBy = m_data.GetTaskAllocBy(dwTaskID);
			
			if (sAllocBy != sTaskAllocBy)
				return _T("");
		}
	}
	
	return sAllocBy;
}

CString CTDLTaskCtrlBase::GetSelectedTaskVersion() const
{
	CString sVersion;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		sVersion = m_data.GetTaskVersion(dwTaskID);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			CString sTaskVersion = m_data.GetTaskVersion(dwTaskID);
			
			if (sVersion != sTaskVersion)
				return _T("");
		}
	}
	
	return sVersion;
}

CString CTDLTaskCtrlBase::GetSelectedTaskStatus() const
{
	CString sStatus;
	
	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		sStatus = m_data.GetTaskStatus(dwTaskID);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);
			CString sTaskStatus = m_data.GetTaskStatus(dwTaskID);
			
			if (sStatus != sTaskStatus)
				return _T("");
		}
	}
	
	return sStatus;
}

int CTDLTaskCtrlBase::GetSelectedTaskArray(TDC_ATTRIBUTE nAttrib, CStringArray& aItems) const
{
	aItems.RemoveAll();

	if (GetSelectedCount())
	{
		// get first item's value as initial
		POSITION pos = GetFirstSelectedTaskPos();
		DWORD dwTaskID = GetNextSelectedTaskID(pos);
		
		m_data.GetTaskArray(dwTaskID, nAttrib, aItems);
		
		while (pos)
		{
			dwTaskID = GetNextSelectedTaskID(pos);

			CStringArray aTaskItems;
			m_data.GetTaskArray(dwTaskID, nAttrib, aTaskItems);
			
			if (!Misc::MatchAll(aItems, aTaskItems))
			{
				aItems.RemoveAll();
				break;
			}
		}
	}
	
	return aItems.GetSize();
}

int CTDLTaskCtrlBase::GetSelectedTaskArray(TDC_ATTRIBUTE nAttrib, CStringArray& aMatched, CStringArray& aMixed) const
{
	int nSelCount = GetSelectedCount();
	CMap<CString, LPCTSTR, int, int&> mapCounts;

	POSITION pos = GetFirstSelectedTaskPos();

	while (pos)
	{
		DWORD dwTaskID = GetNextSelectedTaskID(pos);

		CStringArray aTaskItems;
		int nItem = m_data.GetTaskArray(dwTaskID, nAttrib, aTaskItems);

		while (nItem--)
		{
			int nCount = 0;
			mapCounts.Lookup(aTaskItems[nItem], nCount);

			mapCounts[aTaskItems[nItem]] = ++nCount;
		}
	}

	return SplitSelectedTaskArrayMatchCounts(mapCounts, nSelCount, aMatched, aMixed);
}

int CTDLTaskCtrlBase::SplitSelectedTaskArrayMatchCounts(const CMap<CString, LPCTSTR, int, int&>& mapCounts, int nNumTasks, CStringArray& aMatched, CStringArray& aMixed)
{
	aMatched.RemoveAll();
	aMixed.RemoveAll();

	POSITION pos = mapCounts.GetStartPosition();

	while (pos)
	{
		CString sItem;
		int nCount = 0;

		mapCounts.GetNextAssoc(pos, sItem, nCount);

		if (nCount == nNumTasks)
		{
			aMatched.Add(sItem);
		}
		else if (nCount > 0)
		{
			aMixed.Add(sItem);
		}
	}

	return aMatched.GetSize();
}

int CTDLTaskCtrlBase::GetSelectedTaskCategories(CStringArray& aCats) const
{
	return GetSelectedTaskArray(TDCA_CATEGORY, aCats);
}

int CTDLTaskCtrlBase::GetSelectedTaskCategories(CStringArray& aMatched, CStringArray& aMixed) const
{
	return GetSelectedTaskArray(TDCA_CATEGORY, aMatched, aMixed);
}

int CTDLTaskCtrlBase::GetSelectedTaskTags(CStringArray& aTags) const
{
	return GetSelectedTaskArray(TDCA_TAGS, aTags);
}

int CTDLTaskCtrlBase::GetSelectedTaskTags(CStringArray& aMatched, CStringArray& aMixed) const
{
	return GetSelectedTaskArray(TDCA_TAGS, aMatched, aMixed);
}

int CTDLTaskCtrlBase::GetSelectedTaskDependencies(CStringArray& aDepends) const
{
	return GetSelectedTaskArray(TDCA_DEPENDENCY, aDepends);
}

CString CTDLTaskCtrlBase::GetSelectedTaskFileRef(int nFile) const
{
	if (GetSelectedCount() == 1)
		return m_data.GetTaskFileRef(GetSelectedTaskID(), nFile);
	
	// else
	return _T("");
}

int CTDLTaskCtrlBase::GetSelectedTaskFileRefs(CStringArray& aFiles) const
{
	if (GetSelectedCount() == 1)
		return m_data.GetTaskFileRefs(GetSelectedTaskID(), aFiles);
	
	// else
	aFiles.RemoveAll();
	return 0;
}

int CTDLTaskCtrlBase::GetSelectedTaskFileRefCount() const
{
	if (GetSelectedCount() == 1)
		return m_data.GetTaskFileRefCount(GetSelectedTaskID());
	
	// else
	return 0;
}

CString CTDLTaskCtrlBase::GetSelectedTaskExtID() const
{
	if (GetSelectedCount() == 1)
		return m_data.GetTaskExtID(GetSelectedTaskID());
	
	// else
	return _T("");
}

BOOL CTDLTaskCtrlBase::CanSplitSelectedTask() const
{
	if (IsReadOnly())
		return FALSE;
	
	if (SelectionHasReferences())
		return FALSE;
	
	int nSelCount = GetSelectedCount();
	
	if (nSelCount == 1)
	{
		if (IsSelectedTaskDone() || SelectionHasSubtasks())
			return FALSE;
	}
	
	return (nSelCount > 0);
}

BOOL CTDLTaskCtrlBase::IsSelectedTaskDone() const
{
	return m_data.IsTaskDone(GetSelectedTaskID());
}

BOOL CTDLTaskCtrlBase::IsSelectedTaskDue() const
{
	return m_calculator.IsTaskOverDue(GetSelectedTaskID());
}

CString CTDLTaskCtrlBase::FormatInfoTip(DWORD dwTaskID, int nMaxLen) const
{
	const TODOITEM* pTDI = m_data.GetTrueTask(dwTaskID);
	ASSERT(pTDI);
	
	CString sTip;
	
	if (pTDI)
	{
		// format text multi-lined
		CString sItem;
		sItem.Format(_T("%s %s"), CEnString(IDS_TDCTIP_TASK), pTDI->sTitle);
		sTip += sItem;
		
		CString sComments = pTDI->sComments;
		int nLen = sComments.GetLength();
		
		if (nLen && m_nMaxInfotipCommentsLength != 0)
		{
			if ((m_nMaxInfotipCommentsLength > 0) && (nLen > m_nMaxInfotipCommentsLength))
				sComments = (sComments.Left(m_nMaxInfotipCommentsLength) + _T("..."));
			
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_COMMENTS), sComments);
			sTip += sItem;
		}
		
		if (IsColumnShowing(TDCC_STATUS) && !pTDI->sStatus.IsEmpty())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_STATUS), pTDI->sStatus);
			sTip += sItem;
		}
		
		if (IsColumnShowing(TDCC_CATEGORY) && pTDI->aCategories.GetSize())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_CATEGORY), m_formatter.GetTaskCategories(pTDI));
			sTip += sItem;
		}
		
		if (IsColumnShowing(TDCC_TAGS) && pTDI->aTags.GetSize())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_TAGS), m_formatter.GetTaskTags(pTDI));
			sTip += sItem;
		}
		
		if (IsColumnShowing(TDCC_ALLOCTO) && pTDI->aAllocTo.GetSize())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_ALLOCTO), m_formatter.GetTaskAllocTo(pTDI));
			sTip += sItem;
		}
		
		if (IsColumnShowing(TDCC_ALLOCBY) && !pTDI->sAllocBy.IsEmpty())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_ALLOCBY), pTDI->sAllocBy); 
			sTip += sItem;
		}
		
		if (IsColumnShowing(TDCC_VERSION) && !pTDI->sVersion.IsEmpty())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_VERSION), pTDI->sVersion); 
			sTip += sItem;
		}
		
		DWORD dwDateFmt = HasStyle(TDCS_SHOWDATESINISO) ? DHFD_ISO : 0;
		
		if (pTDI->IsDone())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_DONEDATE), CDateHelper::FormatDate(pTDI->dateDone, dwDateFmt));
			sTip += sItem;
		}
		else
		{
			if (IsColumnShowing(TDCC_PRIORITY) && pTDI->nPriority != FM_NOPRIORITY)
			{
				sItem.Format(_T("\n%s %d"), CEnString(IDS_TDCTIP_PRIORITY), pTDI->nPriority);
				sTip += sItem;
			}
			
			if (IsColumnShowing(TDCC_RISK) && pTDI->nRisk != FM_NORISK)
			{
				sItem.Format(_T("\n%s %d"), CEnString(IDS_TDCTIP_RISK), pTDI->nRisk);
				sTip += sItem;
			}
			
			if (IsColumnShowing(TDCC_STARTDATE) && pTDI->HasStart())
			{
				sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_STARTDATE), CDateHelper::FormatDate(pTDI->dateStart, dwDateFmt));
				sTip += sItem;
			}
			
			if (IsColumnShowing(TDCC_DUEDATE) && pTDI->HasDue())
			{
				if (pTDI->HasDueTime())
					dwDateFmt |= DHFD_TIME | DHFD_NOSEC;
				
				sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_DUEDATE), CDateHelper::FormatDate(pTDI->dateDue, dwDateFmt));
				sTip += sItem;
			}
			
			if (IsColumnShowing(TDCC_PERCENT))
			{
				sItem.Format(_T("\n%s %d"), CEnString(IDS_TDCTIP_PERCENT), m_calculator.GetTaskPercentDone(dwTaskID));
				sTip += sItem;
			}
			
			if (IsColumnShowing(TDCC_TIMEEST))
			{
				sItem.Format(_T("\n%s %.2f %c"), CEnString(IDS_TDCTIP_TIMEEST), m_calculator.GetTaskTimeEstimate(dwTaskID, TDCU_HOURS), CTimeHelper::GetUnits(THU_HOURS));
				sTip += sItem;
			}
			
			if (IsColumnShowing(TDCC_TIMESPENT))
			{
				sItem.Format(_T("\n%s %.2f %c"), CEnString(IDS_TDCTIP_TIMESPENT), m_calculator.GetTaskTimeSpent(dwTaskID, TDCU_HOURS), CTimeHelper::GetUnits(THU_HOURS));
				sTip += sItem;
			}
		}
		
		if (IsColumnShowing(TDCC_COST))
		{
			sItem.Format(_T("\n%s %.2f"), CEnString(IDS_TDCTIP_COST), m_calculator.GetTaskCost(dwTaskID));
			sTip += sItem;
		}
		
		if (IsColumnShowing(TDCC_DEPENDENCY))
		{
			if (pTDI->aDependencies.GetSize())
			{
				sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_DEPENDS), Misc::FormatArray(pTDI->aDependencies));
				sTip += sItem;
			}
			
			CDWordArray aDependents;
			
			if (m_data.GetTaskLocalDependents(dwTaskID, aDependents))
			{
				sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_DEPENDENTS), Misc::FormatArray(aDependents));
				sTip += sItem;
			}
		}
		
		if (IsColumnShowing(TDCC_FILEREF) && pTDI->aFileLinks.GetSize())
		{
			sItem.Format(_T("\n%s %s"), CEnString(IDS_TDCTIP_FILEREF), pTDI->aFileLinks[0]);
			sTip += sItem;
		}
		
		if (pTDI->dateLastMod > 0)
		{
			sItem.Format(_T("\n%s %s (%s)"), CEnString(IDS_TDCTIP_LASTMOD), pTDI->dateLastMod.Format(VAR_DATEVALUEONLY), pTDI->dateLastMod.Format(VAR_TIMEVALUEONLY));
			sTip += sItem;
		}
	}

	// Truncate to fit with ellipsis
	if (sTip.GetLength() > nMaxLen)
		sTip = sTip.Left(nMaxLen - 3) + _T("...");
	
	return sTip;
}

BOOL CTDLTaskCtrlBase::PreTranslateMessage(MSG* pMsg)
{
	m_tooltipColumns.FilterToolTipMessage(pMsg);

	switch (pMsg->message)
	{
	case WM_KEYDOWN:
		// Do our custom column resizing because Windows own
		// does not understand how we do things!
		if ((IsLeft(pMsg->hwnd) || IsRight(pMsg->hwnd)) &&
			(pMsg->wParam == VK_ADD) && 
			Misc::ModKeysArePressed(MKS_CTRL))
		{
			RecalcAllColumnWidths();
			return TRUE;
		}
		break;
	}
	
	// all else
	return CWnd::PreTranslateMessage(pMsg);
}

BOOL CTDLTaskCtrlBase::OnHelpInfo(HELPINFO* /*lpHelpInfo*/)
{
	AfxGetApp()->WinHelp(GetHelpID());
	return TRUE;
}

BOOL CTDLTaskCtrlBase::SaveToImage(CBitmap& bmImage)
{
	if (!CanSaveToImage())
		return FALSE;

	CLockUpdates lock(GetSafeHwnd());

	return CTreeListSyncer::SaveToImage(bmImage, m_crGridLine);
}

BOOL CTDLTaskCtrlBase::CanSaveToImage() const
{
	return (GetTaskCount() > 0);
}