#include <stdlib.h>
#include <string.h>

#include "vadbg.h"
#include "Videojj.h"

CVideojj::CVideojj()
{

}

CVideojj::~CVideojj()
{
	int i;
	for (i = 0; i < _vVjjSEI.size(); i++)
	{
		delete _vVjjSEI[i].szUD;
	}
}

int CVideojj::Process(unsigned char *pNalu, int nNaluLen, int nTimeStamp)
{
	if (pNalu[4] != 0x06 || pNalu[5] != 0x05)
		return 0;
	unsigned char *p = pNalu + 4 + 2;
	while (*p++ == 0xff);
	const char *szVideojjUUID = "VideojjLeonUUID";
	char *pp = (char *)p;
	for (int i = 0; i < strlen(szVideojjUUID); i++)
	{
		if (pp[i] != szVideojjUUID[i])
			return 0;
	}
	
	VjjSEI sei;
	sei.nTimeStamp = nTimeStamp;
	sei.nLen = nNaluLen - (pp - (char *)pNalu) - 16 - 1;
	sei.szUD = new char[sei.nLen];
	memcpy(sei.szUD, pp + 16, sei.nLen);
	_vVjjSEI.push_back(sei);

	return 1;
}
