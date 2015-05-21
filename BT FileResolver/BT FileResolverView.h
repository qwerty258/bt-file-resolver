// BT FileResolverView.h : CBTFileResolverView 类的接口
//


#pragma once
#include <vector>

//排序
typedef struct _SORT_PARAM
{
    BOOL bSortAsc;
    int nColIndex;
}SORT_PARAM, *PSORT_PARAM;

//listctrl的数据
typedef struct _LIST_ITEM
{
    CString FileName;
    CString FileTypeName;
    DWORDLONG FileSize;
    CString InnerPath;
    CString BTFileName;
    CString BTPublisher;
    CString BTCreator;
    CString BTCreationDate;
    CString BTComment;
}LIST_ITEM, *LPLIST_ITEM;

typedef struct tagForRename
{
    CString BTFileName;
    CString BTInnerName;
};//zyf

class CBTFileResolverView: public CListView
{
private:
    int m_ProcessState;
    CWnd* m_wndDialogBar;
    CString m_CurrentPath;
    CProgressCtrl m_Progress;

    /* 最好不要使用set，虽然排序速度快，但由于使用virtual list显示文件，读取数据时不好处理 */
    std::vector<LIST_ITEM> m_vecListItems;
    std::vector<tagForRename> m_vecRenam;//zyf

    CImageList m_ListViewIL;

    void SetStatusText(const int nCommandID, const CString& text);
    BOOL IsTorrentFile(const CString& FileName);
    void FindFile(CString sPath);
    void UpdateFileCount();
    void AddKeywordToList();

protected: // 仅从序列化创建
    CBTFileResolverView();
    DECLARE_DYNCREATE(CBTFileResolverView)

    // 属性
public:
    CBTFileResolverDoc* GetDocument() const;

    // 重写
public:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
    virtual void OnInitialUpdate(); // 构造后第一次调用

    // 实现
public:
    virtual ~CBTFileResolverView();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:

    // 生成的消息映射函数
protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnFileOpenpath();
public:
    afx_msg LRESULT OnWorkDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnProcess(WPARAM wParam, LPARAM lParam);
    afx_msg void OnActionProcess();
    afx_msg void OnActionCancel();
    afx_msg void OnUpdateActions(CCmdUI *pCmdUI);
    afx_msg void OnActionDelete();
    afx_msg void OnActionClear();
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnBtOpenFile();
    afx_msg void OnBtOpenPath();
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnBnClickedDebugTest();
    afx_msg void OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnBnClickedButton1();
    afx_msg void OnViewError();
    afx_msg void OnViewDetail();
    afx_msg void OnFileRenamezyf();
};

#ifndef _DEBUG  // BT FileResolverView.cpp 中的调试版本
inline CBTFileResolverDoc* CBTFileResolverView::GetDocument() const
{
    return reinterpret_cast<CBTFileResolverDoc*>(m_pDocument);
}
#endif

