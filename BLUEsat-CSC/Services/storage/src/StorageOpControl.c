 /**
 *  \file StorageOpControl.c
 *
 *  \brief Translate storage operations to GSA operations
 *
 *  \author $Author: James Qin $
 *  \version 1.0
 *
 *  $Date: 2010-10-24 23:35:54 +1100 (Sun, 24 Oct 2010) $
 *  \warning No Warnings for now
 *  \bug No Bugs for now
 *  \note No Notes for now
 */

#include "StorageOpControl.h"

UnivRetCode enProcessStorageReq(GSACore *pGSACore,
								unsigned portCHAR ucAID,
								StorageContent *pStorageContent)
{
	unsigned portLONG ulRetVal;
	//translate operation
	switch (pStorageContent->Operation)
	{
		case MEM_STORE	:	pGSACore->DebugTrace("AID: %d requested Store\n\r", ucAID, 0, 0);
							return xGSAWrite(pGSACore, ucAID, pStorageContent->DID, pStorageContent->Size, pStorageContent->Ptr);

		case MEM_APPEND	:	pGSACore->DebugTrace("AID: %d requested Append\n\r", ucAID, 0, 0);
							break;

		case MEM_DELETE	:	pGSACore->DebugTrace("AID: %d requested Delete\n\r", ucAID, 0, 0);
							break;

		case MEM_SIZE	:	pGSACore->DebugTrace("AID: %d requested Size\n\r", ucAID, 0, 0);
							ulRetVal = ulGSASize(pGSACore, ucAID, pStorageContent->DID);
							if (pStorageContent->pulRetValue != NULL) *(pStorageContent->pulRetValue) = ulRetVal;
							break;

		case MEM_READ	:	pGSACore->DebugTrace("AID: %d requested Read\n\r", ucAID, 0, 0);
							ulRetVal = ulGSARead(pGSACore, ucAID, pStorageContent->DID, pStorageContent->Offset, pStorageContent->Size, pStorageContent->Ptr);
							if (pStorageContent->pulRetValue != NULL) *(pStorageContent->pulRetValue) = ulRetVal;
							break;

		default			:	pGSACore->DebugTrace("AID: %d requested unknown operation %d!\n\r",
												ucAID,
												pStorageContent->Operation, 0);
							return URC_FAIL;
	}

	return URC_SUCCESS;
}
