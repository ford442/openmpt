/*
 * view_com.h
 * ----------
 * Purpose: Song comments tab, lower panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "CListCtrl.h"
#include "Globals.h"
#include "../soundlib/modcommand.h"

OPENMPT_NAMESPACE_BEGIN

class CViewComments: public CModScrollView
{
public:
	CViewComments() = default;
	DECLARE_SERIAL(CViewComments)

protected:
	CModControlBar m_ToolBar;
	CListCtrlEx m_ItemList;
	CFont m_fixedFont;
	HFONT m_oldFont = nullptr;
	int m_nCurrentListId = 0, m_nListId = 0;
	ModCommand::NOTE m_lastNote = NOTE_NONE;
	CHANNELINDEX m_noteChannel = CHANNELINDEX_INVALID;
	INSTRUMENTINDEX m_noteInstr = INSTRUMENTINDEX_INVALID;
	bool m_editLabel = false;

public:
	void RecalcLayout();
	void UpdateButtonState();

public:
	//{{AFX_VIRTUAL(CViewComments)
	void OnInitialUpdate() override;
	void OnDPIChanged() override;
	BOOL PreTranslateMessage(MSG *pMsg) override;
	LRESULT OnModViewMsg(WPARAM wParam, LPARAM lParam) override;
	void UpdateView(UpdateHint hint, CObject *pObject = nullptr) override;
	//}}AFX_VIRTUAL

protected:
	bool SwitchToList(int list);

	//{{AFX_MSG(CViewGlobals)
	// cppcheck-suppress duplInheritedMember
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnShowSamples();
	afx_msg void OnShowInstruments();
	afx_msg void OnShowPatterns();
	afx_msg void OnEndLabelEdit(LPNMHDR pnmhdr, LRESULT *pLResult);
	afx_msg void OnBeginLabelEdit(LPNMHDR pnmhdr, LRESULT *pLResult);
	afx_msg void OnDblClickListItem(NMHDR *, LRESULT *);
	afx_msg void OnRClickListItem(NMHDR *, LRESULT *);
	afx_msg void OnCustomDrawList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnCopyNames();
	afx_msg void OnPasteNames();
	afx_msg LRESULT OnMidiMsg(WPARAM midiData, LPARAM);
	afx_msg LRESULT OnCustomKeyMsg(WPARAM, LPARAM);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
