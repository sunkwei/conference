#pragma once

/// 对应一个只写的文件
class LocalFile
{
	HANDLE file_;

public:
	// 构造既创建，如果失败，抛出异常
	LocalFile(PTCHAR filename);	// 完整目录名字
	~LocalFile(void);

	// 写入文件
	int Write(const void * data, int len);
};
