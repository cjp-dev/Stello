; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CMainFrame
LastTemplate=CMDIChildWnd
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "Stello.h"
LastPage=0

ClassCount=10
Class1=CStelloApp
Class2=CStelloDoc
Class3=CStelloView
Class4=CMainFrame

ResourceCount=7
Resource1=IDD_ABOUTBOX
Resource2=IDR_STELLOTYPE
Resource3=IDR_MAINFRAME
Class5=CAboutDlg
Class6=CChildFrame
Resource4=IDD_ANALYSE
Resource5=IDR_STELLOTYPE (English (U.S.))
Class7=CAnalyse
Class8=CAnalyseFrame
Resource6=IDR_MAINFRAME (English (U.S.))
Class9=CSpiltid
Class10=CSpilTidFrame
Resource7=IDD_GAMETID

[CLS:CStelloApp]
Type=0
HeaderFile=Stello.h
ImplementationFile=Stello.cpp
Filter=N
BaseClass=CWinApp
VirtualFilter=AC

[CLS:CStelloDoc]
Type=0
HeaderFile=StelloDoc.h
ImplementationFile=StelloDoc.cpp
Filter=N

[CLS:CStelloView]
Type=0
HeaderFile=StelloView.h
ImplementationFile=StelloView.cpp
Filter=C
BaseClass=CView
VirtualFilter=VWC
LastObject=CStelloView

[CLS:CMainFrame]
Type=0
HeaderFile=MainFrm.h
ImplementationFile=MainFrm.cpp
Filter=T
LastObject=ID_SELFPLAY
BaseClass=CMDIFrameWnd
VirtualFilter=fWC


[CLS:CChildFrame]
Type=0
HeaderFile=ChildFrm.h
ImplementationFile=ChildFrm.cpp
Filter=M
BaseClass=CMDIChildWnd
VirtualFilter=mfWC
LastObject=CChildFrame

[CLS:CAboutDlg]
Type=0
HeaderFile=Stello.cpp
ImplementationFile=Stello.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[MNU:IDR_MAINFRAME]
Type=1
Class=CMainFrame
Command1=ID_NYT_SPIL
Command2=ID_FILE_OPEN
Command3=ID_FILE_PRINT_SETUP
Command4=ID_FILE_MRU_FILE1
Command5=ID_APP_EXIT
Command6=ID_VIEW_TOOLBAR
Command7=ID_VIEW_STATUS_BAR
Command8=ID_HELP_FINDER
Command9=ID_APP_ABOUT
CommandCount=9

[TB:IDR_MAINFRAME]
Type=1
Class=CMainFrame
Command1=ID_NYT_SPIL
Command2=ID_FILE_OPEN
Command3=ID_FILE_SAVE
Command4=ID_FILE_PRINT
Command5=ID_APP_ABOUT
Command6=ID_CONTEXT_HELP
CommandCount=6

[MNU:IDR_STELLOTYPE]
Type=1
Class=CStelloView
Command1=ID_NYT_SPIL
Command2=ID_FILE_OPEN
Command3=ID_FILE_SAVE
Command4=ID_FILE_SAVE_AS
Command5=ID_FILE_PRINT
Command6=ID_FILE_PRINT_PREVIEW
Command7=ID_FILE_PRINT_SETUP
Command8=ID_FILE_MRU_FILE1
Command9=ID_APP_EXIT
Command10=ID_SKIFT_SIDE
Command11=ID_SPIL_FLETSPIL
Command12=ID_MINMAXLIB
Command13=ID_SPIL_TID
Command14=ID_SELFPLAY
Command15=ID_FREM
Command16=ID_TILBAGE
Command17=ID_TRAK_NU
Command18=ID_ANALYSE
Command19=ID_VIEW_TOOLBAR
Command20=ID_VIEW_STATUS_BAR
Command21=ID_WINDOW_CASCADE
Command22=ID_WINDOW_TILE_HORZ
Command23=ID_WINDOW_ARRANGE
Command24=ID_HELP_FINDER
Command25=ID_APP_ABOUT
CommandCount=25

[ACL:IDR_MAINFRAME]
Type=1
Class=CMainFrame
Command1=ID_EDIT_COPY
Command2=ID_FREM
Command3=ID_FILE_NEW
Command4=ID_FILE_OPEN
Command5=ID_FILE_PRINT
Command6=ID_FILE_SAVE
Command7=ID_TILBAGE
Command8=ID_EDIT_PASTE
Command9=ID_EDIT_UNDO
Command10=ID_EDIT_CUT
Command11=ID_HELP
Command12=ID_CONTEXT_HELP
Command13=ID_NEXT_PANE
Command14=ID_PREV_PANE
Command15=ID_EDIT_COPY
Command16=ID_EDIT_PASTE
Command17=ID_EDIT_CUT
Command18=ID_EDIT_UNDO
CommandCount=18

[TB:IDR_MAINFRAME (English (U.S.))]
Type=1
Class=?
Command1=ID_NYT_SPIL
Command2=ID_FILE_OPEN
Command3=ID_FILE_SAVE
Command4=ID_FILE_PRINT
Command5=ID_APP_ABOUT
Command6=ID_CONTEXT_HELP
CommandCount=6

[MNU:IDR_MAINFRAME (English (U.S.))]
Type=1
Class=?
Command1=ID_NYT_SPIL
Command2=ID_FILE_OPEN
Command3=ID_FILE_PRINT_SETUP
Command4=ID_FILE_MRU_FILE1
Command5=ID_APP_EXIT
Command6=ID_VIEW_TOOLBAR
Command7=ID_VIEW_STATUS_BAR
Command8=ID_HELP_FINDER
Command9=ID_APP_ABOUT
CommandCount=9

[MNU:IDR_STELLOTYPE (English (U.S.))]
Type=1
Class=CStelloView
Command1=ID_NYT_SPIL
Command2=ID_FILE_OPEN
Command3=ID_FILE_SAVE
Command4=ID_FILE_SAVE_AS
Command5=ID_FILE_PRINT
Command6=ID_FILE_PRINT_PREVIEW
Command7=ID_FILE_PRINT_SETUP
Command8=ID_FILE_MRU_FILE1
Command9=ID_APP_EXIT
Command10=ID_SKIFT_SIDE
Command11=ID_SPIL_TID
Command12=ID_FREM
Command13=ID_TILBAGE
Command14=ID_TRAK_NU
Command15=ID_ANALYSE
Command16=ID_VIEW_TOOLBAR
Command17=ID_VIEW_STATUS_BAR
Command18=ID_WINDOW_CASCADE
Command19=ID_WINDOW_TILE_HORZ
Command20=ID_WINDOW_ARRANGE
Command21=ID_HELP_FINDER
Command22=ID_APP_ABOUT
CommandCount=22

[ACL:IDR_MAINFRAME (English (U.S.))]
Type=1
Class=?
Command1=ID_FILE_NEW
Command2=ID_FILE_OPEN
Command3=ID_FILE_SAVE
Command4=ID_FILE_PRINT
Command5=ID_EDIT_UNDO
Command6=ID_EDIT_CUT
Command7=ID_EDIT_COPY
Command8=ID_EDIT_PASTE
Command9=ID_EDIT_UNDO
Command10=ID_EDIT_CUT
Command11=ID_EDIT_COPY
Command12=ID_EDIT_PASTE
Command13=ID_NEXT_PANE
Command14=ID_PREV_PANE
Command15=ID_CONTEXT_HELP
Command16=ID_HELP
CommandCount=16

[DLG:IDD_ANALYSE]
Type=1
Class=CAnalyse
ControlCount=10
Control1=IDC_STATIC,static,1342308352
Control2=IDC_EDIT1,edit,1350633600
Control3=IDC_STATIC,static,1342308352
Control4=IDC_EDIT2,edit,1350633600
Control5=IDC_STATIC,static,1342308352
Control6=IDC_EDIT3,edit,1350633600
Control7=IDC_EDIT4,edit,1350633600
Control8=IDC_STATIC,static,1342308352
Control9=IDC_STATIC,static,1342308352
Control10=IDC_EDIT5,edit,1350633600

[CLS:CAnalyse]
Type=0
HeaderFile=Analyse.h
ImplementationFile=Analyse.cpp
BaseClass=CFormView
Filter=D
LastObject=IDC_EDIT3
VirtualFilter=VWC

[CLS:CAnalyseFrame]
Type=0
HeaderFile=AnalyseFrame.h
ImplementationFile=AnalyseFrame.cpp
BaseClass=CMDIChildWnd
Filter=M
LastObject=CAnalyseFrame
VirtualFilter=mfWC

[DLG:IDD_GAMETID]
Type=1
Class=CSpiltid
ControlCount=4
Control1=IDC_STATIC,static,1342308352
Control2=IDC_EDIT1,edit,1350631552
Control3=IDC_SPILTID_OK,button,1342242817
Control4=IDC_SPILTID_FORTRYD,button,1342242816

[CLS:CSpiltid]
Type=0
HeaderFile=Spiltid.h
ImplementationFile=Spiltid.cpp
BaseClass=CFormView
Filter=D
LastObject=CSpiltid
VirtualFilter=VWC

[CLS:CSpilTidFrame]
Type=0
HeaderFile=SpilTidFrame.h
ImplementationFile=SpilTidFrame.cpp
BaseClass=CMDIChildWnd
Filter=M
LastObject=CSpilTidFrame
VirtualFilter=mfWC

