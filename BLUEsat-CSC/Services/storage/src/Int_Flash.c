 /**
 *  \file Int_Flash.c
 *
 *  \brief Internal flash memory management
 *
 *  \author $Author: James Qin $
 *  \version 1.0
 *
 *  $Date: 2010-10-24 23:35:54 +1100 (Sun, 24 Oct 2010) $
 *  \warning No Warnings for now
 *  \bug No Bugs for now
 *  \note No Notes for now
 */

#include "service.h"
#include "Int_Flash.h"
#include "iap.h"
#include "debug.h"
#include "gsa.h"
#include "StorageOpControl.h"

#define FLASH_Q_SIZE	1

#define INTFLASH_START_SECTOR				20
#define INTFLASH_START_SECTOR_ADDR			SECTOR20ADDR
#define INTFLASH_END_SECTOR					28
#define INTFLASH_END_SECTOR_ADDR			SECTOR28ADDR
#define	INTFLASH_BLOCK_SIZE					BYTE_512

//task token for accessing services
static TaskToken Flash_TaskToken;

static GSACore IntFlashCore;

//prototype for task function
static portTASK_FUNCTION(vIntFlashTask, pvParameters);

static void vSetupCore(void);

static void vUpdateSectorStatus(void);

static unsigned portLONG WriteBufferFn(unsigned portLONG ulBlockAddr);

static portBASE_TYPE xIsBlockFreeFn(unsigned portLONG ulBlockAddr);

#ifndef NO_DEBUG
static void DebugTraceFn (portCHAR *pcFormat,
						unsigned portLONG Insert1,
						unsigned portLONG Insert2,
						unsigned portLONG Insert3);
#endif /* NO_DEBUG */

void vIntFlash_Init(unsigned portBASE_TYPE uxPriority)
{
	Flash_TaskToken = ActivateTask(TASK_MEM_INT_FLASH,
									"IntFlash",
									SEV_TASK_TYPE,
									uxPriority,
									SERV_STACK_SIZE,
									vIntFlashTask);

	vActivateQueue(Flash_TaskToken, FLASH_Q_SIZE);
}

static unsigned portLONG IntFlashBlockBuffer[INTFLASH_BLOCK_SIZE / sizeof(portLONG)];
static unsigned portCHAR IntFlashStateTable[STATE_TABLE_SIZE(INTFLASH_START_SECTOR_ADDR,
													INTFLASH_END_SECTOR_ADDR,
													INTFLASH_BLOCK_SIZE)];

#define BLOCK_TYPE_FREE		0
#define BLOCK_TYPE_VALID	1
#define BLOCK_TYPE_DEAD		2
#define TOTAL_BLOCK_TYPES	3
#define NUM_SECTORS 		INTFLASH_END_SECTOR-INTFLASH_START_SECTOR
unsigned portCHAR ucSectorStatus[NUM_SECTORS][TOTAL_BLOCK_TYPES];

static portTASK_FUNCTION(vIntFlashTask, pvParameters)
{
	(void) pvParameters;
	UnivRetCode enResult;
	MessagePacket incoming_packet;
	StorageContent *pContentHandle;

	vSetupCore();
	//initialise GSACore
	vInitialiseCore(&IntFlashCore);
    vDebugPrint(Flash_TaskToken, "Ready!\n\r", NO_INSERT, NO_INSERT, NO_INSERT);

	for ( ; ; )
	{
		vUpdateSectorStatus();

		enResult = enGetRequest(Flash_TaskToken, &incoming_packet, portMAX_DELAY);

		if (enResult != URC_SUCCESS) continue;

		pContentHandle = (StorageContent *)incoming_packet.Data;

#ifndef NO_DEBUG
		if (pContentHandle->Operation == STORAGE_FORMAT)
		{
			Erase_Sector(INTFLASH_START_SECTOR, INTFLASH_END_SECTOR - 1);

			vDebugPrint(Flash_TaskToken, "Sectors %d to %d erased!\n\r", INTFLASH_START_SECTOR, INTFLASH_END_SECTOR - 1, NO_INSERT);

			vInitialiseCore(&IntFlashCore);

			vCompleteRequest(incoming_packet.Token, URC_SUCCESS);
			continue;
		}
#endif

		vCompleteRequest(incoming_packet.Token, enProcessStorageReq(&IntFlashCore,
																	incoming_packet.Src,
																	pContentHandle));
	}
}

static void vSetupCore(void)
{
	//setup GSACore
	IntFlashCore.StartAddr 		= INTFLASH_START_SECTOR_ADDR;
	IntFlashCore.EndAddr 		= INTFLASH_END_SECTOR_ADDR;
	IntFlashCore.BlockSize		= INTFLASH_BLOCK_SIZE;
	IntFlashCore.BlockBuffer 	= IntFlashBlockBuffer;
	IntFlashCore.StateTable 	= IntFlashStateTable;
	IntFlashCore.StateTableSize = STATE_TABLE_SIZE(INTFLASH_START_SECTOR_ADDR,
													INTFLASH_END_SECTOR_ADDR,
													INTFLASH_BLOCK_SIZE);
	//setup optimisation
	IntFlashCore.Optimisation = NULL;
	//setup function pointers
	IntFlashCore.xIsBlockFree 	= xIsBlockFreeFn;
	IntFlashCore.WriteBuffer 	= WriteBufferFn;
#ifndef NO_DEBUG
	IntFlashCore.DebugTrace 	= DebugTraceFn;
#endif /* NO_DEBUG */
}

static void vUpdateSectorStatus(void)
{
	unsigned portSHORT usIndex;
	unsigned portSHORT usEraseFlag = 1;

	while (usEraseFlag)
	{
		for (usIndex = 0, usEraseFlag = 0; usIndex < NUM_SECTORS; ++usIndex)
		{
			ucSectorStatus[usIndex][BLOCK_TYPE_FREE] = usBlockStateCount(&IntFlashCore,
																		FlashSecAdds[usIndex+INTFLASH_START_SECTOR],
																		FlashSecAdds[usIndex+INTFLASH_START_SECTOR+1],
																		GSA_EXT_STATE_FREE);
			ucSectorStatus[usIndex][BLOCK_TYPE_VALID] = usBlockStateCount(&IntFlashCore,
																		FlashSecAdds[usIndex+INTFLASH_START_SECTOR],
																		FlashSecAdds[usIndex+INTFLASH_START_SECTOR+1],
																		GSA_EXT_STATE_VALID);
			ucSectorStatus[usIndex][BLOCK_TYPE_DEAD] = usBlockStateCount(&IntFlashCore,
																		FlashSecAdds[usIndex+INTFLASH_START_SECTOR],
																		FlashSecAdds[usIndex+INTFLASH_START_SECTOR+1],
																		GSA_EXT_STATE_DEAD);

			if (ucSectorStatus[usIndex][BLOCK_TYPE_FREE] + ucSectorStatus[usIndex][BLOCK_TYPE_VALID] > 0) continue;

			Erase_Sector(usIndex + INTFLASH_START_SECTOR, usIndex + INTFLASH_START_SECTOR);

			vDebugPrint(Flash_TaskToken,
						"Sector %d erased\n\r",
						usIndex + INTFLASH_START_SECTOR,
						NO_INSERT,
						NO_INSERT);

			vInitialiseCore(&IntFlashCore);

			usEraseFlag = 1;

			break;
		}
	}

	for (usIndex = 0; usIndex < NUM_SECTORS; ++usIndex)
	{
		vDebugPrint(Flash_TaskToken,
					"Sector %d:\n\r",
					usIndex + INTFLASH_START_SECTOR,
					NO_INSERT,
					NO_INSERT);
		vDebugPrint(Flash_TaskToken,
					"Free: %d, Valid: %d, Dead %d\n\r",
					ucSectorStatus[usIndex][BLOCK_TYPE_FREE],
					ucSectorStatus[usIndex][BLOCK_TYPE_VALID],
					ucSectorStatus[usIndex][BLOCK_TYPE_DEAD]);
	}
}

/*********************************** function pointers *********************************/
static unsigned portLONG WriteBufferFn(unsigned portLONG ulBlockAddr)
{
	//TODO implement smart wear leveling free block selection scheme
	//TODO implement data write treatment on write failure
	if (ulBlockAddr == (unsigned portLONG)NULL) ulBlockAddr = ulGetNextFreeBlock(&IntFlashCore, INTFLASH_START_SECTOR_ADDR, INTFLASH_END_SECTOR_ADDR);
	if (ulBlockAddr == (unsigned portLONG)NULL) return ulBlockAddr;

	if (Ram_To_Flash((void *)ulBlockAddr, (void *)IntFlashCore.BlockBuffer, IntFlashCore.BlockSize) != CMD_SUCCESS) return 0;

	vDebugPrint(Flash_TaskToken, "Free block * = %p\n\r", ulBlockAddr, NO_INSERT, NO_INSERT);
	return ulBlockAddr;
}

static portBASE_TYPE xIsBlockFreeFn(unsigned portLONG ulBlockAddr)
{
	unsigned portLONG *pulTmpPtr;

	//fresh erase all bits == 1
	for (pulTmpPtr = (unsigned portLONG *)ulBlockAddr;
		pulTmpPtr < (unsigned portLONG *)(ulBlockAddr + IntFlashCore.BlockSize);
		++pulTmpPtr)
	{
		if (*pulTmpPtr != 0xffffffff) return pdFALSE;
	}

	return pdTRUE;
}

#ifndef NO_DEBUG
static void DebugTraceFn (portCHAR *pcFormat,
						unsigned portLONG Insert1,
						unsigned portLONG Insert2,
						unsigned portLONG Insert3)
{
	vDebugPrint(Flash_TaskToken, pcFormat, Insert1, Insert2, Insert3);
}
#endif /* NO_DEBUG */

