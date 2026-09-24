// AnalyseFrame.cpp : implementation file
//

#include "stdafx.h"
#include "Stello.h"
#include "AnalyseFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAnalyseFrame

IMPLEMENT_DYNCREATE(CAnalyseFrame, CMDIChildWnd)

CAnalyseFrame::CAnalyseFrame()
{
}

CAnalyseFrame::~CAnalyseFrame()
{
}


BEGIN_MESSAGE_MAP(CAnalyseFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CAnalyseFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAnalyseFrame message handlers

BOOL CAnalyseFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.lpszName = "Analyse";
	cs.style &= ~WS_MAXIMIZEBOX;
	cs.style &= ~WS_MINIMIZEBOX;
  cs.style &= ~WS_THICKFRAME;
	cs.style&=~(LONG)FWS_ADDTOTITLE;
	
	return CMDIChildWnd::PreCreateWindow(cs);
}

void CAnalyseFrame::ActivateFrame(int nCmdShow) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	CMDIChildWnd::ActivateFrame(SW_RESTORE);
}
