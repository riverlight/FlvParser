#include <iostream>
#include <fstream>

#include "FlvParser.h"

using namespace std;

#define CheckBuffer(x) { if ((nBufSize-nOffset)<(x)) { nUsedLen = nOffset; return 0;} }


static const unsigned int nH264StartCode = 0x01000000;

CFlvParser::CFlvParser()
{

}

CFlvParser::~CFlvParser()
{
	for (int i = 0; i < _vpTag.size(); i++)
	{
		DestroyTag(_vpTag[i]);
		delete _vpTag[i];
	}
}

int CFlvParser::Parse(unsigned char *pBuf, int nBufSize, int &nUsedLen)
{
	int nOffset = 0;

	if (_pFlvHeader == 0)
	{
		CheckBuffer(9);
		_pFlvHeader = CreateFlvHeader(pBuf+nOffset);
		nOffset += _pFlvHeader->nHeadSize;
	}

	while (1)
	{
		CheckBuffer(15);
		int nPrevSize = ShowU32(pBuf + nOffset);
		nOffset += 4;

		Tag *pTag = CreateTag(pBuf + nOffset, nBufSize-nOffset);
		if (pTag == NULL)
		{
			nOffset -= 4;
			break;
		}
		nOffset += (11 + pTag->header.nDataSize);

		_vpTag.push_back(pTag);
	}

	nUsedLen = nOffset;
	return 0;
}

int CFlvParser::PrintInfo()
{
	Stat();

	cout << "vnum: " << _sStat.nVideoNum << " , anum: " << _sStat.nAudioNum << " , mnum: " << _sStat.nMetaNum << endl;
	cout << "maxTimeStamp: " << _sStat.nMaxTimeStamp << " ,nLengthSize: " << _sStat.nLengthSize << endl;
	cout << "UD tagNum: " << _sStat.nUDTagNum << endl;

	return 1;
}

int CFlvParser::DumpH264(const std::string &path)
{
	fstream f;
	f.open(path, ios_base::out|ios_base::binary);

	vector<Tag *>::iterator it_tag;
	for (it_tag = _vpTag.begin(); it_tag < _vpTag.end(); it_tag++)
	{
		if ((*it_tag)->header.nType != 0x09)
			continue;

		f.write((char *)(*it_tag)->pMedia, (*it_tag)->nMediaLen);
	}
	f.close();

	return 1;
}

int CFlvParser::Stat()
{
	for (int i = 0; i < _vpTag.size(); i++)
	{
		switch (_vpTag[i]->header.nType)
		{
		case 0x08:
			_sStat.nAudioNum++;
			break;
		case 0x09:
			StatVideo(_vpTag[i]);
			break;
		case 0x12:
			_sStat.nMetaNum++;
			break;
		default:
			;
		}
	}

	return 1;
}

int CFlvParser::StatVideo(Tag *pTag)
{
	_sStat.nVideoNum++;
	_sStat.nMaxTimeStamp = pTag->header.nTimeStamp;

	if (pTag->pTagData[0] == 0x17 && pTag->pTagData[1] == 0x00)
	{
		_sStat.nLengthSize = (pTag->pTagData[9] & 0x03) + 1;
	}
	if (IsUserDataTag(pTag) != 0)
		_sStat.nUDTagNum++;

	return 1;
}

int CFlvParser::IsUserDataTag(Tag *pTag)
{
	if (pTag->pTagData[0] != 0x27 || pTag->pTagData[1] != 0x01)
		return 0;

	unsigned char *pNalu = pTag->pTagData + (1 + 1 + 3 + _sStat.nLengthSize);
	if (pNalu[0] != 0x06 || pNalu[1] != 0x05)
		return 0;
	unsigned char *p = pNalu + 2;
	while (*p++ == 0xff);
	const char *szVideojjUUID = "VideojjLeonUUID";
	char *pp = (char *)p;
	for (int i = 0; i < strlen(szVideojjUUID); i++)
	{
		if (pp[i] != szVideojjUUID[i])
			return 0;
	}

	return 1;
}

CFlvParser::FlvHeader *CFlvParser::CreateFlvHeader(unsigned char *pBuf)
{
	FlvHeader *pHeader = new FlvHeader;
	pHeader->nVersion = pBuf[3];
	pHeader->bHaveAudio = (pBuf[4] >> 2) & 0x01;
	pHeader->bHaveVideo = (pBuf[4] >> 0) & 0x01;
	pHeader->nHeadSize = ShowU32(pBuf + 5);

	pHeader->pFlvHeader = new unsigned char[pHeader->nHeadSize];

	return pHeader;
}

int CFlvParser::DestroyFlvHeader(FlvHeader *pHeader)
{
	if (pHeader == NULL)
		return 0;

	delete pHeader->pFlvHeader;
	return 1;
}

CFlvParser::Tag *CFlvParser::CreateTag(unsigned char *pBuf, int nLeftLen)
{
	Tag *pTag = new Tag;
	memset(pTag, 0, sizeof(Tag));

	pTag->header.nType = ShowU8(pBuf+0);
	pTag->header.nDataSize = ShowU24(pBuf + 1);
	pTag->header.nTimeStamp = ShowU24(pBuf + 4);
	pTag->header.nTSEx = ShowU8(pBuf + 7);
	pTag->header.nStreamID = ShowU24(pBuf + 8);
	pTag->header.nTotalTS = unsigned int((pTag->header.nTSEx << 24)) + pTag->header.nTimeStamp;
	//cout << "nLeftLen : " << nLeftLen << " , nDataSize : " << pTag->header.nDataSize << endl;
	if ((pTag->header.nDataSize + 11) > nLeftLen)
	{
		delete pTag;
		return NULL;
	}
	pTag->pTagData = new unsigned char[pTag->header.nDataSize];
	if (pTag->pTagData == NULL)
	{
		delete pTag;
		return NULL;
	}
	memcpy(pTag->pTagData, pBuf + 11, pTag->header.nDataSize);

	unsigned char *pd = pTag->pTagData;
	pTag->nFrameType = (pd[0] & 0xf0) >> 4;
	pTag->nCodecID = pd[0] & 0x0f;
	if (pTag->header.nType == 0x09 && pTag->nCodecID==7)
	{
		pTag->ParseH264Tag(this);
	}

	return pTag;
}

int CFlvParser::DestroyTag(Tag *pTag)
{
	if (pTag->pMedia != NULL)
		delete pTag->pMedia;
	if (pTag->pTagData!=NULL)
		delete pTag->pTagData;

	return 1;
}

int CFlvParser::Tag::ParseH264Tag(CFlvParser *pParser)
{
	unsigned char *pd = pTagData;
	int nAVCPacketType = pd[1];
	int nCompositionTime = CFlvParser::ShowU24(pd + 2);

	if (nAVCPacketType == 0)
	{
		ParseH264Configuration(pParser, pd);
	}
	else if (nAVCPacketType == 1)
	{
		ParseNalu(pParser, pd);
	}
	else
	{

	}
	return 1;
}

int CFlvParser::Tag::ParseH264Configuration(CFlvParser *pParser, unsigned char *pTagData)
{
	unsigned char *pd = pTagData;

	pParser->_nNalUnitLength = (pd[9] & 0x03) + 1;

	int sps_size, pps_size;
	sps_size = CFlvParser::ShowU16(pd + 11);
	pps_size = CFlvParser::ShowU16(pd + 11 + (2 + sps_size) + 1);
	
	nMediaLen = 4 + sps_size + 4 + pps_size;
	pMedia = new unsigned char[nMediaLen];
	memcpy(pMedia, &nH264StartCode, 4);
	memcpy(pMedia + 4, pd + 11 + 2, sps_size);
	memcpy(pMedia + 4 + sps_size, &nH264StartCode, 4);
	memcpy(pMedia + 4 + sps_size + 4, pd + 11 + 2 + sps_size + 2 + 1, pps_size);

	return 1;
}

int CFlvParser::Tag::ParseNalu(CFlvParser *pParser, unsigned char *pTagData)
{
	unsigned char *pd = pTagData;
	int nOffset = 0;

	pMedia = new unsigned char[header.nDataSize+10];
	nMediaLen = 0;

	nOffset = 5;
	while (1)
	{
		if (nOffset >= header.nDataSize)
			break;

		int nNaluLen;
		switch (pParser->_nNalUnitLength)
		{
		case 4:
			nNaluLen = CFlvParser::ShowU32(pd + nOffset);
			break;
		case 3:
			nNaluLen = CFlvParser::ShowU24(pd + nOffset);
			break;
		case 2:
			nNaluLen = CFlvParser::ShowU16(pd + nOffset);
			break;
		default:
			nNaluLen = CFlvParser::ShowU8(pd + nOffset);
		}
		memcpy(pMedia + nMediaLen, &nH264StartCode, 4);
		memcpy(pMedia + nMediaLen + 4, pd + nOffset + pParser->_nNalUnitLength, nNaluLen);
		nMediaLen += (4 + nNaluLen);
		nOffset += (pParser->_nNalUnitLength + nNaluLen);
	}

	return 1;
}
