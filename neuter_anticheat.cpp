// neuter_anticheat.cpp : 
// 1. The code grabs a handle to the target process using OpenProcessByName.
// 2. Once it's handle is aquired, I grab its process ID using GetProcessId.
// 3. Armed with this information, I'm able to take a snapshot of all the running threads and I match them with the target process.
// 4. I compare the thread start times and kill all but the one with the earliest start time(the main thread).
// 5. In the main thread, there's an infinite loop checking whether or not it's threads have been tampered with. To disable the check, I suspend the main thread using... SuspendThread.
// 6. Using WriteProcessMemory, I change the jump if zero to always jump using writeprocessmemory.
// 7. The main thread is then resumed and then at this is as far as I've gotten.

#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

BOOL NeuterAntiCheat(DWORD dwOwnerPID, HANDLE RemoteProcessHandle);
HANDLE OpenProcessByName(LPCTSTR Name, DWORD dwAccess);
BYTE JUMP_PATCH[] = { 0xE9, 0xA5, 0x00, 0x00, 0x00, 0x90 };

int main(void)
{
	HANDLE remoteProcessHandle = OpenProcessByName(L"UsermodeAntiCheat.exe", PROCESS_ALL_ACCESS);
	DWORD  remoteProcessId = GetProcessId(remoteProcessHandle);
	NeuterAntiCheat(remoteProcessId, remoteProcessHandle);
	CloseHandle(remoteProcessHandle);
	return 0;
}

BOOL NeuterAntiCheat(DWORD dwOwnerPID, HANDLE RemotePRocessHandle)
{
	HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;

	// Take a snapshot of all running threads  
	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
		return(FALSE);

	// Fill in the size of the structure before using it. 
	te32.dwSize = sizeof(THREADENTRY32);

	// Retrieve information about the first thread,
	// and exit if unsuccessful
	if (!Thread32First(hThreadSnap, &te32))
	{
		std::cout << L"There was an error with Thread32First: " << GetLastError() << "\n"; // Show cause of failure
		CloseHandle(hThreadSnap);     // Must clean up the snapshot object!
		return(FALSE);
	}
  
  // Needed to avoid terminating processes main thread
  // and patch jump instruction
	SYSTEMTIME mainThreadTime{ 0 };
	HANDLE mainThreadHandle = NULL;
  
  // Now walk the thread list of the system,
	// and display information about each thread
	// associated with the specified process
	do
	{
		if (te32.th32OwnerProcessID == dwOwnerPID)
		{
			FILETIME creationTime, exitTime, kernelTime, userTime;
			SYSTEMTIME humanTime;

			// Get a handle to the current thread
			HANDLE currentThreadHandle = OpenThread(THREAD_ALL_ACCESS, NULL, te32.th32ThreadID);
      
      // Get threads creation time
			GetThreadTimes(currentThreadHandle, &creationTime, &exitTime, &kernelTime, &userTime);
			FileTimeToSystemTime(&creationTime, &humanTime);
			
      // initialize the main thread on the first iteration
			if (mainThreadHandle == NULL) {
				mainThreadHandle = currentThreadHandle;
				mainThreadTime.wMilliseconds = humanTime.wMilliseconds;
				continue;
			}
      // if the current thread has an earlier start time than the previously stored
      // thread, replace it with this one. (Main thread will always have earliest creation time)
			else if (humanTime.wMilliseconds < mainThreadTime.wMilliseconds) {
				std::cout << "Killing thread with millisecond start time: " << mainThreadTime.wMilliseconds << "\n";
				if (!TerminateThread(mainThreadHandle, NULL)) {
					std::cerr << "\nCouldn't kill thread:" << GetLastError();
				}
				mainThreadHandle = currentThreadHandle;
				mainThreadTime.wMilliseconds = humanTime.wMilliseconds;
			}
      // kill the current thread as it has a later start time than the stored "main thread"
			else {
				std::cout << "Killing thread with millisecond start time: " << humanTime.wMilliseconds << "\n";
				if (!TerminateThread(currentThreadHandle, NULL)) {
					std::cout << "\nCouldn't kill thread:" << GetLastError();
				}
			}

			CloseHandle(currentThreadHandle);
		}
	} while (Thread32Next(hThreadSnap, &te32));
  
  // suspend thread main thread so integrity check isn't triggered
	SuspendThread(mainThreadHandle);
  // patch the integrity check
	if (!WriteProcessMemory(RemotePRocessHandle, (LPVOID)0x00007FF756301E9B, &JUMP_PATCH, 6, NULL)) {
		std::cerr << "WriteprocessMemory failing: " << GetLastError();
	}
  
  // resume thread and clean up
	ResumeThread(mainThreadHandle);
	CloseHandle(mainThreadHandle);

	std::cout << "\n";

	//  Don't forget to clean up the snapshot object.
	CloseHandle(hThreadSnap);
	return(TRUE);
}

// returns a handle to the first process that contains the same window name as @Param1: Name
HANDLE OpenProcessByName(LPCTSTR Name, DWORD dwAccess)
{
  // Take a snapshot of all the running processes
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 pe;
		ZeroMemory(&pe, sizeof(PROCESSENTRY32));
		pe.dwSize = sizeof(PROCESSENTRY32);
    
    // read the first processes information into a PROCESSENTRY32 object
		Process32First(hSnap, &pe);
    // iterate through all the processes captured in our snapshot
		do
		{
      // compare the snapshot process window name with @Param1
      // and return it's handle if it matches
			if (!lstrcmpi(pe.szExeFile, Name))
			{
				return OpenProcess(dwAccess, 0, pe.th32ProcessID);
			}
		} while (Process32Next(hSnap, &pe));

	}
  // process with windowname @param1 not found
	return INVALID_HANDLE_VALUE;
}
