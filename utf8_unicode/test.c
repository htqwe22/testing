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
���ļ�ϵͳ�� �� THIS_PART_FAT_START��ʼ�����������û���Ϊ��SIZE_FAT_UNIT+SIZE_BLK����С�ĵ�Ԫ��
һ��SIZE_BLK������������ָ���һ������Ĵ�С����һ��SIZE_FAT_UNIT����ָ������������Ԫ��
GetFileBlocks��������������ȡ FAT�������ɷֳɶ೤������Ԫ�ظ���������ʾ��Ҳ����������

�����ļ�ϵͳ�����¼����ֹ���
	1. MAP_PART_INFO ������������Ϣ��
	2. MAP_PART_SYS_VAR ��ϵͳ��������
	3. FAT���ݣ�����������������һ���ļ�ϵͳ����Ҫ�ĺ����
		THIS_PART_VALIDFAT_SIZE ��ʾ������ռ�õĿռ䣬����(��SIZE_BLK�ı���THIS_PART_FAT_SIZE����ʾ
		3.1 ����ռ�ô�СΪ THIS_PART_VALIDFAT_SIZE
		3.2 ����ռ��ʵ�ʿռ�Ϊ THIS_PART_FAT_SIZE
	4. DATA�����ɷ����BLK��Ҳ���������� THIS_FILEBODY_BLK_NUM
	THIS_PART_FAT_START
		------------------------------------------------------------------
		| MAP_PART_INFO | MAP_PART_SYS_VAR |
		------------------------------------------------------------------

	5. ����������˳��� 0 �� THIS_FILEBODY_BLK_NUM-1�� ÿ��Ԫ�ض�+2
	   ���� 2 �� THIS_FILEBODY_BLK_NUM + 1�� �����һ��Ԫ��Ҳ�����±�ΪTHIS_FILEBODY_BLK_NUM-1��Ԫ��Ϊ0��ָ��GND��Ҳ����
	   [2, ..., THIS_FILEBODY_BLK_NUM, 0]

	6. �ȿ��ļ�ϵͳ��ʼ���͸�ʽ������
		6.1 FS_PART_Init() ����Ƿ�����ʼ��
			�ú�����Ҫ�ǳ�ʼ�� MAP_PART_INFO ����СSIZE_BLK��������֣�����
			baPartSize		��¼��ǰ�����Ĵ�С
			baStartAddr		��ǰ��������ʼ��ַ����ʼ����Ϊ0����֪��������û���õ���
		6.2 OS_PART_FATFormat() ����Ƕ��ļ�ϵͳ���и�ʽ������ʽ�����ļ�ϵͳ�Ϳ���ʹ���ˣ�����
			��ʼ�� SUB_MAP_LOW_FILESYS_VARS ����ֶΣ���Ҫ�ǳ�ʼ��
			ba1stFreeBlkNum ֵΪ��һ����Ч�Ŀ飨V_1st_VALID_BLOCK_NUM = 1��
			baNowFreeBlkNum ֵΪ��һ����Ч�Ŀ飨V_1st_VALID_BLOCK_NUM = 1��
			baFreeBlks ֵΪTHIS_FILEBODY_BLK_NUM����ʾ��ǰ���õĿ��п��С��
			�������õ�һ������ FS_FAT_LowSysVarWtRd�������ú�����Ҫ������д����ģ�����������һ�����������
			MAP_PART_SYS_VAR ����һ��unLowFilePages��һ��4��SUB_MAP_LOW_FILESYS_VARS��С�����飬��дһ��SUB_MAP_LOW_FILESYS_VARS��Ϣ��ʱ�򣬻�����
			�����в������б�ʶbOwnerIsMe Ϊ V_YEAH_ITSME����һ��Ԫ�ء����û���ҵ���Ҳ����˵����û��д���ⲿ�����ݵ�ʱ�򣩴�0���±꿪ʼ��������ҵ�
			���ҵ��������һ��Ԫ�أ�ѭ���ң������µ���Ϣд�������򡣲�����һ�μ�¼�����������������һ�μ�¼��λ�úͱ���д���λ�ò�ͬ��ʱ�򣩡�������ѭ����

			Ȼ���������г�ʼ��������Ѿ��ڵ�5�����ᵽ���ˡ�

     7. ��ʼ���͸�ʽ�������ǾͿ��Զ��ļ����в����ˣ���������Ҫ�����ļ���Ŀ¼�������ǵ��õ� API_File_CreateFile�����������ɵġ�
	    ���������ʹ�þͲ�˵�ˣ�ֱ�ӷ�����
		�����ļ��� bFileType ���ͺ�baFileLen�ļ��Ĵ�С��������ʵĲ���¼�������Ҵ���һ��Ŀ¼��ֻ��Ҫһ��BLK�����ˣ� wFileSize����ֱ��Ϊ0��
		��������Ҫ�� SIZE_BLK�Ĵ�Сһ��Ҫ��MAP_API_FILEHEAD��FIELHEAD_SIZE 32�ֽڣ�����С�
		���� API_File_GetFreeSpace ������ǰ���õĿռ䣨ת��Ϊ�ֽڵ�λ������ȡ��ǰ���õĿռ��Ҫ��SUB_MAP_LOW_FILESYS_VARS����ṹ�ˣ���������ṹ�ᾭ���Ľ���
		��д���Ż���ǰ������һ��д����Ĵ��������ȡ�ռ���Ҫ�ǻ�ȡ baFreeBlks��ֵ����˵�����ֵ��ʼ��ΪTHIS_FILEBODY_BLK_NUM�������� if (wFileSize >= API_File_GetFreeSpace())
		�ͱ�֤�����ٻ���һ��BLK����������дDF��ʱ������Ҫһ��BLK�ġ�

		Ȼ��͵��õ� FS_File_Create�������
		    FS_Chain_Create������ȡָ����С������FS_Chain�������ļ��������ͷŵĻ���������Ԫ����������������������������ȶ�ȡһ��SUB_MAP_LOW_FILESYS_VARS���ݡ�
			Ȼ���ҵ� baNowFreeBlkNum����ǰ���������ͷ�� ���ֵ��ʼ��Ϊ1����Ȼ���ٸ��� baFreeBlksΪ��������ֵ��Ϊ��ȥ��ǰҪ����ĸ������ֵ����Ȼ�������ͷλ�ã�baNowFreeBlkNum����ʼ
			ͨ��FAT_ReadUnit(), ���������ɨ���������������Ҫ������ȡ�±�Ϊָ�����ݵ������������ݣ��±��1��ʼ���൱��һ������
			chain[THIS_FILEBODY_BLK_NUM] = {2, 3, ..., THIS_FILEBODY_BLK_NUM, 0}; �� THIS_FILEBODY_BLK_NUM����FAT_ReadUnit(n, &p)�൱�� *p = chain[n-1]. ����˵����Ҫ���������飬��ǰbaNowFreeBlkNum
			ֵΪ��ʼֵ1,�����Ƕ�ȡ���εõ�ֵΪ3����һ�� chain[1-1] = 2 ��Ϊ��һ�εĲ����� �ڶ��� chan[2-1] = 3;
			Ȼ���ٸ��� baNowFreeBlkNumֵΪ3.
			Ȼ����� FAT_WriteUnit(a, &b)����һ���±굥Ԫ�ӵ�0����ʾһ���µ����������������������ͻ���
			chain[THIS_FILEBODY_BLK_NUM] = {2, 0, ..., THIS_FILEBODY_BLK_NUM, 0};���Ե��ٴ�ɨ���һ�������ʱ���1��ʼ�õ�����Ϊ0�����ݵ�ʱ�򣬹�ɨ�������Ρ�Ҳ����˵��һ��������Ϊ2����
			���Ϸ��������������̣����ڵ�����Ҫ���������Ѿ�ͨ��FS_Chain_Create�ĵ�һ�����������ˡ�

			��������FS_File_Create�����·�����������ݹ��������ļ���������H_FILE����mapLowSys->baMyHeadBlk��(H_FILE�е�mapLowSys �� ϵͳ�е�mapLowSys����ͬһ���ṹ�壬ע��һ��)
			������ʼ���ļ��е�mapLowSys��baBodyStartBlk���óɿգ�Ȼ����ͬ���ķ�ʽΪbaBodyStartBlk��ֵ��baBodyStartBlk��

			Ȼ���ٸ����븸Ŀ¼�İ󶨹�ϵ��
			���û�и�Ŀ¼��mapLowSys��baParentHeadBlk ָ��գ������ָ���Ӧ�þ������ڸ�Ŀ¼�ɣ�
			����и�Ŀ¼�� mapLowSys��baParentHeadBlk ָ��Ŀ¼��baMyHeadBlk
			�����ñ�־mapLowSys->bRFUΪ0XFF�������ļ�ͷ����У�飬Ȼ������д�뵽mapLowSys->baMyHeadBlk��������ݿ��С�
			���õĺ����� FS_File_RewriteHead����������б�Ҫ����һ�£����ļ��Ķ�д���õ����ƵĻ��ƣ���������Ҫ�ĺ�����FS_Blk_WtData��
			FS_Blk_WtData(wBlkNum, wStartBytesOffset, *pbDataToDo, wBytesToDo)��
			��һ����������Ҫд��ķ�������Ŀ�ţ��ڶ�������Ϊ������е�ƫ��λ�ã�����������ΪҪ�����ԭʼ���ݣ����ĸ�����Ϊд������ݳ��ȡ�
			���������ֻ������ĳһ��SIZE_BLK�������Ŀ����д�롣
			����֮ǰ�ķ�����������������������ڵ�һ�γ�ʼ������ҵ���������γ�
			{2,4, 3, 0, 5, ...}�� ����˵��δ������Ĳ���Ϊn��������������������ҵ� THIS_PART_FILEBODY_START��ƫ��Ϊ
			��n-1��*SIZE_BLK + wStartBytesOffset�ĵط�����д�롣��ȡҲ�Ǵ������ַȡ��������������˵������������ʵ��ʾ����THIS_PART_FILEBODY_START�еĵ�(n-1)��BLK��
			�ӷ���ĽǶ���˵�����������blknum���൱��ָ�����Ŀ�����BLK������������������������е�ĳһ��Ԫ�ص�ֵ�������е�������һ��ʼʱ��������ӵģ�Ҳ����˵
			��0������Ԫ��2ָ����FILE_BODY�еĵ�2-1�飬��1��Ԫ��3ָ���3-1����... �� THIS_FILEBODY_BLK_NUM-1��Ԫ�أ�Ҳ������������һ��Ԫ�أ�ֵΪ0��ָ���κο顣��THIS_FILEBODY_BLK_NUM-2��Ԫ��ֵTHIS_FILEBODY_BLK_NUMָ�����
			THIS_FILEBODY_BLK_NUM-1���顣�ǵ�0��������V_1st_VALID_BLOCK_NUM��ֵΪ1����ָ�����ܹ��պ�THIS_FILEBODY_BLK_NUM�� ���û��
			������һ�����ʾ��ǣ�����������������ͻ��кܶ��NULL���������NULLԪ��֮ǰ��ֵΪm,����ָ����ǵ�m-1���飬�����ڱ��NULL�ˣ��Ƿ���ζ�����m-1���û����ָ�����أ�
			����������ʣ��������������������
			1. ��Ҫд������ݴ���SIZE_BLKʱ����ô�����ģ�Ӧ��Ҫ�õ�����
			2. ��Ҫɾ��һ������ʱ����ô�����ģ����ܲ��ܰ�֮ǰ��ʧ��m-1�ſ��һ�����
			3. ������ļ�ָ��ͬһ����Ŀ¼������ôͨ����Ŀ¼���ҵ��������е����ļ���
			��Щ��������Ĺ���ԭ����ء�

			�����ȼ���������@FS_File_RewriteHead()����֮��������ʲô������ǰ����˵�ˣ������Ŀ¼�����ڣ����Լ���parmHeadBlk�óɿգ�������ڣ���ָ��Ŀ¼��baMyHeadBlk��
			���Ե���Ŀ¼���ڵ�ʱ�򣬺���������һ�����������ַ����������
			1. �����Ŀ¼�� baBodyStartBlk == 0��Ӧ���Ǳ�ʾ��Ŀ¼��û�����ļ���
			   ��ʱ��ֻ��Ҫ����Ŀ¼��baBodyStartBlkָ���Լ���baMyHeadBlkͷ�ͺ��ˣ�
			2. �����Ŀ¼�� baBodyStartBlk�����ڣ��Ѿ�ָ��һ���ļ���ͷ����ô������� FS_Chain_Merge������������ϲ�����������ǳ���Ҫ�������ȷ�����
			   ÿ���ļ�ͷ����3��ϵͳ�����Ϣ
			   1. baMyHeadBlk ��ʾ���ļ���ͷ��Ϣ�Ƿ����ĸ�����
			   2. baBodyStartBlk ��ʾ���ļ��������Ƿ����ĸ�����
			   3. baParentHeadBlk ��ʾ���ļ��ĸ�Ŀ¼��ͷ�����ĸ����У�baParentHeadBlk ��ʵ���� parent->baMyHeadBlk��

			   ���������Ҫ�ع�ͷ����������ϸ�����һ�顣
				  �����ȿ�һ�������������һ����Ҫ������
				  FS_Chain_Merge(chain1, chain2); ������������þ��ǰ��������� chain1 ��chain2���һ�� chain2->chain1��������
				  FS_Chain_GetLen������������������б����ĺ�������C�����г��õĵ�������һ����ÿ������ڵ�������һ��nextָ�������һ���ڵ�ĵ�ַ����������������±��������ַ��
				  ���ǵ��������һ������ģ�ֻ��һ��ָ��Ԫ�صĵ�����������
				  FS_Chain_GetLen(n, &blk_num)����˵���� n ��ʾҪ������������ʼԪ�ص�ַ�� blk_numΪ���ص����һ��Ԫ�صĵ�ַ�����һ��Ԫ�ص�ֵָ�� NULL��������ֵ�������������Ԫ�صĸ�����
				  FS_Chain_Seek�ķ����������һҪ��ֻ��FS_Chain_Seek��Ҫ�������������ٸ�Ԫ�ء�
				  �������Ԫ��Ҳ��������ָ���BODY_BLK������Ԫ��ָ��أ�Ҳ��ָ���κ�BLK������˵���� [...,0, ...]�����ܹ�����ٸ�BLK�ռ��أ���ʵ��1������Ȼ����ֵ��ָ���κ�һ����ַ��
				  ���������ˣ�����һ��Ԫ��ָ���������ͷ����������֮ǰ�ᵽ�ģ�baMyHeadBlk��baBodyStartBlk��baParentHeadBlk��Щ�����ǵ�ָ����������ʱ���ָ�����������ĵ�ַ�����Ǽ���Ϊ m��
				  ��������������ļ����ǵ�m��Ԫ�ء�����ÿ������Ԫ�ع�����ļ����������Ԫ�ر����ĵ�ַ��أ�������ֵ��ء�����е���container_of��˵����ô�࣬�����������ƹ���Ҳ�������ˣ����ǻ���
				  һȫ�ֵ�ָ��ָ����������ͷ�����ǿ��е�����ͱ��ļ�ͷ����� baMyHeadBlk�� baBodyStartBlkָ���ˡ����������������ݻ��Ǳ������ŵġ�

			   FS_Chain_Merge(MAKEWORD_BYBYTES(phFileToCreate->mapLowSys.baMyHeadBlk), MAKEWORD_BYBYTES(phParent->mapLowSys.baBodyStartBlk))�����������ǰ�phParent->baBodyStartBlk�����ӳ��ˡ��������´����ļ�
			   ��baMyHeadBlk��������һ��Ŀ¼��Ѱ�����ļ����ļ��о���ͨ����Ŀ¼�� baBodyStartBlk������Ҫʹ�õĺ���ΪFS_File_Enum�� ��Ϊÿ���ļ���baMyHeadBlk�������һ��BLK������ö��һ��������൱��ɨ����һ���ļ���
			   ����ĳ��Ⱦ������ļ���Ŀ¼�ĸ�����
			   ����㣺 ���� FS_Chain_Merge��������һ������û�����׵�w2ndChainStartBlk == NULLʱ��ʾҪ�ͷţ�����ʱϵͳ��baNowFreeBlkNum��Ϊ0��˵��ϵͳ�Ŀ�������Ӧ��Ϊ�գ���ʱ����ϵͳ��baNowFreeBlkNumΪҪ�ͷŵ�����ͷ��
			   �ⲽû���⣬��������ͷֱ��ָ��END�����wEndNum = w1stStartBlk;��������������ֲ��ã��������û�����ף�ͨ���������֮���ֻ�ȡ�������ĳ���Ӧ�þ���1�ˣ�Ȼ���ٸ���ϵͳ�Ŀ��г��ȣ�
			   �ѵ���ʱҪ�ͷŵ�����һ��ֻ��һ��Ԫ����

		8. �ļ��Ķ�д��
		   API_File_RdWt(is_read, pFile, offset, data, data_size)
		   1. is_read��һ�������Ƕ�����д��1Ϊ����0Ϊд
		   2. pFile���ļ�������
		   3. offset���ļ���ƫ�ƴ�
		   4. Ҫд���ȡ�ĳ���
		   ���õ��� FS_Chain_BlkRdWt���������֮ǰ�з���������Ҫ����Ϊ�ļ���baBodyStartBlkָ��������������ݿ�ʼ��ȡ����Ҫ�����飬����Ҫ��ȡ��һ����ĵ�ַȻ����д����ȡ��
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
	debug("FAT ��ʼ��ַΪ %#X\r\n", THIS_PART_FAT_START);
	debug("FAT ������ %d ������ BLK(64K)��С\n", THIS_FILEBODY_BLK_NUM);
	debug("FAT ����ʵ��ռ�� %d �ֽ�\n", THIS_PART_FAT_SIZE);
	debug("FAT ������������Ϊ %d �ֽ�\n", THIS_PART_SIZE - SYS_VAR_SIZE - THIS_PART_FAT_SIZE);
	debug("FAT ��������ʼ��ַΪ %#X\n", THIS_PART_FILEBODY_START);
	debug("�� NUM_FAT_UNIT_OF_BLK ��ʾһ�� BLK�ɷ�%d������\n", (SIZE_BLK / SIZE_FAT_UNIT));
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
	uint8_t keyType;	// HD or not ()�����HD��Ǯ��private key����seed
	uint16_t coinType;
	uint32_t create_time;
	uint8_t detail_len;
	uint8_t detail[];   // �����水���������Ϣ��·�����ȼ�·����Ϣ
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

