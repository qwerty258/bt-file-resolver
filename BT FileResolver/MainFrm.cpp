// MainFrm.cpp : CMainFrame 类的实现
//

#include "stdafx.h"
#include "BT FileResolver.h"

#include "MainFrm.h"
#include "global.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define MIN_WIDTH	= 800
#define MIN_HEIGHT	= 600

// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

static UINT indicators[] =
{
    ID_INDICATOR_CURRENT_FILE_PATH,
    ID_INDICATOR_FILE_COUNT,
    ID_INDICATOR_PROGRESS,
    ID_INDICATOR_PROCESS_STATE,
};


// CMainFrame 构造/析构

CMainFrame::CMainFrame()
{
    // TODO: 在此添加成员初始化代码
}

CMainFrame::~CMainFrame()
{
}


int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if(CFrameWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    if(!m_wndToolBar.CreateEx(this, NULL, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) || !m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
    {
        TRACE0("未能创建工具栏\n");
        return -1;      // 未能创建
    }

    if(!m_wndStatusBar.Create(this) || !m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT)))
    {
        TRACE0("未能创建状态栏\n");
        return -1;      // 未能创建
    }

    if(!m_wndInfoPanel.Create(this, IDD_INFO_PANEL, CBRS_TOOLTIPS | CBRS_LEFT, AFX_IDW_DIALOGBAR))
    {
        TRACE0("未能创建信息面板\n");
        return -1;
    }

    CBitmap bmp, bmp1;
    bmp.LoadBitmap(IDB_TOOLBAR);
    bmp1.LoadBitmap(IDB_TOOLBAR_DISABLE);

    CImageList clImage, clDisImage;
    clImage.Create(48, 48, ILC_COLOR24 | ILC_MASK, 5, 0);
    clDisImage.Create(48, 48, ILC_COLOR24 | ILC_MASK, 5, 0);
    clImage.Add(&bmp, RGB(192, 192, 192));
    clDisImage.Add(&bmp1, RGB(192, 192, 192));

    m_wndToolBar.GetToolBarCtrl().SetImageList(&clImage);
    m_wndToolBar.GetToolBarCtrl().SetDisabledImageList(&clDisImage);

    clImage.Detach();
    clDisImage.Detach();
    bmp.Detach();

    m_wndStatusBar.SetPaneStyle(m_wndStatusBar.CommandToIndex(ID_INDICATOR_PROCESS_STATE), SBPS_STRETCH);

    int nWidth = (int)(::GetSystemMetrics(SM_CXSCREEN) * 0.6);
    int nHeight = (int)(::GetSystemMetrics(SM_CYSCREEN) * 0.6);
    MoveWindow(0, 0, nWidth, nHeight, FALSE);
    CenterWindow();

    return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if(!CFrameWnd::PreCreateWindow(cs))
        return FALSE;
    // TODO: 在此处通过修改
    //  CREATESTRUCT cs 来修改窗口类或样式
    cs.style &= ~FWS_ADDTOTITLE;


    return TRUE;
}


// CMainFrame 诊断

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
    CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
    CFrameWnd::Dump(dc);
}
#endif //_DEBUG


// CMainFrame 消息处理程序

void CMainFrame::OnClose()
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if(hThread)	//线程运行时不允许关闭
    {
        ::MessageBeep(MB_ICONEXCLAMATION);
        return;
    }
    CFrameWnd::OnClose();
}
