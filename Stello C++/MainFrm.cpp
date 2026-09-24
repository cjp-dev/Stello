// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Stello.h"
#include "Spiltid.h"
#include "StelloDoc.h"
#include "brain\reversi.h"
#include "brain\book.h"

#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CStelloApp theApp;
extern CAnalyse * m_pAnalyse;
extern CSpiltid * m_pSpilTidViev;
CMainFrame * m_pMainframe = NULL;
extern short int border_on,lastdyb,lasttid,lastspil,backthink;
extern short int frem,tilbage,anaopen,gamopen,gamfirst,anafirst, posible,calclib;
extern short int hvmul,play,pass,orient,treknu,markoer, tryagain;
extern volatile short int curcl,clon,nytid,clhvid,clsort;
/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_ANALYSE, OnAnalyse)
	ON_UPDATE_COMMAND_UI(ID_ANALYSE, OnUpdateAnalyse)
	ON_COMMAND(ID_NYT_SPIL, OnNytSpil)
	ON_UPDATE_COMMAND_UI(ID_NYT_SPIL, OnUpdateNytSpil)
	ON_COMMAND(ID_SKIFT_SIDE, OnSkiftSide)
	ON_UPDATE_COMMAND_UI(ID_SKIFT_SIDE, OnUpdateSkiftSide)
	ON_COMMAND(ID_SPIL_TID, OnSpilTid)
	ON_UPDATE_COMMAND_UI(ID_SPIL_TID, OnUpdateSpilTid)
	ON_COMMAND(ID_SPIL_FLETSPIL, OnSpilFletspil)
	ON_UPDATE_COMMAND_UI(ID_SPIL_FLETSPIL, OnUpdateSpilFletspil)
	ON_COMMAND(ID_MINMAXLIB, OnMinmaxlib)
	ON_UPDATE_COMMAND_UI(ID_MINMAXLIB, OnUpdateMinmaxlib)
	ON_COMMAND(ID_FREM, OnFrem)
	ON_UPDATE_COMMAND_UI(ID_FREM, OnUpdateFrem)
	ON_COMMAND(ID_TILBAGE, OnTilbage)
	ON_UPDATE_COMMAND_UI(ID_TILBAGE, OnUpdateTilbage)
	ON_COMMAND(ID_SELFPLAY, OnSelfplay)
	ON_UPDATE_COMMAND_UI(ID_SELFPLAY, OnUpdateSelfplay)
	//}}AFX_MSG_MAP
	// Global help commands
	ON_COMMAND(ID_HELP_FINDER, CMDIFrameWnd::OnHelpFinder)
	ON_COMMAND(ID_HELP, CMDIFrameWnd::OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, CMDIFrameWnd::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CMDIFrameWnd::OnHelpFinder)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	
	
}

CMainFrame::~CMainFrame()
{
	m_pMainframe = NULL;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if (!m_wndToolBar.Create(this) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// TODO: Remove this if you don't want tool tips or a resizeable toolbar
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);

	// TODO: Delete these three lines if you don't want the toolbar to
	//  be dockable
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndToolBar);
	m_pMainframe = this;

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CMDIFrameWnd::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers





void CMainFrame::OnAnalyse() 
{
	CStelloDoc * pDoc;
	CMDIChildWnd* pWin = MDIGetActive();

		
	if (pWin != NULL)
		pDoc = (CStelloDoc*)pWin->GetActiveDocument( );
	else
		pDoc = NULL;


  if (pDoc == NULL)
		CStelloDoc * pDoc = (CStelloDoc*)theApp.pAnalyseTemplate->OpenDocumentFile(NULL);
  else {
		if (pDoc == 0)
			return;
		CFrameWnd * pNewFrame = theApp.pAnalyseTemplate->CreateNewFrame(pDoc, NULL);

		if (pNewFrame == NULL)
      return;
		theApp.pAnalyseTemplate->InitialUpdateFrame(pNewFrame, pDoc);
  }
	
}

void CMainFrame::OnUpdateAnalyse(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_pAnalyse == NULL);
}

void CMainFrame::OnNytSpil() 
{
	CStelloDoc * pDoc;
	CMDIChildWnd* pWin = MDIGetActive();

		
	if (pWin != NULL)
		pDoc = (CStelloDoc*)pWin->GetActiveDocument( );
	else
		pDoc = NULL;


  if (pDoc != NULL) {
		pDoc->NytSpil();
		pDoc->UpdateAllViews(NULL);
	}
}

void CMainFrame::OnUpdateNytSpil(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
}

void CMainFrame::OnSkiftSide() 
{
	CStelloDoc * pDoc;
	CMDIChildWnd* pWin = MDIGetActive();

		
	if (pWin != NULL)
		pDoc = (CStelloDoc*)pWin->GetActiveDocument( );
	else
		pDoc = NULL;


  if (pDoc != NULL) {
		if (!gameover) {
			BeginWaitCursor();
			calc = FALSE;
			bcalc = FALSE;
			if (!tilbage) {
		  	tilbage = TRUE;
			}
		  if (frem) {
				frem = FALSE;
			} 
			computer = human;
			human = other(human);
		  get_com();
			EndWaitCursor();
		  
		}
		pDoc->UpdateAllViews(NULL);
	}
}

void CMainFrame::OnUpdateSkiftSide(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!gameover);
}

void CMainFrame::get_com(void)
{
	int trek;
  char str[40],mess[40];
	
  if (makelist(&list, computer, &mainboard)) {
    trek = getcomputer();
    /* putchar(0x07); */
    makemove(trek, computer);
		MessageBeep(0xFFFFFFFF); 
	/*	text_move(trek,str);
		strcpy(mess,"Jeg trak ");
		strcat(mess, str);
		strcat(mess, ". (Ok)");
		alert(mess,'o',0); */
    if ((makelist(&list, computer, &mainboard) +
    		makelist(&list, human, &mainboard)) == 0)
    	gameover = TRUE;
  }
  else { /* computer kan ikke trkke */
		if (makelist(&list, human, &mainboard)) {
		//	alert("Jeg kan ikke trekke. (Ok)",'o',0);
			
			calc = FALSE;
			bcalc = FALSE;
			game.moves[playnm++] = 0;
			game.sidste = playnm;
			game.boards[playnm] = mainboard;
		//	update_move(playnm);
    }
    else
    	gameover = TRUE;
  }
}

void CMainFrame::OnSpilTid() 
{
	CStelloDoc * pDoc;
	CMDIChildWnd* pWin = MDIGetActive();

		
	if (pWin != NULL)
		pDoc = (CStelloDoc*)pWin->GetActiveDocument( );
	else
		pDoc = NULL;


  if (pDoc == NULL)
		CStelloDoc * pDoc = (CStelloDoc*)theApp.pSpilTidTemplate->OpenDocumentFile(NULL);
  else {
		if (pDoc == 0)
			return;
		CFrameWnd * pNewFrame = theApp.pSpilTidTemplate->CreateNewFrame(pDoc, NULL);

		if (pNewFrame == NULL)
      return;
		theApp.pSpilTidTemplate->InitialUpdateFrame(pNewFrame, pDoc);
  }
	
}

void CMainFrame::OnUpdateSpilTid(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_pSpilTidViev == NULL);
	
}

void CMainFrame::OnSpilFletspil() 
{
	short int sum;
	
	if (AfxMessageBox("Spil i database ?",MB_YESNO) == IDYES) {
		convert_game();
		/*	if (playnm > 0) {
				tilbage = TRUE;
			} 
			else {
				tilbage = FALSE;
			}
			frem = FALSE; */
	 // 	curcl = human;
	    sum = makelist(&list, computer, &mainboard) +
						makelist(&list, human, &mainboard);
			if (sum == 0) {
				gameover = TRUE;
  //      clon = FALSE;
      }
      else {
      	gameover = FALSE;
   //     clon = TRUE;
      }

			makelist(&list, human, &mainboard);

		/*if (makelist(&list, human, &mainboard))
			markoer = list.move[0];
		else
			markoer = MARKOER_OFF;
		UpdateDisplay(ALL);*/

		/* if (alert("Hvem vandt ? (s,h)",'s','h') == 0)
			mmgame(1,-32665, &book_root); 
		else
			mmgame(1,32665, &book_root); */
		
		if (AfxMessageBox("Var det sort der vandt ?",MB_YESNO) == IDYES)
			mmgame(1,-32665, &book_root); 
		else
			mmgame(1,32665, &book_root);
		Put_book();
	}

	CStelloDoc * pDoc;
	CMDIChildWnd* pWin = MDIGetActive();

		
	if (pWin != NULL)
		pDoc = (CStelloDoc*)pWin->GetActiveDocument( );
	else
		pDoc = NULL;

	pDoc->UpdateAllViews(NULL);
}

void CMainFrame::OnUpdateSpilFletspil(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
	
}

void CMainFrame::OnMinmaxlib() 
{
	int x;

	if (AfxMessageBox("Minmaxe lib ?",MB_YESNO) == IDYES) {
		tid_kontrol = tid_per_trek;
		GameTid = 5;
		lookahead = 3;
		BeginWaitCursor();
		calc_lib();
		for (x = 0; x < 10; x++)
			minmax_lib();
		sort_lib(&book_root);
		Put_book();
		EndWaitCursor();
	}
	
}

void CMainFrame::OnUpdateMinmaxlib(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
	
}

void CMainFrame::OnFrem() 
{
	CStelloDoc * pDoc;
	CMDIChildWnd* pWin = MDIGetActive();

		
	if (pWin != NULL)
		pDoc = (CStelloDoc*)pWin->GetActiveDocument( );
	else
		pDoc = NULL;

	if (playnm < game.sidste) {
    playnm++;
		timeleft[computer] = timesleft[playnm];
    calc = FALSE;
		bcalc = FALSE;
		delres(&comres[0][0]);
		delres(&humres[0][0]); 
    if (playnm == game.sidste) {
  		frem = FALSE;
    }
    if (!tilbage) {
    	tilbage = TRUE;
  	}
    human = other(playnm % 2);
    computer = other(human);
  //  curcl = human;
    mainboard = game.boards[playnm];
    if (makelist(&list, computer, &mainboard) + 
    	 makelist(&list, human, &mainboard) == 0) {
        gameover = TRUE;
		  	frem = FALSE;
		  	clon = FALSE;
      }
    makelist(&list, human, &mainboard);
		if (pDoc != NULL) {
			pDoc->UpdateAllViews(NULL);
		}
	}
}

void CMainFrame::OnUpdateFrem(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(frem);
}

void CMainFrame::OnTilbage() 
{
	int y;

	CStelloDoc * pDoc;
	CMDIChildWnd* pWin = MDIGetActive();

		
	if (pWin != NULL)
		pDoc = (CStelloDoc*)pWin->GetActiveDocument( );
	else
		pDoc = NULL;
		
	if (playnm > 0) {
    playnm--;
		timeleft[computer] = timesleft[playnm];
   // clon = TRUE;
    calc = FALSE;
		bcalc = FALSE;
	/*	clear_hash(); */
		if (libok) {
      libon = TRUE;
			tryagain = 0;
		}
		delres(&comres[0][0]);
		delres(&humres[0][0]); 
    if (playnm == 0) {
    	tilbage = FALSE;
    }
    if (!frem) {
  		frem = TRUE;
  	}
    if (playnm % 2)
			human = LIGHT;
		else
			human = DARK;
    computer = other(human);
   // curcl = human;
    mainboard = game.boards[playnm];
    makelist(&list, human, &mainboard);
    gameover = FALSE;
		if (pDoc != NULL) {
			pDoc->UpdateAllViews(NULL);
		}
	} 
}

void CMainFrame::OnUpdateTilbage(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(tilbage);
}

void CMainFrame::OnSelfplay() 
{
	calclib = TRUE;
	calc_lib();
	calc_lib();
	selfplay();
	Put_book();
}

void CMainFrame::OnUpdateSelfplay(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
}
