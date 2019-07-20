#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <process.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include "resource.h"
#include <stdio.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")  

typedef enum _NALUTYPE
{
	NALU_TYPE_UNUSE = 0,	//未使用
	NALU_TYPE_SLICE,		//非IDR的片
	NALU_TYPE_DPA,			//片数据A分区
	NALU_TYPE_DPB,			//片数据B分区
	NALU_TYPE_DPC,			//片数据C分区
	NALU_TYPE_IDR,			//IDR图像的片
	NALU_TYPE_SEI,			//补充增强信息单元（SEI）
	NALU_TYPE_SPS,			//序列参数集
	NALU_TYPE_PPS,			//图像参数集
	NALU_TYPE_AUD,			//分界符
	NALU_TYPE_EOSEQ,		//序列结束
	NALU_TYPE_EOSTREAM,		//码流结束
	NALU_TYPE_FILL			//填充
}NALUTYPE;

typedef enum _NALUIDC
{
	NALU_PRIORITY_DISPOSABLE = 0,
	NALU_PRIORITY_LOW,
	NALU_PRIORITY_HIGH,
	NALU_PRIORITY_HIGHEST

}NALUIDC;

typedef struct _NALUInfo
{
	int nStartCodePrefixLen;
	unsigned int nLen;
	unsigned int nMaxSize;
	//禁止位，初始为0，当网络发现NAL单元有比特错误时可设置该比特为1
	//以便接收方纠错或丢掉该单元。
	int nForbiddenBit;
	//nal重要性指示，标志该NAL单元的重要性，值越大，越重要，
	//解码器在解码处理不过来的时候，可以丢掉重要性为0的NALU。
	int nNalReferenceIdc;
	int nNalUnitType;
	char* szBuffer;
}NALUInfo,*PNALUInfo;

HWND g_hDlg = 0;

INT_PTR  CALLBACK TheProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void OnInit(HWND hDlg);
void OnCommand(HWND hDlg,WPARAM wParam,LPARAM lParam);
void AnalyseH264File(HWND hDlg);
unsigned int _stdcall HandleH264File(LPVOID szH264File);
int GetAnnexbNALU(PNALUInfo pInfo, FILE* hH264File);
int GetStartCodeLen2(const char* szBuffer);
int GetStartCodeLen3(const char* szBuffer);

int WINAPI WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd)
{
	g_hDlg = CreateDialogParamA(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), 0, TheProc, 0);
	if (!g_hDlg)
		return -1;
	ShowWindow(g_hDlg, nShowCmd);
	UpdateWindow(g_hDlg);
	MSG stMsg;
	while (GetMessageA(&stMsg, 0, 0, 0))
	{
		TranslateMessage(&stMsg);
		DispatchMessageA(&stMsg);
	}
	return stMsg.wParam;
}

INT_PTR  CALLBACK TheProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		OnInit(hDlg);
		return true;
	case WM_COMMAND:
		OnCommand(hDlg, wParam, lParam);
		break;
	case WM_CLOSE:
		DestroyWindow(hDlg);
		PostQuitMessage(0);
		break;
	}
	return false;
}

void OnInit(HWND hDlg)
{
	HWND hViewListInfo = GetDlgItem(hDlg, IDC_LIST_INFO);
	LVCOLUMNA stColumn;
	memset(&stColumn, 0, sizeof(LVCOLUMNA));
	stColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	stColumn.fmt = LVCFMT_LEFT;
	stColumn.cx = 100;

	stColumn.iSubItem = 0;
	stColumn.pszText = (LPSTR)"Index";
	ListView_InsertColumn(hViewListInfo, 0, &stColumn);
	stColumn.iSubItem = 1;
	stColumn.pszText = (LPSTR)"Start Pos";
	ListView_InsertColumn(hViewListInfo, 1, &stColumn);
	stColumn.iSubItem = 2;
	stColumn.pszText = (LPSTR)"Idc";
	ListView_InsertColumn(hViewListInfo, 2, &stColumn);
	stColumn.iSubItem = 3;
	stColumn.pszText = (LPSTR)"Type";
	ListView_InsertColumn(hViewListInfo, 3, &stColumn);
	stColumn.iSubItem = 4;
	stColumn.pszText = (LPSTR)"Length";
	ListView_InsertColumn(hViewListInfo, 4, &stColumn);

	ListView_SetExtendedListViewStyle(hViewListInfo, 
		LVS_EX_DOUBLEBUFFER| LVS_EX_GRIDLINES| LVS_EX_FULLROWSELECT);

	EnableWindow(GetDlgItem(hDlg, IDC_EDIT_FILE), false);
}

void OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam))
	{
	case IDC_BUTTON_OPEN:
		AnalyseH264File(hDlg);
		break;
	}
}

void AnalyseH264File(HWND hDlg)
{
	static char szH264File[MAX_PATH];
	OPENFILENAME stOpenFile;
	memset(szH264File, 0, MAX_PATH);
	memset(&stOpenFile, 0, sizeof(OPENFILENAME));
	stOpenFile.lStructSize = sizeof(OPENFILENAME);
	stOpenFile.lpstrFilter = "h264文件 \0*.h264";
	stOpenFile.nFilterIndex = 1;
	stOpenFile.lpstrFile = szH264File;
	stOpenFile.nMaxFile = MAX_PATH;
	stOpenFile.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileName(&stOpenFile))
	{
		Edit_SetText(GetDlgItem(hDlg, IDC_EDIT_FILE), szH264File);
		_beginthreadex(0, 0, HandleH264File, szH264File, 0, 0);
	}
}

unsigned int _stdcall  HandleH264File(LPVOID szH264File)
{
	FILE* hH264File = 0;
	hH264File = fopen((const char*)szH264File, "rb+");
	if (!hH264File)
	{
		MessageBoxA(0, "打开H264文件失败", 0, MB_OK);
		return 0;
	}

	static char szBuffer[100000];
	NALUInfo stNalu;
	memset(&stNalu, 0, sizeof(NALUInfo));
	stNalu.nMaxSize = 100000;
	stNalu.szBuffer = szBuffer;

	LVITEMA stItem;
	memset(&stItem, 0, sizeof(LVITEMA));
	stItem.mask = LVIF_TEXT;
	int nIndex = 0;
	int nDataOffset = 0, nNvalCount = 0;
	char szIndex[20];
	char szStart[20];
	char szType[20];
	char szIdc[20];
	char szLen[20];
	while (!feof(hH264File))
	{
		int nDataLength = GetAnnexbNALU(&stNalu,hH264File);
		if (!nDataLength)
		{
			MessageBoxA(0, "读取失败", 0,MB_OK);
			fclose(hH264File);
			return 0;
		}
		switch (stNalu.nNalUnitType)
		{
		case NALU_TYPE_SLICE:sprintf(szType, "SLICE"); break;
		case NALU_TYPE_DPA:sprintf(szType, "DPA"); break;
		case NALU_TYPE_DPB:sprintf(szType, "DPB"); break;
		case NALU_TYPE_DPC:sprintf(szType, "DPC"); break;
		case NALU_TYPE_IDR:sprintf(szType, "IDR"); break;
		case NALU_TYPE_SEI:sprintf(szType, "SEI"); break;
		case NALU_TYPE_SPS:sprintf(szType, "SPS"); break;
		case NALU_TYPE_PPS:sprintf(szType, "PPS"); break;
		case NALU_TYPE_AUD:sprintf(szType, "AUD"); break;
		case NALU_TYPE_EOSEQ:sprintf(szType, "EOSEQ"); break;
		case NALU_TYPE_EOSTREAM:sprintf(szType, "EOSTREAM"); break;
		case NALU_TYPE_FILL:sprintf(szType, "FILL"); break;
		}
		switch (stNalu.nNalReferenceIdc>>5)
		{
		case NALU_PRIORITY_DISPOSABLE:sprintf(szIdc, "DISPOS"); break;
		case NALU_PRIORITY_LOW:sprintf(szIdc, "LOW"); break;
		case NALU_PRIORITY_HIGH:sprintf(szIdc, "HIGH"); break;
		case NALU_PRIORITY_HIGHEST:sprintf(szIdc, "HIGHEST"); break;
		}
		stItem.iItem = nIndex;
		stItem.iSubItem = 0;
		_itoa(nIndex, szIndex, 10);
		stItem.pszText = szIndex;
		ListView_InsertItem(GetDlgItem(g_hDlg, IDC_LIST_INFO), &stItem);
		stItem.iSubItem = 1;
		_itoa(nDataOffset, szStart, 10);
		stItem.pszText = szStart;
		ListView_SetItem(GetDlgItem(g_hDlg, IDC_LIST_INFO), &stItem);
		stItem.iSubItem = 2;
		stItem.pszText = szIdc;
		ListView_SetItem(GetDlgItem(g_hDlg, IDC_LIST_INFO), &stItem);
		stItem.iSubItem = 3;
		stItem.pszText = szType;
		ListView_SetItem(GetDlgItem(g_hDlg, IDC_LIST_INFO), &stItem);
		stItem.iSubItem = 4;
		_itoa(stNalu.nLen, szLen, 10);
		stItem.pszText = szLen;
		ListView_SetItem(GetDlgItem(g_hDlg, IDC_LIST_INFO), &stItem);

		nDataOffset += nDataLength;
		nNvalCount++;
		nIndex++;
	}
	fclose(hH264File);
	return 0;
}

int GetAnnexbNALU(PNALUInfo pInfo,FILE* hH264File)
{
	static char szBuffer[100000];
	pInfo->nStartCodePrefixLen = 3;

	if (3 != fread(szBuffer, 1, 3, hH264File))
		return 0;
	int nPos = 0;
	if (1 != GetStartCodeLen2(szBuffer))
	{
		if (1 != fread(szBuffer + 3, 1, 1, hH264File))
			return 0;
		if (1 != GetStartCodeLen3(szBuffer))
			return 0;
		else
		{
			nPos = 4;
			pInfo->nStartCodePrefixLen = 4;
		}
	}
	else
	{
		nPos = 3;
		pInfo->nStartCodePrefixLen = 3;
	}
	int nCheck = 0, nInfo2 = 0, nInfo3 = 0;
	while (!nCheck)
	{
		if (feof(hH264File))
		{
			pInfo->nLen = (nPos - 1) - pInfo->nStartCodePrefixLen;
			/*
			Note:这里提醒一下，如果要把数据也保存到结构体里面的szBuffer里面的话，
			就要在函数memcpy后面的大小修改为pInfo->nLen，
			注意一点就是确保szBuffer足够大！！！！！
			我们这里不需要复制数据出来，所以就用了1而已
			*/
			memcpy(pInfo->szBuffer, &szBuffer[pInfo->nStartCodePrefixLen], 1);
			pInfo->nForbiddenBit = (pInfo->szBuffer[0]) & 0x80;
			pInfo->nNalReferenceIdc = (pInfo->szBuffer[0]) & 0x60;
			pInfo->nNalUnitType = (pInfo->szBuffer[0]) & 0x1f;
			return nPos - 1;
		}
		szBuffer[nPos++] = fgetc(hH264File);
		nInfo3 = GetStartCodeLen3(&szBuffer[nPos - 4]);
		if (nInfo3 != 1)
			nInfo2 = GetStartCodeLen2(&szBuffer[nPos - 3]);
		nCheck = (nInfo2 == 1 || nInfo3 == 1);
	}
	int nReWin = (nInfo3 == 1) ? -4 : -3;
	if (0 != fseek(hH264File, nReWin, SEEK_CUR))
		return 0;
	pInfo->nLen = (nPos + nReWin) - pInfo->nStartCodePrefixLen;
	/*
	Note:这里提醒一下，如果要把数据也保存到结构体里面的szBuffer里面的话，就要在函数memcpy
	后面的大小修改为pInfo->nLen，注意一点就是确保szBuffer足够大！！！！！
	我们这里不需要复制数据出来，所以就用了1而已
	*/
	memcpy(pInfo->szBuffer, &szBuffer[pInfo->nStartCodePrefixLen], 1);
	pInfo->nForbiddenBit = (pInfo->szBuffer[0]) & 0x80;
	pInfo->nNalReferenceIdc = (pInfo->szBuffer[0]) & 0x60;
	pInfo->nNalUnitType = (pInfo->szBuffer[0]) & 0x1f;
	return nPos + nReWin;
}

int GetStartCodeLen2(const char* szBuffer)
{
	if (szBuffer[0] != 0 ||
		szBuffer[1] != 0 ||
		szBuffer[2] != 1)
		return 0;
	return 1;
}

int GetStartCodeLen3(const char* szBuffer)
{
	if (szBuffer[0] != 0 ||
		szBuffer[1] != 0 ||
		szBuffer[2] != 0 ||
		szBuffer[3] != 1)
		return 0;
	return 1;
}