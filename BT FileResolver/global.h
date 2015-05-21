#pragma once

#include "stdafx.h"
#include <vector>
#include <algorithm>

using namespace std;


/* 自定义消息 */
#ifndef WM_THREAD_PROCESS_RUNNING
#define WM_THREAD_PROCESS_RUNNING	(WM_USER + 100)
#endif

#ifndef WM_THREAD_PROCESS_DONE
#define WM_THREAD_PROCESS_DONE		(WM_USER + 101)
#endif

//过滤文件大小的操作符，小于或大于或未定义
typedef enum FILTER_FILE_SIZE_OPERATOR
{
    OPERATOR_MORE_THAN, OPERATOR_LESS_THAN, OPERATOR_UNDEFINE
};

/* 跨文件全局变量声明，这些变量全部在BT FileResolverView.cpp中定义 */
extern vector<CString> vecBTFiles;//文件列表，包含搜索到的种子文件
extern vector<CString> vecErrorFiles;//解析失败的文件列表
extern DWORD dwLastUpdateUI;	//最后一次更新界面的时间
extern HANDLE hThread;	//线程句柄
extern BOOL bWantTerminate;	//线程的结束标志
extern CString Filter_Type;//搜索类别过滤
extern CString Filter_Keyword;//关键字过滤
extern BOOL Filter_Keyword_BTFile;//是否在种子文件名中查找关键字
extern int Filter_Operator;//过滤文件大小的操作符
extern CString Filter_FileExt;//指定扩展名
extern DWORDLONG Filter_FileSize;//过滤文件大小

