#include "stdafx.h"
#include <windivert.h>

#include "sniffer.h"
#include "logger.h"

#define PACKET_SIZE 65535

/* ��������������� ����� ��� TCP-IP ������ */
typedef struct
{
    WINDIVERT_IPHDR  ip;
    WINDIVERT_TCPHDR tcp;
} PACKET, *PPACKET;

/* ��������������� ����� ��� TCP-IP ������ � ������� */
typedef struct 
{
    PACKET header;
    UINT8 data[];
} DATAPACKET, *PDATAPACKET;

const char *find_in_memory( const char *payload, int payload_len, const char *sql_commands[] );
const char *find_in_memory( const char *buff, int len, const char *str );

/**********************************
 �������: sniffer_thread
 ���������: VOID *lpParameter - �������� ������������ � �������, ��� ������� ����.
 ��������: ����� ����� �������� ������. �������� �� TCP-����� ����� � ���� � ������ 
 ���� �� ���������� SQL (SELECT, INSERT, UPDATE, DELETE). � ������ ���������� - 
 �������� ��� ������������ ������� �� ���������� ��������� ������ - � �����-������.
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
 �������: find_in_memory
 ���������: const char *payload - ��������� �� ������ ������
            int payload_len     - ����� ������
            char *sql_commands[]- �������� ����� ��� ������, 
                                  ������ ������������� nullptr
 ��������: ���� � ������ ���� �� �����
 ����������: nullptr - � ������ ���������� ����� ��� ������,
             ��������� �� ������ ����� �� ����� ��� ������ � ������
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
 �������: find_in_memory
 ���������: const char *buff - ��������� �� �����
            const int len    - ����� ������
            const char *str  - ������ ��� ������
 ��������: ���� � ������ ������
 ����������: nullptr - � ������ ���������� ������ � ������,
             ��������� �� ������ ������ � ������
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

