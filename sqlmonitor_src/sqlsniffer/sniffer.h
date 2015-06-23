#ifndef __SNIFFER_H__
#define __SNIFFER_H__

/* ��������������� ����� ��� �������� ���������� � �����-���������� */
class SnifferThreadParams
{
public:
    SnifferThreadParams( HANDLE hDivert, DWORD loggerThreadID ) 
        : handle_(hDivert), 
          log_thread_id_(loggerThreadID)
    {}
    
    HANDLE handle() const {return handle_;}
    DWORD  thread_id() const {return log_thread_id_;}
    
private:
    HANDLE handle_;
    DWORD  log_thread_id_;
};

DWORD WINAPI sniffer_thread( LPVOID lpParameter );

#endif // __SNIFFER_H__
