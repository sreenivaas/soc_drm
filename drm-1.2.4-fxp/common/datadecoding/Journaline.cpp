/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "Journaline.h"


#ifdef HAVE_JOURNALINE
/* Implementation *************************************************************/
/* Set these to 1 for more debug information. Needed by Journaline library */
#ifdef WIN32
extern "C" int showDdNewsSvcDecErr = 0;
extern "C" int showDdNewsSvcDecInfo = 0;
extern "C" int showDdDabDgDecErr = 0;
extern "C" int showDdDabDgDecInfo = 0;
#else
int showDdNewsSvcDecErr = 0;
int showDdNewsSvcDecInfo = 0;
int showDdDabDgDecErr = 0;
int showDdDabDgDecInfo = 0;
#endif

CJournaline::CJournaline() : dgdec(NULL), newsdec(NULL)
{
	/* This will be the first call to the Journaline decoder open function, the
	   pointer to the decoders must have a defined value (NULL) to avoid
	   unpredictable behaviour in the "ResetOpenJournalineDecoder()" function */
	ResetOpenJournalineDecoder();
}

CJournaline::~CJournaline()
{
	/* Delete decoder instances */
	if (newsdec != NULL)
		NEWS_SVC_DEC_deleteDec(newsdec);

	if (dgdec != NULL)
		DAB_DATAGROUP_DECODER_deleteDec(dgdec);
}

void CJournaline::ResetOpenJournalineDecoder()
{
	/* 1 MB memory for news decoder */
	unsigned long max_memory = 1024 * 1024;

	/* No limit for number of NML objects (except max_memory limitation) */
	unsigned long max_objects = 0;

	/* No extended header will be used */
	unsigned long extended_header_len = 0;

	/* If decoder was initialized before, delete old instance */
	if (newsdec != NULL)
		NEWS_SVC_DEC_deleteDec(newsdec);

	if (dgdec != NULL)
		DAB_DATAGROUP_DECODER_deleteDec(dgdec);

	/* Create decoder instance. Pass the pointer to this object. This is needed
	   for the call-back functions! */
	dgdec = DAB_DATAGROUP_DECODER_createDec(dg_cb, this);
	newsdec = NEWS_SVC_DEC_createDec(obj_avail_cb, max_memory, &max_objects,
		extended_header_len, this);
}

void CJournaline::AddDataUnit(CVector<_BINARY>& vecbiNewData)
{
	const int iSizeBytes = vecbiNewData.Size() / SIZEOF__BYTE;

	if (iSizeBytes > 0)
	{
		/* Bits to byte conversion */
		CVector<_BYTE> vecbyData(iSizeBytes);
		vecbiNewData.ResetBitAccess();

		for (int i = 0; i < iSizeBytes; i++)
			vecbyData[i] = (_BYTE) vecbiNewData.Separate(SIZEOF__BYTE);

		/* Add new data unit to Journaline decoder library */
		DAB_DATAGROUP_DECODER_putData(dgdec, iSizeBytes, &vecbyData[0]);
	}
}

void CJournaline::GetNews(const int iObjID, CNews& News)
{
	/* Init News object in case no object is available */
	News.sTitle = "";
	News.vecItem.Init(0);

	NML::RawNewsObject_t rno;
	unsigned long elen = 0;
	unsigned long len = 0;
	if (NEWS_SVC_DEC_get_news_object(newsdec, iObjID, &elen, &len, rno.nml))
	{
		rno.nml_len = static_cast<unsigned short>(len);
		rno.extended_header_len = static_cast<unsigned short>(elen);
		RemoveNMLEscapeSequences handler;
		NML *nml = NMLFactory::Instance()->CreateNML(rno, &handler);

		/* Title */
		News.sTitle = nml->GetTitle();

		/* Items */
		const int iNumItems = nml->GetNrOfItems();
		News.vecItem.Init(iNumItems);

		/* For "keep in cache" function, init with zero length */
		CVector<NML::NewsObjectId_t> iAvailIDs(0);

		for (int i = 0; i < iNumItems; i++)
		{
			/* Text */
			News.vecItem[i].sText = nml->GetItemText(i);

			/* Link */
			if (nml->isMenu())
			{
				elen = len = 0;

				/* Check availability of linked object */
				if (NEWS_SVC_DEC_get_news_object(newsdec, nml->GetLinkId(i),
					&elen, &len, rno.nml))
				{
					/* Assign link */
					News.vecItem[i].iLink = nml->GetLinkId(i);

					/* Store link in vector for "keep in cache" function */
					iAvailIDs.Add(nml->GetLinkId(i));
				}
				else
				{
					/* Not yet received */
					News.vecItem[i].iLink = JOURNALINE_LINK_NOT_ACTIVE;
				}
			}
			else
				News.vecItem[i].iLink = JOURNALINE_IS_NO_LINK; /* No link */
		}

		const int iAvailIDsSize = iAvailIDs.Size();
		if (iAvailIDsSize > 0)
		{
			/* Tell the decoder to keep the linked objects in cache */
			NEWS_SVC_DEC_keep_in_cache(newsdec, iAvailIDsSize, &iAvailIDs[0]);
		}

		delete nml;
	}
}
#endif
