#if !defined(AFX_SPILTID_H__E67FC510_9C0E_11D1_A7F5_00C0DF80875D__INCLUDED_)
#define AFX_SPILTID_H__E67FC510_9C0E_11D1_A7F5_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// Spiltid.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSpiltid form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CSpiltid : public CFormView
{
protected:
	CSpiltid();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CSpiltid)

// Form Data
public:
	//{{AFX_DATA(CSpiltid)
	enum { IDD = IDD_GAMETID };
	int		m_SpilTid;
	//}}AFX_DATA

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSpiltid)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CSpiltid();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CSpiltid)
	afx_msg void OnSpiltidOk();
	afx_msg void OnSpiltidFortryd();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SPILTID_H__E67FC510_9C0E_11D1_A7F5_00C0DF80875D__INCLUDED_)
