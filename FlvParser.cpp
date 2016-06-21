#include <iostream>
#include <fstream>

#include "FlvParser.h"

using namespace std;

#define CheckBuffer(x) { if ((nBufSize-nOffset)<(x)) { nUsedLen = nOffset; return 0;} }


static const unsigned int nH264StartCode = 0x01000000;

CFlvParser::CFlvParser()
{
	_vjj = new CVideojj();
}

CFlvParser::~CFlvParser()
{
	for (int i = 0; i < _vpTag.size(); i++)
	{
		DestroyTag(_vpTag[i]);
		delete _vpTag[i];
	}
	if (_vjj != NULL)
		delete _vjj;
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
		nOffset += (11 + pTag->_header.nDataSize);

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
	cout << "Vjj SEI num: " << _vjj->_vVjjSEI.size() << endl;
	for (int i = 0; i < _vjj->_vVjjSEI.size(); i++)
		cout << "SEI time : " << _vjj->_vVjjSEI[i].nTimeStamp << endl;
	return 1;
}

int CFlvParser::DumpH264(const std::string &path)
{
	fstream f;
	f.open(path, ios_base::out|ios_base::binary);

	vector<Tag *>::iterator it_tag;
	for (it_tag = _vpTag.begin(); it_tag < _vpTag.end(); it_tag++)
	{
		if ((*it_tag)->_header.nType != 0x09)
			continue;

		f.write((char *)(*it_tag)->_pMedia, (*it_tag)->_nMediaLen);
	}
	f.close();

	return 1;
}

int CFlvParser::Stat()
{
	for (int i = 0; i < _vpTag.size(); i++)
	{
		switch (_vpTag[i]->_header.nType)
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
	_sStat.nMaxTimeStamp = pTag->_header.nTimeStamp;

	if (pTag->_pTagData[0] == 0x17 && pTag->_pTagData[1] == 0x00)
	{
		_sStat.nLengthSize = (pTag->_pTagData[9] & 0x03) + 1;
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

void CFlvParser::Tag::Init(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen)
{
	memcpy(&_header, pHeader, sizeof(TagHeader));

	_pTagData = new unsigned char[_header.nDataSize];
	memcpy(_pTagData, pBuf + 11, _header.nDataSize);

}

CFlvParser::CVideoTag::CVideoTag(TagHeader *pHeader, unsigned char *pBuf, int nLeftLen, CFlvParser *pParser)
{
	Init(pHeader, pBuf, nLeftLen);

	unsigned char *pd = _pTagData;
	nFrameType = (pd[0] & 0xf0) >> 4;
	nCodecID = pd[0] & 0x0f;
	if (_header.nType == 0x09 && nCodecID == 7)
	{
		ParseH264Tag(pParser);
	}
}

CFlvParser::Tag *CFlvParser::CreateTag(unsigned char *pBuf, int nLeftLen)
{
	TagHeader header;
	header.nType = ShowU8(pBuf+0);
	header.nDataSize = ShowU24(pBuf + 1);
	header.nTimeStamp = ShowU24(pBuf + 4);
	header.nTSEx = ShowU8(pBuf + 7);
	header.nStreamID = ShowU24(pBuf + 8);
	header.nTotalTS = unsigned int((header.nTSEx << 24)) + header.nTimeStamp;
	cout << "total TS : " << header.nTotalTS << endl;
	//cout << "nLeftLen : " << nLeftLen << " , nDataSize : " << pTag->header.nDataSize << endl;
	if ((header.nDataSize + 11) > nLeftLen)
	{
		return NULL;
	}

	Tag *pTag;
	switch (header.nType) {
	case 0x09:
		pTag = new CVideoTag(&header, pBuf, nLeftLen, this);
		break;
	default:
		pTag = new Tag();
		pTag->Init(&header, pBuf, nLeftLen);
	}
	
	return pTag;
}

int CFlvParser::DestroyTag(Tag *pTag)
{
	if (pTag->_pMedia != NULL)
		delete pTag->_pMedia;
	if (pTag->_pTagData!=NULL)
		delete pTag->_pTagData;

	return 1;
}

int CFlvParser::CVideoTag::ParseH264Tag(CFlvParser *pParser)
{
	unsigned char *pd = _pTagData;
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

int CFlvParser::CVideoTag::ParseH264Configuration(CFlvParser *pParser, unsigned char *pTagData)
{
	unsigned char *pd = pTagData;

	pParser->_nNalUnitLength = (pd[9] & 0x03) + 1;

	int sps_size, pps_size;
	sps_size = CFlvParser::ShowU16(pd + 11);
	pps_size = CFlvParser::ShowU16(pd + 11 + (2 + sps_size) + 1);
	
	_nMediaLen = 4 + sps_size + 4 + pps_size;
	_pMedia = new unsigned char[_nMediaLen];
	memcpy(_pMedia, &nH264StartCode, 4);
	memcpy(_pMedia + 4, pd + 11 + 2, sps_size);
	memcpy(_pMedia + 4 + sps_size, &nH264StartCode, 4);
	memcpy(_pMedia + 4 + sps_size + 4, pd + 11 + 2 + sps_size + 2 + 1, pps_size);

	return 1;
}

int CFlvParser::CVideoTag::ParseNalu(CFlvParser *pParser, unsigned char *pTagData)
{
	unsigned char *pd = pTagData;
	int nOffset = 0;

	_pMedia = new unsigned char[_header.nDataSize+10];
	_nMediaLen = 0;

	nOffset = 5;
	while (1)
	{
		if (nOffset >= _header.nDataSize)
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
		memcpy(_pMedia + _nMediaLen, &nH264StartCode, 4);
		memcpy(_pMedia + _nMediaLen + 4, pd + nOffset + pParser->_nNalUnitLength, nNaluLen);
		pParser->_vjj->Process(_pMedia+_nMediaLen, 4+nNaluLen, _header.nTotalTS);
		_nMediaLen += (4 + nNaluLen);
		nOffset += (pParser->_nNalUnitLength + nNaluLen);
	}

	return 1;
}
