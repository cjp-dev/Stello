#if !defined(AFX_ANALYSEFRAME_H__E2E6A5D2_9A54_11D1_A7F2_00C0DF80875D__INCLUDED_)
#define AFX_ANALYSEFRAME_H__E2E6A5D2_9A54_11D1_A7F2_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// AnalyseFrame.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAnalyseFrame frame

class CAnalyseFrame : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CAnalyseFrame)
protected:
	CAnalyseFrame();           // protected constructor used by dynamic creation

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAnalyseFrame)
	public:
	virtual void ActivateFrame(int nCmdShow = -1);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CAnalyseFrame();

	// Generated message map functions
	//{{AFX_MSG(CAnalyseFrame)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ANALYSEFRAME_H__E2E6A5D2_9A54_11D1_A7F2_00C0DF80875D__INCLUDED_)
