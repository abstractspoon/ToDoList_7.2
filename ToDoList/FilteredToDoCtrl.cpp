// Fi M_BlISlteredToDoCtrl.cpp: implementation of the CFilteredToDoCtrl class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FilteredToDoCtrl.h"
#include "todoitem.h"
#include "resource.h"
#include "tdcstatic.h"
#include "tdcmsg.h"
#include "TDCCustomAttributeHelper.h"
#include "TDCSearchParamHelper.h"
#include "taskclipboard.h"

#include "..\shared\holdredraw.h"
#include "..\shared\datehelper.h"
#include "..\shared\enstring.h"
#include "..\shared\deferwndmove.h"
#include "..\shared\autoflag.h"
#include "..\shared\holdredraw.h"
#include "..\shared\osversion.h"
#include "..\shared\graphicsmisc.h"
#include "..\shared\savefocus.h"
#include "..\shared\filemisc.h"

#include "..\Interfaces\Preferences.h"
#include "..\Interfaces\IUIExtension.h"

#include <math.h>

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

#ifndef LVS_EX_LABELTIP
#define LVS_EX_LABELTIP     0x00004000
#endif

//////////////////////////////////////////////////////////////////////

const UINT SORTWIDTH = 10;

#ifdef _DEBUG
const UINT ONE_MINUTE = 10000;
#else
const UINT ONE_MINUTE = 60000;
#endif

const UINT TEN_MINUTES = (ONE_MINUTE * 10);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFilteredToDoCtrl::CFilteredToDoCtrl(CUIExtensionMgr& mgrUIExt, CTDLContentMgr& mgrContent, 
									 const CONTENTFORMAT& cfDefault, const TDCCOLEDITFILTERVISIBILITY& visDefault) 
	:
	CTabbedToDoCtrl(mgrUIExt, mgrContent, cfDefault, visDefault),
	m_visColEditFilter(visDefault),
	m_bIgnoreListRebuild(FALSE),
	m_bIgnoreExtensionUpdate(FALSE)
{
}

CFilteredToDoCtrl::~CFilteredToDoCtrl()
{

}

BEGIN_MESSAGE_MAP(CFilteredToDoCtrl, CTabbedToDoCtrl)
//{{AFX_MSG_MAP(CFilteredToDoCtrl)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
	ON_REGISTERED_MESSAGE(WM_TDCN_VIEWPRECHANGE, OnPreTabViewChange)
	ON_NOTIFY(TVN_ITEMEXPANDED, IDC_TASKTREELIST, OnTreeExpandItem)
	ON_CBN_EDITCHANGE(IDC_DUETIME, OnEditChangeDueTime)
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////

BOOL CFilteredToDoCtrl::OnInitDialog()
{
	CTabbedToDoCtrl::OnInitDialog();

	return FALSE;
}

BOOL CFilteredToDoCtrl::SelectTask(DWORD dwTaskID, BOOL bTrue)
{	
	if (CTabbedToDoCtrl::SelectTask(dwTaskID, bTrue))
		return TRUE;
	
	// If the task is filtered out we toggle the filter and try again
	if (HasAnyFilter() && HasTask(dwTaskID))
	{
		ToggleFilter(); // show all tasks
		
		if (CTabbedToDoCtrl::SelectTask(dwTaskID, bTrue))
			return TRUE;

		// else
		ASSERT(0);
		ToggleFilter(); // restore filter
	}
	
	return FALSE;
}

BOOL CFilteredToDoCtrl::SelectTask(CString sPart, TDC_SELECTTASK nSelect)
{
	return CTabbedToDoCtrl::SelectTask(sPart, nSelect); 
}

BOOL CFilteredToDoCtrl::LoadTasks(const CTaskFile& tasks)
{
	// handle reloading of tasklist with a filter present
	if (GetTaskCount() && m_filter.HasAnyFilter())
	{
		SaveSettings();
	}

	BOOL bViewWasSet = IsViewSet();

	if (!CTabbedToDoCtrl::LoadTasks(tasks))
		return FALSE;

	FTC_VIEW nView = GetTaskView();

	// save visible state
	BOOL bHidden = !IsWindowVisible();

	// reload last view
	if (!bViewWasSet)
	{
		LoadSettings();

		// always refresh the tree filter because all other
		// views depend on it
		if (IsFilterSet(FTCV_TASKTREE))
			RefreshTreeFilter(); // always

		// handle other views
		switch (nView)
		{
		case FTCV_TASKLIST:
			if (IsFilterSet(nView))
			{
				RefreshListFilter();
			}
			else if (!GetPreferencesKey().IsEmpty()) // first time
			{
				GetViewData2(nView)->bNeedRefilter = TRUE;
			}
			break;

		case FTCV_UIEXTENSION1:
		case FTCV_UIEXTENSION2:
		case FTCV_UIEXTENSION3:
		case FTCV_UIEXTENSION4:
		case FTCV_UIEXTENSION5:
		case FTCV_UIEXTENSION6:
		case FTCV_UIEXTENSION7:
		case FTCV_UIEXTENSION8:
		case FTCV_UIEXTENSION9:
		case FTCV_UIEXTENSION10:
		case FTCV_UIEXTENSION11:
		case FTCV_UIEXTENSION12:
		case FTCV_UIEXTENSION13:
		case FTCV_UIEXTENSION14:
		case FTCV_UIEXTENSION15:
		case FTCV_UIEXTENSION16:
			// Note: By way of virtual functions CTabbedToDoCtrl::LoadTasks
			// will already have initialized the active view if it is an
			// extension so we only need to update if the tree actually
			// has a filter
			if (IsFilterSet(FTCV_TASKTREE))
				RefreshExtensionFilter(nView);
			break;
		}
	}
	else if (IsFilterSet(nView))
	{
		RefreshFilter();
	}

	// restore previously visibility
	if (bHidden)
		ShowWindow(SW_HIDE);

	return TRUE;
}

BOOL CFilteredToDoCtrl::DelayLoad(const CString& sFilePath, COleDateTime& dtEarliestDue)
{
	if (CTabbedToDoCtrl::DelayLoad(sFilePath, dtEarliestDue))
	{
		LoadSettings();
		return TRUE;
	}
	
	// else
	return FALSE;
}

void CFilteredToDoCtrl::SaveSettings() const
{
	CPreferences prefs;
	m_filter.SaveFilter(prefs, GetPreferencesKey(_T("Filter")));
}

void CFilteredToDoCtrl::LoadSettings()
{
	if (HasStyle(TDCS_RESTOREFILTERS))
	{
		CPreferences prefs;
		m_filter.LoadFilter(prefs, GetPreferencesKey(_T("Filter")), m_aCustomAttribDefs);
	}
}

void CFilteredToDoCtrl::OnDestroy() 
{
	SaveSettings();

	CTabbedToDoCtrl::OnDestroy();
}

void CFilteredToDoCtrl::OnEditChangeDueTime()
{
	// need some special hackery to prevent a re-filter in the middle
	// of the user manually typing into the time field
	BOOL bNeedsRefilter = ModNeedsRefilter(TDCA_DUEDATE, FTCV_TASKTREE, GetSelectedTaskID());
	
	if (bNeedsRefilter)
		SetStyle(TDCS_REFILTERONMODIFY, FALSE, FALSE);
	
	CTabbedToDoCtrl::OnSelChangeDueTime();
	
	if (bNeedsRefilter)
		SetStyle(TDCS_REFILTERONMODIFY, TRUE, FALSE);
}

void CFilteredToDoCtrl::OnTreeExpandItem(NMHDR* /*pNMHDR*/, LRESULT* /*pResult*/)
{
	if (m_filter.HasFilterFlag(FO_HIDECOLLAPSED))
	{
		if (InListView())
			RefreshListFilter();
		else
			GetViewData2(FTCV_TASKLIST)->bNeedRefilter = TRUE;
	}
}

LRESULT CFilteredToDoCtrl::OnPreTabViewChange(WPARAM nOldView, LPARAM nNewView) 
{
	if (nNewView != FTCV_TASKTREE)
	{
		VIEWDATA2* pData = GetViewData2((FTC_VIEW)nNewView);
		BOOL bFiltered = FALSE;

		// take a note of what task is currently singly selected
		// so that we can prevent unnecessary calls to UpdateControls
		DWORD dwSelTaskID = GetSingleSelectedTaskID();

		switch (nNewView)
		{
		case FTCV_TASKLIST:
			// update filter as required
			if (pData->bNeedRefilter)
			{
				bFiltered = TRUE;
				RefreshListFilter();
			}
			break;

		case FTCV_UIEXTENSION1:
		case FTCV_UIEXTENSION2:
		case FTCV_UIEXTENSION3:
		case FTCV_UIEXTENSION4:
		case FTCV_UIEXTENSION5:
		case FTCV_UIEXTENSION6:
		case FTCV_UIEXTENSION7:
		case FTCV_UIEXTENSION8:
		case FTCV_UIEXTENSION9:
		case FTCV_UIEXTENSION10:
		case FTCV_UIEXTENSION11:
		case FTCV_UIEXTENSION12:
		case FTCV_UIEXTENSION13:
		case FTCV_UIEXTENSION14:
		case FTCV_UIEXTENSION15:
		case FTCV_UIEXTENSION16:
			// update filter as required
			if (pData && pData->bNeedRefilter)
			{
				// initialise progress depending on whether extension
				// window is already created
				UINT nProgressMsg = 0;

				if (GetExtensionWnd((FTC_VIEW)nNewView) == NULL)
					nProgressMsg = IDS_INITIALISINGTABBEDVIEW;

				BeginExtensionProgress(pData, nProgressMsg);
				RefreshExtensionFilter((FTC_VIEW)nNewView);

				bFiltered = TRUE;
			}
			break;
		}

		if (bFiltered)
			pData->bNeedFullTaskUpdate = FALSE;
		
		// update controls only if the selection has changed and 
		// we didn't refilter (RefreshFilter will already have called UpdateControls)
		BOOL bSelChange = HasSingleSelectionChanged(dwSelTaskID);
		
		if (bSelChange && !bFiltered)
			UpdateControls();
	}

	return CTabbedToDoCtrl::OnPreTabViewChange(nOldView, nNewView);
}


BOOL CFilteredToDoCtrl::CopyCurrentSelection() const
{
	// NOTE: we are overriding this function else
	// filtered out subtasks will not get copied

	// NOTE: We DON'T override GetSelectedTasks because
	// most often that only wants visible tasks

	if (!GetSelectedCount())
		return FALSE;
	
	ClearCopiedItem();
	
	TDCGETTASKS filter(TDCGT_ALL, TDCGTF_FILENAME);
	CTaskFile tasks;

	PrepareTaskfileForTasks(tasks, filter);
	
	// get selected tasks ordered, removing duplicate subtasks
	CHTIList selection;
	TSH().CopySelection(selection, TRUE, TRUE);
	
	// copy items
	POSITION pos = selection.GetHeadPosition();
	
	while (pos)
	{
		HTREEITEM hti = selection.GetNext(pos);
		DWORD dwTaskID = GetTrueTaskID(hti);

		const TODOSTRUCTURE* pTDS = m_data.LocateTask(dwTaskID);
		const TODOITEM* pTDI = GetTask(dwTaskID);

		if (!pTDS || !pTDI)
			return FALSE;

		// add task
		HTASKITEM hTask = tasks.NewTask(pTDI->sTitle, NULL, dwTaskID, 0);
		ASSERT(hTask);
		
		if (!hTask)
			return FALSE;
		
		SetTaskAttributes(pTDI, pTDS, tasks, hTask, TDCGT_ALL, FALSE);

		// and subtasks
		AddSubTasksToTaskFile(pTDS, tasks, hTask, TRUE);
	}
	
	// extra processing to identify the originally selected tasks
	// in case the user wants to paste as references
	pos = TSH().GetFirstItemPos();
	
	while (pos)
	{
		DWORD dwSelID = TSH().GetNextItemData(pos);
		ASSERT(dwSelID);

		if (!m_data.IsTaskReference(dwSelID))
		{
			HTASKITEM hSelTask = tasks.FindTask(dwSelID);
			ASSERT(hSelTask);

			tasks.SetTaskMetaData(hSelTask, _T("selected"), _T("1"));
		}
	}
	
	// and their titles (including child dupes)
	CStringArray aTitles;
	
	VERIFY(TSH().CopySelection(selection, FALSE, TRUE));
	VERIFY(TSH().GetItemTitles(selection, aTitles));
	
	return CTaskClipboard::SetTasks(tasks, GetClipboardID(), Misc::FormatArray(aTitles, '\n'));
}

BOOL CFilteredToDoCtrl::ArchiveDoneTasks(TDC_ARCHIVE nFlags, BOOL bRemoveFlagged)
{
	if (CTabbedToDoCtrl::ArchiveDoneTasks(nFlags, bRemoveFlagged))
	{
		if (InListView())
		{
			if (IsFilterSet(FTCV_TASKLIST))
				RefreshListFilter();
		}
		else if (IsFilterSet(FTCV_TASKTREE))
		{
			RefreshTreeFilter();
		}

		return TRUE;
	}

	// else
	return FALSE;
}

BOOL CFilteredToDoCtrl::ArchiveSelectedTasks(BOOL bRemove)
{
	if (CTabbedToDoCtrl::ArchiveSelectedTasks(bRemove))
	{
		if (InListView())
		{
			if (IsFilterSet(FTCV_TASKLIST))
				RefreshListFilter();
		}
		else if (IsFilterSet(FTCV_TASKTREE))
		{
			RefreshTreeFilter();
		}

		return TRUE;
	}

	// else
	return FALSE;
}

int CFilteredToDoCtrl::GetArchivableTasks(CTaskFile& tasks, BOOL bSelectedOnly) const
{
	if (bSelectedOnly || !IsFilterSet(FTCV_TASKTREE))
		return CTabbedToDoCtrl::GetArchivableTasks(tasks, bSelectedOnly);

	// else process the entire data hierarchy
	GetCompletedTasks(m_data.GetStructure(), tasks, NULL, FALSE);

	return tasks.GetTaskCount();
}

BOOL CFilteredToDoCtrl::RemoveArchivedTask(DWORD dwTaskID)
{
	ASSERT(m_data.HasTask(dwTaskID));
	
	// note: if the tasks does not exist in the tree then this is not a bug
	// if a filter is set
	HTREEITEM hti = m_taskTree.GetItem(dwTaskID);
	ASSERT(hti || IsFilterSet(FTCV_TASKTREE));
	
	if (!hti && !IsFilterSet(FTCV_TASKTREE))
		return FALSE;
	
	if (hti)
		m_taskTree.Tree().DeleteItem(hti);

	return m_data.DeleteTask(dwTaskID, TRUE); // TRUE == with undo
}

void CFilteredToDoCtrl::GetCompletedTasks(const TODOSTRUCTURE* pTDS, CTaskFile& tasks, HTASKITEM hTaskParent, BOOL bSelectedOnly) const
{
	const TODOITEM* pTDI = NULL;

	if (!pTDS->IsRoot())
	{
		DWORD dwTaskID = pTDS->GetTaskID();

		pTDI = m_data.GetTrueTask(dwTaskID);
		ASSERT(pTDI);

		if (!pTDI)
			return;

		// we add the task if it is completed or it has children
		if (pTDI->IsDone() || pTDS->HasSubTasks())
		{
			HTASKITEM hTask = tasks.NewTask(_T(""), hTaskParent, dwTaskID, 0);
			ASSERT(hTask);

			// copy attributes
			TDCGETTASKS allTasks;
			SetTaskAttributes(pTDI, pTDS, tasks, hTask, allTasks, FALSE);

			// this task is now the new parent
			hTaskParent = hTask;
		}
	}

	// children
	if (pTDS->HasSubTasks())
	{
		for (int nSubtask = 0; nSubtask < pTDS->GetSubTaskCount(); nSubtask++)
		{
			const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubtask);
			GetCompletedTasks(pTDSChild, tasks, hTaskParent, bSelectedOnly); // RECURSIVE call
		}

		// if no subtasks were added and the parent is not completed 
		// (and optionally selected) then we remove it
		if (hTaskParent && tasks.GetFirstTask(hTaskParent) == NULL)
		{
			ASSERT(pTDI);

			if (pTDI && !pTDI->IsDone())
				tasks.DeleteTask(hTaskParent);
		}
	}
}

int CFilteredToDoCtrl::GetFilteredTasks(CTaskFile& tasks, const TDCGETTASKS& filter) const
{
	// synonym for GetTasks which always returns the filtered tasks
	return GetTasks(tasks, GetTaskView(), filter);
}

FILTER_SHOW CFilteredToDoCtrl::GetFilter(TDCFILTER& filter) const
{
	return m_filter.GetFilter(filter);
}

void CFilteredToDoCtrl::SetFilter(const TDCFILTER& filter)
{
	FTC_VIEW nView = GetTaskView();

	if (m_bDelayLoaded)
	{
		m_filter.SetFilter(filter);

		// mark everything needing refilter
		GetViewData2(FTCV_TASKTREE)->bNeedRefilter = TRUE;
		SetListNeedRefilter(TRUE);
		SetExtensionsNeedRefilter(TRUE);
	}
	else
	{
		BOOL bTreeNeedsFilter = !FilterMatches(filter, FTCV_TASKTREE);
		BOOL bListNeedRefilter = !FilterMatches(filter, FTCV_TASKLIST); 

		m_filter.SetFilter(filter);

		if (bTreeNeedsFilter)
		{
			// this will mark all other views as needing refiltering
			// and refilter them if they are active
			RefreshFilter();
		}
		else if (bListNeedRefilter)
		{
			if (nView == FTCV_TASKLIST)
				RefreshListFilter();
			else
				SetListNeedRefilter(TRUE);
		}
	}

	ResetNowFilterTimer();
}
	
void CFilteredToDoCtrl::ClearFilter()
{
	if (m_filter.ClearFilter())
		RefreshFilter();

	ResetNowFilterTimer();
}

void CFilteredToDoCtrl::ToggleFilter()
{
	// PERMANENT LOGGING //////////////////////////////////////////////
	CScopedLogTime log(_T("CFilteredToDoCtrl::ToggleFilter(%s)"), (m_filter.HasAnyFilter() ? _T("off") : _T("on")));
	///////////////////////////////////////////////////////////////////

	if (m_filter.ToggleFilter())
		RefreshFilter();

	ResetNowFilterTimer();
}

BOOL CFilteredToDoCtrl::FiltersMatch(const TDCFILTER& filter1, const TDCFILTER& filter2, FTC_VIEW nView) const
{
	if (nView == FTCV_UNSET)
		return FALSE;

	DWORD dwIgnore = 0;

	if (nView == FTCV_TASKTREE)
		dwIgnore = (FO_HIDECOLLAPSED | FO_HIDEPARENTS);

	return TDCFILTER::FiltersMatch(filter1, filter2, dwIgnore);
}

BOOL CFilteredToDoCtrl::FilterMatches(const TDCFILTER& filter, FTC_VIEW nView) const
{
	if (nView == FTCV_UNSET)
		return FALSE;

	DWORD dwIgnore = 0;

	if (nView == FTCV_TASKTREE)
		dwIgnore = (FO_HIDECOLLAPSED | FO_HIDEPARENTS);

	return m_filter.FilterMatches(filter, NULL, 0L, dwIgnore);
}

BOOL CFilteredToDoCtrl::IsFilterSet(FTC_VIEW nView) const
{
	if (nView == FTCV_UNSET)
		return FALSE;

	DWORD dwIgnore = 0;

	if (nView == FTCV_TASKTREE)
		dwIgnore = (FO_HIDECOLLAPSED | FO_HIDEPARENTS);

	return m_filter.HasAnyFilter(dwIgnore);
}

UINT CFilteredToDoCtrl::GetTaskCount(UINT* pVisible) const
{
	if (pVisible)
	{
		if (InListView())
			*pVisible = m_taskList.GetItemCount();
		else
			*pVisible = m_taskTree.GetItemCount();
	}

	return CTabbedToDoCtrl::GetTaskCount();
}

int CFilteredToDoCtrl::FindTasks(const SEARCHPARAMS& params, CResultArray& aResults) const
{
	if (params.bIgnoreFilteredOut)
		return CTabbedToDoCtrl::FindTasks(params, aResults);
	
	// else all tasks
	return m_matcher.FindTasks(params, aResults, HasDueTodayColor());
}

BOOL CFilteredToDoCtrl::HasAdvancedFilter() const 
{ 
	return m_filter.HasAdvancedFilter(); 
}

CString CFilteredToDoCtrl::GetAdvancedFilterName() const 
{ 
	return m_filter.GetAdvancedFilterName();
}

DWORD CFilteredToDoCtrl::GetAdvancedFilterFlags() const 
{ 
	if (HasAdvancedFilter())
		return m_filter.GetFilterFlags();

	// else
	return 0L;
}

BOOL CFilteredToDoCtrl::SetAdvancedFilter(const TDCADVANCEDFILTER& filter)
{
	if (m_filter.SetAdvancedFilter(filter))
	{
		if (m_bDelayLoaded)
		{
			// mark everything needing refilter
			GetViewData2(FTCV_TASKTREE)->bNeedRefilter = TRUE;
			SetListNeedRefilter(TRUE);
			SetExtensionsNeedRefilter(TRUE);
		}
		else
		{
			RefreshFilter();
		}

		return TRUE;
	}

	ASSERT(0);
	return FALSE;
}

BOOL CFilteredToDoCtrl::FilterMatches(const TDCFILTER& filter, LPCTSTR szCustom, DWORD dwCustomFlags, DWORD dwIgnoreFlags) const
{
	return m_filter.FilterMatches(filter, szCustom, dwCustomFlags, dwIgnoreFlags);
}

void CFilteredToDoCtrl::RefreshFilter() 
{
	CSaveFocus sf;

	RefreshTreeFilter(); // always

	FTC_VIEW nView = GetTaskView();

	switch (nView)
	{
	case FTCV_TASKTREE:
		// mark all other views as needing refiltering
		SetListNeedRefilter(TRUE);
		SetExtensionsNeedRefilter(TRUE);
		break;

	case FTCV_TASKLIST:
		// mark extensions as needing refiltering
		RefreshListFilter();
		SetExtensionsNeedRefilter(TRUE);
		break;

	case FTCV_UIEXTENSION1:
	case FTCV_UIEXTENSION2:
	case FTCV_UIEXTENSION3:
	case FTCV_UIEXTENSION4:
	case FTCV_UIEXTENSION5:
	case FTCV_UIEXTENSION6:
	case FTCV_UIEXTENSION7:
	case FTCV_UIEXTENSION8:
	case FTCV_UIEXTENSION9:
	case FTCV_UIEXTENSION10:
	case FTCV_UIEXTENSION11:
	case FTCV_UIEXTENSION12:
	case FTCV_UIEXTENSION13:
	case FTCV_UIEXTENSION14:
	case FTCV_UIEXTENSION15:
	case FTCV_UIEXTENSION16:
		SetExtensionsNeedRefilter(TRUE);
		SetListNeedRefilter(TRUE);
		RefreshExtensionFilter(nView, TRUE);
		SyncExtensionSelectionToTree(nView);
		break;
	}
}

void CFilteredToDoCtrl::SetListNeedRefilter(BOOL bRefilter)
{
	GetViewData2(FTCV_TASKLIST)->bNeedRefilter = bRefilter;
}

void CFilteredToDoCtrl::SetExtensionsNeedRefilter(BOOL bRefilter, FTC_VIEW nIgnore)
{
	for (int nExt = 0; nExt < m_aExtViews.GetSize(); nExt++)
	{
		FTC_VIEW nView = (FTC_VIEW)(FTCV_UIEXTENSION1 + nExt);

		if (nView == nIgnore)
			continue;

		// else
		VIEWDATA2* pData = GetViewData2(nView);

		if (pData)
			pData->bNeedRefilter = bRefilter;
	}
}

void CFilteredToDoCtrl::RefreshTreeFilter() 
{
	if (m_data.GetTaskCount())
	{
		// save and reset current focus to work around a bug
		CSaveFocus sf;
		SetFocusToTasks();
		
		// grab the selected items for the filtering
		m_taskTree.GetSelectedTaskIDs(m_aSelectedTaskIDsForFiltering, FALSE);
		
		// rebuild the tree
		RebuildTree();
		
		// redo last sort
		if (InTreeView() && IsSorting())
		{
			Resort();
			m_bTreeNeedResort = FALSE;
		}
		else
		{
			m_bTreeNeedResort = TRUE;
		}
	}
	
	// modify the tree prompt depending on whether there is a filter set
	if (IsFilterSet(FTCV_TASKTREE))
		m_taskTree.SetWindowPrompt(CEnString(IDS_TDC_FILTEREDTASKLISTPROMPT));
	else
		m_taskTree.SetWindowPrompt(CEnString(IDS_TDC_TASKLISTPROMPT));
}

HTREEITEM CFilteredToDoCtrl::RebuildTree(const void* pContext)
{
	ASSERT(pContext == NULL);

	// build a find query that matches the filter
	if (HasAnyFilter())
	{
		SEARCHPARAMS params;
		m_filter.BuildFilterQuery(params, m_aCustomAttribDefs);

		return CTabbedToDoCtrl::RebuildTree(&params);
	}

	// else
	return CTabbedToDoCtrl::RebuildTree(pContext);
}

BOOL CFilteredToDoCtrl::WantAddTask(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, const void* pContext) const
{
	BOOL bWantTask = CTabbedToDoCtrl::WantAddTask(pTDI, pTDS, pContext);

#ifdef _DEBUG
	DWORD dwTaskID = pTDS->GetTaskID();
	DWORD dwParentID = pTDS->GetParentTaskID();
#endif
	
	if (bWantTask && (pContext != NULL)) // it's a filter
	{
		const SEARCHPARAMS* pFilter = static_cast<const SEARCHPARAMS*>(pContext);
		SEARCHRESULT result;
		
		// special case: selected item
		if (pFilter->HasAttribute(TDCA_SELECTION))
		{
			// check completion
			if (pFilter->bIgnoreDone && m_calculator.IsTaskDone(pTDI, pTDS))
			{
				bWantTask = FALSE;
			}
			else
			{
				bWantTask = Misc::HasT(m_aSelectedTaskIDsForFiltering, pTDS->GetTaskID());

				// check parents
				if (!bWantTask && pFilter->bWantAllSubtasks)
				{
					TODOSTRUCTURE* pTDSParent = pTDS->GetParentTask();

					while (pTDSParent && !pTDSParent->IsRoot() && !bWantTask)
					{
						bWantTask = Misc::HasT(m_aSelectedTaskIDsForFiltering, pTDSParent->GetTaskID());
						pTDSParent = pTDSParent->GetParentTask();
					}
				}
			}
		}
		else // rest of attributes
		{
			bWantTask = m_matcher.TaskMatches(pTDI, pTDS, *pFilter, result, HasDueTodayColor());
		}

		if (bWantTask && pTDS->HasSubTasks())
		{
			// NOTE: the only condition under which this method is called for
			// a parent is if none of its subtasks matched the filter.
			//
			// So if we're a parent and match the filter we need to do an extra check
			// to see if what actually matched was the absence of attributes
			//
			// eg. if the parent category is "" and the filter rule is 
			// TDCA_CATEGORY is (FOP_NOT_SET or FOP_NOT_INCLUDES or FOP_NOT_EQUAL) 
			// then we don't treat this as a match.
			//
			// The attributes to check are:
			//  Category
			//  Status
			//  Alloc To
			//  Alloc By
			//  Version
			//  Priority
			//  Risk
			//  Tags
			
			int nNumRules = pFilter->aRules.GetSize();
			
			for (int nRule = 0; nRule < nNumRules && bWantTask; nRule++)
			{
				const SEARCHPARAM& sp = pFilter->aRules[nRule];

				if (!sp.OperatorIs(FOP_NOT_EQUALS) && 
					!sp.OperatorIs(FOP_NOT_INCLUDES) && 
					!sp.OperatorIs(FOP_NOT_SET))
				{
					continue;
				}
				
				// else check for empty parent attributes
				switch (sp.GetAttribute())
				{
				case TDCA_ALLOCTO:
					bWantTask = (pTDI->aAllocTo.GetSize() > 0);
					break;
					
				case TDCA_ALLOCBY:
					bWantTask = !pTDI->sAllocBy.IsEmpty();
					break;
					
				case TDCA_VERSION:
					bWantTask = !pTDI->sVersion.IsEmpty();
					break;
					
				case TDCA_STATUS:
					bWantTask = !pTDI->sStatus.IsEmpty();
					break;
					
				case TDCA_CATEGORY:
					bWantTask = (pTDI->aCategories.GetSize() > 0);
					break;
					
				case TDCA_TAGS:
					bWantTask = (pTDI->aTags.GetSize() > 0);
					break;
					
				case TDCA_PRIORITY:
					bWantTask = (pTDI->nPriority != FM_NOPRIORITY);
					break;
					
				case TDCA_RISK:
					bWantTask = (pTDI->nRisk != FM_NORISK);
					break;
				}
			}
		}
	}
	
	return bWantTask; 
}

void CFilteredToDoCtrl::RefreshExtensionFilter(FTC_VIEW nView, BOOL bShowProgress)
{
	CWaitCursor cursor;

	IUIExtensionWindow* pExtWnd = GetCreateExtensionWnd(nView);
	ASSERT(pExtWnd);

	if (pExtWnd)
	{
		VIEWDATA2* pData = GetViewData2(nView);
		ASSERT(pData);

		if (bShowProgress)
			BeginExtensionProgress(pData, IDS_UPDATINGTABBEDVIEW);
		
		// clear all update flags
		pData->bNeedFullTaskUpdate = FALSE;
		pData->bNeedRefilter = FALSE;

		// update view with filtered tasks
		CTaskFile tasks;
		GetAllTasksForExtensionViewUpdate(tasks, pData->mapWantedAttrib);

		UpdateExtensionView(pExtWnd, tasks, IUI_ALL, pData->mapWantedAttrib); 
		
		if (bShowProgress)
			EndExtensionProgress();
	}
}

// base class override
void CFilteredToDoCtrl::RebuildList(const void* pContext)
{
	// This should only be called virtually from base-class
	ASSERT(pContext == NULL);
	UNREFERENCED_PARAMETER(pContext);

	if (!m_bIgnoreListRebuild)
		RefreshListFilter();
}

void CFilteredToDoCtrl::RefreshListFilter() 
{
	GetViewData2(FTCV_TASKLIST)->bNeedRefilter = FALSE;

	// build a find query that matches the filter
	SEARCHPARAMS filter;
	m_filter.BuildFilterQuery(filter, m_aCustomAttribDefs);

	// rebuild the list
	RebuildList(filter);

	// modify the list prompt depending on whether there is a filter set
	if (IsFilterSet(FTCV_TASKLIST))
		m_taskList.SetWindowPrompt(CEnString(IDS_TDC_FILTEREDTASKLISTPROMPT));
	else
		m_taskList.SetWindowPrompt(CEnString(IDS_TDC_TASKLISTPROMPT));
}

void CFilteredToDoCtrl::RebuildList(const SEARCHPARAMS& filter)
{
	// since the tree will have already got the items we want 
	// we can optimize the rebuild under certain circumstances
	// which are: 
	// 1. the list is sorted OR
	// 2. the tree is unsorted OR
	// 3. we only want the selected items 
	// otherwise we need the data in it's unsorted state and the 
	// tree doesn't have it
	if (filter.HasAttribute(TDCA_SELECTION) ||
		m_taskList.IsSorting() || 
		!m_taskTree.IsSorting())
	{
		// rebuild the list from the tree
		CTabbedToDoCtrl::RebuildList(&filter);
	}
	else // rebuild from scratch
	{
		// cache current selection
		TDCSELECTIONCACHE cache;
		CacheListSelection(cache);

		// grab the selected items for the filtering
		m_aSelectedTaskIDsForFiltering.Copy(cache.aSelTaskIDs);

		// remove all existing items
		m_taskList.DeleteAll();

		// do the find
		CResultArray aResults;
		m_matcher.FindTasks(filter, aResults, HasDueTodayColor());

		// add tasks to list
		for (int nRes = 0; nRes < aResults.GetSize(); nRes++)
		{
			const SEARCHRESULT& res = aResults[nRes];

			// some more filtering required
			if (HasStyle(TDCS_ALWAYSHIDELISTPARENTS) || m_filter.HasFilterFlag(FO_HIDEPARENTS))
			{
				if (m_data.TaskHasSubtasks(res.dwTaskID))
					continue;
			}
			else if (m_filter.HasFilterFlag(FO_HIDECOLLAPSED))
			{
				HTREEITEM hti = m_taskTree.GetItem(res.dwTaskID);
				ASSERT(hti);			

				if (m_taskTree.ItemHasChildren(hti) && !TCH().IsItemExpanded(hti))
					continue;
			}

			m_taskList.InsertItem(res.dwTaskID);
		}

		m_taskList.RestoreSelection(cache, TRUE);

		Resort();
	}
}

void CFilteredToDoCtrl::AddTreeItemToList(HTREEITEM hti, const void* pContext)
{
	if (pContext == NULL)
	{
		CTabbedToDoCtrl::AddTreeItemToList(hti, NULL);
		return;
	}

	// else it's a filter
	const SEARCHPARAMS* pFilter = static_cast<const SEARCHPARAMS*>(pContext);

	if (hti)
	{
		BOOL bAdd = TRUE;
		DWORD dwTaskID = GetTaskID(hti);

		// check if parent items are to be ignored
		if ((m_filter.HasFilterFlag(FO_HIDEPARENTS) || HasStyle(TDCS_ALWAYSHIDELISTPARENTS)))
		{
			// quick test first
			if (m_taskTree.ItemHasChildren(hti))
			{
				bAdd = FALSE;
			}
			else // item might have children currently filtered out
			{
				bAdd = !m_data.TaskHasSubtasks(dwTaskID);
			}
		}
		// else check if it's a parent item that's only present because
		// it has matching subtasks
		else if (m_taskTree.ItemHasChildren(hti) && !pFilter->HasAttribute(TDCA_SELECTION))
		{
			const TODOSTRUCTURE* pTDS = m_data.LocateTask(dwTaskID);
			const TODOITEM* pTDI = GetTask(dwTaskID); 

			if (pTDI)
			{
				SEARCHRESULT result;

				// ie. check that parent actually matches
				const TODOSTRUCTURE* pTDS = m_data.LocateTask(dwTaskID);

				if (pTDS)
					bAdd = m_matcher.TaskMatches(pTDI, pTDS, *pFilter, result, FALSE);
			}
		}

		if (bAdd)
			m_taskList.InsertItem(dwTaskID);
	}

	// always check the children unless collapsed tasks ignored
	if (!m_filter.HasFilterFlag(FO_HIDECOLLAPSED) || !hti || TCH().IsItemExpanded(hti))
	{
		HTREEITEM htiChild = m_taskTree.GetChildItem(hti);

		while (htiChild)
		{
			// check
			AddTreeItemToList(htiChild, pContext);
			htiChild = m_taskTree.GetNextItem(htiChild);
		}
	}
}

BOOL CFilteredToDoCtrl::SetStyles(const CTDCStylesMap& styles)
{
	if (CTabbedToDoCtrl::SetStyles(styles))
	{
		// do we need to re-filter?
		if (HasAnyFilter() && GetViewData2(FTCV_TASKLIST)->bNeedRefilter)
		{
			RefreshTreeFilter(); // always

			if (InListView())
				RefreshListFilter();
		}

		return TRUE;
	}

	return FALSE;
}

BOOL CFilteredToDoCtrl::SetStyle(TDC_STYLE nStyle, BOOL bOn, BOOL bWantUpdate)
{
	// base class processing
	if (CTabbedToDoCtrl::SetStyle(nStyle, bOn, bWantUpdate))
	{
		// post-precessing
		switch (nStyle)
		{
		case TDCS_DUEHAVEHIGHESTPRIORITY:
		case TDCS_DONEHAVELOWESTPRIORITY:
		case TDCS_ALWAYSHIDELISTPARENTS:
		case TDCS_TREATSUBCOMPLETEDASDONE:
			GetViewData2(FTCV_TASKLIST)->bNeedRefilter = TRUE;
			break;
		}

		return TRUE;
	}

	return FALSE;
}


void CFilteredToDoCtrl::SetDueTaskColors(COLORREF crDue, COLORREF crDueToday)
{
	// See if we need to refilter
	BOOL bHadDueToday = m_taskTree.HasDueTodayColor();

	CTabbedToDoCtrl::SetDueTaskColors(crDue, crDueToday);

	if (bHadDueToday != m_taskTree.HasDueTodayColor())
	{
		// Because the 'Due Today' colour effectively alters 
		// a task's priority we can treat it as a priority edit
		if (m_filter.ModNeedsRefilter(TDCA_PRIORITY, m_aCustomAttribDefs))
			RefreshFilter();
	}
}

BOOL CFilteredToDoCtrl::SplitSelectedTask(int nNumSubtasks)
{
   if (CTabbedToDoCtrl::SplitSelectedTask(nNumSubtasks))
   {
      if (InListView())
         RefreshListFilter();
 
      return TRUE;
   }
 
   return FALSE;
}

BOOL CFilteredToDoCtrl::CreateNewTask(LPCTSTR szText, TDC_INSERTWHERE nWhere, BOOL bEditText, DWORD dwDependency)
{
	if (CTabbedToDoCtrl::CreateNewTask(szText, nWhere, bEditText, dwDependency))
	{
		SetListNeedRefilter(!InListView());
		SetExtensionsNeedRefilter(TRUE, GetTaskView());

		return TRUE;
	}

	// else
	return FALSE;
}

BOOL CFilteredToDoCtrl::CanCreateNewTask(TDC_INSERTWHERE nInsertWhere) const
{
	return CTabbedToDoCtrl::CanCreateNewTask(nInsertWhere);
}

void CFilteredToDoCtrl::SetModified(BOOL bMod, TDC_ATTRIBUTE nAttrib, DWORD dwModTaskID)
{
	BOOL bTreeRefiltered = FALSE, bListRefiltered = FALSE;

	if (bMod)
	{
		if (ModNeedsRefilter(nAttrib, FTCV_TASKTREE, dwModTaskID))
		{
			// This will also refresh the list view if it is active
			RefreshFilter();

			// Note: This will also have refreshed the list filter if active
			bListRefiltered = (GetTaskView() == FTCV_TASKLIST);
			bTreeRefiltered = TRUE;
		}
		else if (ModNeedsRefilter(nAttrib, FTCV_TASKLIST, dwModTaskID))
		{
			// if undoing then we must also refresh the list filter because
			// otherwise ResyncListSelection will fail in the case where
			// we are undoing a delete because the undone item will not yet be in the list.
			if (InListView() || (nAttrib == TDCA_UNDO))
			{
				RefreshListFilter();
			}
			else
			{
				GetViewData2(FTCV_TASKLIST)->bNeedRefilter = TRUE;
			}
		}
	}

	// This may cause the list to be re-filtered again so if it's already done
	// we set a flag and ignore it
	CAutoFlag af(m_bIgnoreListRebuild, bListRefiltered);
	CAutoFlag af2(m_bIgnoreExtensionUpdate, bTreeRefiltered);

	CTabbedToDoCtrl::SetModified(bMod, nAttrib, dwModTaskID);

	SyncActiveViewSelectionToTree();
}

void CFilteredToDoCtrl::EndTimeTracking(BOOL bAllowConfirm, BOOL bNotify)
{
	BOOL bWasTimeTracking = IsActivelyTimeTracking();
	
	CTabbedToDoCtrl::EndTimeTracking(bAllowConfirm, bNotify);
	
	// do we need to refilter?
	if (bWasTimeTracking && m_filter.HasAdvancedFilter() && m_filter.HasAdvancedFilterAttribute(TDCA_TIMESPENT))
	{
		RefreshFilter();
	}
}

BOOL CFilteredToDoCtrl::ModNeedsRefilter(TDC_ATTRIBUTE nModType, FTC_VIEW nView, DWORD dwModTaskID) const 
{
	// sanity checks
	if ((nModType == TDCA_NONE) || !HasStyle(TDCS_REFILTERONMODIFY))
		return FALSE;

	if (!m_filter.HasAnyFilter())
		return FALSE;

	// we only need to refilter if the modified attribute
	// actually affects the filter
	BOOL bNeedRefilter = m_filter.ModNeedsRefilter(nModType, m_aCustomAttribDefs);

	if (!bNeedRefilter)
	{
		// handle attributes common to both filter types
		switch (nModType)
		{
		case TDCA_NEWTASK:
			// if we refilter in the middle of adding a task it messes
			// up the tree items so we handle it in CreateNewTask
			break;
			
		case TDCA_DELETE:
			// this is handled in CTabbedToDoCtrl::SetModified
			break;
			
		case TDCA_UNDO:
		case TDCA_PASTE:
		case TDCA_MERGE:
			// CTabbedToDoCtrl::SetModified() will force a refilter
			// of the list automatically in response to an undo/paste
			// so we don't need to handle it ourselves
			bNeedRefilter = (nView != FTCV_TASKLIST);
			break;
			
		case TDCA_POSITION: // == move
			bNeedRefilter = (nView == FTCV_TASKLIST && !IsSorting());
			break;

		case TDCA_SELECTION:
			// never need to refilter
			break;
		}
	}

	// finally, if this was a task edit we can just test to 
	// see if the modified task still matches the filter.
	if (bNeedRefilter && dwModTaskID)
	{
		// VERY SPECIAL CASE
		// The task being time tracked has been filtered out
		// in which case we don't need to check if it matches
		if (m_timeTracking.IsTrackingTask(dwModTaskID))
		{
			if (m_taskTree.GetItem(dwModTaskID) == NULL)
			{
				ASSERT(HasTask(dwModTaskID));
				ASSERT(nModType == TDCA_TIMESPENT);

				return FALSE;
			}
			// else fall thru
		}

		SEARCHPARAMS params;
		SEARCHRESULT result;

		// This will handle custom and 'normal' filters
		m_filter.BuildFilterQuery(params, m_aCustomAttribDefs);
		bNeedRefilter = !m_matcher.TaskMatches(dwModTaskID, params, result, FALSE);
		
		// extra handling for 'Find Tasks' filters 
		if (bNeedRefilter && HasAdvancedFilter())
		{
			// don't refilter on Time Spent if time tracking
			bNeedRefilter = !(nModType == TDCA_TIMESPENT && IsActivelyTimeTracking());
		}
	}

	return bNeedRefilter;
}

void CFilteredToDoCtrl::Sort(TDC_COLUMN nBy, BOOL bAllowToggle)
{
	CTabbedToDoCtrl::Sort(nBy, bAllowToggle);
}

VIEWDATA2* CFilteredToDoCtrl::GetViewData2(FTC_VIEW nView) const
{
	return (VIEWDATA2*)CTabbedToDoCtrl::GetViewData(nView);
}

VIEWDATA2* CFilteredToDoCtrl::GetActiveViewData2() const
{
	return GetViewData2(GetTaskView());
}

void CFilteredToDoCtrl::OnTimerMidnight()
{
	CTabbedToDoCtrl::OnTimerMidnight();

	// don't re-filter delay-loaded tasklists
	if (IsDelayLoaded())
		return;

	BOOL bRefilter = FALSE;
	TDCFILTER filter;
	
	if (m_filter.GetFilter(filter) == FS_ADVANCED)
	{
		bRefilter = (m_filter.HasAdvancedFilterAttribute(TDCA_STARTDATE) || 
						m_filter.HasAdvancedFilterAttribute(TDCA_DUEDATE));
	}
	else
	{
		bRefilter = (((filter.nStartBy != FD_NONE) && (filter.nStartBy != FD_ANY)) ||
					((filter.nDueBy != FD_NONE) && (filter.nDueBy != FD_ANY)));
	}
	
	if (bRefilter)
		RefreshFilter();
}

void CFilteredToDoCtrl::ResetNowFilterTimer()
{
	if (m_filter.HasNowFilter())
	{
		SetTimer(TIMER_NOWFILTER, ONE_MINUTE, NULL);
		return;
	}

	// all else
	KillTimer(TIMER_NOWFILTER);
}

void CFilteredToDoCtrl::OnTimer(UINT nIDEvent) 
{
	AF_NOREENTRANT;
	
	switch (nIDEvent)
	{
	case TIMER_NOWFILTER:
		OnTimerNow();
		return;
	}

	CTabbedToDoCtrl::OnTimer(nIDEvent);
}

void CFilteredToDoCtrl::OnTimerNow()
{
	// Since this timer gets called every minute we have to
	// find an efficient way of detecting tasks that are
	// currently hidden but need to be shown
	
	// So first thing we do is find reasons not to:
	
	// We are hidden
	if (!IsWindowVisible())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (Window not visible)\n"));
		return;
	}
	
	// We're already displaying all tasks
	if (m_taskTree.GetItemCount() == m_data.GetTaskCount())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (All tasks showing)\n"));
		return;
	}
	
	// App is minimized or hidden
	if (AfxGetMainWnd()->IsIconic() || !AfxGetMainWnd()->IsWindowVisible())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (App not visible)\n"));
		return;
	}
	
	// App is not the foreground window
	if (GetForegroundWindow() != AfxGetMainWnd())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (App not active)\n"));
		return;
	}
	
	// iterate the full data looking for items not in the
	// tree and test them for inclusion in the filter
	ASSERT(m_taskTree.TreeItemMap().GetCount() < m_data.GetTaskCount());
	
	SEARCHPARAMS params;
	m_filter.BuildFilterQuery(params, m_aCustomAttribDefs);
	
	const TODOSTRUCTURE* pTDS = m_data.GetStructure();
	ASSERT(pTDS);
	
	if (FindNewNowFilterTasks(pTDS, params, m_taskTree.TreeItemMap()))
	{
		TDC_ATTRIBUTE nNowAttrib;

		if (m_filter.HasNowFilter(nNowAttrib))
		{
			BOOL bRefilter = FALSE;
		
			switch (nNowAttrib)
			{
			case TDCA_DUEDATE:
				bRefilter = (AfxMessageBox(CEnString(IDS_DUEBYNOW_CONFIRMREFILTER), MB_YESNO | MB_ICONQUESTION) == IDYES);
				break;

			case TDCA_STARTDATE:
				bRefilter = (AfxMessageBox(CEnString(IDS_STARTBYNOW_CONFIRMREFILTER), MB_YESNO | MB_ICONQUESTION) == IDYES);
				break;

			default:
				if (CTDCCustomAttributeHelper::IsCustomAttribute(nNowAttrib))
				{
					// TODO
					//bRefilter = (AfxMessageBox(CEnString(IDS_CUSTOMBYNOW_CONFIRMREFILTER), MB_YESNO | MB_ICONQUESTION) == IDYES);
				}
				else
				{
					ASSERT(0);
				}
			}
		
			if (bRefilter)
			{
				RefreshFilter();
			}
			else // make the timer 10 minutes so we don't re-prompt them too soon
			{
				SetTimer(TIMER_NOWFILTER, TEN_MINUTES, NULL);
			}
		}
	}
}

BOOL CFilteredToDoCtrl::FindNewNowFilterTasks(const TODOSTRUCTURE* pTDS, const SEARCHPARAMS& params, const CHTIMap& htiMap) const
{
	ASSERT(pTDS);

	// process task
	if (!pTDS->IsRoot())
	{
		// is the task invisible?
		HTREEITEM htiDummy;
		DWORD dwTaskID = pTDS->GetTaskID();

		if (!htiMap.Lookup(dwTaskID, htiDummy))
		{
			// does the task match the current filter
			SEARCHRESULT result;

			// This will handle custom and 'normal' filters
			if (m_matcher.TaskMatches(dwTaskID, params, result, FALSE))
				return TRUE;
		}
	}

	// then children
	for (int nTask = 0; nTask < pTDS->GetSubTaskCount(); nTask++)
	{
		if (FindNewNowFilterTasks(pTDS->GetSubTask(nTask), params, htiMap))
			return TRUE;
	}

	// no new tasks
	return FALSE;
}

BOOL CFilteredToDoCtrl::GetAllTasksForExtensionViewUpdate(CTaskFile& tasks, const CTDCAttributeMap& mapAttrib) const
{
	if (m_bIgnoreExtensionUpdate)
	{
		return FALSE;
	}

	// Special case: No filter is set -> All tasks (v much faster)
	if (!IsFilterSet(FTCV_TASKTREE))
	{
		if (CTabbedToDoCtrl::GetAllTasks(tasks))
		{
			AddGlobalsToTaskFile(tasks, mapAttrib);
			return TRUE;
		}

		// else
		return FALSE;
	}

	// else
	return CTabbedToDoCtrl::GetAllTasksForExtensionViewUpdate(tasks, mapAttrib);
}

void CFilteredToDoCtrl::SetColumnFieldVisibility(const TDCCOLEDITFILTERVISIBILITY& vis)
{
	CTabbedToDoCtrl::SetColumnFieldVisibility(vis);

	m_visColEditFilter = vis;
}

void CFilteredToDoCtrl::GetColumnFieldVisibility(TDCCOLEDITFILTERVISIBILITY& vis) const
{
	CTabbedToDoCtrl::GetColumnFieldVisibility(vis);

	if (vis.GetShowFields() == TDLSA_ANY)
		vis.SetVisibleFilterFields(m_visColEditFilter.GetVisibleFilterFields());
}

const CTDCColumnIDMap& CFilteredToDoCtrl::GetVisibleColumns() const
{
	ASSERT(m_visColEditFilter.GetVisibleColumns().MatchAll(m_visColEdit.GetVisibleColumns()));

	return m_visColEditFilter.GetVisibleColumns();
}

const CTDCAttributeMap& CFilteredToDoCtrl::GetVisibleEditFields() const
{
	ASSERT(m_visColEditFilter.GetVisibleEditFields().MatchAll(m_visColEdit.GetVisibleEditFields()));

	return m_visColEditFilter.GetVisibleEditFields();
}

const CTDCAttributeMap& CFilteredToDoCtrl::GetVisibleFilterFields() const
{
	return m_visColEditFilter.GetVisibleFilterFields();
}

void CFilteredToDoCtrl::SaveAttributeVisibility(CTaskFile& tasks) const
{
	if (HasStyle(TDCS_SAVEUIVISINTASKLIST))
		tasks.SetAttributeVisibility(m_visColEditFilter);
}

void CFilteredToDoCtrl::SaveAttributeVisibility(CPreferences& prefs) const
{
	m_visColEditFilter.Save(prefs, GetPreferencesKey());
}

void CFilteredToDoCtrl::LoadAttributeVisibility(const CTaskFile& tasks, const CPreferences& prefs)
{
	// attrib visibility can be stored inside the file or the preferences
	TDCCOLEDITFILTERVISIBILITY vis;

	if (tasks.GetAttributeVisibility(vis))
	{
		// update style to match
		SetStyle(TDCS_SAVEUIVISINTASKLIST);
	}
	else if (!vis.Load(prefs, GetPreferencesKey()))
	{
		vis = m_visColEditFilter;
	}

	SetColumnFieldVisibility(vis);
}

DWORD CFilteredToDoCtrl::MergeNewTaskIntoTree(const CTaskFile& tasks, HTASKITEM hTask, DWORD dwParentTaskID, BOOL bAndSubtasks)
{
	// If the parent has been filtered out we just add 
	// directly to the data model
	if (dwParentTaskID && !m_taskTree.GetItem(dwParentTaskID))
		return MergeNewTaskIntoTree(tasks, hTask, dwParentTaskID, 0, bAndSubtasks);

	// else
	return CTabbedToDoCtrl::MergeNewTaskIntoTree(tasks, hTask, dwParentTaskID, bAndSubtasks);
}

DWORD CFilteredToDoCtrl::MergeNewTaskIntoTree(const CTaskFile& tasks, HTASKITEM hTask, DWORD dwParentTaskID, DWORD dwPrevSiblingID, BOOL bAndSubtasks)
{
	TODOITEM* pTDI = m_data.NewTask(tasks, hTask);

	DWORD dwTaskID = m_dwNextUniqueID++;
	m_data.AddTask(dwTaskID, pTDI, dwParentTaskID, dwPrevSiblingID);

	if (bAndSubtasks)
	{
		HTASKITEM hSubtask = tasks.GetFirstTask(hTask);
		DWORD dwSubtaskID = 0;

		while (hSubtask)
		{
			dwSubtaskID = MergeNewTaskIntoTree(tasks, hSubtask, dwTaskID, dwSubtaskID, TRUE);
			hSubtask = tasks.GetNextTask(hSubtask);
		}
	}

	return dwTaskID;
}

DWORD CFilteredToDoCtrl::RecreateRecurringTaskInTree(const CTaskFile& task, const COleDateTime& dtNext, BOOL bDueDate)
{
	// We only need handle this if the existing task has been filtered out
	DWORD dwTaskID = task.GetTaskID(task.GetFirstTask());

	if (HasAnyFilter() && (m_taskTree.GetItem(dwTaskID) == NULL))
	{
		// Merge task into data structure after the existing task
		DWORD dwParentID = m_data.GetTaskParentID(dwTaskID);
		DWORD dwNewTaskID = MergeNewTaskIntoTree(task, task.GetFirstTask(), dwParentID, dwTaskID, TRUE);

		InitialiseNewRecurringTask(dwTaskID, dwNewTaskID, dtNext, bDueDate);
		RefreshFilter();
		
		ASSERT(m_taskTree.GetItem(dwNewTaskID) != NULL);
		return dwNewTaskID;
	}

	// all else
	return CTabbedToDoCtrl::RecreateRecurringTaskInTree(task, dtNext, bDueDate);
}

