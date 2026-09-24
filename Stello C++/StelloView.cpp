// StelloView.cpp : implementation of the CStelloView class
//

#include "stdafx.h"
#include "Stello.h"
#include "brain\reversi.h"
#include "MainFrm.h"
#include "StelloDoc.h"
#include "StelloView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CStelloApp theApp;
extern CMainFrame * m_pMainframe;
extern short int frem,tilbage;
CStelloView * m_pStelloView = NULL;
/////////////////////////////////////////////////////////////////////////////
// CStelloView

IMPLEMENT_DYNCREATE(CStelloView, CView)

BEGIN_MESSAGE_MAP(CStelloView, CView)
	//{{AFX_MSG_MAP(CStelloView)
	ON_WM_LBUTTONDOWN()
	//}}AFX_MSG_MAP
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CView::OnFilePrintPreview)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStelloView construction/destruction

CStelloView::CStelloView()
{
	for (int i=0; i<8; i++) {
		for (int j=0; j<8; j++) {
			int x = (i * 60);
			int y = -(j * 60);
			m_rect[i][j].SetRect (x, y, x + 60, y - 60);
			m_rect[i][j].NormalizeRect ();
		}
	}

}

CStelloView::~CStelloView()
{
	m_pStelloView = NULL;
}

BOOL CStelloView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CStelloView drawing

void CStelloView::OnDraw(CDC* pDC)
{
	CStelloDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	// TODO: add draw code for native data here
	//
	// Set the mapping mode to MM_LOENGLISH, where 1 unit = 0.01 inch.
	//
	pDC->SetMapMode (MM_LOENGLISH);

	CBrush WhiteBrush(RGB (255, 255, 255));
	CBrush Blackbrush(RGB (0, 0, 0));
	CBrush* pOldbrush = pDC->SelectObject(&WhiteBrush);
	CPen pen (PS_SOLID, 1, RGB (0, 0, 0));
	CPen* pOldPen = pDC->SelectObject (&pen);

	//
	// Draw the playing grid.
	//
	for (int i=1; i<=8; i++) {
		for (int j=1; j<=8; j++) {
			pDC->SelectObject(&WhiteBrush);
			pDC->Rectangle (m_rect[i-1][j-1]);
			BYTE bVal = mainboard.sq[10*j + i];
			CRect rect;
			rect.CopyRect (m_rect[i-1][j-1]);
			rect.DeflateRect (3, 3);
			if (bVal == 0) {
				pDC->SelectObject(&WhiteBrush);
				pDC->Ellipse (rect);
			}
			else if (bVal == 1) {
				pDC->SelectObject(&Blackbrush);
				pDC->Ellipse (rect);
			}
		}
	} 

	pDC->SelectObject (pOldbrush);
	pDC->SelectObject (pOldPen);
}

/////////////////////////////////////////////////////////////////////////////
// CStelloView printing

BOOL CStelloView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// default preparation
	return DoPreparePrinting(pInfo);
}

void CStelloView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add extra initialization before printing
}

void CStelloView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add cleanup after printing
}

/////////////////////////////////////////////////////////////////////////////
// CStelloView diagnostics

#ifdef _DEBUG
void CStelloView::AssertValid() const
{
	CView::AssertValid();
}

void CStelloView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CStelloDoc* CStelloView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CStelloDoc)));
	return (CStelloDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CStelloView message handlers


void CStelloView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	int trek,play;
	short sksid = FALSE,pass = FALSE;
	//
	// Get a pointer to the document.
	//
	CStelloDoc* pDoc = GetDocument ();

	//
	// Get a DC and convert point to logical coordinates.
	//
	CClientDC dc (this);
	dc.SetMapMode (MM_LOENGLISH);
	dc.DPtoLP (&point);

	//
	// Hit test the grid and draw an X or O if appropriate.
	//
	BOOL bQuit = FALSE;
	play = FALSE;
	for (int i=1; i<=8 && !bQuit; i++) {
		for (int j=1; j<=8  && !bQuit; j++) {
			if (m_rect[i-1][j-1].PtInRect (point)) {
				if (!gameover) {
					trek = 10*j + i;
					if (makelist(&list, human, &mainboard)) 
						if (inlist(trek,&list)) {
							play = TRUE;
						}
						else 
							MessageBeep(0xFFFFFFFF); 
					else {
						pass = TRUE;
						play = TRUE;
					}
				}
				else
					MessageBeep(0xFFFFFFFF); 
				bQuit = TRUE;
			}
		}
		if (play) {
			if (!tilbage) {
		  	tilbage = TRUE;
			}
		  if (frem) {
				frem = FALSE;
			} 
	    if (sksid) 
	    	sksid = FALSE;
 			else {
 				if (pass) {
 					pass = FALSE;
 					if (!makelist(&list, computer, &mainboard)) {
 						makelist(&list, human, &mainboard);
						gameover = TRUE;
					}
					else {
						game.moves[playnm++] = 0;
						game.sidste = playnm;
						game.boards[playnm] = mainboard;
					//	update_move(playnm);
					}
				} 
				else 
  				makemove(trek, human);
			//	Invalidate();
				OnDraw(&dc);
    	//	curcl = other(curcl);
	    } 
	    if (!gameover) {
				BeginWaitCursor();
				if (m_pMainframe != NULL)
					m_pMainframe->get_com();
				EndWaitCursor();
				Invalidate();
		  }
	    /*if (!calculating) {
 				curcl = other(curcl);
 				if (gameover) {
				 	declarewinner(); 
				}
				else
					clon = TRUE; 
 			} */
			play = FALSE;
		}
	} 

	//
	// Let the base class do its thing.
	//
	
	CView::OnLButtonDown(nFlags, point);
	pDoc->UpdateAllViews(this);
}

void CStelloView::DrawX (CDC* pDC, CRect* pRect)
{
	//
	// Make a local copy of the rectangle and shrink it.
	//
	CRect rect;
	rect.CopyRect (pRect);
	rect.DeflateRect (10, 10);

	//
	// Create a red pen and use it to draw an X.
	//
	CPen pen (PS_SOLID, 10, RGB (255, 0, 0));
	CPen* pOldPen = pDC->SelectObject (&pen);

	pDC->MoveTo (rect.left, rect.top);
	pDC->LineTo (rect.right, rect.bottom);
	pDC->MoveTo (rect.left, rect.bottom);
	pDC->LineTo (rect.right, rect.top);

	pDC->SelectObject (pOldPen);
}

void CStelloView::DrawO (CDC* pDC, CRect* pRect)
{
	//
	// Make a local copy of the rectangle and shrink it.
	//
	CRect rect;
	rect.CopyRect (pRect);
	rect.DeflateRect (10, 10);

	//
	// Create a blue pen and use it to draw an O.
	//
	CPen pen (PS_SOLID, 10, RGB (0, 0, 255));
	CPen* pOldPen = pDC->SelectObject (&pen);
//	pDC->SelectStockObject (NULL_BRUSH);


	CBrush brush(RGB (255, 255, 255));
	CBrush* pOldbrush = pDC->SelectObject(&brush);

	pDC->Ellipse (rect);

	pDC->SelectObject (pOldPen);
}


void CStelloView::OnInitialUpdate() 
{

	CView::OnInitialUpdate();
	m_pStelloView = this;
	CClientDC dc (this);
	dc.SetMapMode (MM_LOENGLISH);
	CRect The_size(0,0,485,-485);
	dc.LPtoDP(The_size);
	MoveWindow(The_size);
	GetParent()->CalcWindowRect(The_size);
	GetParent()->MoveWindow(The_size);
	GetParent()->CenterWindow(NULL);
}
