// WorkWithDLL.cpp : Определяет точку входа для приложения.
//

/*
- Разработать DLL с функцией, выполняющей поиск по всей виртуальной памяти заданной строки и замену ее на другую строку.
- Разработать программу, которая выполняет статический импорт DLL и вызывает ее функцию.
- Разработать программу, которая выполняет динамический импорт DLL и вызывает ее функцию.
- Разработать программу, которая внедряет DLL в заданный процесс с помощью дистанционного потока и вызывает функцию DLL.
*/


#include "framework.h"
#include "WorkWithDLL.h"
#include "MathLib.h"

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна
typedef int (*Summary)(int x);
Summary sumfunc;
// Динамически загруженный модуль.
HMODULE hModule;
int error_num = 0;
int moduleNameSize;
wchar_t bufModuleName[MAX_PATH];
HWND hAdditWnd;

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL WINAPI         InjectLib(HWND, DWORD, PCWSTR);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Разместите код здесь.

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WORKWITHDLL, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WORKWITHDLL));

    MSG msg;

    /*
     HANDLE CreateRemoteThread(

     HANDLE hProcess,                         // дескриптор процесса
     LPSECURITY_ATTRIBUTES lpThreadAttributes,// дескриптор защиты (SD)
     SIZE_T dwStackSize,                      // размер начального стека
     LPTHREAD_START_ROUTINE lpStartAddress,   // функция потока
     LPVOID lpParameter,                      // аргументы потока
     DWORD dwCreationFlags,                   // параметры создания
     LPDWORD lpThreadId                       // идентификатор потока
     ); 
    */

    DWORD dwProcessId = 18588;
    PCWSTR pszLibFile = L"C://proc/SwiftKiller.dll";
    if (InjectLib(hAdditWnd, dwProcessId, pszLibFile))
        MessageBox(hAdditWnd, L"Lib is injected", L"Message", MB_OK);
    else
    {
        MessageBox(hAdditWnd, L"Lib failed to inject", L"Message", MB_OK);
    }

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

BOOL WINAPI InjectLib(HWND hWnd, DWORD dwProcessId, PCWSTR pszLibFile)
{
    BOOL fOk = FALSE; 
    HANDLE hProcess = NULL, hThread = NULL;
    PWSTR pszLibFileRemote = NULL;

    __try
    {
        hProcess = OpenProcess(
            PROCESS_ALL_ACCESS,
            FALSE, dwProcessId);

        // Количество байт для строки с именем DLL.
        int cch = 1 + lstrlenW(pszLibFile);
        int cb = cch * sizeof(WCHAR);

        // Выделение блока памяти под строку.
        pszLibFileRemote = (PWSTR)VirtualAllocEx(
            hProcess, // Дескриптор процесса.
            NULL, // Желаемый начальный адрес выбирает функция (если NULL).
            cb, // Размер аллоцируемой (выделяемой) памяти.
            MEM_COMMIT, // Тип выделяемой памяти.
            PAGE_READWRITE // Разрешение писать/читать выделенный блок.
        );
        if (pszLibFileRemote == NULL)
        {
            MessageBox(hWnd, L"Cannot allocate virtual memory", L"Message", MB_OK);
            error_num = GetLastError();
            __leave;
        }

        // Копирование строки с адресом внедряемой DLL в адресное пространство удаленного процесса.
        if (!WriteProcessMemory(hProcess, pszLibFileRemote, (PVOID)pszLibFile, cb, 0))
        {
            MessageBox(hWnd, L"Cannot write to allocated virtual memory", L"Message", MB_OK);
            error_num = GetLastError();
            __leave;
        }

        // Получаем истинный адрес LoadLibraryW в Kernel32.dll.
        PTHREAD_START_ROUTINE pfnThreadRtn = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "LoadLibraryW");
        if (pfnThreadRtn == NULL)
        {
            MessageBox(hWnd, L"Cannot get procedure address 'LoadLibraryW'", L"Message", MB_OK);
            error_num = GetLastError();
            __leave;
        }

        // Создание удаленного потока, вызывающего LoadLibraryW.
        hThread = CreateRemoteThread(hProcess, 0, 0, pfnThreadRtn, pszLibFileRemote, 0, 0);
        if (hThread == NULL)
        {
            MessageBox(hWnd, L"Cannot create remote thread", L"Message", MB_OK);
            error_num = GetLastError();
            __leave;
        }

        WaitForSingleObject(hThread, INFINITE);

        fOk = TRUE;
    }
    __finally
    {
        if (pszLibFileRemote != NULL)
            VirtualFreeEx(hProcess, pszLibFileRemote, 0, MEM_RELEASE);
        if (hThread != NULL)
            CloseHandle(hThread);
        if (hProcess != NULL)
            CloseHandle(hProcess);
    }
    return(fOk);
}

//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WORKWITHDLL));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WORKWITHDLL);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   hAdditWnd = hWnd;

   return TRUE;
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    wchar_t buf[100];
    int x = 10;
    int y = 6;

    switch (message)
    {
    case WM_CREATE:
        {
            // Вариант 1.
            //sumfunc = reinterpret_cast<int(*)(int, int)>(GetProcAddress(LoadLibrary(L"DLLGenerator.dll"), "Sum"));

            // Вариант 2.
            //HMODULE hModule = LoadLibrary(L"DLLGenerator.dll");
            //sumfunc = (Summary)GetProcAddress(hModule, "sqr");             
        }
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Разобрать выбор в меню:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // Вызов функции Sum из подключенной библиотеки DLLGenerator.dll.
            //TextOut(hdc, 200, 200, buf, wsprintf(buf, L"(Статический способ): %d + %d = %d", x, y, Sum(x, y)));
            //TextOut(hdc, 200, 250, buf, wsprintf(buf, L"(Статический способ): %d - %d = %d", x, y, Sub(x, y)));
            //TextOut(hdc, 200, 300, buf, wsprintf(buf, L"(Динамический способ): %d * %d = %d", x, x, sumfunc(x)));
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        FreeLibrary(hModule);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
