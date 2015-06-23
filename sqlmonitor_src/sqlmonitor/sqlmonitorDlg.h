
// sqlmonitorDlg.h : header file
//

#pragma once

#define START 0
#define STOP  1

// CsqlmonitorDlg dialog
class CsqlmonitorDlg : public CDialogEx
{
// Construction
public:
	CsqlmonitorDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_SQLMONITOR_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnFileChoose();
	afx_msg void OnMonitorLog();
	afx_msg void OnTimer( UINT nIDEvent );
	afx_msg void OnAppChanged();
	
	DECLARE_MESSAGE_MAP()
	
private:
    UINT_PTR m_Timer;
    int m_State = START;

    std::map<std::string, std::vector<std::string>> m_Data;
    std::map<std::string, int> m_App2Ind;
    std::map<int, std::string> m_Ind2App;
    std::map<int, int> m_NewDataCounts;
    std::string m_currApp;

    LONG m_LogOffset = 0x0;
    void PopulateChangesFromLog();
    void ProcessLogItem( const std::string &app, const std::string &item );
    
    std::string getApp( const std::string &str ) const;
    std::string getLog( const std::string &str ) const;
    
    std::string makeAppName(const std::string &app) const;
    void updateListItemCounts( CListBox* lb, int ind );

    void ShowLastError(LPTSTR lpszFunction);
};
