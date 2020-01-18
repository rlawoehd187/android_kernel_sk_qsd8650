#include "INC_INCLUDES.h"

#include "INC_INCLUDES.h"

#ifdef INC_EWS_SOURCE_ENABLE
/***********************************************************************
INC_EWS_INIT()함수를 호출한다.                                 

typedef struct _tagST_OUTPUT_EWS
{
	INC_UINT8		ucMsgGovernment;	메시지 발령기관
	INC_UINT8		ucMsgID;			메시지 고유번호
	ST_DATE_T		stDate;				일시
	
	INC_INT8		acKinds[4];			재난 종류
	INC_UINT8		cPrecedence;		우선 순위
	INC_UINT32		ulTime;				재난 시간
	INC_UINT8		ucForm;				재난 지역형식
	INC_UINT8		ucResionCnt;		재난 지역수
	INC_UINT8		aucResionCode[11];	지역 코드
	
	INC_INT8		acEWSMessage[EWS_OUTPUT_BUFF_MAX];	재난 내용.
	
}ST_OUTPUT_EWS, *PST_OUTPUT_EWS;
***********************************************************************/



ST_OUTPUT_EWS			g_stEWSMsg;
ST_OUTPUT_GROUP		g_stGroupMsg;

INC_UINT32 YMDtoMJD(ST_DATE_T stDate)
{
	INC_UINT16 wMJD;
	INC_UINT32 lYear, lMouth, lDay, L;
	INC_UINT32 lTemp1, lTemp2; 
	
	lYear = (INC_UINT32)stDate.usYear - (INC_UINT32)1900;
	lMouth = stDate.ucMonth;
	lDay = stDate.ucDay;
	
	if(lMouth == 1 || lMouth == 2) L = 1;
	else L = 0;
	
	lTemp1 = (lYear - L) * 36525L / 100L;
	lTemp2 = (lMouth + 1L + L * 12L) * 306001L / 10000L;
	
	wMJD = (INC_UINT16)(14956 + lDay + lTemp1 + lTemp2);

	return wMJD;
}

void MJDtoYMD(INC_UINT16 wMJD, ST_DATE_T *pstDate)
{
	INC_UINT32 lYear, lMouth, lTemp;
	
	lYear = (wMJD * 100L - 1507820L) / 36525L;
	lMouth = ((wMJD * 10000L - 149561000L) - (lYear * 36525L / 100L) * 10000L) / 306001L;
	
	pstDate->ucDay = (INC_UINT8)(wMJD - 14956L - (lYear * 36525L / 100L) - (lMouth * 306001L / 10000L));
	
	if(lMouth == 14 || lMouth == 15) lTemp = 1;
	else lTemp = 0;
	
	pstDate->usYear		= (INC_UINT16)(lYear + lTemp + 1900);
	pstDate->ucMonth	= (INC_UINT8)(lMouth - 1 - lTemp * 12);
}

void INC_EWS_INIT(ST_OUTPUT_EWS* pMsgDB)
{
	if(pMsgDB == 0)  return;
	memset(pMsgDB, 0, sizeof(ST_OUTPUT_EWS));
}

void INC_GROUP_INIT(void)
{
	memset(&g_stGroupMsg, 0, sizeof(ST_OUTPUT_GROUP));
}

ST_OUTPUT_EWS* INC_GET_EWS_DB(INC_UINT16 uiIndex)
{
	if(uiIndex >= MAX_EWS_IDS) return 0;
	return &g_stGroupMsg.astEWSMsg[uiIndex];
}

void INC_TYPE5_EXTENSION2(ST_FIB_INFO* pFibInfo)
{
	ST_FIG_HEAD*			pHeader;
	ST_TYPE_5*				pType;
	ST_EWS_INFO*			pEWSDataInfo;
	ST_EWS_TIME*			pstEWSTime = 0;
	EWS_INPUT_INFO*		pInputBuff = 0;
	ST_OUTPUT_EWS*		pEWSOutputMsg = 0;

	INC_INT16			nIndex, nTotalCnt, nTotalSuqNo;
	INC_UINT16		unData, nLoop;
	INC_UINT32		ulData;
	INC_UINT8		aucInfoBuff[5];

	pHeader = (ST_FIG_HEAD*)&pFibInfo->aucBuff[pFibInfo->ucDataPos++];
	pType	= (ST_TYPE_5*)&pFibInfo->aucBuff[pFibInfo->ucDataPos++];

	if(pType->ITEM.bitD2 == 1)
	{
		unData = INC_GET_WORDDATA(pFibInfo);
		pEWSDataInfo = (ST_EWS_INFO*)&unData;

		pEWSOutputMsg = INC_GET_EWS_DB(pEWSDataInfo->ITEM.bitID);
		if(pEWSOutputMsg == 0) return;
		
		pInputBuff = &pEWSOutputMsg->astInputBuff[pEWSDataInfo->ITEM.bitThisSeqNo];
		pInputBuff->uiCnt = pHeader->ITEM.bitLength - 3;

		for(nLoop = 0; nLoop < pInputBuff->uiCnt ; nLoop++)
			pInputBuff->acBuff[nLoop] = INC_GET_BYTEDATA(pFibInfo);
		
		nTotalSuqNo = pEWSDataInfo->ITEM.bitTotalNo + 1;

		for(nLoop = 0; nLoop < nTotalSuqNo; nLoop++){
			pInputBuff = &pEWSOutputMsg->astInputBuff[nLoop];
			if(!pInputBuff->uiCnt) break;
		}

		if(nLoop == nTotalSuqNo)
		{
			memset(pEWSOutputMsg->acTempBuff, 0, sizeof(pEWSOutputMsg->acTempBuff));

			for(nLoop = nTotalCnt = 0; nLoop < nTotalSuqNo ; nLoop++){
				pInputBuff = &pEWSOutputMsg->astInputBuff[nLoop];
				memcpy(&pEWSOutputMsg->acTempBuff[nTotalCnt], pInputBuff->acBuff, pInputBuff->uiCnt);
				nTotalCnt += pInputBuff->uiCnt;
			}
			
			for(nLoop = nIndex = 0; nLoop < 3; nLoop++)
				pEWSOutputMsg->acKinds[nLoop] = pEWSOutputMsg->acTempBuff[nIndex++];
			for(nLoop = 5; nLoop > 0; nLoop--)
				aucInfoBuff[nLoop-1] = pEWSOutputMsg->acTempBuff[nIndex++];

			ulData = ((aucInfoBuff[4]&0x3f)<<24) | (aucInfoBuff[3]<<16) | (aucInfoBuff[2]<<8) | aucInfoBuff[1];
			ulData = ulData >> 2;

			pstEWSTime = (ST_EWS_TIME*)&ulData;

			MJDtoYMD(pstEWSTime->ITEM.bitMJD, &pEWSOutputMsg->stDate);
			pEWSOutputMsg->stDate.ucHour		= (pstEWSTime->ITEM.bitUTCHours + 9) % 24;
			pEWSOutputMsg->stDate.ucMinutes = pstEWSTime->ITEM.bitUTCMinutes;

			pEWSOutputMsg->ucMsgGovernment	= pEWSDataInfo->ITEM.bitMsgGovernment;
			pEWSOutputMsg->ucMsgID					= pEWSDataInfo->ITEM.bitID;
			pEWSOutputMsg->cPrecedence			= ((aucInfoBuff[4] >> 6) & 0x3);
			pEWSOutputMsg->ulTime						= ulData;
			pEWSOutputMsg->ucForm						= (((aucInfoBuff[1] & 0x3)<<1) | (aucInfoBuff[0] >> 7));
			pEWSOutputMsg->ucResionCnt			= ((aucInfoBuff[0]>>3) & 0xf);

			for(nLoop = 0; nLoop < pEWSOutputMsg->ucResionCnt; nLoop++) {
				memcpy(pEWSOutputMsg->astResionCode[nLoop].acResionCode, &pEWSOutputMsg->acTempBuff[nIndex], MAX_RESION_CODE);
				nIndex += MAX_RESION_CODE;
			}
			
			memset(pEWSOutputMsg->acEWSMessage, 0, sizeof(pEWSOutputMsg->acEWSMessage));
			memcpy(pEWSOutputMsg->acEWSMessage, &pEWSOutputMsg->acTempBuff[nIndex], nTotalCnt - nIndex);

			pEWSOutputMsg->ucIsEWSGood = INC_SUCCESS;
		}	
	}
	else 
		pFibInfo->ucDataPos += (pHeader->ITEM.bitLength + 1);
}

void INC_SET_TYPE_5(ST_FIB_INFO* pFibInfo)
{
	ST_TYPE_5*		pExtern;
	ST_FIG_HEAD*	pHeader;
	INC_UINT8		ucType, ucHeader;
	
	ucHeader = INC_GETAT_HEADER(pFibInfo);
	ucType	= INC_GETAT_TYPE(pFibInfo);
	
	pHeader = (ST_FIG_HEAD*)&ucHeader;
	pExtern = (ST_TYPE_5*)&ucType;
	
	switch(pExtern->ITEM.bitExtension){
		case EXTENSION_2: INC_TYPE5_EXTENSION2(pFibInfo); break;
		default: pFibInfo->ucDataPos += (pHeader->ITEM.bitLength + 1); break;
	}
}

INC_UINT8 INC_EWS_PARSING(INC_UINT8* pucFicBuff, INC_INT32 uFicLength)
{
	ST_FIB_INFO* 	pstFib;
	ST_FIG_HEAD* 	pHeader;
	ST_FIC				stEWS;
	INC_UINT8			ucLoop, ucHeader, ucBlockNum;
  INC_UINT16		uiTempIndex = 0;
	ST_OUTPUT_EWS*		pEWSMsg = 0;
	INC_UINT16 unMsgCount, unIndex;

	ucBlockNum = uFicLength / FIB_SIZE;
	pstFib = &stEWS.stBlock;
	
	for(ucLoop = 0; ucLoop < ucBlockNum; ucLoop++)
	{
		INC_SET_UPDATEFIC(pstFib, &pucFicBuff[ucLoop*FIB_SIZE]);
		if(!pstFib->uiIsCRC) continue;
		
		while(pstFib->ucDataPos < FIB_SIZE-2)
		{
			ucHeader = INC_GETAT_HEADER(pstFib);
			pHeader = (ST_FIG_HEAD*)&ucHeader;
			
			if(!INC_GET_FINDTYPE(pHeader) || !INC_GET_NULLBLOCK(pHeader) || !INC_GET_FINDLENGTH(pHeader)) break;
			
			switch(pHeader->ITEM.bitType) {
			case FIG_FICDATA_CHANNEL : INC_SET_TYPE_5(pstFib); break;
			default					 : pstFib->ucDataPos += pHeader->ITEM.bitLength + 1;break;
			}
		}
	}

	for(unIndex = 0; unIndex < MAX_EWS_IDS; unIndex++)
	{
		pEWSMsg = INC_GET_EWS_DB(unIndex);
		if(pEWSMsg->ucIsEWSGood == INC_SUCCESS)
			return INC_SUCCESS;
	}

	return INC_ERROR;
}


#endif



