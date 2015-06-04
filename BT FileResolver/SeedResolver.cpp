#include "StdAfx.h"
#include "SeedResolver.h"

//当解析函数抛出异常时是否中断，便于调试
//#define BREAK_ON_THROW

using namespace std;

#pragma warning(disable : 4244 4996)

CSeedResolver::CSeedResolver(const CString& SeedFileName)
{
    ASSERT(!SeedFileName.IsEmpty());

    SeedInfo.Seed_FileName = SeedFileName;
}

CSeedResolver::~CSeedResolver(void)
{
    //清理
    DeallocAll();
}

//把大写转换为小写，取得种子的编码格式时要用到
void szCharToLower(char* pszString)
{
    int i = 0;
    while(pszString[i])
    {
        pszString[i] = tolower(pszString[i]);
        ++i;
    }
}

BOOL CSeedResolver::Resolve()
{
    char* pBuffer = NULL;
    char* pShadowBuffer = NULL;

    HANDLE hFile = CreateFile(SeedInfo.Seed_FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if(hFile == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    CFile file(hFile);
    m_SeedFileSize = (UINT)file.GetLength();
    m_Position = 0;

    if(!m_SeedFileSize)
    {
        return FALSE;
    }

    pBuffer = new char[m_SeedFileSize];
    file.Read((void*)pBuffer, m_SeedFileSize);
    file.Close();

    //ResovleBuffer函数将移动指针，所以传递一个pBuffer的副本，这样便于释放pBuffer
    pShadowBuffer = pBuffer;

    LPNode pNode = NULL;

#ifndef BREAK_ON_THROW
    try
    {
#endif

        pNode = ResovleBuffer(&pShadowBuffer);

#ifndef BREAK_ON_THROW
    }
    catch(int)
    {
        SAFE_RETURN(pBuffer, FALSE);
    }
#endif	

    //清除文件缓冲区
    SAFE_CLEAN(pBuffer);

    if(pNode->Type != BC_DICT) return FALSE;
    //根必须是个bencode字典

    LPBC_Dict pRootDict = pNode->Data.bcDict;

    /* 开始取值 */

    //首先先取得种子的编码格式
    if(GetNode(pRootDict, KEYWORD_ENCODING, &pNode))
    {
        szCharToLower(pNode->Data.bcString);
        if(!strlen(pNode->Data.bcString) || strcmp(pNode->Data.bcString, "utf-8") == 0)
            SeedInfo.Seed_Encoding = CP_UTF8;
        else if(strcmp(pNode->Data.bcString, "utf-7") == 0)//utf-7
            SeedInfo.Seed_Encoding = CP_UTF7;
        else if(strcmp(pNode->Data.bcString, "gbk") == 0)//简体中文
            SeedInfo.Seed_Encoding = 936;
        else if(strcmp(pNode->Data.bcString, "big5") == 0)//繁体中文
            SeedInfo.Seed_Encoding = 950;
        else if(strcmp(pNode->Data.bcString, "shift_jis") == 0)//日文
            SeedInfo.Seed_Encoding = 932;
        else if(strcmp(pNode->Data.bcString, "windows-874") == 0)//泰文
            SeedInfo.Seed_Encoding = 874;
        else if(strcmp(pNode->Data.bcString, "ks_c_5601-1987") == 0)//韩文
            SeedInfo.Seed_Encoding = 949;
        else//其他都为utf-8
            SeedInfo.Seed_Encoding = CP_UTF8;
    }
    else
    {
        SeedInfo.Seed_Encoding = CP_UTF8;//默认为utf-8
    }

    //seed inner name for rename later zyf begin
    if(GetNode(pRootDict, KEYWORD_NAME_UTF8, &pNode) && pNode->Type == BC_STRING)
    {
        ConvertToUnicode(pNode->Data.bcString, SeedInfo.Seed_InnerName, CP_UTF8);
    }
    else if(GetNode(pRootDict, KEYWORD_NAME, &pNode) && pNode->Type == BC_STRING)
    {
        ConvertToUnicode(pNode->Data.bcString, SeedInfo.Seed_Comment);
    }
    else
    {
        SeedInfo.Seed_InnerName = _T("");
    }
    //zyf end

    //创建日期
    if(GetNode(pRootDict, KEYWORD_CREATION_DATE, &pNode) && pNode->Type == BC_INT)
    {
        CTime time(0);
        CTimeSpan ts(pNode->Data.bcInt);

        time += ts;
        SeedInfo.Seed_CreationDate.Format(_T("%d年%.2d月%.2d日 %.2d时%.2d分"), time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute());
    }
    else
    {
        SeedInfo.Seed_CreationDate = _T("(未知日期)");
    }

    //种子的备注
    if(GetNode(pRootDict, KEYWORD_COMMENT_UTF8, &pNode) && pNode->Type == BC_STRING)//首先看有没有utf-8编码的
    {
        ConvertToUnicode(pNode->Data.bcString, SeedInfo.Seed_Comment, CP_UTF8);
    }
    else if(GetNode(pRootDict, KEYWORD_COMMENT_UTF8, &pNode) && pNode->Type == BC_STRING)//如果没有utf-8编码的就使用内置编码的
    {
        ConvertToUnicode(pNode->Data.bcString, SeedInfo.Seed_Comment);
    }
    else
    {
        SeedInfo.Seed_Comment = _T("(没有备注)");
    }

    //种子的创建工具
    if(GetNode(pRootDict, KEYWORD_CREATED_BY, &pNode) && pNode->Type == BC_STRING)
    {
        ConvertToUnicode(pNode->Data.bcString, SeedInfo.Seed_Creator, CP_UTF8);
    }
    else
    {
        SeedInfo.Seed_Creator = _T("(未知)");
    }

    //种子的发布者
    if(GetNode(pRootDict, KEYWORD_PUBLISHER_UTF8, &pNode) &&//首先看有没有utf-8编码的
       pNode->Type == BC_STRING)
    {
        ConvertToUnicode(pNode->Data.bcString, SeedInfo.Seed_Publisher, CP_UTF8);
    }
    else if(GetNode(pRootDict, KEYWORD_PUBLISHER, &pNode) &&//如果没有utf-8编码的就使用内置编码的
            pNode->Type == BC_STRING)
    {
        ConvertToUnicode(pNode->Data.bcString, SeedInfo.Seed_Publisher);
    }
    else
    {
        SeedInfo.Seed_Publisher = _T("(未知)");
    }

    //读取种子内的所有文件
    LPBC_Dict pInfoDict = NULL;//info字典

    if(!GetNode(pRootDict, KEYWORD_INFO, &pNode) || pNode->Type != BC_DICT) return FALSE;

    pInfoDict = pNode->Data.bcDict;
    pNode = NULL;

    GetNode(pInfoDict, KEYWORD_FILES, &pNode);

    //如果pNode为NULL，即没有找到关键KEYWORD_FILES，那么这是个单文件种子，否则为多文件种子或者无效
    if(!pNode)
    {
        InnerFile iFile;
        int nCodePage = 0;

        //单文件模式info字典的name关键字是文件名
        if(GetNode(pInfoDict, KEYWORD_NAME_UTF8, &pNode))
            nCodePage = CP_UTF8;
        else if(GetNode(pInfoDict, KEYWORD_NAME, &pNode))
            nCodePage = 0;
        else
            return FALSE;

        if(pNode->Type != BC_STRING) return FALSE;

        ConvertToUnicode(pNode->Data.bcString, iFile.FileName, nCodePage);
        iFile.PathName = _T("\\");

        //文件大小
        if(!GetNode(pInfoDict, KEYWORD_LENGTH, &pNode) || pNode->Type != BC_INT)
            return FALSE;
        else
            iFile.FileSize = pNode->Data.bcInt;

        SeedInfo.Seed_Files.push_back(iFile);
    }
    else if(pNode->Type == BC_LIST)
    {
        /* 这里的代码有点不太容易看懂，我额外定义了pFileDict和pPathList两个变量
         * 使代码清晰些，只要记住LPBC_Dict是指向BC_Dict结构的指针，而LPBC_List是
         * 指向Node结构的指针的指针*/
        LPBC_List pFileList = pNode->Data.bcList;//info字典内的files列表

        while(*pFileList)
        {
            if((*pFileList)->Type != BC_DICT) return FALSE;//每个列表项目必须都是字典类型

            LPBC_Dict pFileDict = (*pFileList)->Data.bcDict;

            InnerFile iFile;
            int nCodePage = 0;

            //确定代码页，并取得path列表
            if(GetNode(pFileDict, KEYWORD_PATH_UTF8, &pNode))
                nCodePage = CP_UTF8;
            else if(GetNode(pFileDict, KEYWORD_PATH, &pNode))
                nCodePage = 0;
            else
                return FALSE;

            if(pNode->Type != BC_LIST) return FALSE;//path必须是列表

            LPBC_List pPathList = pNode->Data.bcList;
            vector<CString> vecTemp;//临时存储

            while(*pPathList)
            {
                if((*pPathList)->Type != BC_STRING) return FALSE;//path的每个项目必须是字符串

                CString s;

                ConvertToUnicode((*pPathList)->Data.bcString, s, nCodePage);
                vecTemp.push_back(s);

                ++(pPathList);
            }

            //还要对vecTemp进行解析，分解出文件名和目录
            if(vecTemp.front().Find(_T(BITCOMET_PADDING_FILE_PREFIX)) != 0)//不解析BitComet的内嵌文件
            {
                iFile.FileName = vecTemp.back();
                vecTemp.pop_back();
                for(vector<CString>::iterator iter = vecTemp.begin(); iter != vecTemp.end(); ++iter)
                {
                    iFile.PathName += *iter + _T("\\");
                }
                if(iFile.PathName.IsEmpty()) iFile.PathName = _T("\\");


                //取得文件大小
                if(!GetNode(pFileDict, KEYWORD_LENGTH, &pNode) ||
                   pNode->Type != BC_INT)
                   return FALSE;
                else
                    iFile.FileSize = pNode->Data.bcInt;

                //将iFile加入SeedInfo.Seed_Files
                SeedInfo.Seed_Files.push_back(iFile);
            }

            ++pFileList;
        }
    }
    else
        return FALSE;

    return TRUE;
}

LPNode CSeedResolver::ResovleBuffer(char** pBuffer)
{
    Node* pNode = NULL;

    switch(**pBuffer)
    {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            pNode = (LPNode)malloc(sizeof(Node));

            pNode->Type = BC_STRING;
            pNode->Data.bcString = GetBCString(pBuffer);//不用保存GetBCString返回的指针，GetBCString会决定是否保存这个指针
            vecAllocated.push_back((INT_PTR)pNode);

            if(!pNode->Data.bcString) throw NULL;//种子文件有错误
            break;
        case BENCODE_PREFIX_INT:
            pNode = (LPNode)malloc(sizeof(Node));

            pNode->Type = BC_INT;
            pNode->Data.bcInt = GetBCInt(pBuffer);
            vecAllocated.push_back((INT_PTR)pNode);

            if(pNode->Data.bcInt == _I64_MAX) throw NULL;//种子文件有错误
            break;
        case BENCODE_PREFIX_LIST:
        {
            //跳过开头的BENCODE_PREFIX_LIST
            ++(*pBuffer);
            ++m_Position;

            pNode = (LPNode)malloc(sizeof(Node));

            pNode->Type = BC_LIST;
            pNode->Data.bcList = NULL;
            vecAllocated.push_back((INT_PTR)pNode);

            UINT i = 0;

            while(**pBuffer != BENCODE_SUFFIX)
            {
                pNode->Data.bcList = (LPBC_List)realloc((void*)pNode->Data.bcList, (i + 2)/*多分配一个元素存放NULL*/ * sizeof(BC_List));

                try
                {
                    pNode->Data.bcList[i] = ResovleBuffer(pBuffer);
                }
                catch(INT)
                {
                    vecAllocated.push_back((INT_PTR)pNode->Data.bcList);
                    throw NULL;
                }

                ++i;
            }

            /* 上面那个循环在种子文件有错误或者是个空列表的情况下一次都不执行，抛出异常 */
            if(!i)
                throw NULL;
            else
                vecAllocated.push_back((INT_PTR)pNode->Data.bcList);

            /* bcList是个指针的动态数组（BC_List实际是LPNode），由于无法确定动态数组的长度，在上面为bcList分配空间的时候，
             * 多分配了一个元素的空间，并且为这个元素赋值NULL，这就如同标准c字符串一样，以NULL
             * 结尾，当遍历这个动态数组的时候，读到NULL就认为这个数组结束了*/
            pNode->Data.bcList[i] = NULL;

            //跳过结尾的BENCODE_SUFFIX
            ++(*pBuffer);
            ++m_Position;
        }
        break;
        case BENCODE_PREFIX_DICT:
        {
            //对于bencode字典的解析可参考上面的对bencode列表的解析
            ++(*pBuffer);
            ++m_Position;

            pNode = (LPNode)malloc(sizeof(Node));
            pNode->Type = BC_DICT;
            pNode->Data.bcDict = NULL;
            vecAllocated.push_back((INT_PTR)pNode);

            UINT i = 0;
            while(**pBuffer != BENCODE_SUFFIX)
            {
                pNode->Data.bcDict = (LPBC_Dict)realloc((void*)pNode->Data.bcDict, (i + 2) * sizeof(BC_Dict));

                try
                {
                    pNode->Data.bcDict[i].pszKey = GetBCString(pBuffer);
                    if(!pNode->Data.bcDict[i].pszKey) throw NULL;//GetBCString不会抛出异常，所以必须检查返回值
                    pNode->Data.bcDict[i].pNode = ResovleBuffer(pBuffer);
                }
                catch(INT)
                {
                    vecAllocated.push_back((INT_PTR)pNode->Data.bcList);
                    throw NULL;
                }

                ++i;
            }

            if(!i)
                throw NULL;
            else
                vecAllocated.push_back((INT_PTR)pNode->Data.bcList);

            /* 字典和上面处理列表有点不一样，bcDict的数组元素不是指针，所以就把结尾元素的pszKey付NULL，
             * 这样当遍历bcDict数组时，读到某个元素的Key是NULL时认为结束 */
            pNode->Data.bcDict[i].pszKey = NULL;

            ++(*pBuffer);
            ++m_Position;
        }
        break;
        default:
            //遇到了无效的bencode前缀，直接抛出异常
            throw NULL;
            break;
    }



    return pNode;
}
void CSeedResolver::DeallocAll()
{
    for(vector<INT_PTR>::iterator iter = vecAllocated.begin(); iter != vecAllocated.end(); ++iter)
    {
        free((void*)*iter);
    }
}

BOOL CSeedResolver::IsRangeValid(UINT nOffset /* = 0 */)
{
    return m_Position + nOffset <= m_SeedFileSize;
}

char* CSeedResolver::GetBCString(char** pBuffer)
{
    char* pszRet = NULL;
    char* pszStrLen;
    UINT nLen = 0;

    //bencode字符串必须以0-9的数字开始
    if(**pBuffer < '0' || **pBuffer >'9')
        return pszRet;

    while(*(*pBuffer + nLen) != BENCODE_STRING_DELIMITER)
    {
        ++nLen;

        //检查是否越界
        if(!IsRangeValid(nLen))	return pszRet;
    }

    //nLen为bcstring前导的表示bcstring长度的字符个数
    pszStrLen = new char[nLen + 1];
    ZeroMemory(pszStrLen, nLen + 1);
    memcpy((void*)pszStrLen, (void*)*pBuffer, nLen);

    //检查是否越界
    if(!IsRangeValid(nLen + 1)) return pszRet;

    //移动指针
    *pBuffer += nLen + 1/* 1为':' */;
    m_Position += nLen + 1;

    //nLen这时为bcstring的长度
    nLen = atoi(pszStrLen);
    SAFE_CLEAN(pszStrLen);

    if(!nLen)//nLen=0是个空字符串
    {
        pszRet = (char*)malloc(sizeof(char));
        pszRet[0] = '\0';
        vecAllocated.push_back((INT_PTR)pszRet);
    }
    else
    {
        //检查是否越界
        if(!IsRangeValid(nLen)) return pszRet;

        pszRet = (char*)malloc((nLen + 1) * sizeof(char));
        memset((void*)pszRet, NULL, nLen + 1);
        vecAllocated.push_back((INT_PTR)pszRet);
        memcpy((void*)pszRet, (void*)*pBuffer, nLen);
    }

    //移动指针
    *pBuffer += nLen;
    m_Position += nLen;

    return pszRet;
}

DWORDLONG CSeedResolver::GetBCInt(char** pBuffer)
{
    DWORDLONG dwlRet = _I64_MAX;
    char* psz;
    UINT nLen = 0;

    //bencode整数必须以i开始
    if(**pBuffer != BENCODE_PREFIX_INT)
        return dwlRet;

    //检查是否越界
    if(!IsRangeValid(1)) return dwlRet;

    ++(*pBuffer);
    ++m_Position;

    while(m_Position <= m_SeedFileSize && *(*pBuffer + nLen) != BENCODE_SUFFIX)
    {
        ++nLen;

        //检查是否越界
        if(!IsRangeValid(nLen)) return dwlRet;
    }

    psz = new char[nLen + 1];
    ZeroMemory(psz, nLen + 1);
    memcpy((void*)psz, (void*)*pBuffer, nLen);
    if(*psz == '0' && nLen == 1)//0是bencode有效的数字
        dwlRet = 0;
    else
        dwlRet = _atoi64(psz) ? _atoi64(psz) : _I64_MAX;
    SAFE_CLEAN(psz);

    //检查是否越界
    if(!IsRangeValid(nLen + 1)) return _I64_MAX;

    //移动指针
    *pBuffer += nLen + 1/* 1为'e' */;
    m_Position += nLen + 1;

    return dwlRet;
}

BOOL CSeedResolver::GetNode(LPBC_Dict pDict, const char* pszKey, _Out_ LPNode* pNode)
{
    if(!pDict) return FALSE;

    while(pDict->pszKey)//用pszKey判断动态数组是否结束
    {
        if(strcmp(pDict->pszKey, pszKey) == 0)
        {
            *pNode = pDict->pNode;
            return TRUE;
        }

        ++pDict;
    }

    return FALSE;
}

void CSeedResolver::ConvertToUnicode(const char* pStr, CString& s, int nCodePage /*= 0*/)
{
    wchar_t* szUnicode = NULL;
    UINT nLen;
    int nCP;

    if(!pStr) return;

    /* 这种方法并不好，应该自动检测编码，我对字符的编码解码不熟，只能这样，暂时没有发现有乱码的*/
    if(!nCodePage)//没有指定代码页，使用种子内定义的代码页
        nCP = SeedInfo.Seed_Encoding;
    else
        nCP = nCodePage;

    nLen = MultiByteToWideChar(nCP, NULL, pStr, (int)strlen(pStr) + 1, NULL, NULL);
    szUnicode = new wchar_t[nLen];
    MultiByteToWideChar(nCP, NULL, pStr, (int)strlen(pStr) + 1, szUnicode, nLen);

    s.Format(_T("%s"), szUnicode);

    SAFE_CLEAN(szUnicode);
}
