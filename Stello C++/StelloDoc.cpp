// StelloDoc.cpp : implementation of the CStelloDoc class
//

#include "stdafx.h"
#include "Stello.h"
#include "brain\reversi.h"

#include "StelloDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern short int border_on,lastdyb,lasttid,lastspil,backthink;
extern short int frem,tilbage,anaopen,gamopen,gamfirst,anafirst, posible,calclib;
extern short int hvmul,play,pass,orient,treknu,markoer, tryagain;
extern volatile short int curcl,clon,nytid,clhvid,clsort;

/////////////////////////////////////////////////////////////////////////////
// CStelloDoc

IMPLEMENT_DYNCREATE(CStelloDoc, CDocument)

BEGIN_MESSAGE_MAP(CStelloDoc, CDocument)
	//{{AFX_MSG_MAP(CStelloDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStelloDoc construction/destruction

CStelloDoc::CStelloDoc()
{

}

CStelloDoc::~CStelloDoc()
{
}

BOOL CStelloDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)
/*		init_game();
		if (libok) {
      libon = TRUE;
			tryagain = 0;
		} */
    /* menu_ienable(M_tree,SPBACK,FALSE); */
	//	tilbage = FALSE;
		/* menu_ienable(M_tree,SPFREM,FALSE); */
	//	frem = FALSE;
	//	calc = FALSE;
	//	bcalc = FALSE;
 // 	curcl = DARK;

	//	clon = FALSE;
		/* reset_clock(); */

   // clon = TRUE;

	NytSpil();

	return TRUE;
}

void CStelloDoc::NytSpil()
{
	init_game();
	tilbage = FALSE;
	frem = FALSE;
	if (libok) {
    libon = TRUE;
	  tryagain = 0;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CStelloDoc serialization

void CStelloDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CStelloDoc diagnostics

#ifdef _DEBUG
void CStelloDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CStelloDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CStelloDoc commands
