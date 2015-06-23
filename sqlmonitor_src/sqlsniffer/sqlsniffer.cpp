// sqlsniffer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windivert.h>

#include "logger.h"
#include "sniffer.h"

#define ERR_NO_DIVERT -2
#define ERR_NO_COMMAND_LINE_ARGS -1
#define ERR_NO_ERRORS 0

#define BUFF_SIZE 1024

/**/
HANDLE hDivert = INVALID_HANDLE_VALUE;

void sniffer_run_threads( HANDLE hDivert, int thread_num );
BOOL WINAPI CtrlC_handler( DWORD dwCtrlType );

/**********************************
 Функция: _tmain
 Параметры: int argc       - количество входных параметров
            _TCHAR* argv[] - массив указателей на входные параметры
 Описание: Точка входа. В качестве входных параметров требует указания 
 номера порта ORACLE LISTENER
***********************************/
int _tmain(int argc, _TCHAR* argv[])
{
    if( argc < 2 ){
        wprintf(L"Usage of sqlsniffer: sqlsniffer.exe <ORACLE_LISTENER_PORT_NUMBER>\n");
        wprintf(L"sqlsniffer outputs log into sqlsniffer.log of current directory.\n");
        return ERR_NO_COMMAND_LINE_ARGS;
    }

    char filter[BUFF_SIZE];
    sprintf_s(filter, BUFF_SIZE, "outbound && ip && tcp.PayloadLength > 0 && tcp.DstPort == %ld", wcstol(argv[1], nullptr, 10) );

    hDivert = WinDivertOpen( filter, WINDIVERT_LAYER_NETWORK, 0, WINDIVERT_FLAG_SNIFF);
    if( INVALID_HANDLE_VALUE == hDivert ){
        wprintf(L"Failed open divert device (%d)\n", GetLastError());
        return ERR_NO_DIVERT;
    }

    if (!DivertSetParam(hDivert, WINDIVERT_PARAM_QUEUE_LEN, 8192)){
        wprintf( L"Failed to set packet queue length\n" );
    }
    if (!DivertSetParam(hDivert, WINDIVERT_PARAM_QUEUE_TIME, 1024)){
        wprintf( L"Failed to set packet queue time\n" );
    }

    sniffer_run_threads( hDivert, 4 );

    return ERR_NO_ERRORS;
}

/**********************************
 Функция: sniffer_run_threads
 Параметры: HANDLE hDivert - хэндл Divert
            int thread_num - количество потоков слушателей
 Описание: Запускает потоки слушатели TCP-пакетов в количестве
 указанном в параметре thread_num, запускает поток-логгер в файл.
 Ожидает завершения всех потоков.
 Прерывание работы программы по Ctrl+C или Ctrl+Brk или можно закрыть 
 консольное окно.
***********************************/
void sniffer_run_threads( HANDLE hDivert, int thread_num ){
    HANDLE *threads = new HANDLE[thread_num];
    HANDLE hLogger  = ::CreateThread( nullptr, 0, logger_thread, 0, 0, &idLogger );
    
    for( int i=0;  i<thread_num;  ++i ){
        threads[i] = ::CreateThread( nullptr, 0, sniffer_thread, new SnifferThreadParams( hDivert, idLogger ), 0, nullptr );
    }
    SetConsoleCtrlHandler( CtrlC_handler, TRUE );
    
    wprintf(L"Processing TCP-packets...\n");
	wprintf(L"Press Ctrl+C for exit\n");
	WaitForMultipleObjects(thread_num, threads, TRUE, INFINITE);
    wprintf(L"Closing worker threads done.\n");
    
    delete threads;
    
    wprintf(L"Closing logger thread...\n");
    PostThreadMessage( idLogger, WM_QUIT, 0, 0 );
    WaitForSingleObject( hLogger, INFINITE );
    wprintf(L"Closing logger thread done.\n");
    
    wprintf(L"See \"sqlsniffer.log\" for sniffed sql.\n");
}

/**********************************
 Функция: CtrlC_handler
 Параметры: DWORD dwCtrlType - тип сигнала
 Описание: Закрывает хэндл divert, что приводит к закрытию программы
***********************************/
BOOL WINAPI CtrlC_handler( DWORD dwCtrlType ){
    switch( dwCtrlType ){
        case CTRL_C_EVENT:
            wprintf(L"Ctrl+C catched. Exiting...\n");
            break;
        case CTRL_BREAK_EVENT:
            wprintf(L"Ctrl+Break catched. Exiting...\n");
            break;
        case CTRL_CLOSE_EVENT:
            wprintf(L"Console window is closing. Exiting...\n");
            break;
    }
    wprintf(L"Closing worker threads...\n");
    WinDivertClose( hDivert );
    return TRUE;
}

