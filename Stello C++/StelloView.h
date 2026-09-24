// StelloView.h : interface of the CStelloView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_STELLOVIEW_H__4ED365FE_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
#define AFX_STELLOVIEW_H__4ED365FE_9442_11D1_A7EC_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CStelloView : public CView
{
protected: // create from serialization only
	CStelloView();
	DECLARE_DYNCREATE(CStelloView)
	CRect m_rect[8][8];

// Attributes
public:
	CStelloDoc* GetDocument();
	void DrawO (CDC* pDC, CRect* pRect);
	void DrawX (CDC* pDC, CRect* pRect);


// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CStelloView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
	protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CStelloView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CStelloView)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in StelloView.cpp
inline CStelloDoc* CStelloView::GetDocument()
   { return (CStelloDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STELLOVIEW_H__4ED365FE_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
