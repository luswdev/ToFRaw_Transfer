/**
	TransferRaw.cpp
 */

#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include "TransferRaw.h"

using namespace std;

char newDir[MAX_PATH];
size_t transferCnt = 0;

bool CreateWorkingDir(void)
{
	SYSTEMTIME curTime;
	GetLocalTime(&curTime);

	sprintf_s(newDir, "%04d%02d%02dT%02d%02d%02d",
		curTime.wYear, curTime.wMonth, curTime.wDay, curTime.wHour, curTime.wMinute, curTime.wSecond);

	if (_mkdir(newDir) != 0) {
		cout << "Cannot create directory: " << newDir << endl;
		return false;
	}

	char NPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, NPath);

	cout << "Working directory in " << NPath << "\\" << newDir << endl;
	return true;
}

bool OpenOriginRaw(string path, BYTE* outputRaw)
{
	ifstream fraw(path, ios::in | ios::binary);

	if (!fraw) {
		cout << "Cannot open file: " << path << endl;
		return false;
	}

	/* check file size */
	streampos fsize = 0;
	fsize = fraw.tellg();
	fraw.seekg(0, ios::end);
	fsize = fraw.tellg() - fsize;

	if (streamoff(fsize) != RAW_SIZE) {
		cout << "Raw file size error: " << streamoff(fsize) << endl;
		return false;
	}

	fraw.seekg(0, ios::beg);
	fraw.read((char*)outputRaw, RAW_SIZE);
	fraw.close();

	return true;
}

/**
	3 byte/pixel | AAAA | BBBB | BBAA |
	Turn into raw12:
		A: | 00AA | AAAA |	=>	1H / 0x10 , 1L * 0x10 | 3L
		B: | 00BB | BBBB |	=>	2H / 0x10 , 2L * 0x10 | 3H / 0x10
 */
bool TransferRaw(BYTE* originRaw, BYTE* outputA, BYTE* outputB)
{
	size_t imagePixel = 0;

	for (int i = 0; i < RAW_SIZE; i += 3) {
		if (IS_EBD_LINE(i)) {
			continue;	/* skip embedded line */
		}

		outputA[imagePixel] = originRaw[i] >> 4;
		outputB[imagePixel] = originRaw[i + 1] >> 4;
		++imagePixel;

		outputA[imagePixel] = ((originRaw[i] & 0x0F) << 4) | (originRaw[i + 2] & 0x0F);
		outputB[imagePixel] = ((originRaw[i + 1] & 0x0F) << 4) | (originRaw[i + 2] >> 4);
		++imagePixel;
	}

	if (imagePixel != IMG_SIZE) {
		cout << "Transfer file size error: " << imagePixel << endl;
		return false;
	}

	return true;
}

bool SaveConfidence(BYTE* inputA, BYTE* inputB, BYTE* outputConfidence)
{
	size_t imageIndex = 0;

	for (int i = 0; i < IMG_SIZE / IMG_PHASE; i += 2) {
		int *phase = new int[IMG_PHASE]();

		for (int j = 0; j < IMG_PHASE; ++j) {
			phase[j] = RAW12(inputA, (i + (IMG_SIZE / IMG_PHASE) * j)) - RAW12(inputB, (i + (IMG_SIZE / IMG_PHASE) * j));
		}

		int imgI = phase[0] - phase[2];
		int imgQ = phase[3] - phase[1];
		int confidence = abs(imgI) + abs(imgQ);

		outputConfidence[imageIndex++] = (BYTE)(confidence >> 8);
		outputConfidence[imageIndex++] = (BYTE)(confidence & 0x0FF);

		delete[] phase;
	} 

	if (imageIndex != IMG_SIZE / IMG_PHASE) {
		cout << "Calculate confidence size error: " << imageIndex << endl;
		return false;
	}

	return true;
}

bool SaveResult(BYTE* buf, string path, size_t fileSize)
{
	string filePath(newDir + string("\\") + path + string("_") + to_string(transferCnt) + string(".raw"));
	ofstream fres(filePath, ios::out | ios::binary);

	if (!fres) {
		cout << "Cannot create file: " << path << endl;
		return false;
	}

	fres.write((char*)buf, fileSize);
	fres.close();

	return true;
}

int main(int argc, char* argv[])
{
	string rawPath;

	if (!CreateWorkingDir()) {
		return -1;
	}

	cout << "Drop target raw file here: ";
	while (cin >> rawPath) {
		BYTE* aBuf = new BYTE[IMG_SIZE]();
		BYTE* bBuf = new BYTE[IMG_SIZE]();
		BYTE* originBuf = new BYTE[RAW_SIZE]();
		BYTE* confidenceBuf = new BYTE[IMG_SIZE / IMG_PHASE]();

		ASSERT(OpenOriginRaw(rawPath, originBuf));
		ASSERT(TransferRaw(originBuf, aBuf, bBuf));
		ASSERT(SaveConfidence(aBuf, bBuf, confidenceBuf));
		ASSERT(SaveResult(aBuf, "a", IMG_SIZE));
		ASSERT(SaveResult(bBuf, "b", IMG_SIZE));
		ASSERT(SaveResult(confidenceBuf, "confidence", IMG_SIZE / IMG_PHASE));

		cout << "Transfer success (" << transferCnt++ << ")." << endl;

	CONTINUE:
		delete[] aBuf;
		delete[] bBuf;
		delete[] originBuf;
		delete[] confidenceBuf;

		cout << "Drop target raw file here: ";
	}

	return 0;
}
