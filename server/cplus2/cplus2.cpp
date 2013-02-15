// cplus2.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"


//
// TCPServerTest.cpp
//
// $Id: //poco/1.4/Net/testsuite/src/TCPServerTest.cpp#1 $
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#pragma pack(1)

#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnection.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Thread.h"
#include <iostream>
#include <string>

using namespace std;


using Poco::Net::TCPServer;
using Poco::Net::TCPServerConnection;
using Poco::Net::TCPServerConnectionFactory;
using Poco::Net::TCPServerConnectionFactoryImpl;
using Poco::Net::TCPServerParams;
using Poco::Net::StreamSocket;
using Poco::Net::ServerSocket;
using Poco::Net::SocketAddress;
using Poco::Thread;

#define TW_MAGIC				0xFDFDFDFD
#define	TW_MAGIC_BYTE			0xFD
#define TW_LENGTH_LEN			8

typedef struct _tw_header_t {
	DWORD m_magic;					// 0xFDFDFDFD
	char m_length[TW_LENGTH_LEN];
	char m_end;						// always 0
} TW_HEADER;

typedef struct _tw_login_t {
	TW_HEADER	m_header;			//
	BYTE m_tag;						// 0x0A
	WORD m_name_len;
	char m_data[256];				// (NAME)(WORD m_passwd_len)(PASSWD)
} TW_LOGIN;

typedef struct _tw_ans_t {
	TW_HEADER	m_header;			//
	BYTE	m_tag1;					// second data type
	BYTE	m_tag2;					// data type
	BYTE	m_serial;				// request serial, max 0x7f
	BYTE	m_reserved;				// always 0x00
	SHORT	m_size;					// request data's size
} TW_ANS;

typedef struct tagSTOCK_STRUCTEx {
	BYTE	m_type;					// stock's type, see enum StockType
	char	m_code[6];				// stock code
} STOCK_STRUCTEx,*pSTOCK_STRUCTEx;

typedef	STOCK_STRUCTEx	TW_STOCK;

typedef struct _tw_ask_t {
	TW_HEADER	m_header;			//
	BYTE	m_tag1;					// second data type
	BYTE	m_tag2;					// data type
	BYTE	m_serial;				// request serial, max 0x7f
	BYTE	m_reserved;				// always 0x00
	SHORT	m_size;					// request data's size
	TW_STOCK	m_stocks[32];		// max 32 stocks
} TW_ASK;

typedef struct _tw_ans_init_t {
	BYTE	m_tag;					// = 0xfd
	CHAR	m_name[8];
	BYTE	m_type;
	CHAR	m_code[6];
	DWORD	m_lastclose;			// 昨收 0.001
	DWORD	m_reserved2;
	CHAR	m_shortname[4];
} TW_ANS_INIT;

typedef struct _tw_ans_report_t {
	WORD	m_number;				// No.
	DWORD	m_volnow;				// 现手（单位为股）
	DWORD	m_open;					// 0.001
	DWORD	m_high;					// 0.001
	DWORD	m_low;					// 0.001
	DWORD	m_new;					// 0.001
	DWORD	m_volume;
	DWORD	m_amount;
	DWORD	m_buy1;					// 0.001
	DWORD	m_buy1vol;
	DWORD	m_buy2;					// 0.001
	DWORD	m_buy2vol;
	DWORD	m_buy3;					// 0.001
	DWORD	m_buy3vol;
	DWORD	m_sell1;				// 0.001
	DWORD	m_sell1vol;
	DWORD	m_sell2;				// 0.001
	DWORD	m_sell2vol;
	DWORD	m_sell3;				// 0.001
	DWORD	m_sell3vol;
	WORD	m_reserved;				// = 0x64 0x00
} TW_ANS_REPORT;


typedef struct _tw_detail_t {
	WORD	m_offset;
	DWORD	m_price;				// 0.001
	DWORD	m_volume;
	DWORD	m_buy;					// 0.001
	DWORD	m_sell;					// 0.001
} TW_DETAIL;

typedef struct _tw_minute_t {
	DWORD	m_data1;				// * 0.001 if price
	DWORD	m_data2;				// * 0.001 if price
} TW_MINUTE;

typedef struct _tw_ans_minute_t {
	TW_DETAIL	m_details[11];
	WORD	m_offset;
	DWORD	m_bargain;
	DWORD	m_outter;
	DWORD	m_innter;
	DWORD	m_open;					// 0.001
	DWORD	m_high;					// 0.001
	DWORD	m_low;					// 0.001
	DWORD	m_new;					// 0.001
	DWORD	m_volume;
	DWORD	m_amount;
	DWORD	m_buy1;					// 0.001
	DWORD	m_buy1vol;
	DWORD	m_buy2;					// 0.001
	DWORD	m_buy2vol;
	DWORD	m_buy3;					// 0.001
	DWORD	m_buy3vol;
	DWORD	m_sell1;				// 0.001
	DWORD	m_sell1vol;
	DWORD	m_sell2;				// 0.001
	DWORD	m_sell2vol;
	DWORD	m_sell3;				// 0.001
	DWORD	m_sell3vol;
	WORD	m_reserved;				// = 0x64 0x00
	TW_MINUTE	m_minutes[246];
} TW_ANS_MINUTE;


int ConstructLength( TW_HEADER & header, int len )
{
	string str = itoa(len,header.m_length,16);
	int nZeros = TW_LENGTH_LEN-str.size();
	int i;
	for( i=0; i<nZeros; i++ )
		
	for( i=0; i<TW_LENGTH_LEN; i++ )
	{
		if( i< nZeros )
			header.m_length[i] = '0';
		else
			header.m_length[i] = str[i-nZeros];
	}
	return len;
}

namespace
{
	class EchoConnection: public TCPServerConnection
	{
	public:
		EchoConnection(const StreamSocket& s): TCPServerConnection(s)
		{
		}
		
		void run()
		{
			StreamSocket& ss = socket();
			try
			{
				char buffer[1024];
				
				int n = ss.receiveBytes(buffer, sizeof(buffer));

				//	登录
				TW_LOGIN* pLogin = (TW_LOGIN*)buffer;

				char name[64] = {0};
				char pass[64] = {0};

				memcpy(name, pLogin->m_data, pLogin->m_name_len);

				WORD passLen = *(WORD*)(pLogin->m_data + pLogin->m_name_len);
				memcpy(pass, pLogin->m_data + pLogin->m_name_len + sizeof(WORD), passLen);

				printf("login! user %s pass %s\n", name,pass);

				TW_ANS ack;

				ack.m_header.m_magic = TW_MAGIC;
				ack.m_tag1 = ack.m_tag2 = 0;

				ss.sendBytes(&ack, sizeof(ack));

				while (n > 0)
				{
					n = ss.receiveBytes(buffer, sizeof(buffer));
					printf("receive %d bytes.\n", n);

					TW_HEADER* pHead = (TW_HEADER*)buffer;
					if(pHead->m_magic != TW_MAGIC)
					{
						printf("unknown packet with no magic.\n");
					}

					BYTE tag1 = *(BYTE*)(buffer + sizeof(TW_HEADER));
					BYTE tag2 = *(BYTE*)(buffer + sizeof(TW_HEADER)+ 1);

					if(tag1 == 0x0 && tag2 == 0x03)			//	请求分笔数据
					{	
						TW_ASK* pAsk = (TW_ASK*)buffer;

						printf("ask report! serial %d, size %d\n", pAsk->m_serial, pAsk->m_size);

						size_t dataoffset = 43;

						int nTotalLen = dataoffset + sizeof(TW_ANS_REPORT) * 2;

						char* pbuffer = new char[nTotalLen];

						memset(pbuffer, 0, nTotalLen); 
	
						TW_ANS * pans = (TW_ANS*)pbuffer;

						pans->m_header.m_magic = TW_MAGIC;
						pans->m_serial = pAsk->m_serial;

						ConstructLength(pans->m_header, nTotalLen - sizeof(TW_HEADER));

						pans->m_tag1 = 0x0;
						pans->m_tag2 = 0x3;

						TW_ANS_REPORT * preport = (TW_ANS_REPORT*)(pbuffer + dataoffset);

						preport[0].m_new = 90000000;
						preport[1].m_new = 45360000;
			
						ss.sendBytes(pbuffer, nTotalLen);
						delete []pbuffer;

						for(int i = 0; i < pAsk->m_size; i++)
						{
							printf(" code %6.6s type %d\n", 
									pAsk->m_stocks[i].m_code,
									pAsk->m_stocks[i].m_type);
						}

					}
					else if(tag1 == 0x01 && tag2== 0x04)	//	请求分钟数据
					{	
						TW_ASK* pAsk = (TW_ASK*)buffer;

						printf("ask minute! serial %d, size %d code %6.6s\n", pAsk->m_serial, pAsk->m_size, pAsk->m_stocks[0].m_code);

						size_t dataoffset = 43;

						int nTotalLen = dataoffset + sizeof(TW_ANS_MINUTE);

						char* pbuffer = new char[nTotalLen];

						memset(pbuffer, 0, nTotalLen); 
	
						TW_ANS * pans = (TW_ANS*)pbuffer;

						pans->m_header.m_magic = TW_MAGIC;
						pans->m_serial = pAsk->m_serial;

						ConstructLength(pans->m_header, nTotalLen - sizeof(TW_HEADER));

						pans->m_tag1 = 0x1;
						pans->m_tag2 = 0x4;

						TW_ANS_MINUTE * pminute = (TW_ANS_MINUTE*)(pbuffer + dataoffset);

						for(int i = 0; i < 246; i++)
						{
							pminute->m_offset = 100;
							pminute->m_minutes[i].m_data1 = 1000000 + 100*i;
							pminute->m_minutes[i].m_data2 = 70000 + 37*i;
						}
			
						ss.sendBytes(pbuffer, nTotalLen);
						delete []pbuffer;
					}
					else if(tag2 == 0x09)					//	请求历史数据
					{
						printf("ask history!\n");

						switch(tag1 & 0xF0)
						{
						case 0x30:
							printf("min5 history asked.\n");
							break;
						case 0x40:
							printf("min15 history asked.\n");
							break;
						case 0x50:
							printf("min30 history asked.\n");
							break;
						case 0x60:
							printf("min60 history asked.\n");
							break;
						case 0x10:
							printf("day history asked.\n");
							break;
						case 0x80:
							printf("week history asked.\n");
							break;
						case 0x90:
							printf("month history asked.\n");
							break;
						}
					}
					else if(tag1 == 0x1 && tag2 == 0x1)
					{
						TW_ASK* pAsk = (TW_ASK*)buffer;

						DWORD lastest = pAsk->m_stocks[0].m_type;

						printf("ask! local lastest date %d\n", lastest);

						size_t dataoffset = 469;

						int nTotalLen = dataoffset + sizeof(TW_ANS_INIT) * 2;

						char* pbuffer = new char[nTotalLen];

						memset(pbuffer, 0, nTotalLen); 

						(*(DWORD*)(pbuffer+45))=20130214;
						
						TW_ANS * pans = (TW_ANS*)pbuffer;

						pans->m_header.m_magic = TW_MAGIC;

						ConstructLength(pans->m_header, nTotalLen - sizeof(TW_HEADER));

						pans->m_tag1 = 0x2;
						pans->m_tag2 = 0x1;

						TW_ANS_INIT * pinit = (TW_ANS_INIT*)(pbuffer + dataoffset);

						pinit[0].m_tag = TW_MAGIC_BYTE;
						sprintf(pinit[0].m_code,"%6.6s","999999");
						sprintf(pinit[0].m_name,"%s","QQQ");

						pinit[1].m_tag = TW_MAGIC_BYTE;
						sprintf(pinit[1].m_code,"%6.6s","888666");
						sprintf(pinit[1].m_name,"%s","PTA05");
						
						ss.sendBytes(pbuffer, nTotalLen);
						delete []pbuffer;
					}
					else
					{
						printf("unknown tag 0X%02X 0X%02X\n", tag1, tag2);
					}
										
				}
			}
			catch (Poco::Exception& exc)
			{
				std::cerr << "EchoConnection: " << exc.displayText() << std::endl;
			}
		}
	};
}

int _tmain(int argc, _TCHAR* argv[])
{
	ServerSocket svs(8001);
	TCPServer srv(new TCPServerConnectionFactoryImpl<EchoConnection>(), svs);
	srv.start();

#if 0
	assert (srv.currentConnections() == 0);
	assert (srv.currentThreads() == 0);
	assert (srv.queuedConnections() == 0);
	assert (srv.totalConnections() == 0);
#endif

	while(1) Thread::sleep(1000);

	SocketAddress sa("localhost", svs.address().port());
	StreamSocket ss1(sa);
	std::string data("hello, world");
	ss1.sendBytes(data.data(), (int) data.size());
	char buffer[256];
	int n = ss1.receiveBytes(buffer, sizeof(buffer));

#if 0
	assert (n > 0);
	assert (std::string(buffer, n) == data);
	assert (srv.currentConnections() == 1);
	assert (srv.currentThreads() == 1);
	assert (srv.queuedConnections() == 0);
	assert (srv.totalConnections() == 1);
#endif

	ss1.close();
	Thread::sleep(300);
//	assert (srv.currentConnections() == 0);
	return 0;
}

