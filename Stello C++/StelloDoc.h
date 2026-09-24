// StelloDoc.h : interface of the CStelloDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_STELLODOC_H__4ED365FC_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
#define AFX_STELLODOC_H__4ED365FC_9442_11D1_A7EC_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Analyse.h"


class CStelloDoc : public CDocument
{
protected: // create from serialization only
	CStelloDoc();
	DECLARE_DYNCREATE(CStelloDoc)

// Attributes
public:

// Operations
public:

	void NytSpil(void);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CStelloDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CStelloDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CStelloDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STELLODOC_H__4ED365FC_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
