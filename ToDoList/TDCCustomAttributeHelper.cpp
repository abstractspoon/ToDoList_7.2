// TDCCustomAttributeHelper.cpp: implementation of the CTDCCustomAttributeHelper class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TDCCustomAttributeHelper.h"
#include "TDCstruct.h"
#include "tdliconcombobox.h"

#include "..\shared\DialogHelper.h"
#include "..\shared\checkcombobox.h"
#include "..\shared\autocombobox.h"
#include "..\shared\wclassdefines.h"
#include "..\shared\maskedit.h"
#include "..\shared\orderedtreectrl.h"
#include "..\shared\datetimectrlex.h"
#include "..\shared\misc.h"
#include "..\shared\localizer.h"
#include "..\shared\timecombobox.h"
#include "..\shared\timeedit.h"
#include "..\shared\fileedit.h"
#include "..\shared\FileComboBox.h"
#include "..\shared\HoldRedraw.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CWnd* CTDCCustomAttributeHelper::CreateAttribute(const TDCCUSTOMATTRIBUTEDEFINITION& attribDef, 
													   const CTDCImageList& ilImages, CWnd* pParent, 
													   UINT nCtrlID, BOOL bBuddy, 
													   BOOL bFilter, BOOL bMultiSelectionFilter)
{
	// Sanity check
	if (bBuddy && !AttributeWantsBuddy(attribDef))
	{
		ASSERT(0);
		return NULL;
	}
	
	DWORD dwDataType = attribDef.GetDataType();
	DWORD dwListType = attribDef.GetListType();

	CWnd* pControl = NULL;
	DWORD dwStyle = (WS_CHILD | WS_TABSTOP); // Invisible
	DWORD dwExStyle = WS_EX_CLIENTEDGE;
	LPCTSTR szClass = NULL;

	if (dwListType == TDCCA_NOTALIST)
	{
		switch (dwDataType)
		{
		case TDCCA_STRING:
			pControl = new CEdit;
			szClass = WC_EDIT;
			dwStyle |= (ES_LEFT | ES_AUTOHSCROLL);
			break;

		case TDCCA_DATE:
			if (bBuddy)
			{
				pControl = new CTimeComboBox(TCB_HALFHOURS | TCB_NOTIME | TCB_HOURSINDAY);
				szClass = WC_COMBOBOX;
				dwStyle |= (CBS_DROPDOWN | WS_VSCROLL | CBS_AUTOHSCROLL);
			}
			else
			{
				pControl = new CDateTimeCtrlEx;
				szClass = WC_DATETIMEPICK;
				dwStyle |= (DTS_SHORTDATEFORMAT | DTS_RIGHTALIGN | DTS_SHOWNONE);
			}
			break;

		case TDCCA_INTEGER:
			pControl = new CMaskEdit(_T("0123456789"));
			szClass = WC_EDIT;
			dwStyle |= (ES_LEFT | ES_AUTOHSCROLL);
			break;

		case TDCCA_DOUBLE:
			pControl = new CMaskEdit(_T(".0123456789"), ME_LOCALIZEDECIMAL);
			szClass = WC_EDIT;
			dwStyle |= (ES_LEFT | ES_AUTOHSCROLL);
			break;

		case TDCCA_FILELINK:
			pControl = new CFileEdit(FES_GOBUTTON);
			szClass = WC_EDIT;
			dwStyle |= (ES_LEFT | ES_AUTOHSCROLL);
			break;

		case TDCCA_TIMEPERIOD:
			pControl = new CTimeEdit;
			szClass = WC_EDIT;
			dwStyle |= (ES_LEFT | ES_AUTOHSCROLL);
			break;

			// these don't have controls
		case TDCCA_BOOL:
		case TDCCA_ICON:
			break;

		default:
			// Unknown/unhandled data type
			ASSERT(0);
			return NULL;
		}
	}
	else if (bFilter)
	{
		szClass = WC_COMBOBOX;
		dwStyle |= (CBS_DROPDOWNLIST | WS_VSCROLL | CBS_AUTOHSCROLL);

		switch (dwDataType)
		{
		case TDCCA_ICON:
			pControl = new CTDLIconComboBox(ilImages, bMultiSelectionFilter, TRUE);
			break;

		default:
			pControl = new CEnCheckComboBox(bMultiSelectionFilter, IDS_TDC_NONE, IDS_TDC_ANY);
			break;
		}
	}
	else
	{
		switch (dwListType)
		{
		case TDCCA_AUTOLIST:
			if (dwDataType == TDCCA_FILELINK)
			{
				pControl = new CFileComboBox(FES_GOBUTTON);
				szClass = WC_COMBOBOX;
				dwStyle |= (CBS_DROPDOWN | WS_VSCROLL | CBS_AUTOHSCROLL);
			}
			else
			{
				pControl = new CAutoComboBox(ACBS_ALLOWDELETE);
				szClass = WC_COMBOBOX;
				dwStyle |= (CBS_DROPDOWN | CBS_SORT | WS_VSCROLL | CBS_AUTOHSCROLL);

				// add number mask as required
				switch (dwDataType)
				{
				case TDCCA_INTEGER:
					((CAutoComboBox*)pControl)->SetEditMask(_T("0123456789"));
					break;
				
				case TDCCA_DOUBLE:
					((CAutoComboBox*)pControl)->SetEditMask(_T(".0123456789"), ME_LOCALIZEDECIMAL);
					break;
				}
			}
			break;
		
		case TDCCA_FIXEDLIST:
			{
				szClass = WC_COMBOBOX;
				dwStyle |= (CBS_DROPDOWNLIST | WS_VSCROLL | CBS_AUTOHSCROLL);

				switch (dwDataType)
				{
				case TDCCA_ICON:
					pControl = new CTDLIconComboBox(ilImages, FALSE, FALSE);
					break;
				
				default:
					pControl = new COwnerdrawComboBoxBase; // so they render the same
					break;
				}
			}
			break;
		
		case TDCCA_AUTOMULTILIST:
			{
				pControl = new CCheckComboBox(ACBS_ALLOWDELETE);
				szClass = WC_COMBOBOX;
				dwStyle |= (CBS_DROPDOWN | CBS_SORT | WS_VSCROLL | CBS_AUTOHSCROLL);

				// add number mask as required
				switch (dwDataType)
				{
				case TDCCA_INTEGER:
					((CAutoComboBox*)pControl)->SetEditMask(_T("0123456789"));
					break;
				
				case TDCCA_DOUBLE:
					((CAutoComboBox*)pControl)->SetEditMask(_T(".0123456789"), ME_LOCALIZEDECIMAL);
					break;
				}
			}
			break;
		
		case TDCCA_FIXEDMULTILIST:
			{
				szClass = WC_COMBOBOX;
				dwStyle |= (CBS_DROPDOWNLIST | WS_VSCROLL | CBS_AUTOHSCROLL);
			
				switch (dwDataType)
				{
				case TDCCA_ICON:
					pControl = new CTDLIconComboBox(ilImages, TRUE, FALSE);
					break;
				
				default:
					pControl = new CCheckComboBox;
					break;
				}
			}
			break;
		}
	}

	ASSERT (pControl);

	if (pControl)
	{
		if (!pControl->CreateEx(dwExStyle, 
								szClass, 
								NULL, 
								dwStyle, 
								CRect(0, 0, 0, 0), 
								pParent, 
								nCtrlID))
		{
			delete pControl;
			pControl = NULL;
		}
		else 
		{
			// content is already in user's language
			CLocalizer::EnableTranslation(*pControl, FALSE);

			// font
			pControl->SendMessage(WM_SETFONT, (WPARAM)CDialogHelper::GetFont(pParent)); 

			// add default data to non-filelink lists
			if (attribDef.IsList() && !attribDef.IsDataType(TDCCA_FILELINK)) 
			{
				ASSERT(pControl->IsKindOf(RUNTIME_CLASS(CComboBox)));
				CComboBox* pCB = (CComboBox*)pControl;

				CStringArray aListData;
				attribDef.GetUniqueListData(aListData);
				
				if (pControl->IsKindOf(RUNTIME_CLASS(CEnCheckComboBox)))
				{
					ASSERT(bFilter || (dwDataType == TDCCA_ICON));

					((CEnCheckComboBox*)pCB)->SetStrings(aListData);
				}
				else
				{
					CDialogHelper::SetComboBoxItems(*pCB, aListData);
				}

				if (!bFilter)
				{
					// prepend empty items to single selection lists
					switch (attribDef.GetListType())
					{
					case TDCCA_FIXEDLIST:
					case TDCCA_AUTOLIST:
						pCB->InsertString(0, _T(""));
						break;
					}
				}
			}
		}
	}

	return pControl;
}

BOOL CTDCCustomAttributeHelper::AttributeWantsBuddy(const TDCCUSTOMATTRIBUTEDEFINITION& attribDef)
{
	switch (attribDef.GetDataType())
	{
	case TDCCA_STRING:
	case TDCCA_INTEGER:
	case TDCCA_DOUBLE:
	case TDCCA_BOOL:
	case TDCCA_ICON:
	case TDCCA_FILELINK:
	case TDCCA_TIMEPERIOD:
		return FALSE;
		
	case TDCCA_DATE:
		return attribDef.HasFeature(TDCCAF_SHOWTIME);
	}

	ASSERT(0);
	return FALSE;
}

CString CTDCCustomAttributeHelper::GetControlLabel(const TDCCUSTOMATTRIBUTEDEFINITION& attribDef, BOOL bBuddy)
{
	if (!bBuddy)
	{
		return (attribDef.sLabel.IsEmpty() ? attribDef.sColumnTitle : attribDef.sLabel);
	}
	else if (AttributeWantsBuddy(attribDef))
	{
		switch (attribDef.GetDataType())
		{
		case TDCCA_STRING:
		case TDCCA_INTEGER:
		case TDCCA_DOUBLE:
		case TDCCA_BOOL:
		case TDCCA_ICON:
		case TDCCA_FILELINK:
		case TDCCA_TIMEPERIOD:
			return _T("");
			
		case TDCCA_DATE:
			return CEnString(_T("Time (%s)"), GetControlLabel(attribDef, FALSE));
		}
	}

	ASSERT(0);
	return _T("");
}

CWnd* CTDCCustomAttributeHelper::CreateAttributeLabel(const TDCCUSTOMATTRIBUTEDEFINITION& attribDef, 
															CWnd* pParent, UINT nCtrlID, BOOL bBuddy)
{
	CStatic* pLabel = new CStatic;
	ASSERT (pLabel);

	if (pLabel)
	{
		DWORD dwStyle = (WS_CHILD | SS_CENTERIMAGE); // Invisible

		if (!pLabel->Create(NULL, dwStyle, CRect(0, 0, 0, 0), pParent, nCtrlID))
		{
			delete pLabel;
			pLabel = NULL;
		}
		else // font
		{
			// label is already in user's language
			CLocalizer::EnableTranslation(*pLabel, FALSE);
			
			// THEN set label
			pLabel->SetWindowText(GetControlLabel(attribDef, bBuddy));

			// then font			
			pLabel->SendMessage(WM_SETFONT, (WPARAM)CDialogHelper::GetFont(pParent), 0); 
		}
	}

	return pLabel;
}

BOOL CTDCCustomAttributeHelper::GetControl(UINT nCtrlID, const CTDCCustomControlArray& aControls, 
										   CUSTOMATTRIBCTRLITEM& ctrl)
{
	// sanity check
	if (nCtrlID == 0)
	{
		ASSERT(0);
		return FALSE;
	}

	int nCtrl = aControls.GetSize();

	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrlCustom = aControls.GetData()[nCtrl];

		if ((ctrlCustom.nCtrlID == nCtrlID) ||
			(ctrlCustom.nBuddyCtrlID == nCtrlID))
		{
			ctrl = ctrlCustom;
			return TRUE;
		}
	}

	// not found 
	return FALSE;
}

BOOL CTDCCustomAttributeHelper::GetControl(const CString& sUniqueID, const CTDCCustomControlArray& aControls, 
										   CUSTOMATTRIBCTRLITEM& ctrl)
{
	int nCtrl = aControls.GetSize();

	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrlCustom = aControls.GetData()[nCtrl];

		if (ctrlCustom.sAttribID == sUniqueID)
		{
			ctrl = ctrlCustom;
			return TRUE;
		}
	}

	// not found 
	return FALSE;
}

void CTDCCustomAttributeHelper::CleanupControls(CTDCCustomControlArray& aControls, CWnd* pParent)
{
	aControls.DeleteCtrls(pParent);
	aControls.RemoveAll();
}

BOOL CTDCCustomAttributeHelper::NeedRebuildEditControls(const CTDCCustomAttribDefinitionArray& aOldAttribDefs, 
														const CTDCCustomAttribDefinitionArray& aNewAttribDefs, 
														const CTDCCustomControlArray& aOldControls)
{
	return NeedRebuildControls(aOldAttribDefs, aNewAttribDefs, aOldControls, IDC_FIRST_CUSTOMEDITFIELD, FALSE);
}

BOOL CTDCCustomAttributeHelper::NeedRebuildFilterControls(const CTDCCustomAttribDefinitionArray& aOldAttribDefs, 
														  const CTDCCustomAttribDefinitionArray& aNewAttribDefs, 
															const CTDCCustomControlArray& aOldControls)
{
	return NeedRebuildControls(aOldAttribDefs, aNewAttribDefs, aOldControls, IDC_FIRST_CUSTOMFILTERFIELD, TRUE);
}

BOOL CTDCCustomAttributeHelper::NeedRebuildControls(const CTDCCustomAttribDefinitionArray& aOldAttribDefs, 
													const CTDCCustomAttribDefinitionArray& aNewAttribDefs, 
													const CTDCCustomControlArray& aOldControls, UINT nCtrlIDStart, BOOL bFilter)
{
	CTDCCustomControlArray aNewControls;
	int nNumNewCtrls = GetCustomAttributeCtrls(aNewAttribDefs, aNewControls, nCtrlIDStart, bFilter);

	if (nNumNewCtrls != aOldControls.GetSize())
		return TRUE;

	// Compare each new item with the old, also checking list data
	// Order is important
	for (int nCtrl = 0; nCtrl < nNumNewCtrls; nCtrl++)
	{
		const CUSTOMATTRIBCTRLITEM& ctrlNew = aNewControls[nCtrl];
		const CUSTOMATTRIBCTRLITEM& ctrlOld = aOldControls[nCtrl];

		if (ctrlNew != ctrlOld)
			return TRUE;

		// Definition indices can be different
		int nNewDef = aNewAttribDefs.Find(ctrlNew.sAttribID);
		int nOldDef = aOldAttribDefs.Find(ctrlNew.sAttribID);
		
		ASSERT((nNewDef != -1) && (nOldDef != -1));

		const TDCCUSTOMATTRIBUTEDEFINITION& attribNew = aNewAttribDefs[nNewDef];
		const TDCCUSTOMATTRIBUTEDEFINITION& attribOld = aOldAttribDefs[nOldDef];

		if (!Misc::MatchAll(attribNew.aAutoListData, attribOld.aAutoListData) ||
			!Misc::MatchAll(attribNew.aDefaultListData, attribOld.aDefaultListData))
		{
			return TRUE;
		}
	}

	return FALSE;
}

int CTDCCustomAttributeHelper::GetCustomAttributeCtrls(const CTDCCustomAttribDefinitionArray& aAttribDefs, 
														CTDCCustomControlArray& aControls, UINT nCtrlIDStart, BOOL bFilter)
{
	aControls.RemoveAll();

	UINT nID = nCtrlIDStart;

	for (int nAttrib = 0; nAttrib < aAttribDefs.GetSize(); nAttrib++)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& attribDef = aAttribDefs.GetData()[nAttrib];

		// don't add unwanted controls
		if (!WantControl(attribDef, bFilter))
			continue;

		// NOTE: flag and image types don't need controls because they are 
		// handled by clicking the tasklist directly
		switch (attribDef.GetDataType())
		{
		case TDCCA_BOOL:
			break;

		case TDCCA_ICON:
			if (!attribDef.IsList())
				break;
			// else fall thru

		default:
			{
				CUSTOMATTRIBCTRLITEM ctrl;

				ctrl.nAttrib = attribDef.GetAttributeID();
				ctrl.sAttribID = attribDef.sUniqueID;

				// Main control
				ctrl.nCtrlID = nID++;
				ctrl.nLabelID = nID++;

				// Buddy control
				if (AttributeWantsBuddy(attribDef))
				{
					ctrl.nBuddyCtrlID = nID++;
					ctrl.nBuddyLabelID = nID++;
				}					

				aControls.Add(ctrl);
			}
		}
	}

	return aControls.GetSize();
}

BOOL CTDCCustomAttributeHelper::RebuildEditControls(const CTDCCustomAttribDefinitionArray& aAttribDefs, 
													CTDCCustomControlArray& aControls, 
													const CTDCImageList& ilImages, 
													CWnd* pParent, UINT nCtrlIDPos)
{
	return RebuildControls(aAttribDefs, aControls, ilImages, pParent, nCtrlIDPos, IDC_FIRST_CUSTOMEDITFIELD, FALSE, FALSE);
}

BOOL CTDCCustomAttributeHelper::RebuildFilterControls(const CTDCCustomAttribDefinitionArray& aAttribDefs, 
														CTDCCustomControlArray& aControls, 
														const CTDCImageList& ilImages, 
														CWnd* pParent, UINT nCtrlIDPos, 
														BOOL bMultiSelection)
{
	return RebuildControls(aAttribDefs, aControls, ilImages, pParent, nCtrlIDPos, IDC_FIRST_CUSTOMFILTERFIELD, TRUE, bMultiSelection);
}

BOOL CTDCCustomAttributeHelper::RebuildControls(const CTDCCustomAttribDefinitionArray& aAttribDefs, 
												 CTDCCustomControlArray& aControls, 
												 const CTDCImageList& ilImages,
												 CWnd* pParent, UINT nCtrlIDPos, UINT nCtrlIDStart, 
												 BOOL bFilter, BOOL bMultiSelectionFilter)
{
	ASSERT_VALID(pParent);

	CHoldRedraw hr(*pParent);

	// remove all existing custom attribute fields
	CleanupControls(aControls, pParent);

	// recreate controls and columns
	UINT nID = nCtrlIDStart;

	const CWnd* pInsertAfter = pParent->GetDlgItem(nCtrlIDPos);
	ASSERT(pInsertAfter);

	for (int nAttrib = 0; nAttrib < aAttribDefs.GetSize(); nAttrib++)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& attribDef = aAttribDefs.GetData()[nAttrib];

		// don't add unwanted controls
		if (!WantControl(attribDef, bFilter))
			continue;
		
		// NOTE: flag and image types don't need controls because they are 
		// handled by clicking the tasklist directly
		switch (attribDef.GetDataType())
		{
		case TDCCA_BOOL:
			break;

		case TDCCA_ICON:
			if (!attribDef.IsList())
				break;
			// else fall thru

		default:
			{
				CUSTOMATTRIBCTRLITEM ctrl;
				
				ctrl.nAttrib = attribDef.GetAttributeID();
				ctrl.sAttribID = attribDef.sUniqueID;

				CWnd* pCtrl = NULL;
				CWnd* pLabel = NULL;
				CWnd* pBuddyCtrl = NULL;
				CWnd* pBuddyLabel = NULL;

				// Main control
				ctrl.nCtrlID = nID++;
				ctrl.nLabelID = nID++;
				
				pCtrl = CreateAttribute(attribDef, ilImages, pParent, ctrl.nCtrlID, FALSE, bFilter, bMultiSelectionFilter);

				if (pCtrl)
					pLabel = CreateAttributeLabel(attribDef, pParent, ctrl.nLabelID, FALSE);

				// Buddy control
				BOOL bWantsBuddy = (pCtrl && pLabel && AttributeWantsBuddy(attribDef));

				if (bWantsBuddy)
				{
					ctrl.nBuddyCtrlID = nID++;
					ctrl.nBuddyLabelID = nID++;
					
					pBuddyCtrl = CreateAttribute(attribDef, ilImages, pParent, ctrl.nBuddyCtrlID, TRUE, bFilter, bMultiSelectionFilter);

					if (pBuddyCtrl)
						pBuddyLabel = CreateAttributeLabel(attribDef, pParent, ctrl.nBuddyLabelID, TRUE);
				}					
				
				if (!pCtrl || !pLabel || (bWantsBuddy && (!pBuddyCtrl || !pBuddyLabel)))
				{
					ASSERT(0);
					delete pCtrl;
					delete pLabel;
					delete pBuddyCtrl;
					delete pBuddyLabel;

					// Make resilient to opening newer tasklists 
					// with data types unknown to us
					continue;
				}

				// insert after nCtrlIDPos
				pLabel->SetWindowPos(pInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
				pCtrl->SetWindowPos(pLabel, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
				
				if (bWantsBuddy)
				{
					// insert after nCtrlIDPos
					pBuddyLabel->SetWindowPos(pCtrl, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
					pBuddyCtrl->SetWindowPos(pBuddyLabel, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
					
					pInsertAfter = pBuddyCtrl; // next insertion point
				}
				else
				{
					pInsertAfter = pCtrl; // next insertion point
				}

				aControls.Add(ctrl);
			}
		}
	}

	return TRUE;
}

BOOL CTDCCustomAttributeHelper::WantControl(const TDCCUSTOMATTRIBUTEDEFINITION& attribDef, BOOL bFilter)
{
	if (attribDef.bEnabled)
		return (!bFilter || attribDef.HasFeature(TDCCAF_FILTER));

	return FALSE;
}

CString CTDCCustomAttributeHelper::GetAttributeTypeID(TDC_COLUMN nColID, const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	TDCCUSTOMATTRIBUTEDEFINITION attribDef;

	if (GetAttributeDef(nColID, aAttribDefs, attribDef))
		return attribDef.sUniqueID;

	// all else
	return _T("");
}

TDC_ATTRIBUTE CTDCCustomAttributeHelper::GetAttributeID(TDC_COLUMN nColID, 
											const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	if (!IsCustomColumn(nColID))
		return TDCA_NONE;

	int nDef = aAttribDefs.GetSize();
	
	while (nDef--)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& def = aAttribDefs.GetData()[nDef];

		if (def.GetColumnID() == nColID)
			return def.GetAttributeID();
	}

	// all else
	return TDCA_NONE;
}

TDC_ATTRIBUTE CTDCCustomAttributeHelper::GetAttributeID(const CString& sUniqueID, 
											const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	int nDef = aAttribDefs.GetSize();
	
	while (nDef--)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& def = aAttribDefs.GetData()[nDef];

		if (sUniqueID.CompareNoCase(def.sUniqueID) == 0)
			return def.GetAttributeID();
	}

	// all else
	return TDCA_NONE;
}

CString CTDCCustomAttributeHelper::GetAttributeTypeID(TDC_ATTRIBUTE nAttribID, const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	TDCCUSTOMATTRIBUTEDEFINITION attribDef;

	if (GetAttributeDef(nAttribID, aAttribDefs, attribDef))
		return attribDef.sUniqueID;

	// all else
	return _T("");
}

BOOL CTDCCustomAttributeHelper::GetAttributeDef(TDC_ATTRIBUTE nAttribID, 
												const CTDCCustomAttribDefinitionArray& aAttribDefs,
												TDCCUSTOMATTRIBUTEDEFINITION& attribDef)
{
	if (!IsCustomAttribute(nAttribID))
		return FALSE;

	int nDef = aAttribDefs.GetSize();

	while (nDef--)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& def = aAttribDefs.GetData()[nDef];

		if (def.GetAttributeID() == nAttribID)
		{
			attribDef = def;
			return TRUE;
		}
	}

	// all else
	return FALSE;
}

BOOL CTDCCustomAttributeHelper::GetAttributeDef(const CString& sUniqueID, 
												const CTDCCustomAttribDefinitionArray& aAttribDefs,
												TDCCUSTOMATTRIBUTEDEFINITION& attribDef)
{
	int nAttrib = aAttribDefs.Find(sUniqueID);

	if (nAttrib != -1)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& def = aAttribDefs.GetData()[nAttrib];
		attribDef = def;
		return TRUE;
	}

	// all else
	return FALSE;
}

BOOL CTDCCustomAttributeHelper::GetAttributeDef(const CUSTOMATTRIBCTRLITEM& ctrl,
												const CTDCCustomAttribDefinitionArray& aAttribDefs,
												TDCCUSTOMATTRIBUTEDEFINITION& attribDef)
{
	return GetAttributeDef(ctrl.sAttribID, aAttribDefs, attribDef);
}

BOOL CTDCCustomAttributeHelper::GetAttributeDef(TDC_COLUMN nColID, 
												const CTDCCustomAttribDefinitionArray& aAttribDefs,
												TDCCUSTOMATTRIBUTEDEFINITION& attribDef)
{
	if (!IsCustomColumn(nColID))
		return FALSE;
	
	int nDef = aAttribDefs.GetSize();
	
	while (nDef--)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& def = aAttribDefs.GetData()[nDef];
		
		if (def.GetColumnID() == nColID)
		{
			attribDef = def;
			return TRUE;
		}
	}
	
	// all else
	return FALSE;
}

DWORD CTDCCustomAttributeHelper::GetAttributeDataType(const CString& sUniqueID, 
													const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	int nAttrib = aAttribDefs.Find(sUniqueID);

	if (nAttrib != -1)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& def = aAttribDefs.GetData()[nAttrib];
		return def.GetDataType();
	}

	// all else
	return TDCCA_STRING;
}

BOOL CTDCCustomAttributeHelper::IsColumnSortable(TDC_COLUMN nColID, 
												const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	ASSERT(IsCustomColumn(nColID));

	TDCCUSTOMATTRIBUTEDEFINITION attribDef;

	if (GetAttributeDef(nColID, aAttribDefs, attribDef))
		return attribDef.HasFeature(TDCCAF_SORT);

	// else
	return FALSE;
}

BOOL CTDCCustomAttributeHelper::IsCustomAttribute(TDC_ATTRIBUTE nAttribID)
{
	if ((nAttribID >= TDCA_CUSTOMATTRIB_FIRST) && (nAttribID <= TDCA_CUSTOMATTRIB_LAST))
		return TRUE;

	return (nAttribID == TDCA_CUSTOMATTRIB_ALL);
}

BOOL CTDCCustomAttributeHelper::IsCustomAttributeEnabled(TDC_ATTRIBUTE nAttribID, 
														const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	TDCCUSTOMATTRIBUTEDEFINITION attribDef;

	return (GetAttributeDef(nAttribID, aAttribDefs, attribDef) && attribDef.bEnabled);
}

BOOL CTDCCustomAttributeHelper::IsCustomColumn(TDC_COLUMN nColID)
{
	return (nColID >= TDCC_CUSTOMCOLUMN_FIRST && nColID <= TDCC_CUSTOMCOLUMN_LAST);
}

BOOL CTDCCustomAttributeHelper::IsCustomColumnEnabled(TDC_COLUMN nColID, 
													  const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	TDCCUSTOMATTRIBUTEDEFINITION attribDef;

	return (GetAttributeDef(nColID, aAttribDefs, attribDef) && attribDef.bEnabled);
}

BOOL CTDCCustomAttributeHelper::IsCustomEditControl(UINT nCtrlID)
{
	return (nCtrlID >= IDC_FIRST_CUSTOMEDITFIELD && nCtrlID <= IDC_LAST_CUSTOMEDITFIELD);
}

BOOL CTDCCustomAttributeHelper::IsCustomFilterControl(UINT nCtrlID)
{
	return (nCtrlID >= IDC_FIRST_CUSTOMFILTERFIELD && nCtrlID <= IDC_LAST_CUSTOMFILTERFIELD);
}

CString CTDCCustomAttributeHelper::GetFilterControlTooltip(UINT nCtrlID, CWnd* pParent)
{
	ASSERT(IsCustomFilterControl(nCtrlID));

	return GetControlTooltip(nCtrlID, pParent);
}

CString CTDCCustomAttributeHelper::GetEditControlTooltip(UINT nCtrlID, CWnd* pParent)
{
	ASSERT(IsCustomEditControl(nCtrlID));
	
	return GetControlTooltip(nCtrlID, pParent);
}

CString CTDCCustomAttributeHelper::GetControlTooltip(UINT nCtrlID, CWnd* pParent)
{
	CWnd* pCtrl = pParent->GetDlgItem(nCtrlID);

	if (pCtrl && pCtrl->IsKindOf(RUNTIME_CLASS(CCheckComboBox)))
		return ((CCheckComboBox*)pCtrl)->GetTooltip();

	// else
	return _T("");
}

BOOL CTDCCustomAttributeHelper::FlushEditControl(CWnd* pCtrl, CWnd* pParent,
												 const CTDCCustomControlArray& aControls)
{
	ASSERT(pCtrl && pParent);

	if (aControls.GetSize() == 0)
		return FALSE;

	ASSERT(pCtrl && pParent);

	UINT nCtrlID = pCtrl->GetDlgCtrlID();
	int nCtrl = aControls.GetSize();

	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrl = aControls.GetData()[nCtrl];
		CWnd* pCustomCtrl = pParent->GetDlgItem(ctrl.nCtrlID);
		ASSERT(pCustomCtrl);
		
		if (CDialogHelper::IsChildOrSame(pCustomCtrl, pCtrl))
		{
			if (pCustomCtrl->IsKindOf(RUNTIME_CLASS(CAutoComboBox)))
			{
				((CAutoComboBox*)pCustomCtrl)->Flush();
				return TRUE;
			}

			break;
		}
	}

	return FALSE;
}

void CTDCCustomAttributeHelper::SaveAutoListDataToDefs(const CWnd* pParent, 
														const CTDCCustomControlArray& aControls, 
														const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	int nDef = aAttribDefs.GetSize();
	
	while (nDef--)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& def = aAttribDefs.GetData()[nDef];

		if (def.bEnabled && def.IsAutoList() && !def.IsDataType(TDCCA_FILELINK))
		{
			const CComboBox* pCombo = (const CComboBox*)GetControlFromAttributeDef(pParent, def, aControls);
			ASSERT(pCombo && pCombo->IsKindOf(RUNTIME_CLASS(CComboBox)));

			if (pCombo && pCombo->IsKindOf(RUNTIME_CLASS(CComboBox)))
			{
				CDialogHelper::GetComboBoxItems(*pCombo, def.aAutoListData);

				// remove any default data
				Misc::RemoveItems(def.aDefaultListData, def.aAutoListData);
			}
		}
	}
}

CWnd* CTDCCustomAttributeHelper::GetControlFromAttributeDef(const CWnd* pParent, 
															const TDCCUSTOMATTRIBUTEDEFINITION& attribDef,
															const CTDCCustomControlArray& aControls)
{
	ASSERT_VALID(pParent);

	if (!pParent || !pParent->GetSafeHwnd())
		return FALSE;

	// search for attribute type ID in aControls
	int nCtrl = aControls.GetSize();

	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrl = aControls.GetData()[nCtrl];

		if (ctrl.sAttribID.CompareNoCase(attribDef.sUniqueID) == 0)
			return pParent->GetDlgItem(ctrl.nCtrlID);
	}

	// all else
	return NULL;
}

CString CTDCCustomAttributeHelper::GetAttributeTypeID(UINT nCtrlID, const CTDCCustomControlArray& aControls)
{
	CUSTOMATTRIBCTRLITEM ctrl;

	if (GetControl(nCtrlID, aControls, ctrl))
		return ctrl.sAttribID;

	// not found
	return _T("");
}

void CTDCCustomAttributeHelper::UpdateControls(const CWnd* pParent, CTDCCustomControlArray& aControls,
												const CTDCCustomAttribDefinitionArray& aAttribDefs,
												const CTDCCustomAttributeDataMap& mapData)
{
	int nCtrl = aControls.GetSize();
	TDCCADATA data;

	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrl = aControls.GetData()[nCtrl];
		
		if (mapData.Lookup(ctrl.sAttribID, data))
			UpdateControl(pParent, ctrl, aAttribDefs, data);
		else
			ClearControl(pParent, ctrl, aAttribDefs);
	}
}

void CTDCCustomAttributeHelper::ClearControls(const CWnd* pParent, CTDCCustomControlArray& aControls,
												const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	int nCtrl = aControls.GetSize();

	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrl = aControls.GetData()[nCtrl];
		ClearControl(pParent, ctrl, aAttribDefs);
	}
}

BOOL CTDCCustomAttributeHelper::GetControlData(const CWnd* pParent, const CTDCCustomControlArray& aControls,
												const CTDCCustomAttribDefinitionArray& aAttribDefs,
												CTDCCustomAttributeDataMap& mapData)
{
	mapData.RemoveAll();

	int nCtrl = aControls.GetSize();
	TDCCADATA data;

	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrl = aControls.GetData()[nCtrl];

		if (GetControlData(pParent, ctrl, aAttribDefs, data))
			mapData[ctrl.sAttribID] = data;
	}

	return mapData.GetCount();
}

BOOL CTDCCustomAttributeHelper::GetControlData(const CWnd* pParent, const CUSTOMATTRIBCTRLITEM& ctrl,
												const CTDCCustomAttribDefinitionArray& aAttribDefs,
												TDCCADATA& data)
{
	ASSERT_VALID(pParent);

	DWORD dwDataType = 0, dwListType = 0;
	VERIFY(GetControlAttributeTypes(ctrl, aAttribDefs, dwDataType, dwListType));

	CWnd* pCtrl = pParent->GetDlgItem(ctrl.nCtrlID);
	ASSERT_VALID(pCtrl);

	if (pCtrl == NULL)
		return FALSE;

	CString sText;
	CStringArray aItems;
	COleDateTime date;

	if (dwListType == TDCCA_NOTALIST)
	{
		switch (dwDataType)
		{
		case TDCCA_STRING:
		case TDCCA_INTEGER:
		case TDCCA_DOUBLE:
		case TDCCA_FILELINK:
			pCtrl->GetWindowText(sText);
			data.Set(sText);
			break;

		case TDCCA_DATE:
			((CDateTimeCtrlEx*)pCtrl)->GetTime(date);
			date = CDateHelper::GetDateOnly(date);

			if (CDateHelper::IsDateSet(date) && ctrl.HasBuddy())
			{
				CTimeComboBox* pBuddy = (CTimeComboBox*)pParent->GetDlgItem(ctrl.nBuddyCtrlID);
				ASSERT_VALID(pBuddy);
				
				if (pBuddy == NULL)
					return FALSE;

				double dTime = pBuddy->GetOleTime();

				if (dTime > 0)
					date.m_dt += dTime;
			}
			data.Set(date);
			break;
			
		case TDCCA_TIMEPERIOD:
			{
				TH_UNITS nUnits = ((CTimeEdit*)pCtrl)->GetUnits();
				double dTime = ((CTimeEdit*)pCtrl)->GetTime();

				data.Set(dTime, TDC::MapTHUnitsToUnits(nUnits));
			}
			break;

			// these don't have controls
		case TDCCA_ICON:
		case TDCCA_BOOL:
			ASSERT(0);
			return FALSE;
		}
	}
	else if (IsCustomFilterControl(ctrl.nCtrlID))
	{
		ASSERT(pCtrl->IsKindOf(RUNTIME_CLASS(CEnCheckComboBox)));

		((CEnCheckComboBox*)pCtrl)->GetChecked(aItems);
		data.Set(aItems);
	}
	else 
	{
		switch (dwListType)
		{
		case TDCCA_AUTOLIST:
			if (dwDataType == TDCCA_FILELINK)
			{
				((CFileComboBox*)pCtrl)->GetFileList(aItems);
				data.Set(aItems);
			}
			else
			{
				pCtrl->GetWindowText(sText);
				data.Set(sText);
			}
			break;
		
		case TDCCA_FIXEDLIST:
			if (dwDataType == TDCCA_ICON)
				sText = ((CTDLIconComboBox*)pCtrl)->GetSelectedImage();
			else
				pCtrl->GetWindowText(sText);

			data.Set(sText);
			break;
		
		case TDCCA_AUTOMULTILIST:
		case TDCCA_FIXEDMULTILIST:
			{
				const CCheckComboBox* pCombo = (CCheckComboBox*)pCtrl;
				CStringArray aExtra;

				pCombo->GetChecked(aItems);
				pCombo->GetChecked(aExtra, CCBC_MIXED);

				data.Set(aItems, aExtra);
			}
			break;
		}
	}

	return !data.IsEmpty();
}

CString CTDCCustomAttributeHelper::FormatData(const TDCCADATA& data, const CString& sUniqueID, 
												const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	TDCCUSTOMATTRIBUTEDEFINITION attribDef;

	if (!data.IsEmpty())
		VERIFY(GetAttributeDef(sUniqueID, aAttribDefs, attribDef));

	return FormatData(data, attribDef);
}

CString CTDCCustomAttributeHelper::FormatData(const TDCCADATA& data, const TDCCUSTOMATTRIBUTEDEFINITION& attribDef)
{
	if (attribDef.IsList())
	{
		return data.FormatAsArray('+');
	}
	else if (attribDef.GetDataType() == TDCCA_DATE)
	{
		return data.FormatAsDate();
	}

	// all else
	return data.AsString();
}

void CTDCCustomAttributeHelper::ClearControl(const CWnd* pParent, const CUSTOMATTRIBCTRLITEM& ctrl,
											const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	UpdateControl(pParent, ctrl, aAttribDefs, TDCCADATA());
}

void CTDCCustomAttributeHelper::UpdateControl(const CWnd* pParent, const CUSTOMATTRIBCTRLITEM& ctrl,
											const CTDCCustomAttribDefinitionArray& aAttribDefs,
											const TDCCADATA& data)
{
	ASSERT_VALID(pParent);

	DWORD dwDataType = 0, dwListType = 0;
	VERIFY(GetControlAttributeTypes(ctrl, aAttribDefs, dwDataType, dwListType));

	CWnd* pCtrl = pParent->GetDlgItem(ctrl.nCtrlID);
	ASSERT_VALID(pCtrl);

	if (pCtrl == NULL)
		return;

	CStringArray aItems;

	if (dwListType == TDCCA_NOTALIST)
	{
		switch (dwDataType)
		{
		case TDCCA_STRING:
		case TDCCA_INTEGER:
		case TDCCA_DOUBLE:
		case TDCCA_FILELINK:
			pCtrl->SetWindowText(data.AsString());
			break;

		case TDCCA_DATE:
			{
				CDateTimeCtrlEx* pDTC = (CDateTimeCtrlEx*)pCtrl;
				COleDateTime date = data.AsDate();

				if (!CDateHelper::IsDateSet(date))
				{
					pDTC->SetTime(COleDateTime::GetCurrentTime());
					pDTC->SendMessage(DTM_SETSYSTEMTIME, GDT_NONE, 0);
				}
				else
				{
					pDTC->SetTime(date);
				}
				pDTC->SetMonthCalStyle(MCS_WEEKNUMBERS);


				if (ctrl.HasBuddy() && CDateHelper::DateHasTime(date))
				{
					CTimeComboBox* pBuddy = (CTimeComboBox*)pParent->GetDlgItem(ctrl.nBuddyCtrlID);
					ASSERT_VALID(pBuddy);

					if (pBuddy == NULL)
						return;

					pBuddy->SetOleTime(date);
				}
			}
			break;

		case TDCCA_TIMEPERIOD:
			{
				TDC_UNITS nUnits = TDCU_HOURS;
				double dTime = data.AsTimePeriod(nUnits);

				((CTimeEdit*)pCtrl)->SetTime(dTime, TDC::MapUnitsToTHUnits(nUnits));
			}
			break;

			// these don't have controls
		case TDCCA_ICON:
		case TDCCA_BOOL:
			ASSERT(0);
			break;
		}
	}
	else if (IsCustomFilterControl(ctrl.nCtrlID))
	{
		ASSERT(pCtrl->IsKindOf(RUNTIME_CLASS(CEnCheckComboBox)));

		data.AsArray(aItems);
		((CEnCheckComboBox*)pCtrl)->SetChecked(aItems);
	}
	else
	{
		switch (dwListType)
		{
		case TDCCA_AUTOLIST:
			if (dwDataType == TDCCA_FILELINK)
			{
				data.AsArray(aItems);
				((CFileComboBox*)pCtrl)->SetFileList(aItems);
			}
			else
			{
				CAutoComboBox* pACB = (CAutoComboBox*)pCtrl;

				if (data.IsEmpty())
				{
					pACB->SetCurSel(-1);
				}
				else
				{
					int nItem = pACB->AddString(data.AsString());
					pACB->SetCurSel(nItem);
				}
			}
			break;

		case TDCCA_FIXEDLIST:
			{
				COwnerdrawComboBoxBase* pCB = (COwnerdrawComboBoxBase*)pCtrl;

				if (data.IsEmpty())
				{
					pCB->SetCurSel(-1);
				}
				else if (dwDataType == TDCCA_ICON)
				{
					((CTDLIconComboBox*)pCtrl)->SelectImage(data.AsString());
				}
				else
				{
					int nItem = pCB->FindStringExact(-1, data.AsString(), FALSE);
					pCB->SetCurSel(nItem);
				}
			}
			break;

		case TDCCA_AUTOMULTILIST:
		case TDCCA_FIXEDMULTILIST:
			{
				CStringArray aExtra;
				data.AsArrays(aItems, aExtra);

				((CCheckComboBox*)pCtrl)->SetChecked(aItems, aExtra);
			}
			break;
		}
	}
}

BOOL CTDCCustomAttributeHelper::GetControlAttributeTypes(const CUSTOMATTRIBCTRLITEM& ctrl,
														 const CTDCCustomAttribDefinitionArray& aAttribDefs,
														 DWORD& dwDataType, DWORD& dwListType)
{
	ASSERT(IsCustomEditControl(ctrl.nCtrlID) || IsCustomFilterControl(ctrl.nCtrlID));

	if (!IsCustomEditControl(ctrl.nCtrlID) && !IsCustomFilterControl(ctrl.nCtrlID))
		return FALSE;

	// search attribute defs for unique ID
	int nAttribDef = aAttribDefs.Find(ctrl.sAttribID);

	if (nAttribDef != -1)
	{
		const TDCCUSTOMATTRIBUTEDEFINITION& attribDef = aAttribDefs.GetData()[nAttribDef];
	
		dwDataType = attribDef.GetDataType();
		dwListType = attribDef.GetListType();
		return TRUE;
	}

	// not found
	return FALSE;
}

FIND_ATTRIBTYPE CTDCCustomAttributeHelper::GetAttributeFindType(const CString& sUniqueID, BOOL bRelativeDate,
																const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	TDC_ATTRIBUTE nAttribID = GetAttributeID(sUniqueID, aAttribDefs);

	if (nAttribID == TDCA_NONE)
		return FT_NONE;

	ASSERT(IsCustomAttribute(nAttribID));

	if (!IsCustomAttribute(nAttribID))
		return FT_NONE;

	return GetAttributeFindType(nAttribID, bRelativeDate, aAttribDefs);
}

FIND_ATTRIBTYPE CTDCCustomAttributeHelper::GetAttributeFindType(TDC_ATTRIBUTE nAttribID, BOOL bRelativeDate, 
																const CTDCCustomAttribDefinitionArray& aAttribDefs)
{
	if (!IsCustomAttribute(nAttribID))
		return SEARCHPARAM::GetAttribType(nAttribID, bRelativeDate);

	TDCCUSTOMATTRIBUTEDEFINITION attribDef;
	VERIFY (GetAttributeDef(nAttribID, aAttribDefs, attribDef));

	// treat lists as strings, except for icon lists
	if (attribDef.IsList() && (attribDef.GetDataType() != TDCCA_ICON))
		return FT_STRING;

	// else
	DWORD dwDataType = attribDef.GetDataType();

	switch (dwDataType)
	{
	case TDCCA_STRING:		return FT_STRING;
	case TDCCA_INTEGER:		return FT_INTEGER;
	case TDCCA_DOUBLE:		return FT_DOUBLE;
	case TDCCA_DATE:		return (bRelativeDate ? FT_DATERELATIVE : FT_DATE);
	case TDCCA_BOOL:		return FT_BOOL;
	case TDCCA_ICON:		return FT_ICON;
	case TDCCA_TIMEPERIOD:	return FT_TIMEPERIOD;
	}

	return FT_NONE;
}

int CTDCCustomAttributeHelper::EnableMultiSelectionFilter(const CTDCCustomControlArray& aControls, 
															CWnd* pParent, BOOL bEnable)
{
	int nCtrl = aControls.GetSize(), nNumFound = 0;
	
	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrl = aControls[nCtrl];
		CWnd* pCtrl = pParent->GetDlgItem(ctrl.nCtrlID);

		if (pCtrl->IsKindOf(RUNTIME_CLASS(CEnCheckComboBox)))
		{
			CEnCheckComboBox* pCombo = (CEnCheckComboBox*)pCtrl;

			pCombo->EnableMultiSelection(bEnable);
			nNumFound++;
		}
	}

	return nNumFound;
}

void CTDCCustomAttributeHelper::ClearFilterCheckboxHistory(const CTDCCustomControlArray& aControls, CWnd* pParent)
{
	int nCtrl = aControls.GetSize();
	
	while (nCtrl--)
	{
		const CUSTOMATTRIBCTRLITEM& ctrl = aControls[nCtrl];
		CWnd* pCtrl = pParent->GetDlgItem(ctrl.nCtrlID);

		if (pCtrl->IsKindOf(RUNTIME_CLASS(CEnCheckComboBox)))
		{
			CEnCheckComboBox* pCombo = (CEnCheckComboBox*)pCtrl;

			pCombo->CheckAll(CCBC_UNCHECKED);
		}
	}
}

BOOL CTDCCustomAttributeHelper::AppendFilterRules(const CTDCCustomAttributeDataMap& mapData, 
												const CTDCCustomAttribDefinitionArray& aAttribDefs, 
												CSearchParamArray& aRules)
{
	BOOL bRulesAdded = FALSE;
	POSITION pos = mapData.GetStartPosition();

	while (pos)
	{
		CString sAttribID;
		TDCCADATA data;
		mapData.GetNextAssoc(pos, sAttribID, data);

		TDCCUSTOMATTRIBUTEDEFINITION attribDef;
		VERIFY(CTDCCustomAttributeHelper::GetAttributeDef(sAttribID, aAttribDefs, attribDef));

		if (attribDef.GetListType() == TDCCA_NOTALIST)
		{
			switch (attribDef.GetDataType())
			{
			case TDCCA_STRING:
			case TDCCA_INTEGER:
			case TDCCA_DOUBLE:
			case TDCCA_FILELINK:
			case TDCCA_DATE:
			case TDCCA_TIMEPERIOD:
			case TDCCA_ICON:
			case TDCCA_BOOL:
				// Not yet supported
				ASSERT(0);
				break;
			}
		}
		else // list types
		{
			CStringArray aValues;

			if (data.AsArray(aValues))
			{
				SEARCHPARAM rule;

				rule.SetCustomAttribute(attribDef.GetAttributeID(), sAttribID, FT_STRING);
				rule.SetMatchWholeWord(TRUE); // because lists are read-only

				CString sMatchBy = Misc::FormatArray(aValues);

				// special case: 1 empty value
				if ((aValues.GetSize() == 1) && sMatchBy.IsEmpty())
				{
					rule.SetOperator(FOP_NOT_SET);
				}
				else 
				{
					rule.SetOperator(FOP_INCLUDES);
					rule.SetValue(sMatchBy);
				}

				aRules.Add(rule);
				bRulesAdded = TRUE;
			}
		}
	}

	return bRulesAdded;
}

