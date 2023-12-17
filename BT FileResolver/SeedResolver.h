/*
 * SeedResolver.h 版本：2.0
 *
 * 作者：bluekitty(不若人生一场醉) @ CSDN 2012-01
 *
 * 说明：
 * 本头文件声明了类 CSeedResolver，用于解析BT种子文件，种子文件是完全bencode编码的字节串文件，关于
 * bencode编码方式和种子文件的关键字参看http://wiki.theory.org/BitTorrentSpecification。
 * 整个种子文件内容是一个bencode的字典，包含多个关键字，可以把一个种子文件看成是一个多叉树结构，很
 * 显然，递归是解析种子文件比较直接的方法。
 *
 * 附加说明：
 * 在源代码目录的STL目录内有 CSeedResolver 类的另一种实现，使用标准库的map和vector，暂称为1.0版，
 * 1.0版与2.0版功能基本一样，但1.0版存在某些问题。
 * 根据bencode编码的定义，我首先想到的就是使用标准库的map实现bencode字典，用vector实现bencode列表，
 * 实际测试发现，
 * 如果map直接保存节点（定义为Node）对象，会非常消耗内存，解析一个有6000多个文件的种子时使用内存在
 * 70M左右，而且由于需要复制对象和插入操作，虽然map的插入操作优化后速度非常快，但在有大量数据时仍然
 * 显得时间开销很大，还是那个6000多文件的种子，解析完成要14秒左右。
 * 如果map保存一个节点的指针，虽然解析速度和内存消耗都有很大的改善，但分配的内存很不好释放，当解析了
 * 一个错误的种子文件时，必然出现内存泄漏，由于水平有限，我也没有找到解决的办法，也许定义map的allocator
 * 可以解决内存释放的问题，但我实在是对stl的内存分配器不是很熟悉。如果可以解决内存释放的问题，那么使用
 * 标准库还是不错的，一是代码比较清晰易懂，二是标准库有丰富的算法，没准以后就用得到（但在本程序内用不到，
 * 本程序只是读取种子的信息和内嵌文件，不做其他数据处理）。
 *
 */

#pragma once

#include <vector>

#define SAFE_CLEAN(p) if(p)    \
	                  delete p;\
	                  p = NULL;
#define SAFE_RETURN(p, ret) { SAFE_CLEAN(p);return ret; }

/* bencode的前后缀还有string的分隔符 */
#define BENCODE_PREFIX_INT			'i'
#define BENCODE_PREFIX_LIST			'l'
#define BENCODE_PREFIX_DICT			'd'
#define BENCODE_SUFFIX				'e'
#define BENCODE_STRING_DELIMITER	':'

/* 种子内BitComet的内嵌文件前缀，忽略这些文件 */
#define BITCOMET_PADDING_FILE_PREFIX "_____padding_file"

/* 种子文件的关键字，种子文件整个是一个bencode字典类型，即这个字典的关键字，除非有特别说明，否则所有的字符串值都是utf-8编码
 * 忽略了一些关键字，完整的关键字列表参看说明http://wiki.theory.org/BitTorrentSpecification， 就是定义的这些关键字也不是都用得到*/
#define KEYWORD_ANNOUNCE		"announce"
//bencode字符串，tracker服务器url，根据官方解释，如果存在announce-list关键字则应该忽略这个关键字

#define KEYWORD_ANNOUNCE_LIST	"announce-list"
//bencode列表的列表，非必要关键字，tracker服务器列表，announce-list列表内仍是多个列表，每个列表包含一个或多个url字符串

#define KEYWORD_CREATION_DATE	"creation date"
//bencode整数，非必要关键字，种子的创建时间,是自1970-1-1 00:00:00 UTC 所经过的秒数

#define KEYWORD_COMMENT			"comment"
//bencode字符串，非必要关键字，种子的备注

#define KEYWORD_COMMENT_UTF8	"comment.utf-8"
//bencode字符串,这个关键字我从官方查不到，可能是BitComet扩展的关键字，如果存在这个关键字就忽略上面那个，因为这个一定是UTF-8编码的

#define KEYWORD_CREATED_BY		"created by"
//bencode字符串，非必要关键字，创建该种子文件的软件和其版本，形式是这样 uTorrent/183B，表示uTorrent 183B版创建

#define KEYWORD_ENCODING		"encoding"
/*bencode字符串，非必要关键字，官方解释是种子文件info字典内pieces段的字符串所使用的编码方式，
 *但实际发现该处定义的编码方式同样会影响info字典内的所有字符串，不知是什么情况 */

#define KEYWORD_PUBLISHER		"publisher"	
//bencode字符串，种子的发布者，这个关键字我从官方也查不到，还有publisher-url关键字

#define KEYWORD_PUBLISHER_UTF8	"publisher.utf-8"
//同KEYWORD_COMMENT_UTF8

#define KEYWORD_INFO			"info"
//bencode字典，包含种子内的所有文件，具体看说明http://wiki.theory.org/BitTorrentSpecification

/* 下面这些关键字是info字典的关键字 */
#define KEYWORD_INFO_PIECE_LENGTH	"piece length"
/* bencode整数，每一块的长度，单位字节，官方建议是524288即512k字节，但可能有不同。*/

#define KEYWORD_INFO_PIECES			"pieces"
/* bencode字符串，每个块的杂凑值（SHA1，固定20字节长）的组合，所以这个串可能很长，
 * 官方说明“byte string, i.e. not urlencoded”，显然这个串的长度必须是20的倍数，也可通过
 * 这个串的长度除以20计算一共有多少块。
 * 注意，这个字符串并不是标准的c字符串，它的任何位置都可能是null字符，字符串函数不能适用于该字符串 */

#define KEYWORD_INFO_PRIVATE		"private"
/* bencode整数，非必要关键字，实际这个可以看成是一个BOOL，当为1时，表示只通过种子内的服务器得到peers，
 * 当为0或未设置时，可以从外部获得peers，例如DHT网络 */

/* 以下关键字是和文件相关的，具体看说明http://wiki.theory.org/BitTorrentSpecification */
#define KEYWORD_NAME		"name"//单文件模式是文件名，多文件模式是建议性的文件根目录
#define KEYWORD_NAME_UTF8	"name.utf-8"//同KEYWORD_COMMENT_UTF8
#define KEYWORD_FILES		"files"//文件列表
#define KEYWORD_PATH		"path"//一个列表，最后一项是文件名，前面的是目录名
#define KEYWORD_PATH_UTF8	"path.utf-8"//同KEYWORD_COMMENT_UTF8
#define KEYWORD_LENGTH		"length"//文件长度

/* bencode的类型名称 */
enum BC_TYPE_NAME
{
    BC_STRING, BC_INT, BC_LIST, BC_DICT
};

struct _Node;//树的节点声明，定义在下面

/* bencode的类型 */
typedef char*			BC_String;
typedef DWORDLONG		BC_Int;
typedef struct _Node*	BC_List, **LPBC_List;
typedef struct _BC_Dict
{
    char* pszKey;
    struct _Node* pNode;
} BC_Dict, *LPBC_Dict;


/* 树节点定义，看起来和一般树节点的定义不大一样，参看上面的
 * BC_Dict和BC_List的声明，实际这仍是一个递归定义 */
typedef struct _Node
{
    UINT Type;

    union
    {
        BC_String	bcString;
        BC_Int		bcInt;
        LPBC_List	bcList;
        LPBC_Dict	bcDict;
    } Data;

} Node, *LPNode;

/* 种子内的文件 */
typedef struct _Inner_File
{
    /* 路径名 */
    CString PathName;

    /* 文件名 */
    CString FileName;

    /* 文件大小，单位是字节 */
    DWORDLONG FileSize;

    /* 文件类型，种子文件不直接提供，自己解析，见BT FileResolverView.cpp文件中定义的GetFileInfo函数 */
    CString FileType;

    _Inner_File()
    {
        FileSize = 0;
    }
} InnerFile, *PInnerFile;

/* 这个结构包含本程序要用到的所有数据，种子内的其他数据用不到。
 * 其实解析过程还可以优化，直接跳过不需要的种子数据，但没有实现，
 * 一是懒，二是以后有可能用到，所以还是保留完全解析 */
typedef struct _Values_Needed
{
    CString Seed_FileName;
    CString Seed_CreationDate;
    CString Seed_Comment;
    CString Seed_Creator;
    CString Seed_Publisher;
    int		Seed_Encoding;//CodePage

    CString Seed_InnerName;//zyf add

    std::vector<InnerFile> Seed_Files;//种子内所有文件的列表
} Values_Needed;

class CSeedResolver
{
private:
    UINT m_SeedFileSize;//种子文件的大小
    UINT m_Position;//当前指针在文件缓冲区的位置

    /* 保存所有用malloc或realloc分配的指针，析构时释放这些指针，
     * 递归解析种子文件时所有内存的分配都使用malloc或realloc，
     * 这样便于统一释放，解析过程以外的还是都使用new分配，使用
     * 宏SAFE_CLEAN或SAFE_RETURN释放 */
    std::vector<INT_PTR> vecAllocated;

    /* 检查是否越界(超过文件缓冲区范围) */
    BOOL IsRangeValid(UINT nOffset = 0);

    /* 取得一个bencode字符串(标准c字符串) */
    char* GetBCString(char** pBuffer);

    /* 取得一个bencode整数 */
    DWORDLONG GetBCInt(char** pBuffer);

    /* 解析指定的缓冲区，这是个递归函数 */
    LPNode ResovleBuffer(char** pBuffer);

    /* 从pDict得到一个指定关键字的节点（LPNode*参数），返回值为该关键字是否存在
     * 不能将pDict参数声明为const，因为要移动pDict指针 */
    BOOL GetNode(LPBC_Dict pDict, const char* pszKey, _Out_ LPNode* pNode);

    /* 释放解析过程中分配的所有内存 */
    void DeallocAll();

    /* 标准c字符串 -> Unicode，nCodePage = 0表示使用种子内定义的代码页 */
    void ConvertToUnicode(const char* pStr, CString& s, int nCodePage = 0);

public:
    Values_Needed SeedInfo;

    CSeedResolver(const CString& SeedFileName);
    ~CSeedResolver(void);

    /* 构造后必须首先调用该函数解析，根据返回值确定是否解析成功 */
    BOOL Resolve();

};
