// Stello.h : main header file for the STELLO application
//

#if !defined(AFX_STELLO_H__4ED365F4_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
#define AFX_STELLO_H__4ED365F4_9442_11D1_A7EC_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CStelloApp:
// See Stello.cpp for the implementation of this class
//
typedef struct
		{ char name[30];
			char	Document[80][30];
			short int	DocTop;			/* First line in document  */
			short int	Height;			/* Size of window in chars */
			short int	Lines;			/* Length of doc in lines  */
			short int	WindId;
			short int	isopen;
		} GAME;
		
typedef struct
		{	char name[30];
			short int	WindId;
			short int	isopen;
		} LILLE;
		
typedef struct
		{	short int timedef,leveldyb,leveltid;
			short int 	borderdef;
			short int   hvmudef;
			short int   notadef;
			short int   anadef;
			short int   gamdef;
		} config;
		

class CStelloApp : public CWinApp
{
public:
	CStelloApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CStelloApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

	CMultiDocTemplate* pAnalyseTemplate;
	CMultiDocTemplate* pSpilTidTemplate;

	void opset(int newval);

// Implementation

	//{{AFX_MSG(CStelloApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STELLO_H__4ED365F4_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
