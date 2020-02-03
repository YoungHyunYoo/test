/*
 * Oreka -- A media capture and retrieval platform
 * 
 * Copyright (C) 2005, orecx LLC
 *
 * http://www.orecx.com
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License.
 * Please refer to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "Utils.h"
#include "OrkClient.h"
#include "ace/INET_Addr.h"
#include "ace/SOCK_Connector.h"
#include "ace/SOCK_Stream.h"
#include "LogManager.h"
 
#ifdef WIN32
	#define IOV_TYPE char*
#else
	#define IOV_TYPE void*	
#endif

time_t OrkClient::s_lastErrorReportedTime = 0;

OrkClient::OrkClient()
{
	m_log = OrkLogManager::Instance()->clientLog;
}

void OrkClient::LogError(CStdString& logMsg)
{
	if((time(NULL) - s_lastErrorReportedTime) > 60)
	{
		s_lastErrorReportedTime = time(NULL);
		LOG4CXX_ERROR(m_log, logMsg);
	}
}

bool OrkHttpClient::ExecuteUrl(const CStdString& request, CStdString& response, const CStdString& hostname, const int tcpPort, int timeout)
{
//printf("___url1\n");
//printf("___request :\n%s\n",request.c_str());
//printf("%s\n",hostname.c_str());
//printf("___response :\n%s\n",response.c_str());
	CStdString logMsg;
	response = "";
	ACE_SOCK_Connector  connector;
	ACE_SOCK_Stream peer;
	ACE_INET_Addr peer_addr;
	if(timeout < 1){timeout = 1;}
	ACE_Time_Value aceTimeout (timeout);
	CStdString requestDetails;
	requestDetails.Format("timeout:%d http://%s:%d/%s", timeout, hostname, tcpPort, request);
	time_t beginRequestTimestamp = time(NULL);

	char szTcpPort[10];
	sprintf(szTcpPort, "%d", tcpPort);
	iovec iov[8];
	iov[0].iov_base = (IOV_TYPE)"GET ";
	iov[0].iov_len = 4; // Length of "GET ".
	iov[1].iov_base = (PSTR)(PCSTR)request;
	iov[1].iov_len = request.size();
	iov[2].iov_base = (IOV_TYPE)" HTTP/1.0\r\n";
	iov[2].iov_len = 11;
	iov[3].iov_base = (IOV_TYPE)"Host: ";
	iov[3].iov_len = 6;
	iov[4].iov_base = (PSTR)(PCSTR)hostname;
	iov[4].iov_len = hostname.size();
	iov[5].iov_base = (IOV_TYPE)":";
	iov[5].iov_len = 1;
	iov[6].iov_base = szTcpPort;
	iov[6].iov_len = strlen(szTcpPort);
	iov[7].iov_base = (IOV_TYPE)"\r\n\r\n";
	iov[7].iov_len = 4;

	if (peer_addr.set (tcpPort, (PCSTR)hostname) == -1) // __ not run
	{
//printf("___2\n");
		logMsg.Format("peer_addr.set()  errno=%d %s", errno, requestDetails);
		LogError(logMsg);
		return false;
	}
	else if (connector.connect (peer, peer_addr, &aceTimeout) == -1) // __ not run
	{
//printf("___3\n");
		if (errno == ETIME)
		{
		}
		logMsg.Format("connector.connect()  errno=%d %s", errno, requestDetails);
		LogError(logMsg);
		return false;
	}
	else if (peer.sendv_n (iov, 8, &aceTimeout) == -1) // __ not run
	{
//printf("___4\n");
		logMsg.Format("peer.sendv_n  errno=%d %s", errno, requestDetails);
		LogError(logMsg);
		return false;
	}
//printf("\n\n__Reporting send success\n");
	ssize_t numReceived = 0;
#define BUFSIZE 4096
	char buf [BUFSIZE];

	CStdString header;
	bool gotHeader = false;
	while ( ((numReceived = peer.recv (buf, BUFSIZE, &aceTimeout)) > 0)  && ((time(NULL) - beginRequestTimestamp) <= timeout) )
	{
//printf("\n\n__recv buf : %s\n",buf);
		for(int i=0; i<numReceived; i++)
		{
			if(!gotHeader)
			{
//printf("%c",buf[i]);
				// extract header (delimited by CR-LF-CR-LF)
				header += buf[i];
				size_t headerSize = header.size();
				if (headerSize > 4 &&
					header.GetAt(headerSize-1) == '\n' && 
					header.GetAt(headerSize-2) == '\r' &&
					header.GetAt(headerSize-3) == '\n' &&
					header.GetAt(headerSize-4) == '\r'		)
				{
					gotHeader = true;
				}
			}
			else
			{
//printf("%c",buf[i]);
				// extract content
				response += buf[i];
			}
		}
	}
	peer.close();


//printf("___response :\n%s\n",response.c_str());
	logMsg.Format("%s:%d response:%s",hostname, tcpPort, response);
	LOG4CXX_DEBUG(m_log, logMsg);
	if(numReceived < 0) // __ not run
	{
printf("___5\n");
		logMsg.Format("numReceived:%d %s", numReceived, requestDetails);
		LogError(logMsg);
		return false;
	}
	if(header.size() > 15 && header.GetAt(9) == '4' && header.GetAt(10) == '0' && header.GetAt(11) == '0') // __ not run
	{
printf("___6\n");
		logMsg.Format("HTTP header:%s ** request:%s\nIgnore this message", header, requestDetails);
		LOG4CXX_ERROR(m_log, logMsg);
		return true;
	}
	if(header.size() < 15 || response.size() <= 0) // __ not run
	{
//printf("___7\n");
		logMsg.Format("HTTP header:%s ** request:%s ** response:%s ** header size:%d  response size:%d", header, requestDetails, response, header.size(), response.size());
		LogError(logMsg);
		return false;
	}
	if(	header.GetAt(9) != '2' ||
		header.GetAt(10) != '0' ||
		header.GetAt(11) != '0' ||
		header.GetAt(12) != ' ' ||
		header.GetAt(13) != 'O' ||
		header.GetAt(14) != 'K'		) // __ not run
	{
printf("___8\n");
		logMsg.Format("HTTP header:%s ** request:%s", header, requestDetails);
		LogError(logMsg);
		return false;
	}
//printf("___9\n");
//printf("___response :\n%s\n",response.c_str());
	return true;
}

bool OrkHttpSingleLineClient::Execute(SyncMessage& request, AsyncMessage& response, const CStdString& hostname,const int tcpPort, const CStdString& serviceName, const int timeout)
{
//printf("___1\n");
	CStdString requestString = "/" + serviceName + "/command?";
	requestString += request.SerializeUrl();
//printf("___2\n");
	CStdString responseString;
	if (ExecuteUrl(requestString, responseString, hostname, tcpPort, timeout))
	{
//printf("___3\n");
		response.DeSerializeSingleLine(responseString);
		return true;
	}
//printf("___4\n");
	return false; 
}

