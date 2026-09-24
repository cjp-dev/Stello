// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__4ED365F8_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
#define AFX_MAINFRM_H__4ED365F8_9442_11D1_A7EC_00C0DF80875D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CMainFrame : public CMDIFrameWnd
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Attributes
public:

// Operations
public:
	void get_com(void);
	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnAnalyse();
	afx_msg void OnUpdateAnalyse(CCmdUI* pCmdUI);
	afx_msg void OnNytSpil();
	afx_msg void OnUpdateNytSpil(CCmdUI* pCmdUI);
	afx_msg void OnSkiftSide();
	afx_msg void OnUpdateSkiftSide(CCmdUI* pCmdUI);
	afx_msg void OnSpilTid();
	afx_msg void OnUpdateSpilTid(CCmdUI* pCmdUI);
	afx_msg void OnSpilFletspil();
	afx_msg void OnUpdateSpilFletspil(CCmdUI* pCmdUI);
	afx_msg void OnMinmaxlib();
	afx_msg void OnUpdateMinmaxlib(CCmdUI* pCmdUI);
	afx_msg void OnFrem();
	afx_msg void OnUpdateFrem(CCmdUI* pCmdUI);
	afx_msg void OnTilbage();
	afx_msg void OnUpdateTilbage(CCmdUI* pCmdUI);
	afx_msg void OnSelfplay();
	afx_msg void OnUpdateSelfplay(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__4ED365F8_9442_11D1_A7EC_00C0DF80875D__INCLUDED_)
