/**
	TransferRaw.cpp
 */

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
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

	sprintf_s(newDir, "%04d%02d%02dT%02d%02d%02d",	/* ISO 8601 */
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

/**
	Phase0 = |A0 - B0| (  0 deg)
	Phase1 = |A1 - B1| ( 90 deg)
	Phase2 = |A2 - B2| (180 deg)
	Phase3 = |A3 - B3| (270 deg)

	I = Phase0 - Phase2
	Q = Phase3 - Phase1
	
	Confidence = |I| + |Q|

					     arctan(Q/I)	    C
						------------- * --------
							 2PI		  frec
	Distance (depth) = --------------------------
								   2
 */ 
bool SaveConfidenceAndDepth(BYTE* inputA, BYTE* inputB, BYTE* outputConfidence, double* outputDepth)
{
	size_t imageIndex = 0, depthIndex = 0;

	for (int i = 0; i < IMG_SIZE / IMG_PHASE; i += 2) {
		int *phase = new int[IMG_PHASE]();

		for (int j = 0; j < IMG_PHASE; ++j) {
			phase[j] = RAW12(inputA, (i + (IMG_SIZE / IMG_PHASE) * j)) - 
					   RAW12(inputB, (i + (IMG_SIZE / IMG_PHASE) * j));
		}

		int imgI = phase[0] - phase[2];		/*   0 deg - 180 deg */
		int imgQ = phase[3] - phase[1];		/* 270 deg -  90 deg */

		double angle = atan2((double)imgQ, (double)imgI);
		angle = angle < 0 ? angle + 2 * PI : angle;

		int confidence = abs(imgI) + abs(imgQ);

		outputDepth[depthIndex++] = ((angle / (2 * PI)) * (C / FREC)) / 2;
		outputConfidence[imageIndex++] = (BYTE)(confidence >> 8);
		outputConfidence[imageIndex++] = (BYTE)(confidence & 0x0FF);

		delete[] phase;
	}

	if (imageIndex != IMG_SIZE / IMG_PHASE) {
		cout << "Calculate confidence size error: " << imageIndex << endl;
		return false;
	}

	if (depthIndex != FRAME_SIZE) {
		cout << "Calculate depth size error: " << depthIndex << endl;
		return false;
	}

	return true;
}

bool SaveResult(BYTE* buf, string path, size_t fileSize)
{
	/* save as ${newdir}\${path}_${transferCnt}.raw */
	string filePath(newDir + string("\\") + path + string("_") + to_string(transferCnt) + string(".raw"));
	ofstream fres(filePath, ios::out | ios::binary);

	if (!fres) {
		cout << "Cannot create file: " << filePath << endl;
		return false;
	}

	fres.write((char*)buf, fileSize);
	fres.close();

	cout << filePath << endl;
	return true;
}

bool SaveCSV(double* buf, string path, size_t count) 
{
	/* save as ${newdir}\${path}_${transferCnt}.csv */
	string filePath(newDir + string("\\") + path + string("_") + to_string(transferCnt) + string(".csv"));
	ofstream fres(filePath, ios::out);

	if (!fres) {
		cout << "Cannot create file: " << filePath << endl;
		return false;
	}

	for (int i = 0; i < count; ++i) {
		fres << std::fixed << std::setprecision(8) << buf[i] << ",";
		if (!((i + 1) % IMG_WIDTH)) {
			fres << endl;
		}
	}

	fres.close();

	cout << filePath << endl;
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

		double* depthBuf = new double[FRAME_SIZE]();

		ASSERT(OpenOriginRaw(rawPath, originBuf));
		ASSERT(TransferRaw(originBuf, aBuf, bBuf));
		ASSERT(SaveConfidenceAndDepth(aBuf, bBuf, confidenceBuf, depthBuf));
		ASSERT(SaveResult(aBuf, "a", IMG_SIZE));
		ASSERT(SaveResult(bBuf, "b", IMG_SIZE));
		ASSERT(SaveResult(confidenceBuf, "confidence", IMG_SIZE / IMG_PHASE));
		ASSERT(SaveCSV(depthBuf, "depth", FRAME_SIZE));

		cout << "Transfer success (" << transferCnt++ << ")." << endl;

	CONTINUE:
		delete[] aBuf;
		delete[] bBuf;
		delete[] originBuf;
		delete[] confidenceBuf;
		delete[] depthBuf;

		cout << "Drop target raw file here: ";
	}

	return 0;
}
