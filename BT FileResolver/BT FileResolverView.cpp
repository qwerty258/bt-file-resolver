// BT FileResolverView.cpp : CBTFileResolverView 类的实现
#include "stdafx.h"

#include "BT FileResolver.h"

#include "BT FileResolverDoc.h"
#include "BT FileResolverView.h"
#include "global.h"
#include "WorkThread.h"
#include <afxpriv.h>	//消息 - WM_IDLEUPDATECMDUI

#pragma warning( disable: 4996 )

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define FREE_THREAD_HANDLE(h) { if(h) CloseHandle(h); h = NULL; }
#define UPDATE_TOOLBAR_UI { (CToolBar*) AfxGetMainWnd()->GetDescendantWindow(AFX_IDW_TOOLBAR)->SendMessage(WM_IDLEUPDATECMDUI, (WPARAM) TRUE, NULL); }
#define SET_PROGRESS_BAR_MARQUEE_STYLE(h, b) { b ? ::SetWindowLong(h, GWL_STYLE, ::GetWindowLong(h, GWL_STYLE) | PBS_MARQUEE) : \
	::SetWindowLong(h, GWL_STYLE, ::GetWindowLong(h, GWL_STYLE) & ~PBS_MARQUEE); }

//更新界面的时间间隔，多少毫秒更新一次界面
#define UPDATE_UI_INTERVAL			200
#define UPDATE_PROGRESS_INTERVAL	50

//种子文件的扩展名
#define BT_FILE_EXT_LOWER _T(".torrent")

//过滤类别，可以多添加些类别，注意要大写，结尾的；不能少
#define FILTER_TYPE_ALL		_T("")
#define FILTER_TYPE_VIDEO	_T("AVI;ASF;WMV;AVS;FLV;MKV;MOV;3GP;MP4;MPG;MPEG;DAT;OGM;VOB;RM;RMVB;TS;TP;IFO;NSV;M2TS;")
#define FILTER_TYPE_MUSIC	_T("MP3;AAC;WAV;WMA;CDA;FLAC;M4A;MID;MKA;MP2;MPA;MPC;APE;OFR;OGG;RA;WV;TTA;AC3;DTS;")
#define FILTER_TYPE_PICTURE	_T("BMP;GIF;JPEG;JPG;PNG;TIF;")
#define FILTER_TYPE_SOFT	_T("7Z;RAR;ZIP;ISO;ISZ;")

//处理进程的状态
typedef enum PROCESS_STATE
{
    PROCESS_STATE_IDLE/* 空闲 */, PROCESS_STATE_GENERATING/* 搜索文件 */, PROCESS_STATE_RUNNING/* 运行 */
};

//跨文件全局变量定义，声明在global.h
vector<CString> vecBTFiles;
vector<CString> vecErrorFiles;
DWORD dwLastUpdateUI = 0;
HANDLE hThread = NULL;
BOOL bWantTerminate = FALSE;
CString Filter_Type;
CString Filter_Keyword;
BOOL Filter_Keyword_BTFile;
int Filter_Operator;
CString Filter_FileExt;
DWORDLONG Filter_FileSize;

//全局变量声明
CString FindStr;
SORT_PARAM SortParam;//排序参数
LIST_ITEM CurrentSelItem;//这个是给详细信息对话框的回调使用的，传递当前选择的列表项目

//浏览目录并选择指定文件
void ExploreFile(const CString& FileName)
{
    if(FileName.IsEmpty()) return;

    ShellExecute(::GetDesktopWindow(), _T("open"), _T("explorer.exe"),
                 _T("/select,") + FileName, NULL, SW_SHOWNORMAL);
}

//取得指定文件名的类别名称，并返回对应的图标索引
int GetFileInfo(CString FileName, CString& TypeName)
{
    SHFILEINFO shFileInfo;
    ZeroMemory(&shFileInfo, sizeof(SHFILEINFO));

    if(SHGetFileInfo(FileName, FILE_ATTRIBUTE_NORMAL, &shFileInfo, sizeof(SHFILEINFO),
        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES))
        TypeName.Format(_T("%s"), shFileInfo.szTypeName);
    else
        TypeName = _T("文件");

    return shFileInfo.iIcon;
}

/* 仿函数
 * 排序，这里也可以通过重载LIST_ITEM的<运算符实现，不过不知道怎么根据
 * 汉字的拼音进行排序，排序汉字有点乱，好像是根据笔画排序的 */
BOOL IsLesser(const LIST_ITEM& item1, const LIST_ITEM& item2)
{
    CString s1, s2;
    DWORDLONG dwl1 = _I64_MAX;
    DWORDLONG dwl2 = _I64_MAX;

    switch(SortParam.nColIndex)
    {
        case 0://文件名
            s1 = item1.FileName;
            s2 = item2.FileName;
            break;
        case 1://文件大小
            dwl1 = item1.FileSize;
            dwl2 = item2.FileSize;
            break;
        case 2://列别名称
            s1 = item1.FileTypeName;
            s2 = item2.FileTypeName;
            break;
        case 3://路径
            s1 = item1.InnerPath;
            s2 = item2.InnerPath;
            break;

            /*
            case 4://种子发布者
            s1 = item1.BTPublisher;
            s2 = item2.BTPublisher;
            break;
            case 5://种子文件名
            s1 = item1.BTFileName;
            s2 = item2.BTFileName;
            break;
            case 6://种子创建工具
            s1 = item1.BTCreator;
            s2 = item2.BTCreator;
            break;
            case 7://种子创建日期
            s1 = item1.BTCreationDate;
            s2 = item2.BTCreationDate;
            break;
            case 8://备注
            s1 = item1.BTComment;
            s2 = item2.BTComment;
            break;
            */
    }

    if(dwl1 != _I64_MAX)//比较文件大小
        return SortParam.bSortAsc ? dwl1 < dwl2 : dwl2 < dwl1;
    else//比较字符串
        return SortParam.bSortAsc ? s1 < s2 : s2 < s1;

}

// CBTFileResolverView

IMPLEMENT_DYNCREATE(CBTFileResolverView, CListView)

BEGIN_MESSAGE_MAP(CBTFileResolverView, CListView)
    /* 自定义消息处理 */
    ON_MESSAGE(WM_THREAD_PROCESS_DONE, &CBTFileResolverView::OnWorkDone)
    ON_MESSAGE(WM_THREAD_PROCESS_RUNNING, &CBTFileResolverView::OnProcess)

    /*  */
    ON_COMMAND(ID_FILE_OPENPATH, &CBTFileResolverView::OnFileOpenpath)
    ON_COMMAND(ID_ACTION_PROCESS, &CBTFileResolverView::OnActionProcess)
    ON_UPDATE_COMMAND_UI(ID_ACTION_PROCESS, &CBTFileResolverView::OnUpdateActions)
    ON_COMMAND(ID_ACTION_CANCEL, &CBTFileResolverView::OnActionCancel)
    ON_UPDATE_COMMAND_UI(ID_ACTION_CANCEL, &CBTFileResolverView::OnUpdateActions)
    ON_COMMAND(ID_ACIION_DELETE, &CBTFileResolverView::OnActionDelete)
    ON_COMMAND(ID_ACTION_CLEAR, &CBTFileResolverView::OnActionClear)
    ON_UPDATE_COMMAND_UI(ID_ACIION_DELETE, &CBTFileResolverView::OnUpdateActions)
    ON_UPDATE_COMMAND_UI(ID_ACTION_CLEAR, &CBTFileResolverView::OnUpdateActions)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENPATH, &CBTFileResolverView::OnUpdateActions)
    ON_WM_RBUTTONDOWN()
    ON_COMMAND(ID_BT_OPEN_FILE, &CBTFileResolverView::OnBtOpenFile)
    ON_COMMAND(ID_BT_OPEN_PATH, &CBTFileResolverView::OnBtOpenPath)
    ON_UPDATE_COMMAND_UI(ID_BT_OPEN_FILE, &CBTFileResolverView::OnUpdateActions)
    ON_UPDATE_COMMAND_UI(ID_BT_OPEN_PATH, &CBTFileResolverView::OnUpdateActions)
    ON_WM_INITMENUPOPUP()
    ON_WM_LBUTTONDBLCLK()
    ON_BN_CLICKED(IDC_DEBUG_TEST, &CBTFileResolverView::OnBnClickedDebugTest)
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, &CBTFileResolverView::OnLvnGetdispinfo)
    ON_WM_KEYDOWN()
    ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, &CBTFileResolverView::OnLvnColumnclick)
    ON_BN_CLICKED(IDC_BUTTON1, &CBTFileResolverView::OnBnClickedButton1)
    ON_COMMAND(ID_VIEW_ERROR, &CBTFileResolverView::OnViewError)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ERROR, &CBTFileResolverView::OnUpdateActions)
    ON_COMMAND(ID_VIEW_DETAIL, &CBTFileResolverView::OnViewDetail)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DETAIL, &CBTFileResolverView::OnUpdateActions)
    //    ON_COMMAND(ID_32788,&CBTFileResolverView::On32788)
    ON_COMMAND(ID_FILE_RENAMEZYF, &CBTFileResolverView::OnFileRenamezyf)
END_MESSAGE_MAP()

// CBTFileResolverView 构造/析构

CBTFileResolverView::CBTFileResolverView()
{
    // TODO: 在此处添加构造代码
    m_ProcessState = PROCESS_STATE_IDLE;
}

CBTFileResolverView::~CBTFileResolverView()
{
    //！！一定要分离
    m_ListViewIL.Detach();
}

BOOL CBTFileResolverView::PreCreateWindow(CREATESTRUCT& cs)
{
    // TODO: 在此处通过修改
    //  CREATESTRUCT cs 来修改窗口类或样式

    cs.style = cs.style & ~LVS_TYPEMASK | LVS_REPORT |
        LVS_SHOWSELALWAYS | LVS_OWNERDATA;

    return CListView::PreCreateWindow(cs);
}

void CBTFileResolverView::OnInitialUpdate()
{
    CListView::OnInitialUpdate();

    GetListCtrl().SetExtendedStyle(GetListCtrl().GetExtendedStyle() | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);

    SortParam.bSortAsc = TRUE;
    SortParam.nColIndex = 0;

    //dialogbar的指针
    m_wndDialogBar = AfxGetMainWnd()->GetDescendantWindow(AFX_IDW_DIALOGBAR);

    m_wndDialogBar->CheckRadioButton(IDC_RADIO_ALL, IDC_RADIO_SOFT, IDC_RADIO_ALL);
    m_wndDialogBar->GetDlgItem(IDC_COMBO_SIZE_TYPE)->SendMessage(CB_SETCURSEL, 0, NULL);
    m_wndDialogBar->SetDlgItemText(IDC_FILE_SIZE, _T("0"));
    m_wndDialogBar->CheckDlgButton(IDC_CHECK_AUTO_START, TRUE);
    m_wndDialogBar->CheckDlgButton(IDC_CHECK_KEYWORD_BTFILE, TRUE);
    ((CEdit*)m_wndDialogBar->GetDlgItem(IDC_FILE_SIZE))->SetLimitText(8);//最大 百GB

#ifndef _DEBUG
    m_wndDialogBar->GetDlgItem(IDC_DEBUG_TEST)->ShowWindow(SW_HIDE);//在Release版本中隐藏测试按钮
#endif

    //图标列表
    /*
     *	！！一定要在CBTFileResolverView析构以前从m_ListViewIL分离它的图标列表，因为m_ListViewIL的图标列表是指向
     *	系统的图标列表的，CBTFileResolverView析构时会调用m_ListViewIL的析构造成系统图标列表的某些图标被释放，
     *	见CBTFileResolverView的析构函数。
     *	m_ListViewIL的图标列表是由系统维护的，不要替换或添加或删除它包含的图标，得到对应的图标索引的方法见GetFileInfo函数。
     */
    SHFILEINFO shFileInfo;
    m_ListViewIL.Attach((HIMAGELIST)SHGetFileInfo(_T(""), FILE_ATTRIBUTE_NORMAL,
        &shFileInfo, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES));
    GetListCtrl().SetImageList(&m_ListViewIL, LVSIL_SMALL);

    //创建进度条
    CStatusBar* pBar = (CStatusBar*)AfxGetMainWnd()->GetDescendantWindow(AFX_IDW_STATUS_BAR);
    if(pBar)
    {
        CRect r;
        pBar->GetItemRect(pBar->CommandToIndex(ID_INDICATOR_PROGRESS), &r);
        r.InflateRect(-1, -1, -1, -1);
        m_Progress.Create(WS_CHILD, r, pBar, NULL);
        m_Progress.SetRange(0, 100);
    }

    CString headers[] =
    {
        _T("文件名"),
        _T("大小"),
        _T("类型"),
        _T("种子内路径"),

        /* 下面这几项就不再显示了，看着有点乱，改到详细信息对话框去显示了
        _T("发布者"),
        _T("种子文件名"),
        _T("种子创建工具"),
        _T("种子创建日期"),
        _T("种子备注"),
        _*/
    };

    int colsize[] =
    {
        350,
        120,
        150,
        200,
    };

    int nLen = sizeof(headers) / sizeof(CString);

    CListCtrl& list = GetListCtrl();
    for(int i = 0; i != nLen; ++i)
    {
        if(i == 1)
            list.InsertColumn(i, headers[i], LVCFMT_RIGHT);
        else
            list.InsertColumn(i, headers[i], LVCFMT_LEFT);

        list.SetColumnWidth(i, colsize[i]);
    }
}


// CBTFileResolverView 诊断

#ifdef _DEBUG
void CBTFileResolverView::AssertValid() const
{
    CListView::AssertValid();
}

void CBTFileResolverView::Dump(CDumpContext& dc) const
{
    CListView::Dump(dc);
}

CBTFileResolverDoc* CBTFileResolverView::GetDocument() const // 非调试版本是内联的
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CBTFileResolverDoc)));
    return (CBTFileResolverDoc*)m_pDocument;
}
#endif //_DEBUG


// CBTFileResolverView 消息处理程序

void CBTFileResolverView::OnFileOpenpath()
{
    // TODO: 在此添加命令处理程序代码
    ::BROWSEINFO bi;
    ZeroMemory(&bi, sizeof(BROWSEINFO));
    bi.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    bi.lpszTitle = _T("Select the directory that contains the seed file");
    bi.ulFlags = BIF_RETURNONLYFSDIRS;

    LPITEMIDLIST pItemIdList = ::SHBrowseForFolder(&bi);

    if(pItemIdList)
    {
        TCHAR szPath[MAX_PATH];
        if(SHGetPathFromIDList(pItemIdList, szPath))
        {
            m_CurrentPath = szPath;

            if(m_CurrentPath.ReverseFind(_T('\\')) != m_CurrentPath.GetLength() - 1)
                m_CurrentPath += _T("\\");

            SetStatusText(ID_INDICATOR_CURRENT_FILE_PATH, _T("当前目录：") + m_CurrentPath);

            if(m_wndDialogBar->IsDlgButtonChecked(IDC_CHECK_AUTO_START)) OnActionProcess();
        }
    }
}

void CBTFileResolverView::OnActionProcess()
{
    ASSERT(m_ProcessState == PROCESS_STATE_IDLE);

    /* 取得所有的过滤信息 */
    switch(m_wndDialogBar->GetCheckedRadioButton(IDC_RADIO_ALL, IDC_RADIO_SOFT))
    {
        case IDC_RADIO_ALL:
            Filter_Type = FILTER_TYPE_ALL;
            break;
        case IDC_RADIO_VIDEO:
            Filter_Type = FILTER_TYPE_VIDEO;
            break;
        case IDC_RADIO_MUSIC:
            Filter_Type = FILTER_TYPE_MUSIC;
            break;
        case IDC_RADIO_PICTURE:
            Filter_Type = FILTER_TYPE_PICTURE;
            break;
        case IDC_RADIO_SOFT:
            Filter_Type = FILTER_TYPE_SOFT;
            break;
        default:
            Filter_Type = FILTER_TYPE_ALL;
            break;
    }

    m_wndDialogBar->GetDlgItemText(IDC_FILE_EXTENDSION, Filter_FileExt);
    Filter_FileExt.Trim();Filter_FileExt.MakeUpper();Filter_FileExt += _T(" ");

    m_wndDialogBar->GetDlgItemText(IDC_COMBO_KEYWORD, Filter_Keyword);
    Filter_Keyword.Trim();Filter_Keyword.MakeUpper();


    Filter_Operator = ((CComboBox*)m_wndDialogBar-> \
                       GetDlgItem(IDC_COMBO_SIZE_TYPE))->GetCurSel();

    CString FileSize;
    m_wndDialogBar->GetDlgItemText(IDC_FILE_SIZE, FileSize);
    if(FileSize.IsEmpty())
        Filter_Operator = OPERATOR_UNDEFINE;
    else
    {
        Filter_FileSize = _tstoi64(FileSize.GetString());

        if(Filter_FileSize > (DWORDLONG)_UI64_MAX / 1024)
        {
            AfxMessageBox(_T("输入的搜索目标文件的大小无效。"));
            return;
        }

        if(!Filter_FileSize && Filter_Operator == OPERATOR_LESS_THAN)
        {
            AfxMessageBox(_T("不能设置搜索目标文件的大小不大于0KB。"));
            return;
        }
    }

    Filter_Keyword_BTFile = m_wndDialogBar->IsDlgButtonChecked(IDC_CHECK_KEYWORD_BTFILE);
    /* 取得所有的过滤信息 */

    //清除
    m_vecListItems.clear();
    vecErrorFiles.clear();
    vecBTFiles.clear();
    GetListCtrl().SetItemCountEx(0);
    GetListCtrl().Invalidate();
    GetListCtrl().UpdateWindow();
    UpdateFileCount();

    //开始搜索指定目录内的种子文件
    m_ProcessState = PROCESS_STATE_GENERATING;
    UPDATE_TOOLBAR_UI;

    //搜索文件
    BeginWaitCursor();
    SetStatusText(ID_INDICATOR_PROCESS_STATE, _T("正在搜索文件..."));
    SET_PROGRESS_BAR_MARQUEE_STYLE(m_Progress.GetSafeHwnd(), TRUE);
    m_Progress.ShowWindow(SW_SHOW);
    FindFile(m_CurrentPath);
    SET_PROGRESS_BAR_MARQUEE_STYLE(m_Progress.GetSafeHwnd(), FALSE);
    m_Progress.ShowWindow(SW_HIDE);
    EndWaitCursor();

    //创建工作线程
    bWantTerminate = FALSE;
    hThread = ::CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&ResolveFun,
                             (LPVOID)GetSafeHwnd(), CREATE_SUSPENDED, NULL);

    if(!hThread)
    {
        MessageBox(_T("创建工作线程失败。"), _T("错误"), MB_ICONERROR);
        m_ProcessState = PROCESS_STATE_IDLE;
        SetStatusText(ID_INDICATOR_PROCESS_STATE, _T("创建工作线程失败"));
        return;
    }
    else
    {
        m_Progress.SetPos(0);
        m_Progress.ShowWindow(SW_SHOW);
        AddKeywordToList();//将关键字加入到combo的列表

        //线程开始执行
        m_ProcessState = PROCESS_STATE_RUNNING;
        UPDATE_TOOLBAR_UI;
        ResumeThread(hThread);
    }
}

void CBTFileResolverView::OnActionCancel()
{
    ASSERT(m_ProcessState == PROCESS_STATE_RUNNING);

    // TODO: 停止搜索
    if(IDYES == MessageBox(_T("确定停止搜索么？已经搜索到的结果保留"),
        _T("停止搜索"), MB_ICONQUESTION + MB_YESNO + MB_DEFBUTTON2))
        bWantTerminate = TRUE;
}

void CBTFileResolverView::OnActionDelete()
{
    // TODO: 删除选择的文件
    if(!GetListCtrl().GetSelectedCount()) return;

    while(POSITION pos =
          GetListCtrl().GetFirstSelectedItemPosition())
    {
        int nPos = GetListCtrl().GetNextSelectedItem(pos);
        m_vecListItems.erase(m_vecListItems.begin() + nPos);
        GetListCtrl().DeleteItem(nPos);
    }

    UpdateFileCount();
}

void CBTFileResolverView::OnActionClear()
{
    // TODO: 清空文件列表
    if(IDYES == MessageBox(_T("清空列表内的文件？"), _T("清空"),
        MB_ICONEXCLAMATION + MB_YESNO))
    {
        m_vecListItems.clear();
        GetListCtrl().DeleteAllItems();
        UpdateFileCount();
    }
}

void CBTFileResolverView::OnUpdateActions(CCmdUI *pCmdUI)
{
    // TODO: 在此添加命令更新用户界面处理程序代码

    switch(pCmdUI->m_nID)
    {
        case ID_FILE_OPENPATH:
            pCmdUI->Enable(m_ProcessState == PROCESS_STATE_IDLE);
            break;
        case ID_ACTION_PROCESS:
            pCmdUI->Enable(m_ProcessState == PROCESS_STATE_IDLE &&
                           !m_CurrentPath.IsEmpty());
            break;
        case ID_ACTION_CANCEL:
            pCmdUI->Enable(m_ProcessState == PROCESS_STATE_RUNNING);
            break;
        case ID_ACIION_DELETE:
            pCmdUI->Enable(GetListCtrl().GetSelectedCount() != 0 &&
                           m_ProcessState == PROCESS_STATE_IDLE);
            break;
        case ID_ACTION_CLEAR:
            pCmdUI->Enable(GetListCtrl().GetItemCount() != 0 &&
                           m_ProcessState == PROCESS_STATE_IDLE);
            break;
        case ID_BT_OPEN_FILE:
            pCmdUI->Enable(GetListCtrl().GetSelectedCount() == 1);
            break;
        case ID_BT_OPEN_PATH:
            pCmdUI->Enable(GetListCtrl().GetSelectedCount() == 1);
            break;
        case ID_VIEW_ERROR:
            pCmdUI->Enable(m_ProcessState == PROCESS_STATE_IDLE);
            break;
        case ID_VIEW_DETAIL:
            pCmdUI->Enable(GetListCtrl().GetSelectedCount() == 1);
            break;
    }
}

/* 自定义消息处理 - WM_THREAD_PROCESS_DONE */
LRESULT CBTFileResolverView::OnWorkDone(WPARAM wParam, LPARAM lParam)
{
    /* 线程已经处理完毕并退出 */
    m_ProcessState = PROCESS_STATE_IDLE;
    FREE_THREAD_HANDLE(hThread);

    UPDATE_TOOLBAR_UI;
    UpdateFileCount();
    SetStatusText(ID_INDICATOR_PROCESS_STATE, _T("空闲"));
    m_Progress.ShowWindow(SW_HIDE);
    vecBTFiles.clear();

    //GetListCtrl().SetItemCountEx(m_vecListItems.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
    //original
    GetListCtrl().SetItemCountEx((int)m_vecListItems.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
    return S_OK;
}

/* 自定义消息处理 - WM_THREAD_PROCESS_RUNNING */
LRESULT CBTFileResolverView::OnProcess(WPARAM wParam, LPARAM lParam)
{
    if(!wParam && !lParam) return S_FALSE;

    if(!lParam)//当lParam为NULL时更新进度条
    {
        CString s;

        s.Format(_T("%d"), (int)wParam);//wParam是当前的进度
        SetStatusText(ID_INDICATOR_PROGRESS, s);
    }
    else//更新列表文件
    {
        PInnerFile pInnerFile = (PInnerFile)wParam;//wParam传递的是InnerFile结构的指针
        CSeedResolver* pReso = (CSeedResolver*)lParam;//lParam传递的是CBTFileResolver类的指针

        LIST_ITEM litem;
        tagForRename tagRename;

        litem.FileName = pInnerFile->FileName;
        GetFileInfo(litem.FileName, litem.FileTypeName);
        litem.FileSize = pInnerFile->FileSize;//文件大小将在OnLvnGetdispinfo中被格式化
        litem.InnerPath = pInnerFile->PathName;
        litem.BTPublisher = pReso->SeedInfo.Seed_Publisher;
        litem.BTFileName = pReso->SeedInfo.Seed_FileName;
        litem.BTCreator = pReso->SeedInfo.Seed_Creator;
        litem.BTCreationDate = pReso->SeedInfo.Seed_CreationDate;
        litem.BTComment = pReso->SeedInfo.Seed_Comment;

        tagRename.BTFileName = pReso->SeedInfo.Seed_InnerName;//zyf
        tagRename.BTInnerName = pReso->SeedInfo.Seed_InnerName;//zyf

        m_vecListItems.push_back(litem);
        m_vecRenam.push_back(tagRename);//zyf
        //GetListCtrl().SetItemCountEx(m_vecListItems.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
        //original
        GetListCtrl().SetItemCountEx((int)m_vecListItems.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);

        /* 不要过快的更新界面，更新界面是非常耗时的操作 */
        if(::GetTickCount() - dwLastUpdateUI >= UPDATE_UI_INTERVAL)
        {
            CString s;
            s.Format(_T("正在解析 %s"), pReso->SeedInfo.Seed_FileName);
            SetStatusText(ID_INDICATOR_PROCESS_STATE, s);
            UpdateFileCount();
            dwLastUpdateUI = ::GetTickCount();
        }
    }

    return S_OK;
}

void CBTFileResolverView::SetStatusText(const int nCommanddID, const CString& Text)
{
    CStatusBar* pBar = (CStatusBar*)AfxGetMainWnd()->GetDescendantWindow(AFX_IDW_STATUS_BAR);

    if(!pBar) return;

    if(ID_INDICATOR_PROGRESS != nCommanddID)
    {
        pBar->SetPaneText(pBar->CommandToIndex(nCommanddID), Text);
    }
    else
    {
        /* 更新进度条 */
        int nPos = _tstoi(Text);
        m_Progress.SetPos(nPos <= 100 ? nPos : 100);
    }
}

BOOL CBTFileResolverView::IsTorrentFile(const CString& FileName)
{
    int n = FileName.ReverseFind('.');
    if(n == -1) return FALSE;

    CString s = FileName.Mid(n);

    return s.CompareNoCase(BT_FILE_EXT_LOWER) == 0;
}

void CBTFileResolverView::FindFile(CString sPath)
{
    CString sFind;
    WIN32_FIND_DATA fdata;

    sFind = sPath + _T("*.*");
    HANDLE hFind = ::FindFirstFile(sFind, &fdata);

    if(INVALID_HANDLE_VALUE == hFind)
        return;

    while(TRUE)
    {
        //是否需要更新界面
        if(::GetTickCount() - dwLastUpdateUI >= UPDATE_PROGRESS_INTERVAL)
        {
            m_Progress.StepIt();
            SetStatusText(ID_INDICATOR_PROCESS_STATE, _T("搜索 ") + sPath);
            dwLastUpdateUI = ::GetTickCount();
        }

        if(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if(fdata.cFileName[0] != _T('.'))
            {
                FindFile(sPath + fdata.cFileName + _T("\\"));
            }
        }
        else
        {
            if(IsTorrentFile(fdata.cFileName))
                vecBTFiles.push_back(sPath + fdata.cFileName);
        }

        if(!FindNextFile(hFind, &fdata)) break;
    }

    FindClose(hFind);
}

void CBTFileResolverView::UpdateFileCount()
{
    CString s;
    s.Format(_T("文件总数：%d"), m_vecListItems.size());
    SetStatusText(ID_INDICATOR_FILE_COUNT, s);
}

void CBTFileResolverView::AddKeywordToList()
{
    if(Filter_Keyword.IsEmpty()) return;

    CComboBox* pCombo = (CComboBox*)m_wndDialogBar->GetDescendantWindow(IDC_COMBO_KEYWORD);

    if(!pCombo) return;

    if(pCombo->FindString(-1, Filter_Keyword) < 0)
        pCombo->InsertString(0, Filter_Keyword);
}

void CBTFileResolverView::OnRButtonDown(UINT nFlags, CPoint point)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    CListView::OnRButtonDown(nFlags, point);

    CMenu menu;

    menu.LoadMenu(IDR_POPUP);
    ClientToScreen(&point);
    menu.GetSubMenu(0)->TrackPopupMenu(TPM_LEFTALIGN, point.x, point.y, this);
}

void CBTFileResolverView::OnBtOpenFile()
{
    if(GetListCtrl().GetSelectedCount() != 1) return;

    POSITION pos = GetListCtrl().GetFirstSelectedItemPosition();
    int nPos = GetListCtrl().GetNextSelectedItem(pos);
    CString BT_File = m_vecListItems[nPos].BTFileName;

    if(!BT_File.IsEmpty())
        ShellExecute(::GetDesktopWindow(), _T("open"), BT_File, NULL, NULL, SW_SHOWNORMAL);
}

void CBTFileResolverView::OnBtOpenPath()
{
    if(GetListCtrl().GetSelectedCount() != 1) return;

    POSITION pos = GetListCtrl().GetFirstSelectedItemPosition();
    int nPos = GetListCtrl().GetNextSelectedItem(pos);
    CString BT_File = m_vecListItems[nPos].BTFileName;

    ExploreFile(BT_File);
}

void CBTFileResolverView::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CListView::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    if(bSysMenu) return;

    int nCount = pPopupMenu->GetMenuItemCount();
    CCmdUI cmd;

    cmd.m_pMenu = pPopupMenu;
    cmd.m_nIndexMax = nCount;

    for(int i = 0; i < nCount; i++)
    {
        UINT nID = pPopupMenu->GetMenuItemID(i);
        if(nID != ID_SEPARATOR)
        {
            cmd.m_nIndex = i;
            cmd.m_nID = nID;
            cmd.DoUpdate(this, TRUE);
        }
    }
}

void CBTFileResolverView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    CListView::OnLButtonDblClk(nFlags, point);

    OnViewDetail();
}

void CBTFileResolverView::OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult)
{
    NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码

    LVITEM* pItem = &(pDispInfo)->item;
    int iItem = pItem->iItem;


    if(pItem->mask & LVIF_TEXT)
    {
        switch(pItem->iSubItem)
        {
            case 0://文件名
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].FileName.GetString();
                break;
            case 1://大小，kb
            {
                CString FileSize = _T("");
                int nLen = 0;
                TCHAR szFileSize[64];
                _i64tot((__int64)(m_vecListItems[iItem].FileSize / 1024), szFileSize, 10);
                FileSize.Format(_T("%s"), szFileSize);
                FileSize.MakeReverse();
                int nPos = 3;
                while(nPos < FileSize.GetLength())
                {
                    FileSize.Insert(nPos, _T(','));
                    nPos += 4;
                }
                FileSize.MakeReverse();
                FileSize += _T(" KB");
                _tcscpy(pItem->pszText, FileSize.GetString());
            }
            break;
            case 2://类别名
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].FileTypeName.GetString();
                break;
            case 3://种子内路径
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].InnerPath.GetString();
                break;

                ///* 不再显示
            case 4://种子发布者
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].BTPublisher.GetString();
                break;
            case 5://种子文件名
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].BTFileName.GetString();
                break;
            case 6://种子创建工具
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].BTCreator.GetString();
                break;
            case 7://种子创建日期
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].BTCreationDate.GetString();
                break;
            case 8://种子备注
                pItem->pszText = (LPTSTR)m_vecListItems[iItem].BTComment.GetString();
                break;
                //*/
        }
    }

    if(pItem->mask & LVIF_IMAGE)
        pItem->iImage = GetFileInfo(m_vecListItems[iItem].FileName,
        m_vecListItems[iItem].FileTypeName);

    *pResult = 0;
}

void CBTFileResolverView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值

    switch(nChar)
    {
        case VK_DELETE://处理DEL键
            OnActionDelete();
            break;
    }

    CListView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CBTFileResolverView::OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码

    if(m_ProcessState == PROCESS_STATE_IDLE)
    {
        if(SortParam.nColIndex == pNMLV->iSubItem)
            SortParam.bSortAsc = !SortParam.bSortAsc;
        else
        {
            SortParam.bSortAsc = TRUE;
            SortParam.nColIndex = pNMLV->iSubItem;
        }

        //排序
        stable_sort(m_vecListItems.begin(), m_vecListItems.end(), IsLesser);
        GetListCtrl().Invalidate();
    }

    *pResult = 0;
}

//测试按钮的点击消息，在Release版本中将隐藏测试按钮
void CBTFileResolverView::OnBnClickedDebugTest()
{

}


void CBTFileResolverView::OnBnClickedButton1()
{
    // 说明
    CString Desciptions;

    Desciptions.Format(_T("类别：%s\n\n扩展名：%s\n\n包含关键字：%s\n\n文件大小：%s\n\n选择目录后立即开始：%s"),
                       _T("搜索不同种类的种子内文件，如果不能满足要求，自行指定扩展名。"),
                       _T("指定要搜索的种子内文件扩展名，指定后将忽略“类别”参数，多个扩展名以空格分隔，不要输入点，例如：txt exe dat。"),
                       _T("种子内文件名必须包含指定的关键字，多个关键字以空格分隔，例如：中国 北京。\
                          			\n            如果勾选了“同时在BT文件名中查找”，则如果种子内文件名不包含关键字，而在在BT文件名中包含也算是符合条件。"),
                                    _T("指定种子内文件的大小条件，大于或小于某数值，KB为单位。"),
                                    _T("选择要搜索的目录后立即开始搜索进程。"));

    AfxMessageBox(Desciptions, MB_ICONINFORMATION | MB_OK);
}

//错误文件对话框的回调
BOOL CALLBACK DLG_ERROR_PROC(HWND hwndDlg,
                             UINT message,
                             WPARAM wParam,
                             LPARAM lParam)
{
    switch(message)
    {
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    EndDialog(hwndDlg, wParam);
                    break;
                case IDC_LIST_ERROR_FILES:
                    if(HIWORD(wParam) == LBN_DBLCLK)
                    {
                        CListBox* pList = (CListBox*)CWnd::FromHandle(::GetDlgItem(hwndDlg, IDC_LIST_ERROR_FILES));

                        int nIndex = pList->GetCurSel();
                        if(nIndex >= 0)
                        {
                            CString FileName;
                            pList->GetText(nIndex, FileName);

                            ExploreFile(FileName);
                        }
                    }
                    break;
            }
            return TRUE;
            break;
        case WM_CLOSE:
            EndDialog(hwndDlg, wParam);
            return TRUE;
            break;
        case WM_INITDIALOG:
        {
            CListBox* pList = (CListBox*)CWnd::FromHandle(::GetDlgItem(hwndDlg, IDC_LIST_ERROR_FILES));

            CString Prompt;
            Prompt.Format(_T("文件数：%d"), vecErrorFiles.size());
            ::SetDlgItemText(hwndDlg, IDC_STATIC_ERROR_COUNT, Prompt);
            for(vector<CString>::iterator iter = vecErrorFiles.begin();
                iter != vecErrorFiles.end();
                ++iter)
            {
                pList->InsertString(0, *iter);
            }
        }
        return TRUE;
        break;
    }

    return FALSE;
}

//详细信息对话框的回调
BOOL CALLBACK DLG_DETAIL_PROC(HWND hwndDlg,
                              UINT message,
                              WPARAM wParam,
                              LPARAM lParam)
{
    switch(message)
    {
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    EndDialog(hwndDlg, wParam);
                    break;
                case IDC_BUTTON1:
                    if(HIWORD(wParam) == BN_CLICKED)
                    {
                        ExploreFile(CurrentSelItem.BTFileName);
                    }
                    break;
            }
            return TRUE;
            break;
        case WM_CLOSE:
            EndDialog(hwndDlg, wParam);
            return TRUE;
            break;
        case WM_INITDIALOG:
            ::SetDlgItemText(hwndDlg, IDC_EDIT1, CurrentSelItem.BTFileName);
            ::SetDlgItemText(hwndDlg, IDC_EDIT2, CurrentSelItem.BTCreationDate);
            ::SetDlgItemText(hwndDlg, IDC_EDIT3, CurrentSelItem.BTCreator);
            ::SetDlgItemText(hwndDlg, IDC_EDIT4, CurrentSelItem.BTPublisher);
            ::SetDlgItemText(hwndDlg, IDC_EDIT5, CurrentSelItem.BTComment);
            return TRUE;
            break;
    }

    return FALSE;
}

void CBTFileResolverView::OnViewError()
{
    //DialogBox(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_ERROR), GetSafeHwnd(), DLG_ERROR_PROC);
    //original
}

void CBTFileResolverView::OnViewDetail()
{
    if(GetListCtrl().GetSelectedCount() != 1) return;
    POSITION pos = GetListCtrl().GetFirstSelectedItemPosition();
    int nIndex = GetListCtrl().GetNextSelectedItem(pos);

    CurrentSelItem = m_vecListItems[nIndex];

    //DialogBox(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_DETAIL), GetSafeHwnd(), DLG_DETAIL_PROC);
    //original
}

void CBTFileResolverView::OnFileRenamezyf()
{
    // TODO: Add your command handler code here
    vector<tagForRename>::iterator iter;
    for(iter = m_vecRenam.begin(); iter != m_vecRenam.end(); iter++)
    {
        //_T("\\") +
        MessageBox(iter->BTInnerName);
        if(!::MoveFile(m_CurrentPath + iter->BTFileName, m_CurrentPath + iter->BTInnerName))
            MessageBox(_T("Rename Error!"));
    }
}
