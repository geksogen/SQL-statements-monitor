#include "stdafx.h"
#include <windivert.h>

#include "sniffer.h"
#include "logger.h"

#define PACKET_SIZE 65535

/* Вспомогательный класс для TCP-IP пакета */
typedef struct
{
    WINDIVERT_IPHDR  ip;
    WINDIVERT_TCPHDR tcp;
} PACKET, *PPACKET;

/* Вспомогательный класс для TCP-IP пакета с данными */
typedef struct 
{
    PACKET header;
    UINT8 data[];
} DATAPACKET, *PDATAPACKET;

const char *find_in_memory( const char *payload, int payload_len, const char *sql_commands[] );
const char *find_in_memory( const char *buff, int len, const char *str );

/**********************************
 Функция: sniffer_thread
 Параметры: VOID *lpParameter - параметр передаваемый в функцию, при запуске нити.
 Описание: Точка входа рабочего потока. Выбирает из TCP-стека пакет и ищет в данных 
 один из операторов SQL (SELECT, INSERT, UPDATE, DELETE). В случае нахождения - 
 отсылает все печатываемые символы от найденного оператора вправо - в поток-логгер.
***********************************/
DWORD WINAPI sniffer_thread( VOID *lpParameter )
{
    SnifferThreadParams *params = (SnifferThreadParams *)lpParameter;

    UINT8 packet[PACKET_SIZE];
    WINDIVERT_ADDRESS addr;
    UINT packet_len;

    const char *sql_commands [] = {
        "insert",
        "select",
        "update",
        "delete",
        "alter",
        "create",
        "drop",
        "grant",
        nullptr
    };

    while( WinDivertRecv( params->handle(), &packet, sizeof(packet), &addr, &packet_len ) )
    {
        WINDIVERT_IPHDR  *ip_header;
        WINDIVERT_TCPHDR *tcp_header;
        void *payload = nullptr;
        UINT payload_len = 0;
        
        WinDivertHelperParsePacket(&packet, packet_len, &ip_header, NULL, NULL, NULL, &tcp_header, NULL, &payload, &payload_len);
        
        if( payload != nullptr ){
            const char *sql_beg = find_in_memory( (const char *)payload, payload_len, sql_commands );
            int count = 0;
            if( sql_beg != nullptr ){
                while( sql_beg+count < ((const char *)payload)+payload_len  && 
                    ( isprint(sql_beg[count]) || sql_beg[count]=='\n') ){
                    ++count;
                }
                std::string sql_request(sql_beg, count);
                
                for( std::size_t pos = sql_request.find("\n"); 
                     pos != std::string::npos; 
                     pos = sql_request.find("\n", pos) ){
                     sql_request.replace( pos, 1, " " );
                }
                log( ip_header->SrcAddr, tcp_header->SrcPort, sql_request );
            }
        }
    }

    return 0;
}

/**********************************
 Функция: find_in_memory
 Параметры: const char *payload - указатель на начало буфера
            int payload_len     - длина буфера
            char *sql_commands[]- перечень строк для поиска, 
                                  должен заканчиваться nullptr
 Описание: Ищет в буфере одну из строк
 Возвращает: nullptr - в случае отсутствия строк для поиска,
             указатель на начало одной из строк для поиска в буфере
***********************************/
const char *find_in_memory( const char *payload, int payload_len, const char *sql_commands[] )
{
    const char *rc = nullptr;
    
    int i=0;
    while( sql_commands[i] != nullptr ){
        rc = find_in_memory( payload, payload_len, sql_commands[i] );
        if( rc != nullptr ){
            return rc;
        }
        ++i;
    }
    
    return rc;
}

/**********************************
 Функция: find_in_memory
 Параметры: const char *buff - указатель на буфер
            const int len    - длина буфера
            const char *str  - строка для поиска
 Описание: Ищет в буфере строку
 Возвращает: nullptr - в случае отсутствия строки в буфере,
             указатель на начало строки в буфере
***********************************/
const char *find_in_memory( const char *buff, int len, const char *str ){
    const int str_len = strlen(str);
    if (len == 0 || len < str_len )
        return nullptr;
    int i,j = 0;
    for ( i = 0; i < len; ++i){
        if( tolower(buff[i]) != tolower(str[j]) ) {
            if( !str[j] ){
                return (char *)buff+i-j;
            }
            if( j > 0 ){
                --i;
                j = 0;
            }
            continue;
        } else {
          ++j;
        }
    }
    if (j == str_len){
        return (char *)buff + i - j;
    }
    return nullptr;
}

