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

CFindReplaceDialog* IFindReplace::NewFindReplaceDlg()
{
	return new CFindReplaceDialog;
}

/////////////////////////////////////////////////////////////////////////////

void HandleFindReplaceMsg(IFindReplace* pFindReplace, 
							FIND_STATE* pState, 
							WPARAM /*wParam*/, 
							LPARAM lParam)
{
	ASSERT(lParam);
	ASSERT(pFindReplace);
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
		pFindReplace->OnFindNext(pDialog->GetFindString(), 
									pDialog->SearchDown(),
									pDialog->MatchCase(), 
									pDialog->MatchWholeWord());
	}
	else if (pDialog->ReplaceCurrent())
	{
		ASSERT(!pState->bFindOnly);

		pFindReplace->OnReplaceSel(pDialog->GetFindString(),
									pDialog->SearchDown(), 
									pDialog->MatchCase(), 
									pDialog->MatchWholeWord(),
									pDialog->GetReplaceString());
	}
	else if (pDialog->ReplaceAll())
	{
		ASSERT(!pState->bFindOnly);

		pFindReplace->OnReplaceAll(pDialog->GetFindString(), 
									pDialog->GetReplaceString(),
									pDialog->MatchCase(), 
									pDialog->MatchWholeWord());
	}
}

