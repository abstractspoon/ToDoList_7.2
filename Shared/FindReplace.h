#if !defined(AFX_FINDREPLACE_H__E7F84BEA_24A6_42D4_BE92_4B8891484048__INCLUDED_)
#define AFX_FINDREPLACE_H__E7F84BEA_24A6_42D4_BE92_4B8891484048__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef FINDTEXTEX
#	ifdef _UNICODE
#		define TEXTRANGE	TEXTRANGEW
#	else
#		define TEXTRANGE	TEXTRANGEA
#	endif
#endif

#ifndef TEXTRANGE
#	ifdef _UNICODE
#		define FINDTEXTEX	FINDTEXTEXW
#	else
#		define FINDTEXTEX	FINDTEXTEXA
#	endif
#endif

struct FIND_STATE
{
	FIND_STATE() 
		: 
		pFindReplaceDlg(NULL), 
		bFindOnly(FALSE), 
		bCase(FALSE), 
		bNext(TRUE), 
		bWord(FALSE) 
	{
	}

	CFindReplaceDialog* pFindReplaceDlg;	// find or replace dialog
	BOOL bFindOnly;							// Is pFindReplace the find or replace?
	CString strFind;						// last find string
	CString strReplace;						// last replace string
	BOOL bCase;								// TRUE==case sensitive, FALSE==not
	int bNext;								// TRUE==search down, FALSE== search up
	BOOL bWord;								// TRUE==match whole word, FALSE==not
};


#endif // !defined(AFX_RICHEDITBASECTRL_H__E7F84BEA_24A6_42D4_BE92_4B8891484048__INCLUDED_)
