#include "stdafx.h"
#include "logger.h"

#include <iphlpapi.h>

#define BUFF_SIZE 1024

/* ID потока-логгера*/
DWORD  idLogger = 0;

std::string get_application( UINT32 ip, UINT16 port );

/* Вспомогательный класс для логгирования */
class LogData
{
public:
    LogData( UINT32 ip, UINT16 port, const std::string &str ) 
        : str_(str),
          ip_(ip),
          port_(port)
    {
        ::GetSystemTime( &time_ );
    }
    
    const std::string& str() const {
        return str_;
    }

    UINT32 ip() const {
        return ip_;
    }

    UINT16 port() const {
        return port_;
    }

    const SYSTEMTIME& time() const {
        return time_;
    }
    
private:
    UINT32 ip_;
    UINT16 port_;
    std::string str_;
    SYSTEMTIME time_;
};

/**********************************
 Функция: logger_thread
 Параметры: LPVOID lpParameter - параметр передаваемый в функцию, при запуске нити.
 Описание: Точка входа логгера. Открывается файл "sqlsniffer.log" для дозаписи.
 Из очереди сообщений выбираются данные, помещаемые в лог-файл.
 При получении сообщения WM_QUIT - закрываем файл и выходим.
***********************************/
DWORD WINAPI logger_thread( LPVOID lpParameter ){
    MSG msg;
    
    // открываем лог-файл
    HANDLE hLogFile = ::CreateFile( L"sqlsniffer.log", FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    while( GetMessage( &msg, NULL, 0, 0 ) ){
        LogData *ld = (LogData *)msg.lParam;
        const std::string &sql = ld->str();
        const std::string application = get_application( ld->ip(), ld->port() );
        
        char time[BUFF_SIZE];
        sprintf_s( time, BUFF_SIZE, "%02d-%02d-%04d %02d:%02d:%02d.%03d", ld->time().wDay, ld->time().wMonth, ld->time().wYear,
             ld->time().wHour, ld->time().wMinute, ld->time().wSecond, ld->time().wMilliseconds );
        
        const int len = application.length() + 1 + strlen(time) + 1 + sql.length() + 2 + 1; // \r\n +\0
        char *buff = (char *)::HeapAlloc( GetProcessHeap(), 0, len );
        sprintf_s( buff, len, "%s!%s!%s\r\n", application.c_str(), time, sql.c_str() );
        
        // пишем в лог-файл
        DWORD written;
        ::WriteFile( hLogFile, buff, strlen(buff), &written, NULL);
        ::FlushFileBuffers( hLogFile );
        
        ::HeapFree( GetProcessHeap(), 0, buff );
    }
    
    // закрываем лог-файл
    ::CloseHandle( hLogFile );
    return 0;
}

/**********************************
 Функция: log
 Параметры: const std::string &sql - sql-запрос
 Описание: Отсылка полученного sql-запроса в поток логгера
***********************************/
void log( UINT32 ip, UINT16 port, const std::string &sql ){
    ::PostThreadMessage( idLogger, WM_USER, 0, (LPARAM)new LogData(ip, port, sql) );
}

/**********************************
 Функция: get_application
 Параметры: UINT32 ip   - локальный IP
            UINT16 port - локальный PORT
 Возвращает: std::string содержащий имя EXE файла.
 Описание: По локальному IP:PORT определяет ЕХЕ-файл
***********************************/
std::string get_application( UINT32 ip, UINT16 port ){
    std::string rc = "unknown";
    
    MIB_TCPTABLE2 *tcpTable;
    ULONG ulSize = 0;
    DWORD dwRetVal = 0;
    
    tcpTable = (MIB_TCPTABLE2 *)::HeapAlloc(GetProcessHeap(), 0, sizeof(MIB_TCPTABLE2));
    if (tcpTable == nullptr) {
        wprintf(L"Error allocating memory for TCP table. Oracle client will be unknown.\n");
        return rc;
    }
    ulSize = sizeof (MIB_TCPTABLE2);
    
    if( GetTcpTable2(tcpTable, &ulSize, TRUE) == ERROR_INSUFFICIENT_BUFFER ){
        ::HeapFree(GetProcessHeap(), 0, tcpTable);
        tcpTable = (MIB_TCPTABLE2 *) ::HeapAlloc( GetProcessHeap(), 0, ulSize );
        if (tcpTable == nullptr) {
            wprintf(L"Error allocating memory for TCP table. Oracle client will be unknown.\n");
            return rc;
        }
    }
    
    if( GetTcpTable2(tcpTable, &ulSize, TRUE) == NO_ERROR) {
        for( int i=0;  i<(int)tcpTable->dwNumEntries;  ++i ) {
            const MIB_TCPROW2 &row = tcpTable->table[i];
            if( row.dwLocalAddr == ip  &&
                row.dwLocalPort == port ){
                    char exe_name[MAX_PATH];
                    DWORD exe_len = MAX_PATH;
                    HANDLE hProcess = ::OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, row.dwOwningPid );
                    ::QueryFullProcessImageNameA( hProcess, 0, exe_name, &exe_len );
                    ::CloseHandle( hProcess );
                    rc = exe_name;
                    break;
                }
        }
    }
    return rc;
}

