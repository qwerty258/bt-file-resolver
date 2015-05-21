// BT FileResolverDoc.cpp : CBTFileResolverDoc 类的实现
//

#include "stdafx.h"
#include "BT FileResolver.h"

#include "BT FileResolverDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CBTFileResolverDoc

IMPLEMENT_DYNCREATE(CBTFileResolverDoc, CDocument)

BEGIN_MESSAGE_MAP(CBTFileResolverDoc, CDocument)
END_MESSAGE_MAP()


// CBTFileResolverDoc 构造/析构

CBTFileResolverDoc::CBTFileResolverDoc()
{
    // TODO: 在此添加一次性构造代码

}

CBTFileResolverDoc::~CBTFileResolverDoc()
{
}

BOOL CBTFileResolverDoc::OnNewDocument()
{
    if(!CDocument::OnNewDocument())
        return FALSE;

    // TODO: 在此添加重新初始化代码
    // (SDI 文档将重用该文档)

    return TRUE;
}




// CBTFileResolverDoc 序列化

void CBTFileResolverDoc::Serialize(CArchive& ar)
{
    if(ar.IsStoring())
    {
        // TODO: 在此添加存储代码
    }
    else
    {
        // TODO: 在此添加加载代码
    }
}


// CBTFileResolverDoc 诊断

#ifdef _DEBUG
void CBTFileResolverDoc::AssertValid() const
{
    CDocument::AssertValid();
}

void CBTFileResolverDoc::Dump(CDumpContext& dc) const
{
    CDocument::Dump(dc);
}
#endif //_DEBUG

