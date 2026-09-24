// Spiltid.cpp : implementation file
//

#include "stdafx.h"
#include "Stello.h"
#include "StelloDoc.h"
#include "brain\reversi.h"
#include "Spiltid.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CSpiltid * m_pSpilTidViev = NULL;

/////////////////////////////////////////////////////////////////////////////
// CSpiltid

IMPLEMENT_DYNCREATE(CSpiltid, CFormView)

CSpiltid::CSpiltid()
	: CFormView(CSpiltid::IDD)
{
	//{{AFX_DATA_INIT(CSpiltid)
	m_SpilTid = 0;
	//}}AFX_DATA_INIT
}

CSpiltid::~CSpiltid()
{
	m_pSpilTidViev = NULL;
}

void CSpiltid::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSpiltid)
	DDX_Text(pDX, IDC_EDIT1, m_SpilTid);
	DDV_MinMaxInt(pDX, m_SpilTid, 1, 60);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSpiltid, CFormView)
	//{{AFX_MSG_MAP(CSpiltid)
	ON_BN_CLICKED(IDC_SPILTID_OK, OnSpiltidOk)
	ON_BN_CLICKED(IDC_SPILTID_FORTRYD, OnSpiltidFortryd)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpiltid diagnostics

#ifdef _DEBUG
void CSpiltid::AssertValid() const
{
	CFormView::AssertValid();
}

void CSpiltid::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSpiltid message handlers

void CSpiltid::OnInitialUpdate() 
{
	m_SpilTid = GameTid;
	CFormView::OnInitialUpdate();
	
	CStelloDoc* pDoc = (CStelloDoc*)GetDocument ();
	m_pSpilTidViev = this;
	ResizeParentToFit(FALSE); // default argument is TRUE 
	GetParent()->CenterWindow(NULL);
	
}

void CSpiltid::OnSpiltidOk() 
{
	int x;

	if (!UpdateData())
		return;	

	int tid = m_SpilTid*60*1000;
	GameTid = m_SpilTid;

	tid_kontrol = spil_tid;
  /* menu_icheck(M_tree,SOGE,FALSE); 
  menu_icheck(M_tree,TIDPTK,TRUE); */

	timeleft[0] = tid;
  timeleft[1] = tid;
  for (x = 0; x <= playnm; x++)
    timesleft[x] = tid;
  calc = bcalc = b1calc = FALSE;

	GetParentFrame()->PostMessage(WM_CLOSE);
}

void CSpiltid::OnSpiltidFortryd() 
{
	GetParentFrame()->PostMessage(WM_CLOSE);
}
