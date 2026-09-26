// Stello.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Stello.h"

#include "MainFrm.h"
#include "ChildFrm.h"
#include "StelloDoc.h"
#include "StelloView.h"
#include "Analyse.h"
#include "AnalyseFrame.h"
#include "Spiltid.h"
#include "SpilTidFrame.h"
#include "brain\reversi.h"
#include "brain\book.h"
#include "brain\hash.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


									
GAME Text;
LILLE Ana,Rev;
config revdef;

short int border_on,lastdyb,lasttid,lastspil,backthink;
short int frem,tilbage,anaopen,gamopen,gamfirst,anafirst, posible,calclib;
short int hvmul,play,pass,orient,treknu,markoer, tryagain;
volatile short int curcl,clon,nytid,clhvid,clsort;

char nmoves[30];        /* forventede trek */
char nmoves1[30];
gamerec game;
savegame sgame;

/////////////////////////////////////////////////////////////////////////////
// CStelloApp

BEGIN_MESSAGE_MAP(CStelloApp, CWinApp)
	//{{AFX_MSG_MAP(CStelloApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStelloApp construction

CStelloApp::CStelloApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CStelloApp object

CStelloApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CStelloApp initialization

BOOL CStelloApp::InitInstance()
{
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif 

	// Change the registry key under which our settings are stored.
	// You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("ClausJPedersen"));

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMultiDocTemplate(
		IDR_STELLOTYPE,
		RUNTIME_CLASS(CStelloDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CStelloView));
	AddDocTemplate(pDocTemplate);

	pAnalyseTemplate = new CMultiDocTemplate(
		IDR_STELLOTYPE,
		RUNTIME_CLASS(CStelloDoc),
		RUNTIME_CLASS(CAnalyseFrame), // custom MDI child frame
		RUNTIME_CLASS(CAnalyse));
	AddDocTemplate(pAnalyseTemplate);

	pSpilTidTemplate = new CMultiDocTemplate(
		IDR_STELLOTYPE,
		RUNTIME_CLASS(CStelloDoc),
		RUNTIME_CLASS(CSpilTidFrame), // custom MDI child frame
		RUNTIME_CLASS(CSpiltid));
	AddDocTemplate(pSpilTidTemplate);

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;

	FILE * fd;
	
		clhvid = 0;
		clsort = 0;
		clon = TRUE;
		nytid = TRUE;
		curcl = DARK;
		play = FALSE;
		pass = FALSE;
		calc = FALSE;
		bcalc = FALSE;
		
		tilbage = FALSE;
		frem = FALSE;
		treknu = FALSE;
		calclib = TRUE;
		
		if ((fd = fopen("rev.cfg","rb")) != NULL) {
	    if (fread(&revdef, sizeof(revdef),1,fd) == 1) 
		    opset(TRUE);
		  else
		  	opset(FALSE);
		  fclose(fd);
	  } 
	  else
			opset(FALSE);

		init_nodes(ANTAL_KNUDER);
		
		if (Get_book()) {
      libok = TRUE;
      libon = TRUE;
			tryagain = 0;
    }
    else {
      libok = FALSE;
      libon = FALSE;
			tryagain = 5;
    } 
		
		make_node(&root); 
 
		f_hash_init ();
		init_rev();
		
		
#ifdef DEBUG
		debugf = fopen("moves", "w");
#endif 


	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);
	cmdInfo.m_nShellCommand = CCommandLineInfo::FileNothing;

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

		
 
	// The main window has been initialized, so show and update it.
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	pDocTemplate->OpenDocumentFile(NULL);

	return TRUE;
}

void CStelloApp::opset(int newval)
{
/*	if (newval) {
		if (revdef.timedef == tid_per_trek) { */
		/*	menu_icheck(M_tree,TIDPTK,TRUE); */
		/*	tid_kontrol = tid_per_trek;
			lookahead = revdef.leveltid;
			lastdyb = revdef.leveldyb;
			lasttid = revdef.leveltid;*/
/*		} else { */
  /*		menu_icheck(M_tree,SOGE,TRUE); */
  	/*	tid_kontrol = sogedybde;
			lookahead = revdef.leveldyb;
			lastdyb = revdef.leveldyb;
			lasttid = revdef.leveltid;
  	}
  	border_on = revdef.borderdef;*/
/*  	if ((hvmul = revdef.hvmudef) == TRUE)
  		menu_icheck(M_tree,MULTRK,TRUE);
  	if ((orient = revdef.notadef) == TRUE)
  		menu_icheck(M_tree,VENDB,TRUE);
  	if ((gamopen = revdef.gamdef) == TRUE)
  		menu_icheck(M_tree,GAMWIN,TRUE);
  	if ((anaopen = revdef.anadef) == TRUE)
  		menu_icheck(M_tree,ANAWIN,TRUE); */
  /*	hvmul = revdef.hvmudef;
  	orient = revdef.notadef;
  	gamopen = revdef.gamdef;
  	anaopen = revdef.anadef;*/
  //} else {
 /* 	menu_icheck(M_tree,TIDPTK,TRUE); */
		tid_kontrol = spil_tid;
		GameTid = 5;
		lookahead = 8;
		lastdyb = 3;
		lasttid = 3;
		lastspil = 3;
		backthink = FALSE;
		border_on = FALSE;
		hvmul = FALSE;
		orient = FALSE;
		anaopen = FALSE;
		gamopen = FALSE;  
	//}
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CStelloApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CStelloApp commands

int CStelloApp::ExitInstance() 
{
	// TODO: Add your specialized code here and/or call the base class

//	free(root); 
	free(v_hentry); 
	
	#ifdef DEBUG
	fclose(debugf);
	#endif 
	
	return CWinApp::ExitInstance();
}
