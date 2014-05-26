#include "StdAfx.h"
#include "LocalFile.h"

LocalFile::LocalFile(PTCHAR filename)
{
	file_ = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
}

LocalFile::~LocalFile(void)
{
	if (file_ != INVALID_HANDLE_VALUE)
		CloseHandle(file_);
}

int LocalFile::Write(const void * data, int len)
{
	DWORD bytes;
	return WriteFile(file_, data, len, &bytes, 0);
}
