#ifndef FLVPARSER_H
#define FLVPARSER_H

#include <vector>

using namespace std;

#if 0
class CMediaBase
{
public:
	CMediaBase();
	virtual ~CMediaBase();

protected:

};

class CMediaH264
{
public:
	CMediaH264();
	virtual ~CMediaH264();

private:

};
#endif

class CFlvParser
{
public:
	CFlvParser();
	virtual ~CFlvParser();

	int Parse(unsigned char *pBuf, int nBufSize, int &nUsedLen);
	int PrintInfo();
	int DumpH264(const std::string &path);

private:
	typedef struct FlvHeader_s
	{
		int nVersion;
		int bHaveVideo, bHaveAudio;
		int nHeadSize;

		unsigned char *pFlvHeader;
	} FlvHeader;
	typedef struct TagHeader_s
	{
		int nType;
		int nDataSize;
		int nTimeStamp;
		int nTSEx;
		int nStreamID;

		unsigned int nTotalTS;
	} TagHeader;
	typedef struct Tag_s
	{
		TagHeader header;
		unsigned char *pTagData;
		unsigned char *pMedia;
		int nMediaLen;

		int nFrameType;
		int nCodecID;
		int ParseH264Tag(CFlvParser *pParser);
		int ParseH264Configuration(CFlvParser *pParser, unsigned char *pTagData);
		int ParseNalu(CFlvParser *pParser, unsigned char *pTagData);
	} Tag;
	typedef struct FlvStat_s
	{
		int nMetaNum, nVideoNum, nAudioNum;
		int nMaxTimeStamp;
		int nLengthSize = 0;
		int nUDTagNum = 0;
	} FlvStat;

	static unsigned int ShowU32(unsigned char *pBuf) { return (pBuf[0] << 24) + (pBuf[1] << 16) + (pBuf[2] << 8) + pBuf[3]; }
	static unsigned int ShowU24(unsigned char *pBuf) { return (pBuf[0] << 16) + (pBuf[1] << 8) + (pBuf[2]); }
	static unsigned int ShowU16(unsigned char *pBuf) { return (pBuf[0] << 8) + (pBuf[1]); }
	static unsigned int ShowU8(unsigned char *pBuf) { return (pBuf[0]); }

	friend Tag;
	
private:

	FlvHeader *CreateFlvHeader(unsigned char *pBuf);
	int DestroyFlvHeader(FlvHeader *pHeader);
	Tag *CreateTag(unsigned char *pBuf, int nLeftLen);
	int DestroyTag(Tag *pTag);
	int Stat();
	int StatVideo(Tag *pTag);
	int IsUserDataTag(Tag *pTag);

private:

	FlvHeader* _pFlvHeader;
	vector<Tag *> _vpTag;
	FlvStat _sStat;

	// H.264
	int _nNalUnitLength;

};

#endif // FLVPARSER_H
