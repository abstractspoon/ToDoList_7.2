// FindReplace.cpp : implementation file
//

#include "stdafx.h"
#include "FindReplace.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

CFindReplaceDialog* IFindReplaceCmdHandler::NewFindReplaceDlg()
{
	return new CFindReplaceDialog;
}

/////////////////////////////////////////////////////////////////////////////

BOOL FindReplace::Initialise(CWnd* pParent, 
							IFindReplaceCmdHandler* pCmdHandler, 
							FIND_STATE* pState, 
							BOOL bFindOnly, 
							BOOL bShowSearchUp,
							LPCTSTR szTitle,
							LPCTSTR szFind)
{
	ASSERT(pParent);
	ASSERT(pCmdHandler);
	ASSERT(pState);

	if (pState->pFindReplaceDlg != NULL)
	{
		if (pState->bFindOnly == bFindOnly)
		{
			pState->pFindReplaceDlg->SetActiveWindow();
			pState->pFindReplaceDlg->ShowWindow(SW_SHOW);

			return TRUE;
		}

		// else
		pState->pFindReplaceDlg->SendMessage(WM_CLOSE); // deletes as well
		ASSERT(pState->pFindReplaceDlg == NULL);
	}

	CString strFind(szFind);

	// if selection is empty or spans multiple lines use old find text
	if (strFind.IsEmpty() || (strFind.FindOneOf(_T("\n\r")) != -1))
		strFind = pState->strFind;

	CString strReplace = pState->strReplace;
	pState->pFindReplaceDlg = pCmdHandler->NewFindReplaceDlg();
	ASSERT(pState->pFindReplaceDlg != NULL);

	DWORD dwFlags = NULL;

	if (pState->bNext)
		dwFlags |= FR_DOWN;

	if (pState->bCase)
		dwFlags |= FR_MATCHCASE;

	if (pState->bWord)
		dwFlags |= FR_WHOLEWORD;

	if (!bShowSearchUp)
		dwFlags |= FR_HIDEUPDOWN;

	if (!pState->pFindReplaceDlg->Create(bFindOnly, strFind, strReplace, dwFlags, pParent))
	{
		pState->pFindReplaceDlg = NULL;
		return FALSE;
	}

	ASSERT(pState->pFindReplaceDlg != NULL);

	// set the title
	if (szTitle && *szTitle)
		pState->pFindReplaceDlg->SetWindowText(szTitle);

	pState->bFindOnly = bFindOnly;
	pState->pFindReplaceDlg->SetActiveWindow();
	pState->pFindReplaceDlg->ShowWindow(SW_SHOW);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

void FindReplace::HandleCmd(IFindReplaceCmdHandler* pCmdHandler, 
							FIND_STATE* pState, 
							WPARAM /*wParam*/, 
							LPARAM lParam)
{
	ASSERT(lParam);
	ASSERT(pCmdHandler);
	ASSERT(lParam);

	CFindReplaceDialog* pDialog = CFindReplaceDialog::GetNotifier(lParam);

	ASSERT(pDialog != NULL);
	ASSERT(pDialog == pState->pFindReplaceDlg);

	if (pDialog->IsTerminating())
	{
		::SetFocus(pDialog->m_fr.hwndOwner);
		pState->pFindReplaceDlg = NULL;
	}
	else if (pDialog->FindNext())
	{
		pCmdHandler->OnFindNext(pDialog->GetFindString(), 
									pDialog->SearchDown(),
									pDialog->MatchCase(), 
									pDialog->MatchWholeWord());
	}
	else if (pDialog->ReplaceCurrent())
	{
		ASSERT(!pState->bFindOnly);

		pCmdHandler->OnReplaceSel(pDialog->GetFindString(),
									pDialog->SearchDown(), 
									pDialog->MatchCase(), 
									pDialog->MatchWholeWord(),
									pDialog->GetReplaceString());
	}
	else if (pDialog->ReplaceAll())
	{
		ASSERT(!pState->bFindOnly);

		pCmdHandler->OnReplaceAll(pDialog->GetFindString(), 
									pDialog->GetReplaceString(),
									pDialog->MatchCase(), 
									pDialog->MatchWholeWord());
	}
}

/////////////////////////////////////////////////////////////////////////////

void FindReplace::AdjustDialogPosition(FIND_STATE* pState, const CPoint& ptScreen)
{
	ASSERT(pState);
	ASSERT(pState->pFindReplaceDlg != NULL);

	CRect rectDlg;
	pState->pFindReplaceDlg->GetWindowRect(&rectDlg);

	if (rectDlg.PtInRect(ptScreen))
	{
		if (ptScreen.y > rectDlg.Height())
		{
			rectDlg.OffsetRect(0, ptScreen.y - rectDlg.bottom - 20);
		}
		else
		{
			int nVertExt = GetSystemMetrics(SM_CYSCREEN);

			if (ptScreen.y + rectDlg.Height() < nVertExt)
				rectDlg.OffsetRect(0, 40 + ptScreen.y - rectDlg.top);
		}

		pState->pFindReplaceDlg->MoveWindow(&rectDlg);
	}
}


/////////////////////////////////////////////////////////////////////////////
