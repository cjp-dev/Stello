#if !defined(AFX_ANALYSE_H__E2E6A5D1_9A54_11D1_A7F2_00C0DF80875D__INCLUDED_)
#define AFX_ANALYSE_H__E2E6A5D1_9A54_11D1_A7F2_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// Analyse.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAnalyse form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CAnalyse : public CFormView
{
protected:
	CAnalyse();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CAnalyse)

// Form Data
public:
	//{{AFX_DATA(CAnalyse)
	enum { IDD = IDD_ANALYSE };
	int		m_Knuder;
	CString	m_traek;
	int		m_aeval;
	CString	m_Eval;
	CString	m_Tid;
	//}}AFX_DATA

// Attributes
public:

// Operations
public:
	void make_result(short int value, long tid);
	void make_try(short int move,short int look);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAnalyse)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CAnalyse();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CAnalyse)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ANALYSE_H__E2E6A5D1_9A54_11D1_A7F2_00C0DF80875D__INCLUDED_)
