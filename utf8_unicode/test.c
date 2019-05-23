#include <stdlib.h>
#include <util.h>
#include "src/util_string.h"
#include "inc_layers.h"
#include "sconv.h"

/*
	MAP_PHY_PART_VIEW = {
		MAP_PART_INFO	(SIZE_BLK)	+ 
		MAP_PART_SYS_VAR	(SIZE_BLK * 8)+ 
		saFATStart
	}

	MAP_PART_SYS_VAR = {
		SUB_MAP_INIT_VARS + 
		SUB_MAP_FORMAT_VARS +
		SUB_MAP_API_VARS +
		SUB_MAP_LOW_FILESYS_VARS
	 }
*/
/*
本文件系统将 从 THIS_PART_FAT_START开始到最后的区域用化分为（SIZE_FAT_UNIT+SIZE_BLK）大小的单元，
一个SIZE_BLK就是我们链表指向的一块区域的大小，而一个SIZE_FAT_UNIT就是指向该区域的链表元素
GetFileBlocks函数就是用来获取 FAT数据区可分成多长的链表（元素个数）来表示，也就是链表长度

整个文件系统由以下几部分构成
	1. MAP_PART_INFO （分区管理信息）
	2. MAP_PART_SYS_VAR （系统参数区）
	3. FAT内容（包含链表），里面有一皯文件系统中重要的宏参数
		THIS_PART_VALIDFAT_SIZE 表示该链表占用的空间，化整(即SIZE_BLK的倍数THIS_PART_FAT_SIZE来表示
		3.1 链表占用大小为 THIS_PART_VALIDFAT_SIZE
		3.2 链表占用实际空间为 THIS_PART_FAT_SIZE
	4. DATA区域点可分配的BLK数也就是链表长度 THIS_FILEBODY_BLK_NUM
	THIS_PART_FAT_START
		------------------------------------------------------------------
		| MAP_PART_INFO | MAP_PART_SYS_VAR |
		------------------------------------------------------------------

	5. 正常的链表顺序从 0 到 THIS_FILEBODY_BLK_NUM-1， 每个元素都+2
	   即： 2 到 THIS_FILEBODY_BLK_NUM + 1， 但最后一个元素也就是下标为THIS_FILEBODY_BLK_NUM-1的元素为0（指向GND）也就是
	   [2, ..., THIS_FILEBODY_BLK_NUM, 0]

	6. 先看文件系统初始化和格式化流程
		6.1 FS_PART_Init() 这个是分区初始化
			该函数主要是初始化 MAP_PART_INFO （大小SIZE_BLK）这个部分，其中
			baPartSize		记录当前分区的大小
			baStartAddr		当前分区的起始地址（初始化化为0，不知道后面有没有用到）
		6.2 OS_PART_FATFormat() 这个是对文件系统进行格式化，格式化后文件系统就可以使用了，首先
			初始化 SUB_MAP_LOW_FILESYS_VARS 这个字段，主要是初始化
			ba1stFreeBlkNum 值为第一个有效的块（V_1st_VALID_BLOCK_NUM = 1）
			baNowFreeBlkNum 值为第一个有效的块（V_1st_VALID_BLOCK_NUM = 1）
			baFreeBlks 值为THIS_FILEBODY_BLK_NUM，表示当前可用的空闲块大小。
			这里有用到一个函数 FS_FAT_LowSysVarWtRd（），该函数主要是用来写均衡的，我们来分析一下这个函数：
			MAP_PART_SYS_VAR 中有一个unLowFilePages是一个4个SUB_MAP_LOW_FILESYS_VARS大小的数组，当写一个SUB_MAP_LOW_FILESYS_VARS信息的时候，会先在
			数组中查找其中标识bOwnerIsMe 为 V_YEAH_ITSME的那一个元素。如果没有找到（也就是说从来没有写过这部分数据的时候）从0下下标开始，如果有找到
			则找到数组的下一个元素（循环找），将新的信息写到该区域。并将上一次记录的数组擦除掉（当上一次记录的位置和本次写入的位置不同的时候）。就这样循环。

			然后对链表进行初始化，这个已经在第5点中提到过了。

     7. 初始化和格式化后我们就可以对文件进行操作了，首先我们要创建文件（目录），都是调用的 API_File_CreateFile这个函数来完成的。
	    这个函数的使用就不说了，直接分析：
		根据文件的 bFileType 类型和baFileLen文件的大小来分配合适的并记录，比如我创建一个目录，只需要一个BLK就行了， wFileSize变量直接为0，
		所以我们要求 SIZE_BLK的大小一定要比MAP_API_FILEHEAD（FIELHEAD_SIZE 32字节）大才行。
		函数 API_File_GetFreeSpace 用来当前可用的空间（转化为字节单位），获取当前可用的空间就要读SUB_MAP_LOW_FILESYS_VARS这个结构了，所以这个结构会经常的进行
		读写，才会在前面做了一个写均衡的处理，这个获取空间主要是获取 baFreeBlks的值我们说了这个值初始化为THIS_FILEBODY_BLK_NUM。这里用 if (wFileSize >= API_File_GetFreeSpace())
		就保证了至少会有一个BLK的余量，在写DF的时候是需要一个BLK的。

		然后就调用到 FS_File_Create这个函数
		    FS_Chain_Create用来获取指定大小的链表（FS_Chain操作是文件块分配和释放的基本操作单元），我们来分析这个函数。这里先读取一个SUB_MAP_LOW_FILESYS_VARS数据。
			然后找到 baNowFreeBlkNum（当前空闲链表的头， 这个值初始化为1），然后再更新 baFreeBlks为分配后的数值（为减去当前要分配的个数后的值），然后从链表头位置（baNowFreeBlkNum）开始
			通过FAT_ReadUnit(), 这个函数来扫描链表。这个函数主要用来读取下标为指定数据的数组数据内容，下标从1开始。相当于一个数组
			chain[THIS_FILEBODY_BLK_NUM] = {2, 3, ..., THIS_FILEBODY_BLK_NUM, 0}; 共 THIS_FILEBODY_BLK_NUM个。FAT_ReadUnit(n, &p)相当于 *p = chain[n-1]. 比如说我们要分配两个块，当前baNowFreeBlkNum
			值为起始值1,则我们读取两次得到值为3，第一次 chain[1-1] = 2 作为下一次的参数， 第二次 chan[2-1] = 3;
			然后再更新 baNowFreeBlkNum值为3.
			然后调用 FAT_WriteUnit(a, &b)将上一个下标单元接到0，表示一个新的链表结束，这样整个链表就会变成
			chain[THIS_FILEBODY_BLK_NUM] = {2, 0, ..., THIS_FILEBODY_BLK_NUM, 0};所以当再次扫描第一个链表的时候从1开始得到结束为0的数据的时候，共扫描了两次。也就是说第一个链表长度为2个。
			以上分析的链表分配过程，现在得了想要的新链表，已经通过FS_Chain_Create的第一个参数返回了。

			继续分析FS_File_Create，将新分配的链表数据关连到新文件描述符（H_FILE）的mapLowSys->baMyHeadBlk中(H_FILE中的mapLowSys 和 系统中的mapLowSys不是同一个结构体，注意一下)
			继续初始化文件中的mapLowSys将baBodyStartBlk先置成空，然后用同样的方式为baBodyStartBlk赋值（baBodyStartBlk）

			然后再更新与父目录的绑定关系：
			如果没有父目录则将mapLowSys的baParentHeadBlk 指向空（我理解指向空应该就是属于根目录吧）
			如果有父目录则将 mapLowSys的baParentHeadBlk 指向父目录的baMyHeadBlk
			再设置标志mapLowSys->bRFU为0XFF。最后对文件头进行校验，然后将数据写入到mapLowSys->baMyHeadBlk代表的数据块中。
			调用的函数： FS_File_RewriteHead，这个函数有必要分析一下，对文件的读写都用到类似的机制：其中最主要的函数是FS_Blk_WtData。
			FS_Blk_WtData(wBlkNum, wStartBytesOffset, *pbDataToDo, wBytesToDo)。
			第一个参数就是要写入的分配出来的块号，第二个参数为这个块中的偏移位置，第三个参数为要定入的原始数据，第四个参数为写入的数据长度。
			而这个函数只适用于某一个SIZE_BLK数据区的块进行写入。
			根据之前的分析，链表数组里面的内容在第一次初始化后会乱掉，比如会形成
			{2,4, 3, 0, 5, ...}， 比如说这次传进来的参数为n，这个函数的做法就是找到 THIS_PART_FILEBODY_START中偏移为
			（n-1）*SIZE_BLK + wStartBytesOffset的地方进行写入。读取也是从这个地址取出数据来，所以说这个传入参数其实表示的是THIS_PART_FILEBODY_START中的第(n-1)个BLK。
			从分配的角度来说，分配出来的blknum就相当于指定了哪块数据BLK。而这个数据又是链表数组中的某一个元素的值。链表中的数据在一开始时是逐个增加的，也就是说
			第0个数组元素2指向了FILE_BODY中的第2-1块，第1个元素3指向第3-1个块... 第 THIS_FILEBODY_BLK_NUM-1个元素（也就是数组的最后一个元素）值为0不指向任何块。而THIS_FILEBODY_BLK_NUM-2个元素值THIS_FILEBODY_BLK_NUM指向的是
			THIS_FILEBODY_BLK_NUM-1个块。那第0个块是由V_1st_VALID_BLOCK_NUM（值为1）来指向。则总共刚好THIS_FILEBODY_BLK_NUM。 这个没错。
			这里有一个疑问就是，当数组分配多个链表后就会有很多个NULL。假设这个NULL元素之前的值为m,那它指向的是第m-1个块，那现在变成NULL了，是否意味着这个m-1块就没人来指向了呢？
			带着这个疑问，我们有两个问题分析：
			1. 当要写入的数据大于SIZE_BLK时是怎么操作的（应该要用到链表）
			2. 当要删除一个链表时是怎么操作的，还能不能把之前丢失的m-1号块找回来。
			3. 多个子文件指向同一个父目录，那怎么通过父目录来找到里面所有的子文件？
			这些都与链表的工作原理相关。

			我们先继续看代码@FS_File_RewriteHead()函数之后又做了什么处理。先前我们说了，如果父目录不存在，则将自己的parmHeadBlk置成空，如果存在，则指向父目录的baMyHeadBlk。
			所以当父目录存在的时候，后续又做了一个处理：这里又分两种情况：
			1. 如果父目录的 baBodyStartBlk == 0，应该是表示父目录下没有子文件，
			   这时，只需要将父目录的baBodyStartBlk指向自己的baMyHeadBlk头就好了，
			2. 如果父目录的 baBodyStartBlk不存在，已经指向一个文件的头，那么它会调用 FS_Chain_Merge来将两个链表合并，这个函数非常重要，我们先分析它
			   每个文件头中有3个系统相关信息
			   1. baMyHeadBlk 表示本文件的头信息是放在哪个块中
			   2. baBodyStartBlk 表示本文件的内容是放在哪个块中
			   3. baParentHeadBlk 表示本文件的父目录的头是在哪个块中（baParentHeadBlk 其实就是 parent->baMyHeadBlk）

			   在这里，我们要回过头来把链表详细的理解一遍。
				  我们先看一个链表操作的另一个重要方法：
				  FS_Chain_Merge(chain1, chain2); 这个函数的作用就是把两个链表 chain1 和chain2变成一个 chain2->chain1的新链表。
				  FS_Chain_GetLen就是来对链表操作进行遍历的函数，像C语言中常用的单向链表一个，每个链表节点中总有一个next指向的是下一个节点的地址。而我们这个就用下标来代表地址。
				  我们的链表就是一个单向的，只有一个指针元素的单向链表。所以
				  FS_Chain_GetLen(n, &blk_num)参数说明： n 表示要搜索的链表起始元素地址， blk_num为返回的最后一个元素的地址，最后一个元素的值指向 NULL。而返回值就是这个链表中元素的个数。
				  FS_Chain_Seek的分析和上面的一要，只是FS_Chain_Seek主要是用来遍历多少个元素。
				  这里，链表元素也代表了它指向的BODY_BLK，最后个元素指向地，也不指向任何BLK，比如说链表 [...,0, ...]，它能管理多少个BLK空间呢，其实是1个，虽然它的值不指向任何一个地址，
				  但别忘记了，总有一个元素指向这个链表头，比如我们之前提到的，baMyHeadBlk，baBodyStartBlk，baParentHeadBlk这些，他们的指向这个链表的时候就指向了这个链表的地址，我们假设为 m，
				  则这个链表管理的文件块是第m号元素。所以每个链表元素管理的文件地是与这个元素本声的地址相关，而不是值相关。这点有点像container_of。说了这么多，整个链表的设计过程也就清晰了，我们还有
				  一全局的指针指向空闲链表的头，而非空闲的链表就被文件头里面的 baMyHeadBlk， baBodyStartBlk指向了。所以整个链表数据还是被管理着的。

			   FS_Chain_Merge(MAKEWORD_BYBYTES(phFileToCreate->mapLowSys.baMyHeadBlk), MAKEWORD_BYBYTES(phParent->mapLowSys.baBodyStartBlk))。所以这里是把phParent->baBodyStartBlk链表延长了。包含了新创建文件
			   的baMyHeadBlk。所以在一个目录下寻找子文件或文件夹就是通过父目录的 baBodyStartBlk来的主要使用的函数为FS_File_Enum， 因为每个文件的baMyHeadBlk不会大于一个BLK，所以枚举一个链表就相当于扫描了一个文件。
			   链表的长度就是子文件或目录的个数。
			   问题点： 对于 FS_Chain_Merge函数中有一个问题没看明白当w2ndChainStartBlk == NULL时表示要释放，而此时系统的baNowFreeBlkNum又为0。说明系统的空闲链表应该为空，此时设置系统的baNowFreeBlkNum为要释放的链表头，
			   这步没问题，但把链表头直接指向END，随后wEndNum = w1stStartBlk;而这个变量后面又不用，这个过程没想明白，通过这个操作之后，又获取这个链表的长度应该就是1了，然后再更新系统的空闲长度，
			   难到此时要释放的链表一定只有一个元素吗？

		8. 文件的读写：
		   API_File_RdWt(is_read, pFile, offset, data, data_size)
		   1. is_read第一个参数是读还是写，1为读，0为写
		   2. pFile，文件描述符
		   3. offset，文件的偏移处
		   4. 要写或读取的长度
		   调用的是 FS_Chain_BlkRdWt这个函数，之前有分析过。主要参数为文件的baBodyStartBlk指向的链表代表的数据开始读取。主要跨多个块，则需要读取下一个块的地址然后再写入或读取。
*/
#define	CARD_STATE_UNKNOWN		0x00
#define	CARD_STATE_NO_PART		0x01

#define	CARD_STATE_NEED_INIT	0x11
#define	CARD_STATE_NEED_FORMAT	0x12
#define	CARD_STATE_MF_OK		0x14
#define	CARD_STATE_CARD_BLOCK	0x15
#define	CARD_STATE_MF_BLOCK		0x16
#define CARD_STATE_MF_NULL		0x17


#define GENERAL_INFO_FILE_ID 	0x0002
#define TOKEN_DIR_FILE_ID		0x0002
#define SESSION_KEY_ID			0x0003	


#define KID_LEN		4

V_APPRET APP_File_GetMF(OUT PMAP_API_FILEHEAD pMF);
void debug_MAP_PART_INFO_info(void);
void debug_MAP_PART_SYS_VAR_info(void);
BYTE APP_Reset(void);
void debug_THIS_PART_FAT(void);
void test_format(void);
void debug_MAP_API_FILEHEAD(MAP_API_FILEHEAD *pFile);
void cmd_ls_dir(MAP_API_FILEHEAD *df);
void debug_general_info(void);
void print_file(MAP_API_FILEHEAD *mapFile, BYTE level);
void debug_all_fs(void);
void check_chain(void);
int basic_init(void);
void debug_account(void);

static 	MAP_API_FILEHEAD mapCurrentMF;

void re_init_fs(void)
{
//	V_APIRET result;
	FS_PART_Init();
	OS_PART_FATFormat();
#if 0
	mapCurrentMF.mapHead.mapDF.bFileType = 0;
	mapCurrentMF.mapHead.mapDF.baFileId[0] = 0x3f;
	mapCurrentMF.mapHead.mapDF.baFileId[1] = 0x00;

	mapCurrentMF.mapHead.mapDF.bACL_Create = 0xfe;
	mapCurrentMF.mapHead.mapDF.bACL_Del = 0xfe;
	result = API_File_CreateFile(NULL, &mapCurrentMF);
	if (result != ERR_OK)
	{
		return ;
	}
#endif
}

int start_test(void)
{
	BYTE fs_state;
	// flash_tmp_buf
//	MAP_API_FILEHEAD parentDF, mapFile;
	if (host_is_little_endian()) {
		irq_printf("host is litte endian\n");
	}else {
		irq_printf("host is big endian\n");
	}
	debug_MAP_PART_INFO_info();
	debug_THIS_PART_FAT();
	check_chain();
//	re_init_fs();
//	basic_init();
//	check_chain();
//	return 0;
	debug_account();
	return 0;
	fs_state = APP_Reset();
	debug("fs state %02X\n", fs_state);
	debug_MAP_PART_SYS_VAR_info();
	debug("Deug MF\n");

	debug_MAP_API_FILEHEAD(&mapCurrentMF);
//	cmd_ls_dir(&mapCurrentMF);
	debug_all_fs();
	check_chain();
	return 0;
}
V_APPRET APP_File_GetMF
(
	OUT PMAP_API_FILEHEAD pMF
)
{
	V_APPRET ret;
	ret = FS_File_Enum(NULL, 1, (H_FILE)pMF);
	if (ret == ERR_OK)
	{
		if (ITS_DF(pMF))
		{
			return FS_File_HeadChkByte(true, (BYTE*)pMF);
		}
		return FILE_NOT_FOUND;
	}
	return ret;
}


void debug_MAP_PART_INFO_info(void)
{
	MAP_PART_INFO mapPartInfo;
	debug("MAP_PART_INFO size %d bytes\n", sizeof(MAP_PART_INFO));
	PART_RdData(THIS_PART_START_ADDR, (BYTE*)&mapPartInfo, sizeof(MAP_PART_INFO));
	ibug("This part size is %#x bytes\n", u32_from_big_endian(mapPartInfo.baPartSize));
	ibug("start address at %#x \n", u32_from_big_endian(mapPartInfo.baStartAddr));
}

void debug_MAP_PART_SYS_VAR_info(void)
{
//	MAP_PART_SYS_VAR
	debug("MAP_PART_SYS_VAR size %d\r\n", sizeof(MAP_PART_SYS_VAR));
	/* We just concern about SUB_MAP_LOW_FILESYS_VARS*/
	SUB_MAP_LOW_FILESYS_VARS mapLowFileSys;
	FS_FAT_LowSysVarWtRd(true, &mapLowFileSys);
	ibug("THIS_FILEBODY_BLK_NUM is %d\n", THIS_FILEBODY_BLK_NUM);
	ibug("ba1stFreeBlkNum is %d\n", u16_from_big_endian(mapLowFileSys.ba1stFreeBlkNum));
	ibug("baNowFreeBlkNum is %d\n", u16_from_big_endian(mapLowFileSys.baNowFreeBlkNum));
	ibug("baFreeBlks is %d\n", u16_from_big_endian(mapLowFileSys.baFreeBlks));
}


void debug_THIS_PART_FAT(void)
{
	debug("FAT 起始地址为 %#X\r\n", THIS_PART_FAT_START);
	debug("FAT 可容纳 %d 个数据 BLK(64K)大小\n", THIS_FILEBODY_BLK_NUM);
	debug("FAT 链表实际占用 %d 字节\n", THIS_PART_FAT_SIZE);
	debug("FAT 数据区理论上为 %d 字节\n", THIS_PART_SIZE - SYS_VAR_SIZE - THIS_PART_FAT_SIZE);
	debug("FAT 数据区起始地址为 %#X\n", THIS_PART_FILEBODY_START);
	debug("宏 NUM_FAT_UNIT_OF_BLK 表示一个 BLK可放%d个链表\n", (SIZE_BLK / SIZE_FAT_UNIT));
}

V_APIRET app_read_file(IN PMAP_API_FILEHEAD pFile, IN WORD wOffset, OUT BYTE *pRetData, IN OUT WORD *pwBytesToDo)
{
	return !API_File_RdWt(true, pFile, wOffset, pRetData, pwBytesToDo);
}

V_APIRET app_write_file(IN PMAP_API_FILEHEAD pFile, IN WORD wOffset, IN BYTE *pInData, IN OUT WORD *pwBytesToDo)
{
	return !API_File_RdWt(false, pFile, wOffset, pInData, pwBytesToDo);
}
void test_format(void)
{
	FS_PART_Init();
	OS_PART_FATFormat();
}

BYTE APP_Reset(void)
{
	WORD RAM_FIELD_I wFlag;
	BYTE format_flag;
	FS_PART_InitVarsRd(offsetof(SUB_MAP_INIT_VARS, baCardInitFlags), (BYTE*)&wFlag, CARD_INIT_FLAG_LEN);
	big_endian_to_host((BYTE*)&wFlag, 2);
	irq_debug("baCardInitFlags = %X\r\n", wFlag);
	if (wFlag != (V_CARD_INIT_OK_WORD)) {
		return CARD_STATE_NEED_INIT;
	}
	FS_PART_FormatVarsWtRd(true, offsetof(SUB_MAP_FORMAT_VARS, bCardFormatFlag), &format_flag, 1);
	if (CARD_FORMAT_DONE != format_flag)
	{
		return  CARD_STATE_NEED_FORMAT;
	}
	if (ERR_OK != APP_File_GetMF(&mapCurrentMF))
	{
		return CARD_STATE_MF_NULL;
	}
	return CARD_STATE_MF_OK;
}

void debug_MAP_API_FILEHEAD(MAP_API_FILEHEAD *pFile)
{
	WORD file_len = 0;
	BYTE file_type = pFile->mapHead.mapComm.bFileType;
	ibug("-------------------------- Debug %s File --------------------------------\n", ITS_DF(pFile) ? "DF": "EF");
	ibug("bFileType:0x%02X\n", file_type);
	if (ITS_DF(pFile)) {
		ibug("This is DF file\n");
	}
	if (ITS_EF(pFile)) {
		ibug("This is EF file\n");
	}
	if (IS_STRUCT_BIN(pFile)) {
		ibug("This is BIN file\n");
	}
	if (file_type == FILETYPE_PRIKEY) {
		ibug("This is FILETYPE_PRIKEY file\n");
	}
	ibug("File ID: %02X %02X\n", pFile->mapHead.mapComm.baFileId[0], pFile->mapHead.mapComm.baFileId[1]);
	ibug("bRight1: %02X ,bRight2 %02X\n", pFile->mapHead.mapComm.bRight1, pFile->mapHead.mapComm.bRight2);

	if (IS_STRUCT_BIN(pFile)) {
		file_len = MAKEWORD(pFile->mapHead.mapBIN.baFileLen[0], pFile->mapHead.mapBIN.baFileLen[1]);
	}
	else if (IS_STRUCT_FIXREC(pFile)) {
		file_len = (REC_HEAD_LEN) * ((WORD)pFile->mapHead.mapRec.bRecMaxCount);
	}
	else if (IS_STRUCT_CYCLREC(pFile)) {
		file_len = (REC_HEAD_LEN) * ((WORD)pFile->mapHead.mapRec.bRecMaxCount);
	}
	else if (IS_STRUCT_VARREC(pFile))
	{
		//var rec 
		file_len = MAKEWORD(pFile->mapHead.mapVarRec.baFileLen[0], pFile->mapHead.mapVarRec.baFileLen[1]);
	}
	else if (!ITS_DF(pFile)){
		//pending
		ebug("file type unknow\n");
	}
	ibug("File len = %d bytes\n", file_len);
	ibug("bChkByte is %02X\n", pFile->mapLowFileSys.bChkByte);
	
	ibug("baMyHeadBlk at addr %d, USE %d blocks\n", (pFile->mapLowFileSys.baMyHeadBlk[0] << 8) | pFile->mapLowFileSys.baMyHeadBlk[1], \
		FS_Chain_GetLen(MAKEWORD_BYBYTES(pFile->mapLowFileSys.baMyHeadBlk), NULL));

	ibug("baBodyStartBlk at addr %d, USE %d blocks\n", (pFile->mapLowFileSys.baBodyStartBlk[0] << 8) | pFile->mapLowFileSys.baBodyStartBlk[1], \
		FS_Chain_GetLen(MAKEWORD_BYBYTES(pFile->mapLowFileSys.baBodyStartBlk), NULL));

	ibug("baParentHeadBlk at addr %d, USE %d blocks\n", (pFile->mapLowFileSys.baParentHeadBlk[0] << 8) | pFile->mapLowFileSys.baParentHeadBlk[1], \
		FS_Chain_GetLen(MAKEWORD_BYBYTES(pFile->mapLowFileSys.baParentHeadBlk), NULL));
	ibug("------------------------------------------------------------------\n");

}

void debug_with_level(BYTE level)
{
	BYTE prefix_len = 4;
	while (level--) {
		while (prefix_len--)
			ibug(" ");
	}
}

void cmd_ls_dir(MAP_API_FILEHEAD *pParent)
{
	MAP_API_FILEHEAD iter;
	WORD subfiles_num;
	BYTE bIndex = 1, ret;

	if (ITS_DF(pParent)) {
		subfiles_num = API_File_GetDFSubFileNum(pParent);
		ibug("++++++++++++++++ LS Get %d sub files +++++++++++++++++++++++++\n", subfiles_num);
		while (1) {
			ret = FS_File_Enum((H_FILE)pParent, bIndex, (H_FILE)&iter);
			if (ret != ERR_OK)
			{
				break;
			}
			debug_MAP_API_FILEHEAD(&iter);
			bIndex++;
		}
		
	}
	else {
		ebug("this if not a DF file, type %02X\n", pParent->mapHead.mapComm.bFileType);
	}
	
}

void debug_general_info(void)
{
	MAP_API_FILEHEAD mapFile;
	if (false == API_File_Find(&mapCurrentMF, GENERAL_INFO_FILE_ID, FILETYPE_BIN, 1, NULL, &mapFile)) {
		ibug("brief not exit\r\n");
		return ;
	}
	debug_MAP_API_FILEHEAD(&mapFile);
}

void print_file(MAP_API_FILEHEAD *mapFile, BYTE level)
{
	WORD file_len;
	if (ITS_EF(mapFile)) {
		if (IS_STRUCT_BIN(mapFile)) {
			file_len = MAKEWORD(mapFile->mapHead.mapBIN.baFileLen[0], mapFile->mapHead.mapBIN.baFileLen[1]);
		}else if (IS_STRUCT_FIXREC(mapFile)) {
			file_len = (REC_HEAD_LEN) * ((WORD)mapFile->mapHead.mapRec.bRecMaxCount);
		}
		else if (IS_STRUCT_CYCLREC(mapFile)) {
			file_len = (REC_HEAD_LEN) * ((WORD)mapFile->mapHead.mapRec.bRecMaxCount);
		}else if (IS_STRUCT_VARREC(mapFile))
		{
			//var rec 
			file_len = MAKEWORD(mapFile->mapHead.mapVarRec.baFileLen[0], mapFile->mapHead.mapVarRec.baFileLen[1]);
		}
		else if (!ITS_DF(mapFile)) {
			//pending
			debug_with_level(level);
			ebug("file type unknow\n");
		}

		BYTE *pRetData = (BYTE *)malloc(file_len);
		if (pRetData == NULL) {
			debug_with_level(level);
			ebug("file len %d alloc fail\n", file_len);
			return;
		}
		API_File_RdWt(true, mapFile, 0, pRetData, &file_len);
		debug_with_level(level);
		ibug("Debug EF ID %02X %02X\n", mapFile->mapHead.mapBIN.baFileId[0], mapFile->mapHead.mapBIN.baFileId[1]);
		debug_data("EF", pRetData, file_len);
		free(pRetData);
	}
	else {
		ibug("this is a DF file\r\n");
	}
}

void debug_df(MAP_API_FILEHEAD *df, BYTE level)
{
	MAP_API_FILEHEAD iter;
	V_APIRET ret;
	WORD bIndex = 1;

	debug_with_level(level);
	ibug("level %d has %d sub files\n", level, API_File_GetDFSubFileNum(df));
	debug_with_level(level);
	ibug("Debug DF ID %02X %02X\n", df->mapHead.mapComm.baFileId[0], df->mapHead.mapComm.baFileId[1]);
	while (1) {
		ret = FS_File_Enum((H_FILE)df, bIndex, (H_FILE)&iter);
		if (ret != ERR_OK)
		{
			break;
		}
		if (ITS_DF(&iter)) {
//			cmd_ls_dir(&iter);
			debug_df(&iter, level + 1);
		}
		else {
			print_file(&iter, level);
		}
		bIndex++;
	}
}

void debug_all_fs(void)
{
	ibug("---------------------- debug al fs ------------------------\r\n");
	debug_df(&mapCurrentMF, 0);
}


uint32_t check_df_chain(MAP_API_FILEHEAD *df, BYTE level)
{
	MAP_API_FILEHEAD iter;
	V_APIRET ret;
	WORD bIndex = 1;
	uint32_t chain_size = 1; // 1 is df`s baMyHeadBlk
	if (FS_File_HeadChkByte(1, (BYTE*)df) != ERR_OK) {
		return 0;
	}
	while (1) {
		ret = FS_File_Enum((H_FILE)df, bIndex, (H_FILE)&iter);
		if (ret != ERR_OK)
		{
			break;
		}
		if (ITS_DF(&iter)) {
			chain_size += check_df_chain(&iter, level + 1);
		}
		else {
			if (FS_File_HeadChkByte(1, (BYTE*)&iter) != ERR_OK) {
				ebug("DIR ID%02X %2X, file ID %02X 02X head check wrong\n", df->mapHead.mapComm.baFileId[0], df->mapHead.mapComm.baFileId[1], iter.mapHead.mapComm.baFileId[0], iter.mapHead.mapComm.baFileId[1]);
			}
			chain_size += FS_Chain_GetLen(MAKEWORD_BYBYTES(iter.mapLowFileSys.baBodyStartBlk), NULL);
			chain_size++; // add ef`s head block
		}
		bIndex++;
	}
	return chain_size;
}

void check_chain(void)
{
	WORD free_num;
	SUB_MAP_LOW_FILESYS_VARS mapLowFileSys;
	FS_FAT_LowSysVarWtRd(true, &mapLowFileSys);
	if (ERR_OK != APP_File_GetMF(&mapCurrentMF)) {
		ebug("CHECK Not found MF file\n");
		return;
	}
//	debug_MAP_PART_SYS_VAR_info();
	uint32_t all_chain_len =check_df_chain(&mapCurrentMF, 0);
//	u16_from_big_endian(mapLowFileSys.baNowFreeBlkNum);
	free_num = MAKEWORD_BYBYTES(mapLowFileSys.baFreeBlks);
	if (free_num != FS_Chain_GetLen(MAKEWORD_BYBYTES(mapLowFileSys.baNowFreeBlkNum), NULL)) {
		ebug("check free num error\n");
	}
	all_chain_len += free_num;
	
	if (all_chain_len != THIS_FILEBODY_BLK_NUM) {
		ebug("Check chain error, shoud be %d , but get %d\n", THIS_FILEBODY_BLK_NUM, all_chain_len);
	}
	else {
		ibug("\n\n===================== check chain OK all %u ================================\n\n", all_chain_len);
	}
}


int basic_init(void)
{
	V_APIRET result;
	uint8_t key_nums;
	WORD len;

	MAP_API_FILEHEAD parentDF, mapFile;

#ifdef TOKEN_DIR_FILE_ID
	MAP_API_FILEHEAD tokenDF;
#endif

	memset(&parentDF, 0, sizeof(parentDF));
	if (ERR_OK != APP_File_GetMF(&parentDF)) {
		parentDF.mapHead.mapDF.bFileType = 0;
		parentDF.mapHead.mapDF.baFileId[0] = 0x3f;
		parentDF.mapHead.mapDF.baFileId[1] = 0x00;

		parentDF.mapHead.mapDF.bACL_Create = 0xfe;
		parentDF.mapHead.mapDF.bACL_Del = 0xfe;
		result = API_File_CreateFile(NULL, &parentDF);
		if (result != ERR_OK)
		{
			return ERR_NO_APP_DF;
		}
	}
	
#ifdef TOKEN_DIR_FILE_ID	
	// Create token dir ... 	

	memset(&tokenDF, 0, sizeof(tokenDF));

	tokenDF.mapHead.mapDF.bFileType = FILETYPE_DF;
	tokenDF.mapHead.mapDF.baFileId[0] = TOKEN_DIR_FILE_ID >> 8;
	tokenDF.mapHead.mapDF.baFileId[1] = TOKEN_DIR_FILE_ID & 0xff;
	tokenDF.mapHead.mapDF.bACL_Create = 0xfe;
	tokenDF.mapHead.mapDF.bACL_Del = 0xfe;
	result = API_File_CreateFile(&parentDF, &tokenDF);
	if (result != ERR_OK)
	{
		return result;
	}
	memset(&mapFile, 0, sizeof(mapFile));

	mapFile.mapHead.mapBIN.bFileType = FILETYPE_BIN;

	mapFile.mapHead.mapBIN.baFileId[0] = GENERAL_INFO_FILE_ID >> 8;
	mapFile.mapHead.mapBIN.baFileId[1] = GENERAL_INFO_FILE_ID & 0xff;

	mapFile.mapHead.mapBIN.bReadRight = 0x10;
	mapFile.mapHead.mapBIN.bWriteRight = 0x10;

	len = 300;
	mapFile.mapHead.mapBIN.baFileLen[0] = LOBYTE(len);
	mapFile.mapHead.mapBIN.baFileLen[1] = HIBYTE(len);
	result = API_File_CreateFile(&tokenDF, &mapFile);
	if (result != ERR_OK)
	{
		return result;
	}
	// Set default value.
#endif	

	if (false == API_File_Find(&parentDF, GENERAL_INFO_FILE_ID, FILETYPE_BIN, 1, NULL, &mapFile)) {
		// Create general info file .	
		memset(&mapFile, 0, sizeof(mapFile));
		mapFile.mapHead.mapBIN.bFileType = FILETYPE_BIN;

		mapFile.mapHead.mapBIN.baFileId[0] = GENERAL_INFO_FILE_ID >> 8;
		mapFile.mapHead.mapBIN.baFileId[1] = GENERAL_INFO_FILE_ID & 0xff;

		mapFile.mapHead.mapBIN.bReadRight = 0x10;
		mapFile.mapHead.mapBIN.bWriteRight = 0x10;

		mapFile.mapHead.mapBIN.baFileLen[0] = LOBYTE(256);
		mapFile.mapHead.mapBIN.baFileLen[1] = HIBYTE(256);
		result = API_File_CreateFile(&parentDF, &mapFile);
		if (result == ERR_OK) {
			key_nums = 0;		 //set the default key_num is 0;
			len = sizeof(key_nums);
//			app_write_file(&mapFile, 0, &key_nums, &len);
		}
		else {
			return result;
		}
	}

	if (false == API_File_Find(&parentDF, SESSION_KEY_ID, FILETYPE_BIN, 1, NULL, &mapFile)) {
		// Create general info file .	
		memset(&mapFile, 0, sizeof(mapFile));
		mapFile.mapHead.mapBIN.bFileType = FILETYPE_BIN;

		mapFile.mapHead.mapBIN.baFileId[0] = SESSION_KEY_ID >> 8;
		mapFile.mapHead.mapBIN.baFileId[1] = SESSION_KEY_ID & 0xff;

		mapFile.mapHead.mapBIN.bReadRight = 0x10;
		mapFile.mapHead.mapBIN.bWriteRight = 0x10;

		mapFile.mapHead.mapBIN.baFileLen[0] = LOBYTE(256);
		mapFile.mapHead.mapBIN.baFileLen[1] = HIBYTE(256);
		result = API_File_CreateFile(&parentDF, &mapFile);
	}
	return result;

}


#pragma  pack (push,1) 


struct kid_item
{
	WORD coin_type; // btc or etc
	WORD which;
	uint8_t kid[KID_LEN];
};

struct key_data
{
	uint8_t key_type;
	uint8_t mnemonic_len; // not include '\0'
	union {
		uint8_t seed[64];
		uint8_t priv_key[32];
	}data;
	char mnemonic[240]; // only used in HD
};

struct keys_attr
{
	uint8_t path_len;
	uint8_t  kId[KID_LEN];
	uint8_t keyType;	// HD or not ()如果是HD的钱包private key保存seed
	uint16_t coinType;
	uint32_t create_time;
	uint8_t detail_len;
	uint8_t detail[];   // 这里面按序放描述信息和路径长度及路径信息
};
struct general_info
{
	uint8_t keys_num;
	struct kid_item items[]; // just a ....
};
#pragma  pack (pop) 



WORD get_ef_len(MAP_API_FILEHEAD *mapFile)
{
	WORD file_len = 0;
	if (ITS_EF(mapFile)) {
		if (IS_STRUCT_BIN(mapFile)) {
			file_len = MAKEWORD(mapFile->mapHead.mapBIN.baFileLen[0], mapFile->mapHead.mapBIN.baFileLen[1]);
		}
		else if (IS_STRUCT_FIXREC(mapFile)) {
			file_len = (REC_HEAD_LEN) * ((WORD)mapFile->mapHead.mapRec.bRecMaxCount);
		}
		else if (IS_STRUCT_CYCLREC(mapFile)) {
			file_len = (REC_HEAD_LEN) * ((WORD)mapFile->mapHead.mapRec.bRecMaxCount);
		}
		else if (IS_STRUCT_VARREC(mapFile))
		{
			//var rec 
			file_len = MAKEWORD(mapFile->mapHead.mapVarRec.baFileLen[0], mapFile->mapHead.mapVarRec.baFileLen[1]);
		}
	}
	return file_len;
}


void debug_file_head(MAP_API_FILEHEAD *pFile)
{
	WORD file_len;
	file_len = get_ef_len(pFile);
	ibug("file len :%d ", file_len);
	ibug("File ID: %02X %02X\n", pFile->mapHead.mapComm.baFileId[0], pFile->mapHead.mapComm.baFileId[1]);
}

int utf8_to_gbk(const char *utf8str, int slen, char *outbuf, int osize)
{
	int len;
	// first get length
	len = sconv_utf8_to_unicode(utf8str, slen, NULL, 0);
	wchar *ucode = (wchar *)malloc(len + 1);
	len = sconv_utf8_to_unicode(utf8str, slen, ucode, len);
	ucode[len] = 0;

	len = sconv_unicode_to_gbk(ucode, -1, NULL, 0);
	len = sconv_unicode_to_gbk(ucode, len, outbuf, osize);
	outbuf[len] = 0;
	free(ucode);
	return len;
}

void debug_attr_file(MAP_API_FILEHEAD *pFile, WORD f_id)
{
	uint8_t attr_buff[300];
	struct keys_attr *attr = (struct keys_attr *)attr_buff;
	memset(attr_buff, 0, sizeof(attr_buff));
	WORD len;
	uint8_t get_path_len;
	len = sizeof(struct keys_attr);
	app_read_file(pFile, 0, (BYTE *)attr, &len);
	len = sizeof(struct keys_attr) + attr->detail_len + attr->path_len + 1;
	app_read_file(pFile, 0, (BYTE *)attr, &len);
	big_endian_to_host(&attr->coinType, 2);
	big_endian_to_host(&attr->create_time, 4);
	get_path_len = attr->detail[attr->detail_len];
	attr->detail[attr->detail_len] = 0;
	ibug("----------- ATTR FILE (ID %04X)----------\n", f_id);
	ibug("path len = %d %d\n", attr->path_len, get_path_len);
	ibug("key type %d\n", attr->keyType);
	ibug("coin type %02X\n", attr->coinType);
	ibug("create time %u\n", attr->create_time);
	char gbk_buf[100];
	utf8_to_gbk(attr->detail, strlen(attr->detail), gbk_buf, sizeof(gbk_buf));
	printf("name; %s\n", gbk_buf);
	irq_debug_data("Detail", attr->detail, attr->detail_len);
	printf("Path:%s\n", &attr->detail[attr->detail_len + 1]);
}

void debug_key_file(MAP_API_FILEHEAD *pFile, WORD f_id)
{
	struct key_data key;
	WORD len;
	len = sizeof(key);
	app_read_file(pFile, 0, (BYTE *)&key, &len);
	ibug("----------- ACCOUNT FILE (ID %04X)----------\n", f_id);
	if (key.key_type == 1) {
		ibug("HD account mnemonic len = %d\n%s\n", key.mnemonic_len, key.mnemonic);
		irq_debug_data("Seed", key.data.seed, 64);
	}
	else if (key.key_type == 0) {
		ibug("Normal account\n");
		irq_debug_data("Key", key.data.priv_key, 32);
	}
	else {
		ebug("Unknow key type\n");
	}

}

void debug_account(void)
{
	MAP_API_FILEHEAD parentDF, generlFile, attrFile, keyFile;
	uint8_t general_buff[512], attr_buff[300];
	struct general_info *general = (struct general_info *)general_buff;
	struct keys_attr *attr = (struct keys_attr *)attr_buff;
	uint8_t keys_num;
	WORD len;
	int i;

	if (ERR_OK != APP_File_GetMF(&parentDF)) {
		ebug("MF not found\n");
		return;
	}

	if (false == API_File_Find(&parentDF, GENERAL_INFO_FILE_ID, FILETYPE_BIN, 1, NULL, &generlFile)) {
		ebug("brief not exit\r\n");
		return;
	}
	debug_file_head(&generlFile);
	len = sizeof(keys_num);
	app_read_file(&generlFile, 0, &keys_num, &len);
	len = sizeof(general->keys_num) + keys_num * sizeof(struct kid_item);
	// Reuse mapKeyFile
	app_read_file(&generlFile, 0, (BYTE *)general, &len);

	debug("all %d account\n", general->keys_num);
	for (i = 0; i < general->keys_num; i++) {
		big_endian_to_host(&general->items[i].coin_type, 2);
		big_endian_to_host(&general->items[i].which, 2);

		ibug("coint_type %04X\n", general->items[i].coin_type);
		ibug("file_id %04X\n", general->items[i].which);
		irq_debug_data("KID", general->items[i].kid, KID_LEN);

		if (false == API_File_Find(&parentDF, general->items[i].which, FILETYPE_BIN, 1, NULL, &attrFile)) {
			ebug("Unexceptable attr file not exists\r\n");
			return;
		}

		if (false == API_File_Find(&parentDF, general->items[i].which, FILETYPE_PRIKEY, 1, NULL, &keyFile)) {
			ebug("Unexceptable key file not exists\r\n");
			return;
		}
		debug_attr_file(&attrFile, general->items[i].which);
		debug_key_file(&keyFile, general->items[i].which);
	}

}

