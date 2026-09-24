// SpilTidFrame.cpp : implementation file
//

#include "stdafx.h"
#include "Stello.h"
#include "SpilTidFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpilTidFrame

IMPLEMENT_DYNCREATE(CSpilTidFrame, CMDIChildWnd)

CSpilTidFrame::CSpilTidFrame()
{
}

CSpilTidFrame::~CSpilTidFrame()
{
}


BEGIN_MESSAGE_MAP(CSpilTidFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CSpilTidFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpilTidFrame message handlers

BOOL CSpilTidFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.lpszName = "Analyse";
	cs.style &= ~WS_MAXIMIZEBOX;
	cs.style &= ~WS_MINIMIZEBOX;
  cs.style &= ~WS_THICKFRAME;
	cs.style&=~(LONG)FWS_ADDTOTITLE;
	
	return CMDIChildWnd::PreCreateWindow(cs);
}

void CSpilTidFrame::ActivateFrame(int nCmdShow) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	CMDIChildWnd::ActivateFrame(SW_RESTORE);
}
