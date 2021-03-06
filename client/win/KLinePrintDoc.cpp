
// KLinePrintDoc.cpp : CKLinePrintDoc 类的实现
//

#include "stdafx.h"
#include "KLinePrint.h"

#include "MainFrm.h"
#include "KLinePrintView.h"
#include "KLinePrintDoc.h"
#include "TickReader.h"
#include "KLineWriter.h"
#include "KLineCollection.h"
#include "CalendarGenerator.h"
#include "Utility.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CKLinePrintDoc

IMPLEMENT_DYNCREATE(CKLinePrintDoc, CDocument)

BEGIN_MESSAGE_MAP(CKLinePrintDoc, CDocument)
	ON_COMMAND(ID_GEN_DAYLINE, &CKLinePrintDoc::OnGenDayline)
	ON_COMMAND(ID_STRATEGY, &CKLinePrintDoc::OnStrategy)
END_MESSAGE_MAP()


// CKLinePrintDoc 构造/析构

CKLinePrintDoc::CKLinePrintDoc()
{
	m_nCurrentTickIdx = 0;
}

CKLinePrintDoc::~CKLinePrintDoc()
{
}

BOOL CKLinePrintDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	return TRUE;
}

BOOL CKLinePrintDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;

	m_Strategy.SetData(&m_1MinData);

	LoadKLineGroup(lpszPathName);

	return TRUE;
}

// CKLinePrintDoc 序列化

void CKLinePrintDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: 在此添加存储代码
	}
	else
	{
		// TODO: 在此添加加载代码
	}
}


// CKLinePrintDoc 诊断

#ifdef _DEBUG
void CKLinePrintDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CKLinePrintDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

// CKLinePrintDoc 命令

void CKLinePrintDoc::LoadKLineGroup(string targetCsvFile)
{
	CKLinePrintView* pView = (CKLinePrintView*)((CMainFrame*)::AfxGetMainWnd())->GetActiveView();

	//	只考虑主力合约，确定分笔数据文件和日线文件
	string tmp;

	tmp = Utility::GetMajorContractPath(targetCsvFile);

	if(!tmp.size()) return;

	m_TradeRecords.clear();

	//	强制平仓，防止价格错乱
	EXCHANGE.SetTick(GetTick());
	EXCHANGE.Close();

	//	重置盈亏.手续费等统计数据
	EXCHANGE.ResetStats();

	m_CurCsvFile = tmp;
	m_CurDayFile = Utility::GetDayLinePath(m_CurCsvFile);

	//	用于交易记录
	EXCHANGE.m_nFilePath = m_CurCsvFile;

	//	读入分笔数据
	m_TickData.clear();

	DWORD before = GetTickCount();
	m_TickReader.Read(m_CurCsvFile, m_TickData);
	DWORD after = GetTickCount();
	TRACE("\ntick file %s read, use %d ticks", m_CurCsvFile.c_str(), after - before);

	int nWeekDay = Utility::GetWeekDayByDate(Utility::GetDateByPath(m_CurCsvFile));

	CString csvFileName(m_CurCsvFile.c_str());
	CString dayFileName(m_CurDayFile.c_str());
	CString title;

	title.Format(_T("%s | %s | 星期%d"), csvFileName, dayFileName, nWeekDay);

	//	更新标题
	SetTitle(title);
	
	::WritePrivateProfileStringA("Files","Current", 
							m_CurCsvFile.c_str(), (Utility::GetProgramPath() + "klinep.ini").c_str()); 
}

void CKLinePrintDoc::ReloadByDate(int nDate)
{
	string tmp = Utility::GetPathByDate(m_CurCsvFile, nDate);

	if(!tmp.size()) return;

	OnOpenDocument(CString(tmp.c_str()));
}

void CKLinePrintDoc::ViewNeighborDate(BOOL bPrev)
{
	string tmp = Utility::GetNeighborCsvFile(m_CurCsvFile, bPrev, TRUE/* 必须是主力合约 */);

	if(!tmp.size()) return;

	OnOpenDocument(CString(tmp.c_str()));

	DisplayTill(-1, -1);
}

BOOL CKLinePrintDoc::LoadNextDay()
{
	// 根据回放配置决定下一个交易日的日期
	int nNextDate = theApp.GetPlaybackDate();

	string tmp = Utility::GetPathByDate(m_CurCsvFile, nNextDate);

	if(!tmp.size()) return FALSE;

	LoadKLineGroup(tmp);
	int nDate = Utility::GetDateByPath(tmp);
	DisplayTill(0, nDate);
	return TRUE;
}

Tick CKLinePrintDoc::GetTick(int nOffset)
{
	Tick tmp;

	memset(&tmp, 0, sizeof(tmp));

	if(m_nCurrentTickIdx + nOffset < m_TickData.size())
		return m_TickData[m_nCurrentTickIdx + nOffset];
	else
	{
		if(!m_TickData.size()) return tmp;

		tmp = m_TickData[m_TickData.size() - 1];
		tmp.time_ms = 0;
		return tmp;
	}
}

void CKLinePrintDoc::PlayTillTime(int nTillMilliTime, bool bTest)
{
	int nDate = Utility::GetDateByPath(m_CurCsvFile);

	Tick lastQuote = m_TickData[m_nCurrentTickIdx];

	if(bTest)
	{
		while(m_nCurrentTickIdx + 1 < m_TickData.size())
		{
			//	检查下一个尚未播放的tick
			Tick tickToQuote = m_TickData[m_nCurrentTickIdx + 1];
			m_1MinData.Quote(tickToQuote);
			EXCHANGE.SetTick(tickToQuote);
			m_Strategy.Quote(tickToQuote);
			m_nCurrentTickIdx++;
		}

		return;
	}

	while(m_nCurrentTickIdx + 1 < m_TickData.size())
	{
		//	检查下一个尚未播放的tick
		Tick tickToQuote = m_TickData[m_nCurrentTickIdx + 1];
				
		//	未到达指定时间，播放
		if(nTillMilliTime == -1 || tickToQuote.time_ms <= nTillMilliTime)
		{
			Tick tmp = tickToQuote;
			tmp.time_ms = nDate;

			m_1MinData.Quote(tickToQuote);

			lastQuote = tickToQuote;
			m_nCurrentTickTime = tickToQuote.time_ms;
			m_nCurrentTickIdx++;

		}
		else	// 越过指定时间
		{
			Tick tmp = lastQuote;

			//	有可能数据缺失，虚拟的Quote一次，不记录持仓变化和成交量
			tmp.time_ms = nTillMilliTime;
			tmp.interest = 0;
			tmp.vol = 0;
			tmp.totalvol = 0;

			m_1MinData.Quote(tmp);

			//	如果相差的时间大于5分钟，应该是盘中休息时间，跳过

			if(tickToQuote.time_ms - nTillMilliTime > 300 * 1000)
				m_nCurrentTickTime = tickToQuote.time_ms - 10 * 1000;
			else
				m_nCurrentTickTime = nTillMilliTime;

			break;
		}
	}
}

void CKLinePrintDoc::DisplayTill(int nTillMilliTime, int nTillDate, bool bTest)
{
	KLineCollection m_DayData;

	m_1MinData.Clear();

	//	当前日期
	int nDate = Utility::GetDateByPath(m_CurCsvFile);

	if(!bTest)
	{
		DWORD before = GetTickCount();
		m_KLineReader.Read(m_CurDayFile, m_DayData, -1);
		DWORD after = GetTickCount();
		TRACE("\nday line %s read, use %d ticks", m_CurDayFile.c_str(), after - before);

		//  获取前一交易日日K线
		KLine prevDayKLine;
		prevDayKLine = m_DayData.GetKLineByTime(CALENDAR.GetPrev(nDate));

		//	不关注前日的开盘价
		prevDayKLine.open = prevDayKLine.close;
		prevDayKLine.start_time = prevDayKLine.time = 0;
		prevDayKLine.vol = prevDayKLine.vol_acc = 0;
		prevDayKLine.interest = 0;
		prevDayKLine.EMA12 = prevDayKLine.EMA26 = prevDayKLine.close;
		prevDayKLine.MACD = prevDayKLine.avgMACD9 = prevDayKLine.MACDDiff = 0;

		m_1MinData.AddKeyPrice(prevDayKLine.avg,  "AVG1");
		m_1MinData.AddKeyPrice(prevDayKLine.close, "C1");
		m_1MinData.AddKeyPrice(prevDayKLine.high, "H1");
		m_1MinData.AddKeyPrice(prevDayKLine.low, "L1");

		m_1MinData.AddKeyPrice(prevDayKLine.ma5, "MA5");
		m_1MinData.AddKeyPrice(prevDayKLine.ma10, "MA10");
		m_1MinData.AddKeyPrice(prevDayKLine.ma20, "MA20");
		m_1MinData.AddKeyPrice(prevDayKLine.ma60, "MA60");

		m_1MinData.AddKeyPrice(prevDayKLine.high5, "H5");
		m_1MinData.AddKeyPrice(prevDayKLine.high10, "H10");
		m_1MinData.AddKeyPrice(prevDayKLine.high20, "H20");
		m_1MinData.AddKeyPrice(prevDayKLine.high60, "H60");

		m_1MinData.AddKeyPrice(prevDayKLine.low5, "L5");
		m_1MinData.AddKeyPrice(prevDayKLine.low10, "L10");
		m_1MinData.AddKeyPrice(prevDayKLine.low20, "L20");
		m_1MinData.AddKeyPrice(prevDayKLine.low60, "L60");

		/* 前日日K */
		m_1MinData.AddToTail(prevDayKLine);
	}
	
	//	生成本日第一条K线
	m_nCurrentTickIdx = 0;

	m_1MinData.SetPeriod(60 * 1000);

	m_1MinData.StartQuote(m_TickData[m_nCurrentTickIdx]);

	if(bTest)
	{
		//	对每tick执行策略
		EXCHANGE.SetTick(m_TickData[m_nCurrentTickIdx]);
		m_Strategy.Quote(m_TickData[m_nCurrentTickIdx]);
	}

	//	记录下当前时间
	m_nCurrentTickTime = m_TickData[m_nCurrentTickIdx].time_ms;

	//	继续生成
	PlayTillTime(nTillMilliTime, bTest);


//	if(!bTest)
	{
		CKLinePrintView* pView = (CKLinePrintView*)((CMainFrame*)::AfxGetMainWnd())->GetActiveView();

		//	设置数据
		pView->Set1MinData(&m_1MinData);

		//	显示
		pView->Render();	
		this->UpdateAllViews(0);
	}
}

void CKLinePrintDoc::OnGenDayline()
{
	int nCurDate, nCurKLineIdx;

	KLineWriter klineWriter;
	KLineCollection klcorg, klc;

	if(!m_CurCsvFile.size()) return;

	if(!CALENDAR.size()) return;

	//	根据最新的日历搜索当前品种所有合约的日线数据，如果缺失则补全
	vector<string> contracts = Utility::GetAllContractPath(m_CurCsvFile);

	for(int i = 0; i < contracts.size(); i++)
	{
		string daylinePath = Utility::GetDayLinePath(contracts[i]);
		
		klc.Clear();
		klcorg.Clear();

		m_KLineReader.Read(daylinePath, klcorg);

		//	从该合约首个交易日开始检查缺失的日线
		//	但未必从首个交易日起就有分笔数据文件

		if(klcorg.size())
			nCurDate = klcorg[0].time;
		else
			nCurDate = CALENDAR.GetFirst();

		nCurKLineIdx = 0;

		//	寻找首个有分笔数据文件的交易日
		TickCollection ticks;

		while(nCurDate > 0 && nCurKLineIdx < klcorg.size())
		{
			string quotefile = Utility::GetPathByDate(contracts[i], nCurDate);

			m_TickReader.Read(quotefile, ticks);

			if(!ticks.size())
			{
				//	不存在分笔数据，只拷贝日线数据
				klc.AddToTail(klcorg[nCurKLineIdx]);
				nCurKLineIdx++;			
			}
			else
			{
				//	首个有分笔数据文件的交易日
				break;
			}

			nCurDate = CALENDAR.GetNext(nCurDate);
		}

		while(nCurDate > 0)
		{
			//	该交易日日线数据存在
			if(nCurKLineIdx < klcorg.size() && nCurDate == klcorg[nCurKLineIdx].time)
			{
				//	添加到最终的数据集中
				klc.AddToTail(klcorg[nCurKLineIdx]);
				nCurKLineIdx++;
			}
			else // 日线不存在
			{
//				TRACE("\ncontract %s missing %d ", contracts[i].c_str(), nCurDate);

				//	从分笔数据生成日线
				KLine kl = GenerateDayLineFromQuoteData(contracts[i], nCurDate);

				//	增加到数据集中
				if(kl.time)
				{
					TRACE("\ncontract %s missing %d ", contracts[i].c_str(), nCurDate);
					klc.AddToTail(kl);
					TRACE("regenerated.");
				}
				else
				{
//					TRACE("generation failed!");
				}
			}

			//	检查各合约在该日是否有日线数据
			nCurDate = CALENDAR.GetNext(nCurDate);

			//	若没有日线数据，则从分笔数据中生成
		}

		//	保存各合约的日线数据到文件，备份原日线数据文件
		klineWriter.Write(daylinePath, klc);
	}

	AfxMessageBox(_T("日线数据补充生成完毕"));

}

KLine CKLinePrintDoc::GenerateDayLineFromQuoteData(string path, int date)
{
	string quotefile = Utility::GetPathByDate(path, date);

	KLine kline;
	TickCollection ticks;
	float totalPrice = 0;

	m_TickReader.Read(quotefile, ticks);

	if(!ticks.size())
	{
		kline.time = 0;
		return kline;
	}

	kline.time = date;
	kline.open = kline.high = kline.low = ticks[0].price;
	kline.close = ticks[ticks.size()-1].price;
	kline.vol = 0;

	for(int i = 0; i < ticks.size(); i++)
	{
		kline.vol += ticks[i].vol;
		totalPrice += (ticks[i].price * ticks[i].vol);

		if(ticks[i].price > kline.high)
			kline.high = ticks[i].price;

		if(ticks[i].price < kline.low)
			kline.low = ticks[i].price;
	}

	kline.avg = (int)(totalPrice / kline.vol);

	return kline;
}

void CKLinePrintDoc::OnStrategy()
{
	char buf[256];
	//	根据回放日期范围，依次回放所有交易日
	int nCurDate = Utility::GetDateByPath(m_CurCsvFile);

	//  TODO : 显示进度信息/可以多线程同时测试
	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	sprintf(buf, "%4d%02d%02d-%02d%02d%02d.log.txt", 
			1900 + timeinfo->tm_year, timeinfo->tm_mon + 1, timeinfo->tm_mday,
			timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_min);

	EXCHANGE.SetLogFile(Utility::GetProgramPath() + "log\\" + buf);

	while(nCurDate > 0)
	{
		string tmp = Utility::GetPathByDate(m_CurCsvFile, nCurDate);

		if(!tmp.size()) return;

		LoadKLineGroup(tmp);
		int nDate = Utility::GetDateByPath(tmp);
		DisplayTill(0, nDate, true);

		EXCHANGE.ResetStats();

		nCurDate = PBCAL.GetNext(nCurDate);
	}	

	EXCHANGE.SetLogFile(Utility::GetProgramPath() + "log\\manual.log.txt");
}
