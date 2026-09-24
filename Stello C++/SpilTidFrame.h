#if !defined(AFX_SPILTIDFRAME_H__E67FC511_9C0E_11D1_A7F5_00C0DF80875D__INCLUDED_)
#define AFX_SPILTIDFRAME_H__E67FC511_9C0E_11D1_A7F5_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// SpilTidFrame.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSpilTidFrame frame

class CSpilTidFrame : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CSpilTidFrame)
protected:
	CSpilTidFrame();           // protected constructor used by dynamic creation

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSpilTidFrame)
	public:
	virtual void ActivateFrame(int nCmdShow = -1);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CSpilTidFrame();

	// Generated message map functions
	//{{AFX_MSG(CSpilTidFrame)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SPILTIDFRAME_H__E67FC511_9C0E_11D1_A7F5_00C0DF80875D__INCLUDED_)
