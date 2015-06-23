
// sqlmonitorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sqlmonitor.h"
#include "sqlmonitorDlg.h"
#include "afxdialogex.h"
#include <strsafe.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define BUFFER_SIZE 128

#define ID_TIMER_UPDATE 1

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CsqlmonitorDlg dialog

CsqlmonitorDlg::CsqlmonitorDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CsqlmonitorDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CsqlmonitorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CsqlmonitorDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_QUERYDRAGICON()
	ON_COMMAND(IDC_LOGFILE_CHOOSE, OnFileChoose)
	ON_COMMAND(IDC_LOGFILE_MONITOR, OnMonitorLog)
	ON_LBN_SELCHANGE(IDC_APPNAMES, OnAppChanged)
END_MESSAGE_MAP()


// CsqlmonitorDlg message handlers

BOOL CsqlmonitorDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CsqlmonitorDlg::OnFileChoose(){
    wchar_t fn_buffer[MAX_PATH]={0};
    OPENFILENAME ofn = {sizeof(OPENFILENAME)};
    ofn.hwndOwner = GetSafeHwnd();
    
    ofn.lpstrFilter = L"sqlsniffer logs\0sqlsniffer.log\0All Files(*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = fn_buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    
    if( GetOpenFileName(&ofn) ){
        ((CEdit *)GetDlgItem(IDC_LOGFILE_NAME))->SetWindowText(ofn.lpstrFile);
        
        // При смене файла при включённом мониторинге
        // 1. Останавливаем таймер
        // 2. Обнуляем смещение
        // 3. Запускаем таймер
        if( m_State == STOP ){
            KillTimer( m_Timer );
            m_LogOffset = 0;
            m_Timer = SetTimer( ID_TIMER_UPDATE, 1500, NULL );
        } else {
            m_LogOffset = 0;
        }
        
        // Обнуляем содержимое
        m_Data.clear();
        m_currApp.clear();
        m_NewDataCounts.clear();
    }
}

void CsqlmonitorDlg::OnMonitorLog(){
    // 1. Запускаем\останавливаем таймер дочитки изменений
    if( m_State == START ){
        m_Timer = SetTimer( ID_TIMER_UPDATE, 1500, NULL );
        m_State = STOP;
    } else {
        KillTimer( m_Timer );
        m_State = START;
    }
    // 2. Меняем статус потока и надписи на кнопках
    GetDlgItem(IDC_LOGFILE_MONITOR)->SetWindowText( m_State == START ? L"Start" : L"Stop" );
}

void CsqlmonitorDlg::OnTimer( UINT nIDEvent ){
    if( nIDEvent == ID_TIMER_UPDATE ){
        PopulateChangesFromLog();
    }
}

void CsqlmonitorDlg::PopulateChangesFromLog(){
    CString file_name;
    ((CEdit *)GetDlgItem(IDC_LOGFILE_NAME))->GetWindowText(file_name);
    HANDLE hLog = ::CreateFile( file_name, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( hLog == INVALID_HANDLE_VALUE ){
        ShowLastError(L"Opening log file");
        return;
    }
    
    DWORD rc = ::SetFilePointer( hLog, m_LogOffset, NULL, FILE_BEGIN );
    if( rc == INVALID_SET_FILE_POINTER ){
        ShowLastError(L"Setting infile position");
        return;
    }
    
    // читаем в буфер
    // чтение всегда начинается с начала строки
    DWORD readed = 0;
    char buffer[BUFFER_SIZE];
    std::string line;
    while( ReadFile( hLog, buffer, BUFFER_SIZE, &readed, NULL )  &&  readed > 0 ){
        DWORD buff_pos = 0;
        
        while( buff_pos < readed ){
            while( buff_pos < readed ){
                line.push_back( buffer[buff_pos] );
                ++buff_pos;
                if( buffer[buff_pos-1] == '\n' )
                    break;
            }
            
            if( line.at( line.length()-1 ) != '\n' ){
                continue;
            }
            
            // process line
            const std::string app = getApp(line);
            const std::string log = getLog(line);
            
            ProcessLogItem( app, log );
            
            m_LogOffset += line.length();
            line.clear();
        }
    }

    ::CloseHandle( hLog );
}

void CsqlmonitorDlg::ProcessLogItem( const std::string &app, const std::string &item ){
    USES_CONVERSION;
    
    bool no_app = (m_Data.find( app ) == m_Data.end());

    CListBox *appsListCtrl = (CListBox *)GetDlgItem( IDC_APPNAMES );
    CEdit    *contentCtrl = (CEdit *)GetDlgItem( IDC_LOGFILE_CONTENT );

    const std::string visible = makeAppName(app);
    
    if( no_app ){
        int ind = appsListCtrl->AddString( A2W(visible.c_str()) );
        m_App2Ind[visible] = ind;
        m_Ind2App[ind] = app;
    }

    m_Data[app].push_back( item );
    
    if( m_currApp.empty() ){
        m_currApp = app;
        appsListCtrl->SetCurSel( m_App2Ind[makeAppName(app)] );
    }
    
    if( m_currApp == app ){
        // Добавить в EDITBOX новую строку лога
        CString content;
        contentCtrl->GetWindowText( content );
        content += A2W( item.c_str() );
        contentCtrl->SetWindowText( content );
    } else {
        // Прибавить +1 к количеству новых строк в LISTBOX
        int ind = m_App2Ind[visible];
        ++m_NewDataCounts[ind];
        
        updateListItemCounts( appsListCtrl, ind );
    }
}

void CsqlmonitorDlg::updateListItemCounts( CListBox* lb, int ind ){
    USES_CONVERSION;
    
    int currSel = lb->GetCurSel();
    
    lb->DeleteString( ind );
    
    const std::string visible = makeAppName( m_Ind2App[ind] );
    
    char tmp[256];
    int new_recs = m_NewDataCounts[ind];
    if( new_recs == 0 ){
        sprintf_s( tmp, 256, "%s", visible.c_str() );
    } else {
        sprintf_s( tmp, 256, "(%d) %s", new_recs, visible.c_str() );
    }
    
    lb->InsertString(ind, A2W(tmp));
    lb->SetCurSel( currSel );
}

std::string CsqlmonitorDlg::makeAppName(const std::string &app) const
{
    std::size_t pos = app.find_last_of("\\/");
    if( pos != std::string::npos ){
        return app.substr(pos+1, app.length()-(pos+1));
    }
    return app;
}

std::string CsqlmonitorDlg::getApp( const std::string &str ) const
{
    std::size_t pos = str.find("!");
    return str.substr( 0, pos );
}

std::string CsqlmonitorDlg::getLog( const std::string &str ) const
{
    std::size_t pos = str.find("!");
    return str.substr( pos+1, str.length()-(pos+1) );
}

void CsqlmonitorDlg::ShowLastError(LPTSTR lpszFunction){
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    
    DWORD dw = GetLastError();
    
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );
        
    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
        
    StringCchPrintf((LPTSTR)lpDisplayBuf,  LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf); 
    MessageBox((LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree( lpMsgBuf );
    LocalFree( lpDisplayBuf );
}


void CsqlmonitorDlg::OnAppChanged(){
    CEdit    *contentCtrl = (CEdit *)GetDlgItem( IDC_LOGFILE_CONTENT );
    CListBox *appsListCtrl = (CListBox *)GetDlgItem( IDC_APPNAMES );
    int ind = appsListCtrl->GetCurSel();
    m_NewDataCounts[ind] = 0;
    updateListItemCounts( appsListCtrl, ind );
    m_currApp = m_Ind2App[ind];
    
    USES_CONVERSION;
    CString str;
    const std::vector<std::string> &log = m_Data[m_currApp];
    
    for( UINT i=0;  i<log.size();  ++i ){
        str += A2W(log[i].c_str());
    }
    contentCtrl->SetWindowText(str);
}

void CsqlmonitorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CsqlmonitorDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CsqlmonitorDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

