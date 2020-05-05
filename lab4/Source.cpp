#include <Windows.h>
#include <tchar.h>
#include "resource.h"
#include <time.h>
#define N 5 /*количество точек на графике*/
#ifdef UNICODE /*возможность использования UNICODE и C строк*/
#define _tstrtod wcstod 
#else
#define _tstrtod strtod
#endif

struct LINE_COLOR //цвет линии графика
{
	int nColorR;
	int nColorG;
	int nColorB;
};
struct GRAPH_POINT //хранение точек графика
{
	double x;
	double y;
};
struct INF_FOR_FIRST_THREAD //информация для первого потока
{
	TCHAR str_x[N][80];//массив X координат
	TCHAR str_y[N][80];//массив Y координат
	GRAPH_POINT pt[N];//набор точек графика
	int precision;//число принимаемых знаков от ввода пользователя
	GRAPH_POINT graphs[30][N];
	int count = 0;
	bool printPoint[30][N];
	int heightMax, widthMax;
	bool flagEdit[N]; //флаги присутствия данных в полях ввода
	int add;
};
struct INF_FOR_SECOND_THREAD //информация для второго потока
{
	LINE_COLOR color;//цвет линии
	HWND hMainWnd; //дескриптор главного окна приложения
	DWORD time;//время обновления графика
};

HWND hDlg; //дескриптор диалога, содержащего поля для ввода данных
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);//Оконная процедура главного окна
BOOL CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);//Процедура диалога
DWORD WINAPI ThreadFirstFunc(LPVOID pvParam);//функция первого потока
DWORD WINAPI ThreadSecondFunc(LPVOID pvParam);//функция второго потока

void GetHeightWidth(GRAPH_POINT* pt, int& height, int& width, int add);//получение высоты и ширины области вывода графика
void ToDouble(TCHAR str_x[N][80], TCHAR str_y[N][80], GRAPH_POINT* pt);//преобразования строк из полей ввода в число
void IsEmptyEdit(bool* flagEdit);//проверка на пустоту полей ввода

bool flagNewGraph = false;//флаг нажатия кнопки для рисования нового графика
bool flagNewColor = false;
bool firstLunch = true;

CRITICAL_SECTION csTable;
CRITICAL_SECTION csColor;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	srand(time(NULL));
	HWND hWnd;
	MSG msg; 
	WNDCLASS wc;
	TCHAR szClassName[] = _T("lab4");
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = szClassName;
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClass(&wc))
		return 0;
	hWnd = CreateWindow(szClassName, _T("Лаб №4"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);
	hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgProc);
	ShowWindow(hWnd, nCmdShow);
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(hDlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HANDLE hThreadFirst, hThreadSecond;
	static int sx, sy;//клиентские координаты
	HDC hdc;
	PAINTSTRUCT ps; 
	static INF_FOR_FIRST_THREAD infFirstThread;
	static INF_FOR_SECOND_THREAD infSecondThread;
	infFirstThread.precision = 5;//число считываемых знаков
	infFirstThread.add = 10;
	infSecondThread.time = 10000;//время обновления графика
	infSecondThread.hMainWnd = hWnd;//дескриптор главного окна
	HPEN oldPen;
	static LINE_COLOR color[30];
	switch (msg)
	{
	case WM_CREATE:
		InitializeCriticalSection(&csTable);
		InitializeCriticalSection(&csColor);
		hThreadFirst = CreateThread(NULL, 0, ThreadFirstFunc, &infFirstThread, 0, NULL); //правая половина окна - сбор данных
		if (!hThreadFirst)
			MessageBox(hWnd, _T("Не удалось создать первый поток"), _T("Ошибка!"), MB_OK);
		hThreadSecond = CreateThread(NULL, 0, ThreadSecondFunc, &infSecondThread, 0, NULL); //левая половина окна - график
		if (!hThreadSecond)
			MessageBox(hWnd, _T("Не удалось создать второй поток"), _T("Ошибка!"), MB_OK);
		break;
	case WM_SIZE:
		sx = LOWORD(lParam);
		sy = HIWORD(lParam);
		MoveWindow(hDlg, sx / 2, 0, sx / 2, sy, TRUE);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EnterCriticalSection(&csTable);
		EnterCriticalSection(&csColor);
		color[infFirstThread.count].nColorB = infSecondThread.color.nColorB;
		color[infFirstThread.count].nColorG = infSecondThread.color.nColorG;
		color[infFirstThread.count].nColorR = infSecondThread.color.nColorR;
		LeaveCriticalSection(&csColor);
	
		SetMapMode(hdc, MM_ANISOTROPIC);//установка режима
		SetWindowExtEx(hdc, infFirstThread.widthMax, -infFirstThread.heightMax, NULL);//установка логического размера вывода
		SetViewportExtEx(hdc, sx / 2, sy, NULL);//установка физического размера
		SetViewportOrgEx(hdc, sx / 4, sy / 2, NULL);//установка начала координат
		MoveToEx(hdc, -infFirstThread.widthMax / 2, 0, NULL);
		LineTo(hdc, infFirstThread.widthMax / 2, 0);//вывод оси X
		MoveToEx(hdc, 0, infFirstThread.heightMax / 2, NULL);
		LineTo(hdc, 0, -infFirstThread.heightMax / 2);//вывод оси Y
		for (int k = 0; k <= infFirstThread.count; k++)
		{
			SelectObject(hdc, CreatePen(PS_SOLID, NULL, RGB(color[k].nColorR,
				color[k].nColorG, color[k].nColorB)));
			for (int i = 1; i < N; i++)
			{
				if (infFirstThread.printPoint[k][i] != false)
				{
					MoveToEx(hdc, infFirstThread.graphs[k][i - 1].x, infFirstThread.graphs[k][i - 1].y, NULL);
					LineTo(hdc, infFirstThread.graphs[k][i].x, infFirstThread.graphs[k][i].y);
				}
			}
		}
		LeaveCriticalSection(&csTable);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		DeleteCriticalSection(&csTable);
		DeleteCriticalSection(&csColor);
		CloseHandle(hThreadFirst);
		CloseHandle(hThreadSecond);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

BOOL CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON1:
			SendMessage(GetDlgItem(hWnd, IDC_EDIT1), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT2), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT3), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT4), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT5), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT6), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT7), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT8), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT9), WM_SETTEXT, WPARAM(0), LPARAM(""));
			SendMessage(GetDlgItem(hWnd, IDC_EDIT10), WM_SETTEXT, WPARAM(0), LPARAM(""));
			flagNewGraph = true;
			flagNewColor = true;
			return TRUE;
		default:
			return FALSE;
		}
	default:
		break;
	}
	return FALSE;
}

DWORD WINAPI ThreadFirstFunc(LPVOID pvParam)
{
	INF_FOR_FIRST_THREAD* inf = (INF_FOR_FIRST_THREAD*)pvParam;
	while (true)
	{
		EnterCriticalSection(&csTable);
		GetDlgItemText(hDlg, IDC_EDIT1, inf->str_x[0], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT2, inf->str_y[0], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT3, inf->str_x[1], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT4, inf->str_y[1], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT5, inf->str_x[2], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT6, inf->str_y[2], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT7, inf->str_x[3], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT8, inf->str_y[3], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT9, inf->str_x[4], inf->precision);
		GetDlgItemText(hDlg, IDC_EDIT10, inf->str_y[4], inf->precision);
		ToDouble(inf->str_x, inf->str_y, inf->pt);
		IsEmptyEdit(inf->flagEdit);
		if (flagNewGraph == true)
		{
			inf->count++;
			flagNewGraph = false;
		}
		for (int i = 0; i < N; i++)
			if (inf->flagEdit[i] != false)
			{
				inf->printPoint[inf->count][i] = true;
				inf->graphs[inf->count][i].x = inf->pt[i].x;
				inf->graphs[inf->count][i].y = inf->pt[i].y;
			}
			else
				inf->printPoint[inf->count][i] = false;
		int height = 0, width = 0;
		GetHeightWidth(inf->graphs[inf->count], height, width, inf->add);
		if (abs(height) > abs(inf->heightMax))
			inf->heightMax = height;
		if (abs(width) > abs(inf->widthMax))
			inf->widthMax = width;
		LeaveCriticalSection(&csTable);
		Sleep(5000);
	}
	return 0;
}

DWORD WINAPI ThreadSecondFunc(LPVOID pvParam)
{
	INF_FOR_SECOND_THREAD* inf = (INF_FOR_SECOND_THREAD*)pvParam;
	while (true)
	{
		if (firstLunch || flagNewColor)
		{
			EnterCriticalSection(&csColor);
			inf->color.nColorB = rand() % 255;
			inf->color.nColorG = rand() % 255;
			inf->color.nColorR = rand() % 255;
			LeaveCriticalSection(&csColor);
			firstLunch = false;
			flagNewColor = false;
		}
		Sleep(inf->time);
		InvalidateRect(inf->hMainWnd, NULL, true);
	}
	return 0;
}

void GetHeightWidth(GRAPH_POINT* pt, int& height, int& width, int add)
{
	GRAPH_POINT min_point;
	GRAPH_POINT max_point;
	min_point = max_point = pt[0];
	for (int i = 1; i < N; i++)
	{
		if (pt[i].y < min_point.y)
			min_point.y = pt[i].y;
		if (pt[i].x < min_point.x)
			min_point.x = pt[i].x;
		if (pt[i].y > max_point.y)
			max_point.y = pt[i].y;
		if (pt[i].x > max_point.x)
			max_point.x = pt[i].x;
	}
	if (abs((int)max_point.y) > abs((int)min_point.y))
		height = 2 * abs((int)max_point.y) + add;
	else
		height = 2 * abs((int)min_point.y) + add;
	if (abs((int)max_point.x) > abs((int)min_point.x))
		width = 2 * abs((int)max_point.x) + add;
	else
		width = 2 * abs((int)min_point.x) + add;
	return;
}

void ToDouble(TCHAR str_x[N][80], TCHAR str_y[N][80], GRAPH_POINT* pt)
{
	for (int i = 0; i < N; i++)
	{
		pt[i].x = _tstrtod(str_x[i], 0);
		pt[i].y = _tstrtod(str_y[i], 0);
	}
	return;
}

void IsEmptyEdit(bool *flagEdit)
{
	int size_x, size_y;
	size_x = SendMessage(GetDlgItem(hDlg, IDC_EDIT1), EM_LINELENGTH, -1, 0);
	size_y = SendMessage(GetDlgItem(hDlg, IDC_EDIT2), EM_LINELENGTH, -1, 0);
	if (size_x == 0 || size_y == 0)
		flagEdit[0] = false;
	else
		flagEdit[0] = true;
	size_x = SendMessage(GetDlgItem(hDlg, IDC_EDIT3), EM_LINELENGTH, -1, 0);
	size_y = SendMessage(GetDlgItem(hDlg, IDC_EDIT4), EM_LINELENGTH, -1, 0);
	if (size_x == 0 || size_y == 0)
		flagEdit[1] = false;
	else
		flagEdit[1] = true;
	size_x = SendMessage(GetDlgItem(hDlg, IDC_EDIT5), EM_LINELENGTH, -1, 0);
	size_y = SendMessage(GetDlgItem(hDlg, IDC_EDIT6), EM_LINELENGTH, -1, 0);
	if (size_x == 0 || size_y == 0)
		flagEdit[2] = false;
	else
		flagEdit[2] = true;
	size_x = SendMessage(GetDlgItem(hDlg, IDC_EDIT7), EM_LINELENGTH, -1, 0);
	size_y = SendMessage(GetDlgItem(hDlg, IDC_EDIT8), EM_LINELENGTH, -1, 0);
	if (size_x == 0 || size_y == 0)
		flagEdit[3] = false;
	else
		flagEdit[3] = true;
	size_x = SendMessage(GetDlgItem(hDlg, IDC_EDIT9), EM_LINELENGTH, -1, 0);
	size_y = SendMessage(GetDlgItem(hDlg, IDC_EDIT10), EM_LINELENGTH, -1, 0);
	if (size_x == 0 || size_y == 0)
		flagEdit[4] = false;
	else
		flagEdit[4] = true;
	return;
}
