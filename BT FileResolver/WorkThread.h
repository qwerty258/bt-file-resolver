#pragma once

#include "stdafx.h"
#include "global.h"
#include "SeedResolver.h"

BOOL CheckFilter(const CString& FileName, const CString& BTFileName, const DWORDLONG& FileSize)
{
    if(FileName.IsEmpty()) return FALSE;

    CString BTName, Name, Ext;
    CString CurrentKeywords = Filter_Keyword;//保存副本，CurrentKeywords会被修改
    BOOL bPassKeyword = FALSE;
    BOOL bPassType = FALSE;
    BOOL bPassSize = FALSE;

    int nPos = FileName.ReverseFind(_T('.'));

    BTName = BTFileName;
    if(nPos == -1)
    {
        Ext = _T("");
        Name = FileName;
    }
    else
    {
        Ext = FileName.Mid(nPos + 1);
        Name = FileName.Mid(0, nPos);
    }
    Ext.MakeUpper();
    Name.MakeUpper();
    BTName.MakeUpper();

    switch(Filter_Operator)
    {
        case OPERATOR_UNDEFINE:
            bPassSize = TRUE;
            break;
        case OPERATOR_LESS_THAN:
            bPassSize = FileSize <= Filter_FileSize * 1024;
            break;
        case OPERATOR_MORE_THAN:
            bPassSize = FileSize >= Filter_FileSize * 1024;
            break;
        default:
            bPassSize = FALSE;
    }

    if(Filter_FileExt == _T(" "))//未指定扩展名，不要检测IsEmpty()，因为一定会在结尾添加一个空格
        bPassType = Filter_Type.IsEmpty() ? TRUE : Ext.IsEmpty() ? FALSE : Filter_Type.Find(Ext + _T(";")) >= 0;
    else
        bPassType = Ext.IsEmpty() ? FALSE : Filter_FileExt.Find(Ext + _T(" ")) >= 0;

    if(!CurrentKeywords.IsEmpty())
    {
        while(!CurrentKeywords.IsEmpty())
        {
            CString Keyword = CurrentKeywords.SpanExcluding(_T(" "));

            if(Name.Find(Keyword) >= 0 ||
               (Filter_Keyword_BTFile && BTName.Find(Keyword) >= 0)/* 在种子文件名中查找 */)
            {
                bPassKeyword = TRUE;
                break;
            }

            CurrentKeywords.Delete(0, Keyword.GetLength() + 1);
        }
    }
    else
        bPassKeyword = TRUE;

    //只有关键字、类型、大小都满足条件的才返回TRUE
    return bPassKeyword && bPassType && bPassSize;

}

DWORD WINAPI ResolveFun(LPVOID lpParam /* 主调的窗口类句柄 */)
{
    int nPos = 0;
    //int nFileCount = vecBTFiles.size();
    //original
    int nFileCount = (int)vecBTFiles.size();

    for(vector<CString>::const_iterator iter = vecBTFiles.begin(); iter != vecBTFiles.end(); ++iter)
    {
        CSeedResolver resolver(*iter);

        if(resolver.Resolve())
        {
            vector<InnerFile> vecFiles;

            for(vector<InnerFile>::iterator iter = resolver.SeedInfo.Seed_Files.begin();
                iter != resolver.SeedInfo.Seed_Files.end();
                ++iter)
            {
                if(bWantTerminate)	break;
                if(CheckFilter(iter->FileName, resolver.SeedInfo.Seed_FileName, iter->FileSize))
                    ::SendMessage((HWND)lpParam, WM_THREAD_PROCESS_RUNNING, (WPARAM)&(*iter), (LPARAM)&resolver);
            }
        }
        else
            vecErrorFiles.push_back(*iter);

        if(bWantTerminate)	break;
        ::SendMessage((HWND)lpParam, WM_THREAD_PROCESS_RUNNING, (WPARAM)(int)(++nPos * 100 / nFileCount), NULL);
    }

    ::SendMessage((HWND)lpParam, WM_THREAD_PROCESS_DONE, NULL, NULL);
    return 0;
}
