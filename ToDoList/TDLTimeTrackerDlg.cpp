// TDCTimeTrackerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "TDLTimeTrackerDlg.h"
#include "tdcmsg.h"
#include "taskfile.h"

#include "..\shared\regkey.h"
#include "..\shared\HoldRedraw.h"
#include "..\shared\misc.h"
#include "..\shared\graphicsmisc.h"
#include "..\shared\dlgunits.h"
#include "..\shared\autoflag.h"
#include "..\shared\localizer.h"

#include "..\Interfaces\Preferences.h"

/////////////////////////////////////////////////////////////////////////

enum // btns
{
	BTN_STARTENABLED,
	BTN_STOPENABLED,
	BTN_STARTDISABLED,
	BTN_STOPDISABLED,
};

const int ID_RESET_ELAPSED = 1;

/////////////////////////////////////////////////////////////////////////

TRACKITEM::TRACKITEM() : dwTaskID(0), bParent(FALSE) 
{
}

BOOL TRACKITEM::operator==(const TRACKITEM& ti) const
{
	return (sTaskTitle == ti.sTaskTitle) && 
			(dwTaskID == ti.dwTaskID) &&
			(bParent == ti.bParent);
}

BOOL TRACKITEM::operator!=(const TRACKITEM& ti) const
{
	return !(*this == ti);
}

CString TRACKITEM::GetTaskTitle(BOOL bWantPath) const
{
	CString sTemp;

	if (bWantPath && !sTaskPath.IsEmpty())
	{
#ifdef _DEBUG
		sTemp.Format(_T("%s (%d) (%s)"), sTaskTitle, dwTaskID, sTaskPath);
#else
		sTemp.Format(_T("%s (%s)"), sTaskTitle, sTaskPath);
#endif
	}
	else
	{
#ifdef _DEBUG
		sTemp.Format(_T("%s (%d)"), sTaskTitle, dwTaskID);
#else
		sTemp = sTaskTitle;
#endif
	}

	return sTemp;
}

/////////////////////////////////////////////////////////////////////////

TRACKTASKLIST::TRACKTASKLIST() 
	: 
	pTDC(NULL), 
	dwTrackedTaskID(0), 
	bTrackingPaused(FALSE),
	pTasks(new CTrackItemArray) 
{
}
	
TRACKTASKLIST::~TRACKTASKLIST()
{
	delete pTasks;
}

int TRACKTASKLIST::SetTasks(const CTaskFile& tasks)
{
	ASSERT(pTasks);
	pTasks->RemoveAll();

	CMapTaskIndex mapTasks; // empty
	UpdateTasks(tasks, NULL, _T(""), mapTasks);

	return pTasks->GetSize();
}
	
BOOL TRACKTASKLIST::UpdateTasks(const CTaskFile& tasks, HTASKITEM hTask, const CString& sParentPath, const CMapTaskIndex& mapTasks)
{
	ASSERT(pTasks);

	BOOL bChange = FALSE;
	BOOL bDone = FALSE;
	CString sTaskPath;

	if (hTask)
	{
		bDone = tasks.IsTaskDone(hTask);

		if (!bDone)
		{
			DWORD dwTaskID = tasks.GetTaskID(hTask);
			ASSERT(dwTaskID);

			TRACKITEM ti;

			ti.sTaskTitle = tasks.GetTaskTitle(hTask);
			ti.sTaskPath = sParentPath;
			ti.dwTaskID = dwTaskID;
			ti.bParent = (tasks.GetFirstTask(hTask) != NULL);

			int nExist = -1;

			if (mapTasks.Lookup(dwTaskID, nExist))
			{
				ASSERT(nExist != -1);
				TRACKITEM& tiExist = pTasks->GetAt(nExist);

				if (tiExist != ti)
				{
					tiExist = ti;
					bChange = TRUE;
				}
			}
			else // new
			{
				pTasks->Add(ti);
				bChange = TRUE;
			}

			if (tasks.IsTaskParent(hTask))
			{
				if (!sParentPath.IsEmpty())
					sTaskPath = (sParentPath + '\\');

				sTaskPath += ti.sTaskTitle;
			}
		}
	}
	
	// children
	if (!bDone)
	{
		HTASKITEM hSubtask = tasks.GetFirstTask(hTask);
	
		while (hSubtask)
		{
			bChange |= UpdateTasks(tasks, hSubtask, sTaskPath, mapTasks);
			hSubtask = tasks.GetNextTask(hSubtask);
		}
	}

	return bChange;
}

BOOL TRACKTASKLIST::UpdateTasks(const CTaskFile& tasks)
{
	CMapTaskIndex mapTasks;
	BuildTaskMap(mapTasks);

	return UpdateTasks(tasks, NULL, _T(""), mapTasks);
}

BOOL TRACKTASKLIST::RemoveTasks(DWORD dwToRemove)
{
	int nNumTask = pTasks->GetSize(), nTask = nNumTask;

	while (nTask--)
	{
		const TRACKITEM& ti = pTasks->GetData()[nTask];

		BOOL bRemove = (Misc::HasFlag(dwToRemove, TTL_REMOVEDELETED) && 
						!pTDC->HasTask(ti.dwTaskID));

		if (!bRemove)
		{
			bRemove = (Misc::HasFlag(dwToRemove, TTL_REMOVEDONE) && 
						pTDC->IsTaskDone(ti.dwTaskID));

			// if (!bRemove)
			// {
			//    ...
			// }
		}

		if (bRemove)
			pTasks->RemoveAt(nTask);
	}

	return (pTasks->GetSize() != nNumTask);
}

void TRACKTASKLIST::BuildTaskMap(CMapTaskIndex& mapTasks) const
{
	mapTasks.RemoveAll();

	if (pTasks)
	{
		int nTask = pTasks->GetSize();

		while (nTask--)
		{
			const TRACKITEM& ti = pTasks->GetData()[nTask];
			mapTasks[ti.dwTaskID] = nTask;
		}
	}
}

BOOL TRACKTASKLIST::IsTracking(DWORD dwTaskID) const
{
	if (dwTrackedTaskID == 0)
		return FALSE;

	if (dwTaskID && (dwTaskID != dwTrackedTaskID))
		return FALSE;

	return !bTrackingPaused;
}

/////////////////////////////////////////////////////////////////////////

CTDCTrackTasklistArray::CTDCTrackTasklistArray()
{
}

CTDCTrackTasklistArray::~CTDCTrackTasklistArray()
{
	DeleteAllTasklists();
}

int CTDCTrackTasklistArray::GetNumTasklists() const
{
	return GetSize();
}

int CTDCTrackTasklistArray::AddTasklist(const CFilteredToDoCtrl* pTDC, const CTaskFile& tasks)
{
	if (!pTDC || (FindTasklist(pTDC) != -1))
	{
		ASSERT(0);
		return -1;
	}

	TRACKTASKLIST* pTTL = new TRACKTASKLIST;

	pTTL->pTDC = pTDC;

	if (tasks.GetTaskCount())
		pTTL->SetTasks(tasks);

	return CArray<TRACKTASKLIST*, TRACKTASKLIST*&>::Add(pTTL);
}

BOOL CTDCTrackTasklistArray::DeleteTasklist(const CFilteredToDoCtrl* pTDC)
{
	int nTasklist = FindTasklist(pTDC);
	ASSERT(nTasklist != -1);

	if (nTasklist != -1)
	{
		delete GetAt(nTasklist);
		RemoveAt(nTasklist);

		return TRUE;
	}

	// else
	return FALSE;
}

void CTDCTrackTasklistArray::DeleteAllTasklists()
{
	int nTasklist = GetNumTasklists();
	
	while (nTasklist--)
	{
		delete GetAt(nTasklist);
		RemoveAt(nTasklist);
	}
}

BOOL CTDCTrackTasklistArray::UpdateTracking(const CFilteredToDoCtrl* pTDC)
{
	return UpdateTracking(GetTasklist(pTDC));
}

BOOL CTDCTrackTasklistArray::UpdateTracking(TRACKTASKLIST* pTTL)
{
	if (!pTTL)
	{
		ASSERT(0);
		return FALSE;
	}

	// else
	DWORD dwTrackedTaskID = pTTL->pTDC->GetTimeTrackTaskID(FALSE);

	pTTL->dwTrackedTaskID = dwTrackedTaskID;
	pTTL->bTrackingPaused = (dwTrackedTaskID && (pTTL->pTDC->GetTimeTrackTaskID(TRUE) != dwTrackedTaskID));

	return TRUE;
}

int CTDCTrackTasklistArray::FindTasklist(const CFilteredToDoCtrl* pTDC) const
{
	int nTasklist = GetNumTasklists();
	
	while (nTasklist--)
	{
		if (GetAt(nTasklist)->pTDC == pTDC)
			return nTasklist;
	}

	// else
	return -1;
}

const TRACKTASKLIST* CTDCTrackTasklistArray::GetTasklist(const CFilteredToDoCtrl* pTDC) const
{
	int nFind = FindTasklist(pTDC);
	ASSERT(nFind != -1);
	
	return ((nFind != -1) ? GetAt(nFind) : NULL);
}

TRACKTASKLIST* CTDCTrackTasklistArray::GetTasklist(const CFilteredToDoCtrl* pTDC)
{
	int nFind = FindTasklist(pTDC);
	ASSERT(nFind != -1);
	
	return ((nFind != -1) ? GetAt(nFind) : NULL);
}

const TRACKTASKLIST* CTDCTrackTasklistArray::GetTasklist(int nTasklist) const
{
	ASSERT((nTasklist >= 0) && (nTasklist < GetNumTasklists()));

	if ((nTasklist >= 0) && (nTasklist < GetNumTasklists()))
		return GetAt(nTasklist);

	return NULL;
}

const CTrackItemArray* CTDCTrackTasklistArray::GetTasks(const CFilteredToDoCtrl* pTDC) const
{
	const TRACKTASKLIST* pTTL = GetTasklist(pTDC);

	return (pTTL ? pTTL->pTasks : NULL);
}

CTrackItemArray* CTDCTrackTasklistArray::GetTasks(const CFilteredToDoCtrl* pTDC)
{
	TRACKTASKLIST* pTTL = GetTasklist(pTDC);
	
	return (pTTL ? pTTL->pTasks : NULL);
}

/////////////////////////////////////////////////////////////////////////

CTDLTimeTrackerDlg::CTDLTimeTrackerDlg()
	: 
	CDialog(CTDLTimeTrackerDlg::IDD), 
	m_pWndNotify(NULL),
	m_btnStart(32),
	m_sizeMin(0, 0),
	m_sizeMax(32000, 32000),
	m_sizeLast(-1, -1),
	m_bCollapsed(FALSE),
	m_bRecreating(FALSE),
	m_bCentreOnShow(FALSE),
	m_dwOptions(0)
{
}

CTDLTimeTrackerDlg::~CTDLTimeTrackerDlg()
{
}

void CTDLTimeTrackerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TASKLISTS, m_cbTasklists);
	DDX_Control(pDX, IDC_TASKS, m_cbTasks);
	DDX_Control(pDX, IDC_STARTSTOP, m_btnStart);
	DDX_Control(pDX, IDC_ELAPSEDTIME, m_eElapsedTime);
	DDX_Text(pDX, IDC_TASKTIME, m_sTaskTimes);
	DDX_Text(pDX, IDC_ELAPSEDTIME, m_sElapsedTime);
	DDX_Text(pDX, IDC_QUICKFIND, m_sQuickFind);
}

/////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CTDLTimeTrackerDlg, CDialog)
	ON_BN_CLICKED(IDC_STARTSTOP, OnStartStopTracking)
	ON_WM_CTLCOLOR()
	ON_WM_NCHITTEST()
	ON_CBN_SELCHANGE(IDC_TASKLISTS, OnSelchangeTasklist)
	ON_CBN_SELCHANGE(IDC_TASKS, OnSelchangeTask)
	ON_EN_CHANGE(IDC_QUICKFIND, OnChangeQuickFind)
	ON_WM_ERASEBKGND()
	ON_WM_ACTIVATE()
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXT, 0, 0xFFFF, OnToolTipNotify)
	ON_WM_SHOWWINDOW()
	ON_WM_GETMINMAXINFO()
	ON_WM_DESTROY()
	ON_WM_NCLBUTTONDBLCLK()
	ON_COMMAND(ID_TIMETRACKER_ONTOP, OnToggleTopMost)
	ON_COMMAND(ID_TIMETRACK_HELP, OnHelp)
	ON_WM_HELPINFO()
	ON_REGISTERED_MESSAGE(WM_EE_BTNCLICK, OnEEBtnClick)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////

void CTDLTimeTrackerDlg::OnHelp()
{
	AfxGetApp()->WinHelp(IDD_TIMETRACK_DIALOG);
}

BOOL CTDLTimeTrackerDlg::OnHelpInfo(HELPINFO* /*lpHelpInfo*/)
{
	OnHelp();
	return TRUE;
}

BOOL CTDLTimeTrackerDlg::Create(CWnd* pNotify, DWORD dwOptions)
{
	// Called externally so get topmost state from prefs
	m_bAlwaysOnTop = CPreferences().GetProfileInt(_T("TimeTracker"), _T("AlwaysOnTop"), TRUE);
	m_dwOptions = dwOptions;

	if (!Create(FALSE))
		return FALSE;

	m_pWndNotify = pNotify;
	return TRUE;
}

BOOL CTDLTimeTrackerDlg::Create(BOOL bVisible)
{
	DPI_AWARENESS_CONTEXT nPrev = GraphicsMisc::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

	BOOL bResult = CDialog::Create(IDD_TIMETRACK_DIALOG, m_bAlwaysOnTop ? GetDesktopWindow() : AfxGetMainWnd());

	GraphicsMisc::SetThreadDpiAwarenessContext(nPrev);

	if (!bResult)
	{
		ASSERT(0);
		m_pWndNotify = NULL;

		return FALSE;
	}

	if (m_bAlwaysOnTop)
		SetWindowPos(&CWnd::wndTopMost, 0, 0, 0, 0, (SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));

	if (bVisible)
		ShowWindow(SW_SHOW);
	
	return TRUE;
}

BOOL CTDLTimeTrackerDlg::Recreate()
{
	CAutoFlag af(m_bRecreating, TRUE);
	
	// Cache current state
	const CFilteredToDoCtrl* pTDC = GetSelectedTasklist();
	DWORD dwTaskID = GetSelectedTaskID();
	
	CRect rPrev;
	GetWindowRect(rPrev);
	
	DestroyWindow();
	
	if (!Create(TRUE)) // visible
		return FALSE;
	
	if (!RebuildTasklistCombo())
		return FALSE;
	
	if (!SelectTaskList(pTDC))
		return FALSE;
	
	if (CB_ERR == SelectItemByData(m_cbTasks, dwTaskID))
		return FALSE;
	
	// restore position
	MoveWindow(rPrev);
	return TRUE;
}

BOOL CTDLTimeTrackerDlg::OnInitDialog()
{	
	CDialog::OnInitDialog();

	CLocalizer::EnableTranslation(m_cbTasks, FALSE);
	CLocalizer::EnableTranslation(m_cbTasklists, FALSE);

	GetWindowText(m_sOrgTitle);
	
	if (m_ilBtns.Create(32, 32, (ILC_COLOR32 | ILC_MASK), 0, 0))
	{
		CBitmap bmp;
		
		if (bmp.LoadBitmap(IDB_TIMETRACK_BTNS))
			m_ilBtns.Add(&bmp, RGB(255, 0, 255));
	}

	if (m_toolbar.CreateEx(this) && m_toolbar.LoadToolBar(IDR_TIMETRACKER_TOOLBAR))
	{
		m_toolbar.SetImage(IDB_TIMETRACK_TOOLBAR_STD, RGB(255, 0, 255));
		m_toolbar.SetDlgCtrlID(IDC_TOOLBAR);
		m_toolbar.MoveWindow(GetCtrlRect(this, IDC_TOOLBAR));
		m_toolbar.GetToolBarCtrl().CheckButton(ID_TIMETRACKER_ONTOP, m_bAlwaysOnTop);
		m_toolbar.SetBackgroundColors(m_theme.crAppBackLight, CLR_NONE, FALSE, FALSE);
		m_toolbar.SetHotColor(m_theme.crToolbarHot);

		m_tbHelper.Initialize(&m_toolbar, this);
	}

	m_mgrPrompts.SetEditPrompt(IDC_QUICKFIND, *this, IDS_QUICKTASKFIND);

	m_iconResetElapsed.LoadIcon(IDI_RESET_ELAPSED);
	m_eElapsedTime.AddButton(ID_RESET_ELAPSED, m_iconResetElapsed, CEnString(IDS_RESET_ELAPSED), 15);
		
	m_iconDlg.LoadIcon(IDR_MAINFRAME_STD);
	SetIcon(m_iconDlg, TRUE);

	EnableToolTips(TRUE);
	CalcMinMaxSizes();
	LoadSettings();
	
	return TRUE;
}

BOOL CTDLTimeTrackerDlg::PreTranslateMessage(MSG* pMsg)
{
	// we need to check for <return> in quick find
	if ((pMsg->message == WM_KEYDOWN) && 
		(::GetDlgCtrlID(pMsg->hwnd) == IDC_QUICKFIND) &&
		(::GetFocus() == pMsg->hwnd))
	{
		switch (pMsg->wParam)
		{
		case VK_RETURN:
		case VK_F3:
			{
				BOOL bForward = !Misc::ModKeysArePressed(MKS_SHIFT);
				
				int nSel = m_cbTasks.GetCurSel();
				int nFrom = (bForward ? (nSel + 1) : (nSel - 1));
				
				int nNext = QuickFindNextTaskComboItem(nFrom, bForward);
				
				if ((nNext != CB_ERR) && (nNext != nSel))
				{
					m_cbTasks.SetCurSel(nNext);
					
					UpdatePlayButton();
					UpdateTaskTime(GetSelectedTasklist());
					
					return TRUE;
				}
			}
			break;
		}
	}
	
	// else default handling
	return CDialog::PreTranslateMessage(pMsg);
}

void CTDLTimeTrackerDlg::SetUITheme(const CUIThemeFile& theme)
{
	CUIThemeFile oldTheme = m_theme;
	m_theme = theme;
	
	if (m_theme != oldTheme)
	{
		m_brBack.DeleteObject();

		// Use crAppBackLight so the toolbar merges with the bkgnd
		m_toolbar.SetBackgroundColors(m_theme.crAppBackLight, m_theme.crAppBackLight, m_theme.HasGradient(), m_theme.HasGlass());
		m_toolbar.SetHotColor(m_theme.crToolbarHot);
		
		Invalidate(TRUE);
		SendMessage(WM_NCPAINT);
	}
}

DWORD CTDLTimeTrackerDlg::GetTasklistTrackID(const CFilteredToDoCtrl* pTDC) const
{
	const TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(pTDC);
	ASSERT(pTTL);
	
	return (pTTL ? pTTL->dwTrackedTaskID : 0);
}

int CTDLTimeTrackerDlg::GetTasklistCBIndex(const CFilteredToDoCtrl* pTDC) const
{
	ASSERT(m_cbTasklists.GetSafeHwnd());
	
	return CDialogHelper::FindItemByData(m_cbTasklists, (DWORD)pTDC);
}

BOOL CTDLTimeTrackerDlg::HasTasklist(const CFilteredToDoCtrl* pTDC) const
{
	return (m_aTasklists.FindTasklist(pTDC) != -1);
}

BOOL CTDLTimeTrackerDlg::AddTasklist(const CFilteredToDoCtrl* pTDC)
{
	CTaskFile tasks; // empty
	return AddTasklist(pTDC, tasks);
}

BOOL CTDLTimeTrackerDlg::AddTasklist(const CFilteredToDoCtrl* pTDC, const CTaskFile& tasks)
{
	if (m_aTasklists.AddTasklist(pTDC, tasks) == -1)
	{
		ASSERT(0);
		return FALSE;
	}
	
	// else
	CEnString sTitle;
	
	if (pTDC->IsDelayLoaded())
		sTitle.Format(IDS_TIMETRACKER_DELAYLOADED, pTDC->GetFriendlyProjectName());
	else
		sTitle = pTDC->GetFriendlyProjectName();

	int nTDC = AddString(m_cbTasklists, sTitle, (DWORD)pTDC);
	
	if (nTDC == CB_ERR)
	{
		ASSERT(0);
		return FALSE;
	}
	
	// Select first non-delay-loaded tasklist
	if ((m_cbTasklists.GetCurSel() == CB_ERR) && !pTDC->IsDelayLoaded())
	{
		m_cbTasklists.SetCurSel(nTDC);
		
		UpdatePlayButton();
		RebuildTaskCombo();
	}
	
	RefreshMaxDropWidth(m_cbTasklists);
	
	return TRUE;
}

BOOL CTDLTimeTrackerDlg::UpdateTasks(const CFilteredToDoCtrl* pTDC, const CTaskFile& tasks)
{
	TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(pTDC);

	if (!pTTL)
	{
		ASSERT(0);
		return FALSE;
	}

	UpdateTasklistName(pTDC);

	BOOL bChanges = pTTL->UpdateTasks(tasks);
	bChanges |= pTTL->RemoveTasks(TTL_REMOVEDELETED);
	
	if (bChanges && IsSelectedTasklist(pTDC))
	{
		RebuildTaskCombo();
	}
	
	UpdatePlayButton();
	UpdateTaskTime(pTDC);
	
	return TRUE;
}

void CTDLTimeTrackerDlg::UpdateTasklistName(const CFilteredToDoCtrl* pTDC)
{
	int nTDC = FindItemByData(m_cbTasklists, (DWORD)pTDC);

	if (nTDC == CB_ERR)
	{
		ASSERT(0);
		return;
	}

	CString sCurName = GetItem(m_cbTasklists, nTDC);
	CString sNewName = pTDC->GetFriendlyProjectName();

	if (sNewName != sCurName)
	{
		BOOL bSelected = IsSelectedTasklist(pTDC);

		m_cbTasklists.DeleteString(nTDC);

		int nTDC = AddString(m_cbTasklists, sNewName, (DWORD)pTDC);
		ASSERT(nTDC != CB_ERR);

		// Restore 
		if (bSelected)
			m_cbTasklists.SetCurSel(nTDC);
	}
}

void CTDLTimeTrackerDlg::RemoveDeletedTasks(const CFilteredToDoCtrl* pTDC)
{
	RemoveTasks(pTDC, TTL_REMOVEDELETED);
}

void CTDLTimeTrackerDlg::RemoveCompletedTasks(const CFilteredToDoCtrl* pTDC)
{
	RemoveTasks(pTDC, TTL_REMOVEDONE);
}

void CTDLTimeTrackerDlg::RemoveTasks(const CFilteredToDoCtrl* pTDC, DWORD dwToRemove)
{
	TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(pTDC);

	if (!pTTL)
	{
		ASSERT(0);
		return;
	}

	if (pTTL->RemoveTasks(dwToRemove) && IsSelectedTasklist(pTDC))
	{
		RebuildTaskCombo();
	}

	UpdatePlayButton();
	UpdateTaskTime(pTDC);
}

BOOL CTDLTimeTrackerDlg::SelectTaskList(const CFilteredToDoCtrl* pTDC)
{
	// Select the tasklist
	if (CB_ERR == SelectItemByData(m_cbTasklists, (DWORD)pTDC))
		return FALSE;

	// Update UI
	OnSelchangeTasklist();

	return TRUE;
}

const CFilteredToDoCtrl* CTDLTimeTrackerDlg::GetSelectedTasklist() const
{
	return (const CFilteredToDoCtrl*)GetSelectedItemData(m_cbTasklists);
}

BOOL CTDLTimeTrackerDlg::IsSelectedTasklist(const CFilteredToDoCtrl* pTDC) const
{
	return (pTDC && (pTDC == GetSelectedTasklist()));
}

DWORD CTDLTimeTrackerDlg::GetSelectedTaskID() const
{
	return GetSelectedItemData(m_cbTasks);
}

BOOL CTDLTimeTrackerDlg::RebuildTasklistCombo()
{
	int nTasklist = m_aTasklists.GetNumTasklists();

	while (nTasklist--)
	{
		const TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(nTasklist);

		if (!pTTL)
		{
			ASSERT(0);
			return FALSE;
		}

		CEnString sTitle;
		
		if (pTTL->pTDC->IsDelayLoaded())
			sTitle.Format(IDS_TIMETRACKER_DELAYLOADED, pTTL->pTDC->GetFriendlyProjectName());
		else
			sTitle = pTTL->pTDC->GetFriendlyProjectName();
		
		if (AddString(m_cbTasklists, sTitle, (DWORD)pTTL->pTDC) == CB_ERR)
		{
			ASSERT(0);
			return FALSE;
		}
	}
	
	RefreshMaxDropWidth(m_cbTasklists);
	return TRUE;
}

void CTDLTimeTrackerDlg::RebuildTaskCombo()
{
	DWORD dwSelID = GetSelectedTaskID();
	
	m_cbTasks.ResetContent();

	if (m_aTasklists.GetNumTasklists() == 0)
		return;

	const CFilteredToDoCtrl* pTDC = GetSelectedTasklist();
	ASSERT(pTDC);

	const CTrackItemArray* pTasks = m_aTasklists.GetTasks(pTDC);

	if (pTasks)
	{
		CHoldRedraw hr(m_cbTasks);
	
		BOOL bWantParents = HasOption(TTDO_ALLOWPARENTTRACKING);
		BOOL bWantPath = HasOption(TTDO_SHOWTASKPATH);

		int nTask = pTasks->GetSize();
	
		while (nTask--)
		{
			const TRACKITEM& ti = pTasks->GetData()[nTask];
		
			if (!bWantParents && ti.bParent)
				continue;

			VERIFY(AddString(m_cbTasks, ti.GetTaskTitle(bWantPath), ti.dwTaskID) != CB_ERR);
		}
	
		RefreshMaxDropWidth(m_cbTasks);
		SelectItemByData(m_cbTasks, dwSelID);
	}
}

BOOL CTDLTimeTrackerDlg::RemoveTasklist(const CFilteredToDoCtrl* pTDC)
{
	ASSERT(m_pWndNotify && GetSafeHwnd());
	
	int nCBTasklist = GetTasklistCBIndex(pTDC);
	int nTasklist = m_aTasklists.FindTasklist(pTDC);
	
	if ((nTasklist == CB_ERR) || (nTasklist == -1))
	{
		ASSERT(0);
		return FALSE;
	}
	
	// else
	if (IsSelectedTasklist(pTDC))
		m_cbTasks.ResetContent();
	
	m_cbTasklists.DeleteString(nCBTasklist);
	m_aTasklists.DeleteTasklist(pTDC);

	return TRUE;
}

void CTDLTimeTrackerDlg::RemoveAllTasklists()
{
	ASSERT(m_pWndNotify && GetSafeHwnd());
	
	m_cbTasklists.ResetContent();
	m_cbTasks.ResetContent();
	m_aTasklists.DeleteAllTasklists();

	m_sTaskTimes.Empty();
	m_sElapsedTime.Empty();

	UpdateData(FALSE);
	UpdatePlayButton();
}

BOOL CTDLTimeTrackerDlg::UpdateTracking(const CFilteredToDoCtrl* pTDC)
{
	ASSERT(m_pWndNotify && GetSafeHwnd());
	ASSERT(pTDC);
	
	// Update data struct first
	TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(pTDC);

	if (!pTTL)
	{
		ASSERT(0);
		return FALSE;
	}

	BOOL bWasTracking = pTTL->IsTracking();

	VERIFY(m_aTasklists.UpdateTracking(pTTL));
	ASSERT((bWasTracking && !pTTL->IsTracking()) || (!bWasTracking && pTTL->IsTracking()));

	// If we've just started tracking, switch to that tasklist
	// and show the dialog if required
	if (pTTL->IsTracking() && !bWasTracking)
	{
		SelectTaskList(pTDC);

		if (HasOption(TTDO_SHOWONBEGINTRACKING))
		{
			ShowWindow(SW_SHOWNORMAL);
			SetForegroundWindow();
		}
	}
	else
	{
		UpdatePlayButton();
		UpdateTaskTime(pTDC);
	}
	
	RefreshTitleText();

	return TRUE;
}

BOOL CTDLTimeTrackerDlg::IsSelectedTask(DWORD dwTaskID) const
{
	return (dwTaskID && (dwTaskID == GetSelectedTaskID()));
}

BOOL CTDLTimeTrackerDlg::IsTrackingSelectedTasklistAndTask() const
{
	CString sUnused;
	return IsTrackingSelectedTasklistAndTask(sUnused);
}

BOOL CTDLTimeTrackerDlg::IsTrackingSelectedTasklistAndTask(CString& sTaskTitle) const
{
	const CFilteredToDoCtrl* pTDC = GetSelectedTasklist();

	if (!pTDC)
		return FALSE;

	const TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(pTDC);

	if (!pTTL)
	{
		ASSERT(0);
		return FALSE;
	}

	DWORD dwSelID = GetSelectedTaskID();

	if (!pTTL->IsTracking(dwSelID))
		return FALSE;

	sTaskTitle = pTTL->pTDC->GetTaskTitle(dwSelID);
	return TRUE;
}

void CTDLTimeTrackerDlg::UpdatePlayButton(BOOL bCheckVisibility)
{
	if (bCheckVisibility && (!IsWindowVisible() || m_bCollapsed))
		return;

	BOOL bEnable = ((m_cbTasklists.GetCurSel() != CB_ERR) &&
					(m_cbTasks.GetCurSel() != CB_ERR));
	
	if (bEnable)
	{
		BOOL bTracking = IsTrackingSelectedTasklistAndTask();

		m_btnStart.SetIcon(m_ilBtns.ExtractIcon(bTracking ? BTN_STOPENABLED : BTN_STARTENABLED));
		m_btnStart.SetTooltip(CEnString(bTracking ? IDS_STOP_TRACKING : IDS_START_TRACKING));
	}
	else
	{
		m_btnStart.SetIcon(m_ilBtns.ExtractIcon(BTN_STARTDISABLED));
		m_btnStart.SetTooltip(NULL);
	}

	m_btnStart.EnableWindow(bEnable);
}

void CTDLTimeTrackerDlg::UpdateTaskTime(const CFilteredToDoCtrl* pTDC)
{
	if (!IsWindowVisible() || !IsSelectedTasklist(pTDC))
		return;

	DWORD dwSelTaskID = GetSelectedTaskID();

	CTimeHelper th;
	double dTimeEst = 0.0, dTimeSpent = 0.0;
	TDC_UNITS nEstUnits = TDCU_HOURS, nSpentUnits = TDCU_HOURS;
	
	m_sTaskTimes.Empty();
	m_sElapsedTime.Empty();
	
	if (dwSelTaskID)
		pTDC->GetTaskTimes(dwSelTaskID, dTimeEst, nEstUnits, dTimeSpent, nSpentUnits);

	m_sElapsedTime = pTDC->FormatTimeTrackingElapsedTime();
	
	if (HasOption(TTDO_FORMATTIMESASHMS))
	{
		m_sTaskTimes.Format(_T("%s : %s"),
			th.FormatTimeHMS(dTimeEst, TDC::MapUnitsToTHUnits(nEstUnits), (HMS_ALLOWZERO | HMS_DECIMALPLACES)),
			th.FormatTimeHMS(dTimeSpent, TDC::MapUnitsToTHUnits(nSpentUnits), (HMS_ALLOWZERO | HMS_DECIMALPLACES)));
	}
	else
	{
		m_sTaskTimes.Format(_T("%s : %s"),
			th.FormatTime(dTimeEst, TDC::MapUnitsToTHUnits(nEstUnits), 2),
			th.FormatTime(dTimeSpent, TDC::MapUnitsToTHUnits(nSpentUnits), 2));
	}
		
	if (m_bCollapsed)
		RefreshTitleText();

	UpdateData(FALSE);

	if (IsTrackingSelectedTasklistAndTask())
	{
		GetDlgItem(IDC_TASKTIME)->Invalidate(FALSE);
		GetDlgItem(IDC_ELAPSEDTIME)->Invalidate(FALSE);
	}
}

void CTDLTimeTrackerDlg::OnStartStopTracking()
{
	ASSERT(m_cbTasklists.GetCurSel() != CB_ERR);
	
	const TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(GetSelectedTasklist());

	if (!pTTL)
	{
		ASSERT(0);
		return;
	}
	
	DWORD dwSelTaskID = GetSelectedTaskID();

	if (pTTL->dwTrackedTaskID == dwSelTaskID)
	{
		// notify parent to STOP tracking
		m_pWndNotify->SendMessage(WM_TDLTTN_STOPTRACKING, 0, (LPARAM)pTTL->pTDC);
	}
	else
	{
		// notify parent to START tracking
		m_pWndNotify->SendMessage(WM_TDLTTN_STARTTRACKING, dwSelTaskID, (LPARAM)pTTL->pTDC);
	}

	UpdateTracking(pTTL->pTDC);
	RefreshTitleText();

	// redraw text colour
	GetDlgItem(IDC_TASKTIME)->Invalidate(FALSE);
	GetDlgItem(IDC_ELAPSEDTIME)->Invalidate(FALSE);
}

HBRUSH CTDLTimeTrackerDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if (!Misc::IsHighContrastActive() && !m_bRecreating)
	{
		if (nCtlColor == CTLCOLOR_STATIC)
		{
			pDC->SetBkMode(TRANSPARENT);

			if (!m_brBack.GetSafeHandle())
				m_brBack.CreateSolidBrush(GetBkgndColor());

			hbr = (HBRUSH)m_brBack.GetSafeHandle();

			pDC->SetTextColor(m_theme.crAppText);
		}

		if (IsTrackingSelectedTasklistAndTask())
		{
			switch (pWnd->GetDlgCtrlID())
			{
			case IDC_TASKTIME:
			case IDC_ELAPSEDTIME:
				pDC->SetTextColor(255);
				break;
			}
		}
	}

	return hbr;
}

#if _MSC_VER >= 1400
afx_msg LRESULT CTDLTimeTrackerDlg::OnNcHitTest(CPoint point)
#else
afx_msg UINT CTDLTimeTrackerDlg::OnNcHitTest(CPoint point)
#endif
{
	LRESULT nHitTest = CDialog::OnNcHitTest(point);
	
	if (nHitTest == HTCLIENT)
		nHitTest = HTCAPTION;
	
	return nHitTest;
}

void CTDLTimeTrackerDlg::OnSelchangeTasklist()
{
	const CFilteredToDoCtrl* pTDC = GetSelectedTasklist();
	ASSERT(pTDC);

	if (pTDC->IsDelayLoaded())
	{
		int nSel = m_cbTasklists.GetCurSel();

		if (m_pWndNotify->SendMessage(WM_TDLTTN_LOADDELAYEDTASKLIST, 0, (LPARAM)pTDC) == FALSE)
			return;

		UpdateTasklistName(pTDC);
	}

	// Select the tasklist
	const TRACKTASKLIST* pTTL = m_aTasklists.GetTasklist(pTDC);

	if (!pTTL)
	{
		ASSERT(0);
		return;
	}
	
	// Build task combo and select the tracked task if any
	RebuildTaskCombo();
	
	DWORD dwTaskID = (pTTL->dwTrackedTaskID ? pTTL->dwTrackedTaskID : pTDC->GetSelectedTaskID());
	SelectItemByData(m_cbTasks, dwTaskID);
	
	UpdatePlayButton();
	UpdateTaskTime(pTDC);
	RefreshTitleText();
}

void CTDLTimeTrackerDlg::OnSelchangeTask()
{
	UpdatePlayButton();
	UpdateTaskTime(GetSelectedTasklist());
	RefreshTitleText();
}

BOOL CTDLTimeTrackerDlg::OnEraseBkgnd(CDC* pDC)
{
	if (!Misc::IsHighContrastActive())
	{
		CRect rClient;
		GetClientRect(rClient);
		
		pDC->FillSolidRect(rClient, GetBkgndColor());
		
		return TRUE;
	}

	// else
	return CDialog::OnEraseBkgnd(pDC);
}

COLORREF CTDLTimeTrackerDlg::GetBkgndColor() const
{
	return m_theme.crAppBackLight;
}

void CTDLTimeTrackerDlg::OnChangeQuickFind()
{
	UpdateData();

	const CFilteredToDoCtrl* pTDC = GetSelectedTasklist();

	if (pTDC)
	{
		int nSel = m_cbTasks.GetCurSel();
		int nNext = QuickFindNextTaskComboItem(nSel, TRUE);
		
		if ((nNext != CB_ERR) && (nNext != nSel))
		{
			m_cbTasks.SetCurSel(nNext);

			UpdatePlayButton();
			UpdateTaskTime(pTDC);
		}
	}
}

int CTDLTimeTrackerDlg::QuickFindNextTaskComboItem(int nFrom, BOOL bForward) const
{
	int nNumItems = m_cbTasks.GetCount(), nNext = CB_ERR;

	if (bForward)
	{
		if ((nFrom < 0) || (nFrom >= nNumItems))
			nFrom = 0;

		// From nFrom to end of combo
		if (QuickFindNextTaskComboItem(nFrom, (nNumItems - 1), 1, nNext))
			return nNext;

		// From start of combo to nFrom
		if (QuickFindNextTaskComboItem(0, (nFrom - 1), 1, nNext))
			return nNext;
	}
	else // backwards
	{
		if ((nFrom < 0) || (nFrom >= nNumItems))
			nFrom = (nNumItems - 1);

		// From nFrom to start of combo
		if (QuickFindNextTaskComboItem(nFrom, 0, -1, nNext))
			return nNext;

		// From end of combo to nFrom
		if (QuickFindNextTaskComboItem((nNumItems - 1), (nFrom + 1), -1, nNext))
			return nNext;
	}

	// else
	return CB_ERR;
}

BOOL CTDLTimeTrackerDlg::QuickFindNextTaskComboItem(int nFrom, int nTo, int nIncrement, int& nNext) const
{
	ASSERT(((nIncrement == 1) && (nTo >= nFrom)) || ((nIncrement == -1) && (nTo <= nFrom)));

	nNext = CB_ERR;
	nTo += nIncrement; // so the != will work to stop the loop

	for (int nItem = nFrom; nItem != nTo; nItem += nIncrement)
	{
		CString sItem(GetItem(m_cbTasks, nItem));

		if (Misc::Find(m_sQuickFind, sItem, FALSE, FALSE) != -1)
		{
			nNext = nItem;
			break;
		}
	}

	return (nNext != CB_ERR);
}

BOOL CTDLTimeTrackerDlg::OnToolTipNotify(UINT /*id*/, NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
	
    UINT nCtrlID = pNMHDR->idFrom;
	
    if (pTTT->uFlags & TTF_IDISHWND)
		nCtrlID = ::GetDlgCtrlID((HWND)nCtrlID);

	static CString sTooltip;
		
    switch (nCtrlID)
	{
	case IDC_TASKLISTS:
		sTooltip = GetSelectedItem(m_cbTasklists);
		break;
		
	case IDC_TASKS:
		sTooltip = GetSelectedItem(m_cbTasks);
		break;

	default:
		sTooltip.Empty();
		break;
	}
	
	if (!sTooltip.IsEmpty())
	{
		Misc::Trim(sTooltip);
		pTTT->lpszText = (LPTSTR)(LPCTSTR)sTooltip;
	}

	return TRUE;
}

void CTDLTimeTrackerDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	if (bShow)
	{
		if (m_bCentreOnShow)
		{
			CenterWindow(m_pWndNotify);
			m_bCentreOnShow = FALSE;
		}

		UpdatePlayButton(FALSE);
		UpdateTaskTime(GetSelectedTasklist());
	}
}

void CTDLTimeTrackerDlg::OnDestroy()
{
	SaveSettings();

	m_ilBtns.DeleteImageList();
	m_iconResetElapsed.Destroy();
	m_eElapsedTime.DeleteAllButtons();

	CDialog::OnDestroy();
}

void CTDLTimeTrackerDlg::LoadSettings()
{
	if (!m_bRecreating)
	{
		CPreferences prefs;
		
		m_bCollapsed = FALSE; // always
		m_bCentreOnShow = TRUE; // fallback

		int nWidth = prefs.GetProfileInt(_T("TimeTracker"), _T("Width"), 0);
		int nHeight = prefs.GetProfileInt(_T("TimeTracker"), _T("Height"), 0);

		if ((nWidth > 0) && (nHeight > 0))
		{
			CRect rWindow;
			GetWindowRect(rWindow);

			int nPos = prefs.GetProfileInt(_T("TimeTracker"), _T("Position"), -1);

			if (nPos != -1)
			{
				CRect rTemp(rWindow), rScreen;

				rTemp.OffsetRect((GET_X_LPARAM(nPos) - rTemp.left),
								(GET_Y_LPARAM(nPos) - rTemp.top));

				if (GraphicsMisc::GetAvailableScreenSpace(rTemp, rScreen))
				{
					rWindow = rTemp;
					m_bCentreOnShow = FALSE;
				}
			}

			rWindow.right = (rWindow.left + nWidth);
			rWindow.bottom = (rWindow.top + nHeight);

			MoveWindow(rWindow);
			Resize();
		}
	}
}

void CTDLTimeTrackerDlg::SaveSettings() const
{
	// Note: we don't save the collapsed state itself
	// so that the tracker always starts uncollapsed
	CPreferences prefs;
	
	CRect rWindow;
	GetWindowRect(rWindow);

	if (m_bCollapsed)
	{
		prefs.WriteProfileInt(_T("TimeTracker"), _T("Width"), m_sizeLast.cx);
		prefs.WriteProfileInt(_T("TimeTracker"), _T("Height"), m_sizeLast.cy);
	}
	else
	{
		prefs.WriteProfileInt(_T("TimeTracker"), _T("Width"), rWindow.Width());
		prefs.WriteProfileInt(_T("TimeTracker"), _T("Height"), rWindow.Height());
	}

	prefs.WriteProfileInt(_T("TimeTracker"), _T("Position"), MAKELPARAM(rWindow.left, rWindow.top));
	prefs.WriteProfileInt(_T("TimeTracker"), _T("AlwaysOnTop"), m_bAlwaysOnTop);
}

void CTDLTimeTrackerDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	
	Resize(cx, cy);
}

void CTDLTimeTrackerDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	// Limit minimum size
	lpMMI->ptMinTrackSize.x = m_sizeMin.cx;

	if (!m_bCollapsed)
		lpMMI->ptMinTrackSize.y = m_sizeMin.cy;
	
	// Prevent height changing
	lpMMI->ptMaxTrackSize.y = m_sizeMax.cy;
}

void CTDLTimeTrackerDlg::Resize(int cx, int cy)
{
	if (m_btnStart.GetSafeHwnd())
	{
		CRect rClient(0, 0, cx, cy);

		// Parameter-less call
		if ((cx == 0) && (cy == 0))
			GetClientRect(rClient);

		if (rClient.IsRectEmpty())
			return;

		// work out how many rows we can display
		int nRows = CalcAvailableRows(rClient.Height());

		// remove borders
		CDlgUnits dlu(this);
		rClient.DeflateRect(dlu.ToPixelsX(4), dlu.ToPixelsY(4));

		// We _always_ display the button bottom-right
		// together with the task combo and time displays
		CRect rCurBtn = GetChildRect(&m_btnStart), rNewBtn;

		rNewBtn.right = rClient.right;
		rNewBtn.bottom = rClient.bottom;
		rNewBtn.left = (rNewBtn.right - rCurBtn.Width());
		rNewBtn.top = (rNewBtn.bottom - rCurBtn.Height());
		
		m_btnStart.MoveWindow(rNewBtn);

		int nXOffset = (rNewBtn.right - rCurBtn.right);
		int nYOffset = (rNewBtn.bottom - rCurBtn.bottom);

		OffsetCtrl(this, IDC_TASKS, 0, nYOffset);
		ResizeCtrl(this, IDC_TASKS, nXOffset, 0);
		OffsetCtrl(this, IDC_TASKS_LABEL, 0, nYOffset);
		ResizeCtrl(this, IDC_TASKS_LABEL, nXOffset, 0);

		OffsetCtrl(this, IDC_TASKTIME, 0, nYOffset);
		ResizeCtrl(this, IDC_TASKTIME, nXOffset, 0);
		OffsetCtrl(this, IDC_TASKTIME_LABEL, 0, nYOffset);
		ResizeCtrl(this, IDC_TASKTIME_LABEL, nXOffset, 0);

		OffsetCtrl(this, IDC_ELAPSEDTIME, 0, nYOffset);
		ResizeCtrl(this, IDC_ELAPSEDTIME, nXOffset, 0);
		OffsetCtrl(this, IDC_ELAPSEDTIME_LABEL, 0, nYOffset);
		ResizeCtrl(this, IDC_ELAPSEDTIME_LABEL, nXOffset, 0);

		// Then the rest if there is space
		BOOL bShowTasklists = ((nRows == 5) || ((nRows == 4) && (m_aTasklists.GetNumTasklists() > 1)));
		BOOL bShowQuickFind = ((nRows == 5) || ((nRows == 4) && !bShowTasklists));

		ShowCtrl(&m_toolbar, bShowQuickFind);
		ShowCtrl(this, IDC_QUICKFIND, bShowQuickFind);
		ShowCtrl(this, IDC_TASKLISTS, bShowTasklists);
		ShowCtrl(this, IDC_TASKLISTS_LABEL, bShowTasklists);

		OffsetCtrl(this, IDC_TASKLISTS, 0, nYOffset);
		ResizeCtrl(this, IDC_TASKLISTS, nXOffset, 0);
		OffsetCtrl(this, IDC_TASKLISTS_LABEL, 0, nYOffset);
		ResizeCtrl(this, IDC_TASKLISTS_LABEL, nXOffset, 0);
		ResizeCtrl(this, IDC_QUICKFIND, nXOffset, 0);
		
		Invalidate();
		RefreshTitleText();
	}
}

int CTDLTimeTrackerDlg::CalcAvailableRows(int nHeight) const
{
	int nRowHeight = CDlgUnits(this).ToPixelsY(22);

	return (nHeight / nRowHeight);
}

void CTDLTimeTrackerDlg::CalcMinMaxSizes()
{
	if (m_btnStart.GetSafeHwnd())
	{
		CRect rWindow;
		GetWindowRect(rWindow);

		m_sizeMax.cy = rWindow.Height();
		m_sizeMin.cx = rWindow.Width();

		CRect rClient, rLabel = GetCtrlRect(this, IDC_TASKS_LABEL);
		GetClientRect(rClient);

		CDlgUnits dlu(this);
		m_sizeMin.cy = ((rWindow.Height() - rClient.Height()) + (rClient.bottom - rLabel.top) + 4);
	}
}

void CTDLTimeTrackerDlg::OnNcLButtonDblClk(UINT nHitTest, CPoint point)
{
	CDialog::OnNcLButtonDblClk(nHitTest, point);

	if (nHitTest == HTCAPTION)
	{
		m_bCollapsed = !m_bCollapsed;

		CRect rWindow;
		GetWindowRect(rWindow);
		
		if (m_bCollapsed)
		{
			m_sizeLast.cx = rWindow.Width();
			m_sizeLast.cy = rWindow.Height();
			
			CRect rClient;
			GetClientRect(rClient);
			
			rWindow.bottom -= rClient.Height();
			rWindow.right = (rWindow.left + m_sizeMin.cx);
		}
		else
		{
			rWindow.right = (rWindow.left + m_sizeLast.cx);
			rWindow.bottom = (rWindow.top + m_sizeLast.cy);
		}
		
		MoveWindow(rWindow);
	}
}

void CTDLTimeTrackerDlg::RefreshTitleText()
{
	if (m_bCollapsed)
	{
		SetWindowText(m_sTaskTimes);
	}
	else
	{
		CRect rClient;
		GetClientRect(rClient);

		// work out how many rows we can display
		int nRows = CalcAvailableRows(rClient.Height());

		if (nRows < 3)
		{
			CString sTaskTitle;

			if (IsTrackingSelectedTasklistAndTask(sTaskTitle))
			{
				CString sTitle;
				sTitle.Format(_T("%s - %s"), sTaskTitle, m_sOrgTitle);

				SetWindowText(sTitle);
				return;
			}
		}

		// else
		SetWindowText(m_sOrgTitle);
	}
}

void CTDLTimeTrackerDlg::OnToggleTopMost()
{
	SetAlwaysOnTop(!m_bAlwaysOnTop);
}

void CTDLTimeTrackerDlg::SetAlwaysOnTop(BOOL bTopMost)
{
	if (bTopMost == m_bAlwaysOnTop)
		return;

	m_bAlwaysOnTop = bTopMost;
	VERIFY(Recreate());
}

void CTDLTimeTrackerDlg::SetOption(DWORD dwOption, BOOL bEnable)
{
	if (Misc::ModifyFlags(m_dwOptions, (bEnable ? 0 : dwOption), (bEnable ? dwOption : 0)))
	{
		const CFilteredToDoCtrl* pTDC = GetSelectedTasklist();

		if (pTDC)
		{
			if (Misc::HasFlag(dwOption, TTDO_SHOWTASKPATH) ||
				Misc::HasFlag(dwOption, TTDO_ALLOWPARENTTRACKING))
			{
				RebuildTaskCombo();
			}

			if (Misc::HasFlag(dwOption, TTDO_FORMATTIMESASHMS))
			{
				UpdateTaskTime(pTDC);
			}
		}
	}
}

BOOL CTDLTimeTrackerDlg::HasOption(DWORD dwOption) const
{
	return Misc::HasFlag(m_dwOptions, dwOption);
}

void CTDLTimeTrackerDlg::SetWindowIcons(HICON hIconBig, HICON hIconSmall)
{
	ASSERT(::IsWindow(GetSafeHwnd()));

	SetIcon(hIconBig, TRUE);
	SetIcon(hIconSmall, FALSE);
}

LRESULT CTDLTimeTrackerDlg::OnEEBtnClick(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case IDC_ELAPSEDTIME:
		switch (lParam)
		{
		case ID_RESET_ELAPSED:
			{
				const CFilteredToDoCtrl* pTDC = GetSelectedTasklist();

				m_pWndNotify->SendMessage(WM_TDLTTN_RESETELAPSEDTIME, 0, (LPARAM)pTDC);
				UpdateTaskTime(pTDC);
			}
			break;
		}
		break;
	}

	return 0L;
}

