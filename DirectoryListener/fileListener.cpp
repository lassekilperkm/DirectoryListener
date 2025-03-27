#include "fileListener.h"

using namespace std;

atomic<bool> stopListening(false);
thread listenerThread;
FILE* listenerLog = nullptr;

static void ListenInsideDirectory(const wstring& dir)
{
	if (!listenerLog)
		return;

	HANDLE hDirHandle = CreateFile(
		dir.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, 
		NULL
	);

	if (hDirHandle == INVALID_HANDLE_VALUE)
	{
		fprintf_s(listenerLog, "Failed to open directory %ls\n", dir.c_str());
		fclose(listenerLog);
		return;
	}

	while (!stopListening)
	{
		fprintf_s(listenerLog, "Listening for changes in %ls\n", dir.c_str());
		DWORD returnBytes;
		FILE_NOTIFY_INFORMATION buf[1024]{};
		if (ReadDirectoryChangesW(
			hDirHandle,
			buf,
			sizeof(buf),
			TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
			&returnBytes,
			NULL,
			NULL
		)) {
			wstring_convert<codecvt_utf8<wchar_t>> converter;
			string dirStr = converter.to_bytes(dir);
			fprintf_s(listenerLog, "Change detected in %s\n", dirStr.c_str());
		}
	}
	CloseHandle(hDirHandle);
}

extern "C" __declspec(dllexport) void StartListening(wstring dir)
{
	stopListening = false;
	listenerThread = thread(ListenInsideDirectory, dir);
}

extern "C" __declspec(dllexport) void StopListening()
{
	stopListening = true;
}

static BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			listenerLog = _fsopen("listenerLog.txt", "a", _SH_DENYNO);
			break;
		case DLL_PROCESS_DETACH:
			if (listenerThread.joinable())
				listenerThread.join();
			fclose(listenerLog);
			break;
		default:
			break;
	}
}