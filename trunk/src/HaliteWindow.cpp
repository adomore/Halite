
//         Copyright E�in O'Callaghan 2006 - 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <boost/format.hpp>
#include <boost/bind.hpp>

#include "stdAfx.hpp"
#include "Halite.hpp"
#include "HaliteWindow.hpp"

#include "CSSFileDialog.hpp"
#include "RadioPaneDlg.hpp"

#include "HaliteDialog.hpp"
#include "AdvHaliteDialog.hpp"
#include "AddTorrentDialog.hpp"
#include "NewTorrentDialog.hpp"

#include "ConfigOptions.hpp"
#include "halConfig.hpp"

HaliteWindow::HaliteWindow(unsigned areYouMe = 0) :
	iniClass("HaliteWindow", "HaliteWindow"),
	haliteList(*this),
	WM_AreYouMe_(areYouMe),
	splitterPos(100),
	use_tray(true),
	advancedUI(false),
	closeToTray(false),
	confirmClose(true),
	activeTab(0)
{
	rect.top = 10;
	rect.left = 10;
	rect.bottom = 430;
	rect.right = 620;
	
	load();
}

HaliteWindow::~HaliteWindow()
{
	save();
	ATLASSERT(!::IsWindow(m_hWnd));
}

BOOL HaliteWindow::PreTranslateMessage(MSG* pMsg)
{
	if(CFrameWindowImpl<HaliteWindow>::PreTranslateMessage(pMsg))
		return TRUE;

	if (!advancedUI)
		return mp_dlg->PreTranslateMessage(pMsg);
	else
		return mp_advDlg->PreTranslateMessage(pMsg);
}

LRESULT HaliteWindow::OnCreate(LPCREATESTRUCT lpcs)
{
	try
	{
	HAL_DEV_MSG(L"HaliteWindow::OnCreate");
	
	SetWindowText(L"Halite");
	MoveWindow(rect.left, rect.top,	rect.right-rect.left, rect.bottom-rect.top, false);	


	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Loading Halite config...")));
	hal::config().load();
	
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Applying setting...")));
	if (!hal::config().settingsChanged())
	{
		hal::event().post(boost::shared_ptr<hal::EventDetail>(
			new hal::EventDebug(hal::Event::critical, hal::app().res_wstr(HAL_WINDOW_SOCKETS_FAILED))));
			
		MessageBox(hal::app().res_wstr(HAL_WINDOW_SOCKETS_FAILED).c_str(), 0, 0);
		
		DestroyWindow();
		return 0;
	}
	
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Starting GUI...")));
	
	RECT rc; GetClientRect(&rc);
	SetMenu(0);
	
	//Init ToolBar
	HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);
	
	// Init ReBar
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);
	
	// Init the StatusBar	
	m_hWndStatusBar = m_StatusBar.Create(*this);
	UIAddStatusBar(m_hWndStatusBar);
	
	int panes[] = {ID_DEFAULT_PANE, IDPANE_FILTER, IDPANE_DHT, IDPANE_STATUS};
	m_StatusBar.SetPanes(panes, 4, false);
	
	// Create the Splitter Control
	m_Split.Create(m_hWnd, rc, NULL, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN);
	m_Split.SetSplitterExtendedStyle(!SPLIT_PROPORTIONAL, SPLIT_PROPORTIONAL);
	m_Split.SetSplitterPos(splitterPos);
	
	m_hWndClient = m_Split.m_hWnd;

	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Creating main listview...")));	
	// Create ListView and Dialog
	haliteList.Create(m_Split.m_hWnd, rc, NULL, 
		LVS_REPORT|WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|LVS_SHOWSELALWAYS);
	haliteList.manager().attach(bind(&HaliteWindow::issueUiUpdate, this));


	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Creating classic dialog...")));		
	mp_dlg.reset(new HaliteDialog(*this)),
	mp_dlg->Create(m_Split.m_hWnd);
//	mp_dlg->ShowWindow(true);
	

	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Creating advanced dialog...")));
	mp_advDlg.reset(new AdvHaliteDialog(*this));
	mp_advDlg->Create(m_Split.m_hWnd);
//	mp_advDlg->ShowWindow(true);
	
//	m_Split.SetSplitterPanes(*mp_list, *mp_dlg);
	
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Creating tray icon...")));	
	// Create the tray icon.
	trayIcon_.Create(this, IDR_TRAY_MENU, L"Halite", 
		CTrayNotifyIcon::LoadIconResource(IDR_APP_ICON), WM_TRAYNOTIFY, IDR_TRAY_MENU);
	trayIcon_.Hide();
	
	// Add ToolBar and register it along with StatusBar for UIUpdates
	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	UISetCheck(IDR_TRAY_MENU, 1);
	
	// Register UIEvents and the timer for the monitoring interval
	SetTimer(ID_UPDATE_TIMER, 500);
	SetTimer(ID_SAVE_TIMER, 5000);
	connectUiUpdate(bind(&HaliteWindow::updateWindow, this));
	
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Registering drop target...")));	
	RegisterDropTarget();
	
	// Register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	assert(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);
	
//	haliteList.manager().setSelected(0);
	setCorrectDialog();
	
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Starting event reciever...")));
	hal::bittorrent().startEventReceiver();
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"Initial setup complete!")));
	issueUiUpdate();
	
	}
	catch(const std::exception& e)
	{
		hal::event().post(boost::shared_ptr<hal::EventDetail>(
			new hal::EventStdException(hal::Event::critical, e, L"HaliteWindow::OnCreate"))); 

		DestroyWindow();
	}
	
	return 0;
}

LRESULT HaliteWindow::OnAdvanced(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	advancedUI = !advancedUI;
	setCorrectDialog();
	
	return 0;
}

LRESULT HaliteWindow::OnTrayNotification(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam)
{
    trayIcon_.OnTrayNotification(wParam, lParam);
    
    return 0;
}

void HaliteWindow::setCorrectDialog()
{
	if (!advancedUI)
	{		
		mp_dlg->ShowWindow(SW_SHOW);
		mp_advDlg->ShowWindow(SW_HIDE);
		m_Split.SetSplitterPanes(haliteList, *mp_dlg);
	}
	else
	{		
		mp_dlg->ShowWindow(SW_HIDE);
		mp_advDlg->ShowWindow(SW_SHOW);
		m_Split.SetSplitterPanes(haliteList, *mp_advDlg);
	}
	ui().update();
}

void HaliteWindow::updateWindow()
{
	try
	{
	
	hal::SessionDetail details = hal::bittorrent().getSessionDetails();
	
	if (details.port > -1)
		UISetText(0, 
			(wformat(hal::app().res_wstr(HAL_PORT_OPEN)) % details.port ).str().c_str());	
	else
		UISetText(0, hal::app().res_wstr(HAL_NOT_LISTENING).c_str());
	
	wstring downloadRates = (wformat(hal::app().res_wstr(HAL_DOWN_RATES)) 
			% (details.speed.first/1024) 
			% (details.speed.second/1024)).str();
	
	UISetText(3, downloadRates.c_str());	
	trayIcon_.SetTooltipText(downloadRates.c_str());
	
	if (details.dht_on)
	{
		wstring dht = (wformat(hal::app().res_wstr(HAL_DHT_ON))
			% details.dht_nodes).str();
			
		UISetText(2, dht.c_str());
	}
	else
	{
		UISetText(2, hal::app().res_wstr(HAL_DHT_OFF).c_str());
	}
	
	if (details.ip_filter_on)
	{
		wstring filter = (wformat(hal::app().res_wstr(HAL_IPFILTER_ON))
			% details.ip_ranges_filtered).str();
		
		UISetText(1, filter.c_str());
	}
	else
	{
		UISetText(1, hal::app().res_wstr(HAL_IPFILTER_OFF).c_str());
	}
	
	}
	catch (std::exception& e)
	{
		hal::event().post(shared_ptr<hal::EventDetail>(\
			new hal::EventStdException(hal::Event::info, e, L"updateWindow")));
	}
}

void HaliteWindow::OnTimer(UINT uTimerID)
{		
	if (uTimerID == ID_UPDATE_TIMER) 
	{	
		issueUiUpdate();
	}
	else if (uTimerID == ID_SAVE_TIMER) 
	{
		try
		{
		
		halite().saveIniData();
		hal::bittorrent().saveTorrentData();	
	
		}
		catch (std::exception& e)
		{
			hal::event().post(shared_ptr<hal::EventDetail>(\
				new hal::EventStdException(hal::Event::info, e, L"saveTimer")));
		}
	}
	else 
	{		
		SetMsgHandled(false);
	}	
}	

void HaliteWindow::issueUiUpdate()
{	
	try
	{
	
	const hal::TorrentDetails& torrents = hal::bittorrent().updateTorrentDetails(
		haliteList.manager().selected(), haliteList.manager().allSelected());

	ui_update_signal_(torrents);

	}
	catch (std::exception& e)
	{
		hal::event().post(shared_ptr<hal::EventDetail>(
			new hal::EventStdException(hal::Event::info, e, L"updateTimer")));
	}
}

LRESULT HaliteWindow::OnCopyData(HWND, PCOPYDATASTRUCT pCSD)
{
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"I recieved data.")));
		
	switch (pCSD->dwData)
	{
		case HALITE_SENDING_CMD:
		{	
			wstring filename(static_cast<wchar_t*>(pCSD->lpData), pCSD->cbData/sizeof(wchar_t));
			
			hal::event().post(shared_ptr<hal::EventDetail>(
				new hal::EventMsg((wformat(L"Recieved data: %1%.") % filename), hal::Event::info)));
		
			ProcessFile(filename.c_str());
			break;
		}
		default:
			break;
	}
	return 0;
}

void HaliteWindow::ProcessFile(LPCTSTR lpszPath)
{
	try
	{
	
	wstring saveDirectory = wpath(hal::config().defaultSaveFolder).native_file_string();
	bool startPaused = false;
	bool compactStorage = false;
	
	if (!boost::filesystem::exists(saveDirectory))
		boost::filesystem::create_directory(saveDirectory);
	
	if (hal::config().savePrompt)
	{
		AddTorrentDialog addTorrent(saveDirectory, startPaused, compactStorage);	
		
		if (IDOK != addTorrent.DoModal())
			return;
	}
	
	wpath file(lpszPath, boost::filesystem::native);	
	hal::bittorrent().addTorrent(file, wpath(saveDirectory), startPaused, compactStorage);

	ui().update();

	}
	catch(const boost::filesystem::filesystem_error&)
	{
		hal::event().post(shared_ptr<hal::EventDetail>(
			new hal::EventDebug(hal::Event::warning, L"filesystem error")));
	}
}

void HaliteWindow::OnClose()
{
	if (closeToTray && trayIcon_.IsHidden())
	{		
		ShowWindow(SW_MINIMIZE);
	}
	else
	{
		if (!confirmClose || (confirmClose && 
			MessageBox(hal::app().res_wstr(HAL_WINDOW_CLOSECONFRIM).c_str(), 
				hal::app().res_wstr(HAL_HALITE).c_str(), MB_YESNO) == IDYES))
		{
			DestroyWindow();
		}
	}
}
 
void HaliteWindow::OnDestroy()
{
	splitterPos = m_Split.GetSplitterPos();

	hal::config().save();
	save();
	PostQuitMessage(0);
}

void HaliteWindow::OnSize(UINT type, CSize)
{
	if (type == SIZE_MINIMIZED)
	{
		if (use_tray)
		{
			ShowWindow(SW_HIDE);
			trayIcon_.Show();
		}
	}
	else
	{
		if (trayIcon_.IsShowing())
			trayIcon_.Hide();

		GetWindowRect(rect);
	}
	
	SetMsgHandled(false);
}	

void HaliteWindow::OnMove(CSize)
{
	WINDOWPLACEMENT wnd = { sizeof(WINDOWPLACEMENT ) };
	GetWindowPlacement(&wnd);
	
	if (wnd.showCmd != SW_SHOWMINIMIZED)
		GetWindowRect(rect);

	SetMsgHandled(false);	
}

void HaliteWindow::OnShowWindow(BOOL bShow, UINT nStatus)
{
	SetMsgHandled(false);
}

LRESULT HaliteWindow::OnTrayOpenHalite(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	trayIcon_.Hide();
	ShowWindow(SW_RESTORE);
	
	return 0;
}

LRESULT HaliteWindow::OnTrayExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	PostMessage(WM_CLOSE, 0, 0);
	
	return 0;
}

LRESULT HaliteWindow::OnFileOpen(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CSSFileDialog dlgOpen(TRUE, NULL, NULL, OFN_HIDEREADONLY, L"Torrents (*.torrent)|*.torrent|", m_hWnd);

	if (dlgOpen.DoModal() == IDOK) 
	{
		ProcessFile(dlgOpen.m_ofn.lpstrFile);
	}
	
	return 0;	
}

LRESULT HaliteWindow::OnFileNew(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	WTLx::CRadioPaneDialog newTorrent;	
    newTorrent.DoModal();

//	MessageBox(L"This feature is under development and currently disabled", 
//		L"Feature not availible", 0);
	return 0;
	
	CSSFileDialog dlgOpen(TRUE, NULL, NULL, OFN_HIDEREADONLY, L"All Files|*.*|", m_hWnd);

	if (dlgOpen.DoModal() != IDOK) 
		return 0;
	
	wstring files = dlgOpen.m_ofn.lpstrFile;
	
	CSSFileDialog dlgSave(FALSE, NULL, NULL, OFN_HIDEREADONLY, L"Torrents (*.torrent)|*.torrent|", m_hWnd);
	
	if (dlgSave.DoModal() != IDOK) 
		return 0;
	
	wstring torrent_filename = dlgSave.m_ofn.lpstrFile;
	
	hal::bittorrent().newTorrent(wpath(torrent_filename), wpath(files));
	
	ui().update();
	
	return 0;
}

LRESULT HaliteWindow::OnSettings(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	ConfigOptionsProp sheet(this, L"Settings");	
    sheet.DoModal();
	
	hal::config().settingsChanged();
	setCorrectDialog();
	
	return 0;
}

LRESULT HaliteWindow::OnPauseAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	hal::bittorrent().pauseAllTorrents();
	
	ui().update();
	return 0;
}

LRESULT HaliteWindow::OnResumeAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	hal::bittorrent().unpauseAllTorrents();
	
	ui().update();
	return 0;
}

LRESULT HaliteWindow::OnHelp(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	ConfigOptionsProp sheet(this, L"Settings", 4);	
    sheet.DoModal();
	
	hal::config().settingsChanged();
	
	return 0;
}

LRESULT HaliteWindow::OnViewStatusBar(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	
	UpdateLayout();
	
	return 0;
}	

LRESULT HaliteWindow::OnEraseBkgnd(HDC dc)
{
	return 1;
}

LRESULT HaliteWindow::OnPaint(HDC dc)
{
	return 1;
}

LRESULT HaliteWindow::OnAreYouMe(UINT, WPARAM, LPARAM, BOOL&) 
{
	hal::event().post(shared_ptr<hal::EventDetail>(
		new hal::EventMsg(L"I tried to contact me.")));		

	return WM_AreYouMe_; 
}
