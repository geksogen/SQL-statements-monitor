#ifndef __LOGGER_H__
#define __LOGGER_H__

/* ID ������-�������*/
extern DWORD idLogger;


DWORD WINAPI logger_thread( LPVOID lpParameter );
void log( UINT32 ip, UINT16 port, const std::string &sql );

#endif // __LOGGER_H__
