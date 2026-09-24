// Analyse.cpp : implementation file
//

#include "stdafx.h"
#include "Stello.h"
#include "brain\Reversi.h"
#include "Analyse.h"
#include "StelloDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CAnalyse * m_pAnalyse = NULL;

/////////////////////////////////////////////////////////////////////////////
// CAnalyse

IMPLEMENT_DYNCREATE(CAnalyse, CFormView)

CAnalyse::CAnalyse()
	: CFormView(CAnalyse::IDD)
{
	//{{AFX_DATA_INIT(CAnalyse)
	m_Knuder = 0;
	m_traek = _T("");
	m_aeval = 0;
	m_Eval = _T("");
	m_Tid = _T("");
	//}}AFX_DATA_INIT

}

CAnalyse::~CAnalyse()
{
	m_pAnalyse = NULL;
}

void CAnalyse::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAnalyse)
	DDX_Text(pDX, IDC_EDIT1, m_Knuder);
	DDX_Text(pDX, IDC_EDIT5, m_traek);
	DDX_Text(pDX, IDC_EDIT4, m_aeval);
	DDX_Text(pDX, IDC_EDIT2, m_Eval);
	DDX_Text(pDX, IDC_EDIT3, m_Tid);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAnalyse, CFormView)
	//{{AFX_MSG_MAP(CAnalyse)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAnalyse diagnostics

#ifdef _DEBUG
void CAnalyse::AssertValid() const
{
	CFormView::AssertValid();
}

void CAnalyse::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CAnalyse message handlers

void CAnalyse::OnInitialUpdate() 
{
	CFormView::OnInitialUpdate();

	CStelloDoc* pDoc = (CStelloDoc*)GetDocument ();
	m_pAnalyse = this;
	ResizeParentToFit(FALSE); // default argument is TRUE 
	GetParent()->CenterWindow(NULL);
}

void CAnalyse::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	
}

void make_res(short int value, long tid)
{	

	if (m_pAnalyse != NULL)
		m_pAnalyse->make_result(value,tid);
	/* char temp[10];
	int x,hasp;
	tree *ntree;
		if (calclib)
		return;

	flushinp();
	
	gotoXY (TAB, 10);
	if (bcalc)
  	printw ("Braedtvaerdi = %d p    ",value);
	else if(b1calc)
		printw ("Braedtvaerdi = %d e    ",value);
	else
	  printw ("Braedtvaerdi = %d      ",value);
	gotoXY (TAB, 11);
  printw ("Knuder = %ld      ",aknud);

	gotoXY (TAB, 12);
  printw ("Evalueringer = %ld       ",aeval);
 
 	gotoXY (TAB, 13);
  printw ("Tid = %ld       ",tid);

	gotoXY (TAB, 15);
	strcpy(nmoves,"Traek = ");
	x = 0;
	ntree = root->barn;
	while (x++ <= 5 && ntree != NULL) {
		text_move(ntree->move, temp);
		strcat(nmoves,temp);
		ntree = ntree->barn;
	}
  printw ("%s              ",nmoves);
	gotoXY (TAB, 16);
	strcpy(nmoves1,"        ");
	x = 0;
	while (x++ <= 5 && ntree != NULL) {
		text_move(ntree->move, temp);
		strcat(nmoves1,temp);
		ntree = ntree->barn;
	}
	printw ("%s              ",nmoves1);

	gotoXY (TAB, 17);
  printw ("Allokeringer = %ld      ",aalloc);

	gotoXY (TAB, 18);
  printw ("Hashcall = %d     ", HashCall);
	gotoXY (TAB, 19);
	if (HashCall != 0)
	  hasp = (HashHit*100)/HashCall;
	else
	  hasp = 0;
  printw ("Hashhit  = %d %%%d    ", HashHit,hasp);
	gotoXY (TAB, 20);
	if (HashCall != 0)
	  hasp = (TransHit*100)/HashCall;
	else
	  hasp = 0;
  printw ("Transhit = %d %%%d    ", TransHit,hasp);

	refresh (); */
}

void CAnalyse::make_result(short int value, long tid) 
{
	if (bcalc)
  	m_Eval.Format("%d p    ",(int)value);
	else if(b1calc)
		m_Eval.Format("%d e    ",(int)value);
	else
	  m_Eval.Format("%d      ",(int)value);
	m_Tid.Format("%d.%d",tid/1000,tid % 1000);
	m_Knuder = aknud;
	m_aeval = aeval;
	UpdateData(FALSE);
	UpdateWindow( );
}

void make_try(short int move,short int look)
{  
	/* char temp[10];
	if (calclib)
		return;

  flushinp();

	text_move(move, temp);
  gotoXY (TAB, 14);
  printw ("Treak = %sDybde = %d, %d/%d   ", temp, look, sofar + 1,posible);
	refresh (); */
	if (m_pAnalyse != NULL)
		m_pAnalyse->make_try(move,look);
}

void text_move(short int move, char *tx)
{	char temp[4];
	
	strcpy(temp,"   ");                          
	if (move == 0) {
		temp[0] = '-';
		temp[1] = '-';
	} 
	else {
		temp[0] = 'A' - 1 + (move % 10);
	//	if (orient == 0)
			temp[1] = '0' + (move / 10);
	//	else
//			temp[1] = '0' + 9 - (move / 10);
	}
	strcpy(tx,temp);
}

void CAnalyse::make_try(short int move,short int look)
{
	char temp[10];
	text_move(move, temp);
	m_traek.Format("%sDybde = %d, %d/%d   ", temp, look, sofar + 1,posible);
	UpdateData(FALSE);
}
